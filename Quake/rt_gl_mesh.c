/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2009 John Fitzgibbons and others
Copyright (C) 2010-2014 QuakeSpasm developers
Copyright (C) 2016 Axel Gneiting

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// rt_gl_mesh.c: triangle model functions (RayTracedGL1 renderer)
//
// The vanilla 1.36 renderer meshes an alias model into Vulkan VBOs hanging off
// aliashdr_t. Under RT_RENDERER we instead build CPU-side arrays on qmodel_t that
// are streamed to RTGL1 every frame by rt_r_alias.c:
//
//   m->rtindices  : uint32_t[hdr->numindexes], winding reversed vs. the MDL data
//   m->rtvertices : RgVertex[hdr->numposes * hdr->numverts_vbo]
//                   pose p, vbo vertex v  ->  m->rtvertices[p * hdr->numverts_vbo + v]
//                   position = raw trivertx_t.v (unscaled; scale/scale_origin are applied
//                   by the entity transform in rt_r_alias.c), normal = r_avertexnormals[],
//                   texCoord = (st + 0.5) / skin size, packedColor = white.
//
// MD5 replacement models (the 2021 re-release's enhanced monsters, weapons and items) keep
// a CPU copy per surface in hdr->rtskinned (see GLMesh_UploadBuffers); RT_SkinAliasSurface
// skins it on the CPU every frame with the same maths as the vanilla md5.vert shader.
// MD3 is not supported under RT (its GLMesh_UploadBuffers call is a no-op).

#include "quakedef.h"

#define NUMVERTEXNORMALS 162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];

extern cvar_t rt_classic_render;

// gl_model.c model registry (not exported through a header in 1.36)
extern qmodel_t mod_known[MAX_MODELS];
extern int		mod_numknown;

/*
================
RT_FreeAliasMesh

Free the per-model RT arrays.
================
*/
static void RT_FreeAliasMesh (qmodel_t *m)
{
	if (m->rtvertices != NULL)
	{
		Mem_Free (m->rtvertices);
		m->rtvertices = NULL;
	}

	if (m->rtindices != NULL)
	{
		Mem_Free (m->rtindices);
		m->rtindices = NULL;
	}
}

/*
================
RT_BuildAliasMesh

Build m->rtindices / m->rtvertices from the deduplicated mesh description.
Mirrors the fork's GLMesh_LoadVertexBuffer, but reads the poses straight from
gl_model.c's poseverts[] instead of a posedata copy inside aliashdr_t.

Original code by MH from RMQEngine
================
*/
static void RT_BuildAliasMesh (qmodel_t *m, const aliashdr_t *hdr, const aliasmesh_t *desc, const unsigned short *indexes)
{
	RT_FreeAliasMesh (m);

	if (isDedicated)
		return;
	if (!hdr->numindexes)
		return;
	if (!hdr->numposes)
		return;
	if (!hdr->numverts_vbo)
		return;

	// create and fill the 32-bit index buffer (reversed winding for RTGL1)
	{
		m->rtindices = (uint32_t *)Mem_Alloc (hdr->numindexes * sizeof (uint32_t));
		assert (hdr->numindexes % 3 == 0);

		for (int k = 0; k < hdr->numindexes / 3; k++)
		{
			m->rtindices[k * 3 + 0] = indexes[k * 3 + 2];
			m->rtindices[k * 3 + 1] = indexes[k * 3 + 1];
			m->rtindices[k * 3 + 2] = indexes[k * 3 + 0];
		}
	}

	// create the vertex buffer (Mem_Alloc returns zeroed memory)
	{
		size_t sz = (size_t)hdr->numposes * ((size_t)hdr->numverts_vbo * sizeof (RgVertex));
		m->rtvertices = (RgVertex *)Mem_Alloc (sz);
	}

	// fill in the vertices, one block of numverts_vbo per pose
	for (int p = 0; p < hdr->numposes; p++)
	{
		RgVertex		 *dstpose = m->rtvertices + ((size_t)hdr->numverts_vbo * p);
		const trivertx_t *srctv = poseverts[p];

		for (int v = 0; v < hdr->numverts_vbo; v++)
		{
			trivertx_t trivert = srctv[desc[v].vertindex];

			dstpose[v].position[0] = trivert.v[0];
			dstpose[v].position[1] = trivert.v[1];
			dstpose[v].position[2] = trivert.v[2];

			dstpose[v].normal[0] = r_avertexnormals[trivert.lightnormalindex][0];
			dstpose[v].normal[1] = r_avertexnormals[trivert.lightnormalindex][1];
			dstpose[v].normal[2] = r_avertexnormals[trivert.lightnormalindex][2];

			// texCoord is same in all poses
			dstpose[v].texCoord[0] = ((float)desc[v].st[0] + 0.5f) / (float)hdr->skinwidth;
			dstpose[v].texCoord[1] = ((float)desc[v].st[1] + 0.5f) / (float)hdr->skinheight;

			dstpose[v].packedColor = RT_PACKED_COLOR_WHITE;
		}
	}
}

