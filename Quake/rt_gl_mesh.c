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
// Only the classic .mdl path (PV_QUAKE1) is meshed for RT. MD3/MD5 are out of scope
// (their GLMesh_UploadBuffers calls are no-ops here).

#include "quakedef.h"

#define NUMVERTEXNORMALS 162
extern float r_avertexnormals[NUMVERTEXNORMALS][3];

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

/*
================
GLMesh_UploadBuffers

1.36 entry point used by the MD3/MD5 loaders. Under RT only the classic .mdl path is
meshed (see GL_MakeAliasModelDisplayLists), so this is a no-op.
================
*/
void GLMesh_UploadBuffers (
	qmodel_t *mod, aliashdr_t *hdr, unsigned short *indexes, byte *vertexes, aliasmesh_t *desc, jointpose_t *joints, unsigned short *skeleton_indexes,
	int num_skeleton_indexes)
{
	(void)mod;
	(void)hdr;
	(void)indexes;
	(void)vertexes;
	(void)desc;
	(void)joints;
	(void)skeleton_indexes;
	(void)num_skeleton_indexes;
}

/*
================
GLMesh_DeleteMeshBuffers

1.36 signature takes only the aliashdr_t. The RT arrays live on the qmodel_t, so we
look up the owning model by its PV_QUAKE1 extradata pointer; headers for other pose
vertex types (MD3/MD5) never had RT arrays built and are ignored.
================
*/
void GLMesh_DeleteMeshBuffers (aliashdr_t *mainhdr)
{
	if (!mainhdr)
		return;

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