/*
================
GL_MakeAliasModelDisplayLists

Called from Mod_LoadAliasModel once stverts[] / triangles[] / poseverts[] are populated.
Vertex dedup identical to 1.36's gl_mesh.c; the result is stored on the qmodel_t as
RT arrays instead of being uploaded to a Vulkan VBO.

Original code by MH from RMQEngine
================
*/
static uint32_t AliasMeshHash (const void *const p)
{
	aliasmesh_t *mesh = (aliasmesh_t *)p;
	uint32_t	 vertindex = mesh->vertindex;
	return HashCombine (HashInt32 (&vertindex), HashCombine (HashFloat (&mesh->st[0]), HashFloat (&mesh->st[1])));
}

void GL_MakeAliasModelDisplayLists (qmodel_t *m, aliashdr_t *paliashdr)
{
	assert (paliashdr->poseverttype == PV_QUAKE1);

	Con_DPrintf2 ("meshing %s...\n", m->name);

	// there can never be more than this number of verts
	const int maxverts_vbo = paliashdr->numtris * 3;
	TEMP_ALLOC_ZEROED (aliasmesh_t, desc, maxverts_vbo);
	// there will always be this number of indexes
	TEMP_ALLOC_ZEROED (unsigned short, indexes, maxverts_vbo);

	hash_map_t *vertex_to_index_map = HashMap_Create (aliasmesh_t, unsigned short, &AliasMeshHash, NULL);
	HashMap_Reserve (vertex_to_index_map, maxverts_vbo);

	paliashdr->numindexes = 0;
	paliashdr->numverts_vbo = 0;

	ZEROED_STRUCT (aliasmesh_t, mesh);
	for (int i = 0; i < paliashdr->numtris; i++)
	{
		for (int j = 0; j < 3; j++)
		{
			// index into the pose vertex arrays
			unsigned short vertindex = triangles[i].vertindex[j];

			// basic s/t coords
			int s = stverts[vertindex].s;
			int t = stverts[vertindex].t;

			// check for back side and adjust texcoord s
			if (!triangles[i].facesfront && stverts[vertindex].onseam)
				s += paliashdr->skinwidth / 2;

			mesh.st[0] = s;
			mesh.st[1] = t;
			mesh.vertindex = vertindex;

			// Check if this vert already exists
			unsigned short	index;
			unsigned short *found_index;
			if ((found_index = HashMap_Lookup (unsigned short, vertex_to_index_map, &mesh)))
				index = *found_index;
			else
			{
				// doesn't exist; emit a new vert and index
				index = paliashdr->numverts_vbo;
				HashMap_Insert (vertex_to_index_map, &mesh, &index);
				desc[paliashdr->numverts_vbo].vertindex = vertindex;
				desc[paliashdr->numverts_vbo].st[0] = s;
				desc[paliashdr->numverts_vbo++].st[1] = t;
			}

			indexes[paliashdr->numindexes++] = index;
		}
	}

	HashMap_Destroy (vertex_to_index_map);

	// build the RT arrays immediately (poseverts[] is only valid during load)
	paliashdr->poseverttype = PV_QUAKE1;
	RT_BuildAliasMesh (m, paliashdr, desc, indexes);

	TEMP_FREE (indexes);
	TEMP_FREE (desc);
}

static void RT_FreeSkinnedMesh (aliashdr_t *hdr)
{
	rt_skinnedmesh_t *sk = hdr->rtskinned;
	if (!sk)
		return;
	Mem_Free (sk->indices);
	Mem_Free (sk->vertexes);
	Mem_Free (sk->joints);
	Mem_Free (sk);
	hdr->rtskinned = NULL;
}

/*
================
GLMesh_UploadBuffers

1.36 entry point used by the MD3/MD5 loaders once per surface. The loaders free their
vertex and joint arrays afterwards, so under RT an MD5 surface is copied into
hdr->rtskinned for RT_SkinAliasSurface. MD3 is not meshed for RT.
================
*/
void GLMesh_UploadBuffers (
	qmodel_t *mod, aliashdr_t *hdr, unsigned short *indexes, byte *vertexes, aliasmesh_t *desc, jointpose_t *joints, unsigned short *skeleton_indexes,
	int num_skeleton_indexes)
{
	(void)mod;
	(void)desc;
	(void)skeleton_indexes;
	(void)num_skeleton_indexes;

	if (isDedicated)
		return;
	if (hdr->poseverttype != PV_MD5 && hdr->poseverttype != PV_MD5_8)
		return;
	// no .md5anim: the loader has no joint matrices to give us, so let the .mdl draw instead
	if (!joints || hdr->numframes <= 0 || hdr->numjoints <= 0 || !hdr->numindexes || !hdr->numverts)
		return;

	RT_FreeSkinnedMesh (hdr);

	const size_t vertex_size = (hdr->poseverttype == PV_MD5_8) ? sizeof (md5vert8_t) : sizeof (md5vert_t);

	rt_skinnedmesh_t *sk = (rt_skinnedmesh_t *)Mem_Alloc (sizeof (rt_skinnedmesh_t));
	sk->numverts = hdr->numverts;
	sk->numindexes = hdr->numindexes;
	sk->numjoints = hdr->numjoints;
	sk->numposes = hdr->numframes;

	sk->indices = (uint32_t *)Mem_Alloc (sizeof (uint32_t) * sk->numindexes);
	for (int k = 0; k < sk->numindexes / 3; k++)
	{
		sk->indices[k * 3 + 0] = indexes[k * 3 + 2];
		sk->indices[k * 3 + 1] = indexes[k * 3 + 1];
		sk->indices[k * 3 + 2] = indexes[k * 3 + 0];
	}

	sk->vertexes = (byte *)Mem_Alloc (vertex_size * sk->numverts);
	memcpy (sk->vertexes, vertexes, vertex_size * sk->numverts);

	sk->joints = (jointpose_t *)Mem_Alloc (sizeof (jointpose_t) * sk->numjoints * sk->numposes);
	memcpy (sk->joints, joints, sizeof (jointpose_t) * sk->numjoints * sk->numposes);

	hdr->rtskinned = sk;
}

/*
================
RT_SkinAliasSurface

CPU version of md5.vert + skinning.inc: skin an MD5 surface between two animation poses.
Skinning is linear in the joint matrices, so blending the two poses' matrices first and
skinning once gives the same result as the shader's mix of two skinned positions.
Returns a scratch buffer valid until the next call.
================
*/
static float RT_AvertexNormalDot (const float *n, const float *shadevector)
{
	float dot = DotProduct (n, shadevector);
	return dot < 0.0f ? 1.0f + dot * (13.0f / 44.0f) : 1.0f + dot;
}

const RgVertex *RT_SkinAliasSurface (const aliashdr_t *hdr, int pose1, int pose2, float blend, const float *shadevector, const float *lightcolor)
{
	static RgVertex	   *out = NULL;
	static size_t		out_numverts = 0;
	static jointpose_t *mats = NULL;
	static size_t		mats_numjoints = 0;

	const rt_skinnedmesh_t *sk = hdr->rtskinned;
	assert (sk != NULL);

	if ((size_t)sk->numverts > out_numverts)
	{
		out_numverts = (sk->numverts + 4095) & ~4095;
		Mem_Free (out);
		out = (RgVertex *)Mem_Alloc (out_numverts * sizeof (RgVertex));
	}
	if ((size_t)sk->numjoints > mats_numjoints)
	{
		mats_numjoints = (sk->numjoints + 255) & ~255;
		Mem_Free (mats);
		mats = (jointpose_t *)Mem_Alloc (mats_numjoints * sizeof (jointpose_t));
	}

	pose1 = CLAMP (0, pose1, sk->numposes - 1);
	pose2 = CLAMP (0, pose2, sk->numposes - 1);

	const jointpose_t *j1 = sk->joints + (size_t)pose1 * sk->numjoints;
	const jointpose_t *j2 = sk->joints + (size_t)pose2 * sk->numjoints;
	for (int j = 0; j < sk->numjoints; j++)
		for (int k = 0; k < 12; k++)
			mats[j].mat[k] = j1[j].mat[k] + (j2[j].mat[k] - j1[j].mat[k]) * blend;

	const qboolean vertex_lighting = CVAR_TO_BOOL (rt_classic_render);
	const qboolean eight = (hdr->poseverttype == PV_MD5_8);
	const int	   count = eight ? NUM_JOINT_INFLUENCES_8_WEIGHT : NUM_JOINT_INFLUENCES_4_WEIGHT;

	for (int v = 0; v < sk->numverts; v++)
	{
		const byte	*weights, *indices;
		const float *px, *py, *pz, *norm, *st;
		if (eight)
		{
			const md5vert8_t *in = (const md5vert8_t *)sk->vertexes + v;
			weights = in->joint_weights, indices = in->joint_indices;
			px = in->joint_position_x, py = in->joint_position_y, pz = in->joint_position_z;
			norm = in->norm, st = in->st;
		}
		else
		{
			const md5vert_t *in = (const md5vert_t *)sk->vertexes + v;
			weights = in->joint_weights, indices = in->joint_indices;
			px = in->joint_position_x, py = in->joint_position_y, pz = in->joint_position_z;
			norm = in->norm, st = in->st;
		}

		float weight_sum = 0.0f;
		for (int i = 0; i < count; i++)
			weight_sum += weights[i];
		const float weight_scale = weight_sum > 0.0f ? 1.0f / weight_sum : 0.0f;

		vec3_t pos = {0, 0, 0}, nrm = {0, 0, 0};
		for (int i = 0; i < count; i++)
		{
			const float w = weights[i] * weight_scale;
			const float *m = mats[indices[i] < sk->numjoints ? indices[i] : 0].mat;
			for (int r = 0; r < 3; r++)
			{
				pos[r] += m[r * 4 + 0] * px[i] + m[r * 4 + 1] * py[i] + m[r * 4 + 2] * pz[i] + m[r * 4 + 3] * w;
				nrm[r] += w * (m[r * 4 + 0] * norm[0] + m[r * 4 + 1] * norm[1] + m[r * 4 + 2] * norm[2]);
			}
		}
		VectorNormalize (nrm);

		RgVertex *dst = &out[v];
		dst->position[0] = pos[0];
		dst->position[1] = pos[1];
		dst->position[2] = pos[2];
		dst->normal[0] = nrm[0];
		dst->normal[1] = nrm[1];
		dst->normal[2] = nrm[2];
		dst->texCoord[0] = st[0];
		dst->texCoord[1] = st[1];

		if (vertex_lighting)
		{
			const float dot = RT_AvertexNormalDot (nrm, shadevector);
			dst->packedColor = RT_PackColorToUint32_FromFloat01 (lightcolor[0] * dot, lightcolor[1] * dot, lightcolor[2] * dot, 1.0f);
		}
		else
		{
			dst->packedColor = RT_PACKED_COLOR_WHITE;
		}
	}

	return out;
}

/*
================
GLMesh_DeleteMeshBuffers

1.36 signature takes only the aliashdr_t. MD5 surfaces own their hdr->rtskinned copy;
the .mdl arrays live on the qmodel_t, so the owning model is looked up by its PV_QUAKE1
extradata pointer.
================
*/
void GLMesh_DeleteMeshBuffers (aliashdr_t *mainhdr)
{
	if (!mainhdr)
		return;

	for (aliashdr_t *hdr = mainhdr; hdr != NULL; hdr = hdr->nextsurface)
		RT_FreeSkinnedMesh (hdr);

	for (int i = 0; i < mod_numknown; i++)
	{
		qmodel_t *m = &mod_known[i];
		if (m->type != mod_alias)
			continue;
		if ((aliashdr_t *)m->extradata[PV_QUAKE1] != mainhdr)
			continue;

		RT_FreeAliasMesh (m);
		return;
	}
}

/*
================
GLMesh_DeleteAllMeshBuffers

Free the RT arrays for all precached alias models
================
*/
void GLMesh_DeleteAllMeshBuffers (void)
{
	qmodel_t *m;

	for (int j = 1; j < MAX_MODELS; j++)
	{
		if (!(m = cl.model_precache[j]))
			break;
		if (m->type != mod_alias)
			continue;

		RT_FreeAliasMesh (m);
	}
}

/*
================
GLMesh_LoadVertexBuffers / GLMesh_DeleteVertexBuffers

Fork-era names kept for rt_glquake.h compatibility. The RT arrays are built at model
load time from poseverts[], which is only valid inside Mod_LoadAliasModel, so a
standalone "reload all" pass is not possible here; models that need rebuilding go
through Mod_LoadModel again (which calls GL_MakeAliasModelDisplayLists).
================
*/
void GLMesh_LoadVertexBuffers (void)
{
}

void GLMesh_DeleteVertexBuffers (void)
{
	GLMesh_DeleteAllMeshBuffers ();
}
