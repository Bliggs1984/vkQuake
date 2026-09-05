/*
 * RT: built-in default.cfg for the RayTracedGL1 build (re-applied from modernise-2026, whose base 1.20.3 still
 * carried this file; upstream removed it in 529671f2 and now ships default.cfg inside the embedded vkquake.pak).
 *
 * The RT build must *always* use these binds instead of the pak's default.cfg (fork cmd.c, Cmd_Exec_f):
 *
 *     #ifdef RT_RENDERER
 *     if (!q_strcasecmp (path, "default.cfg")) { Cbuf_InsertText ("\n"); Cbuf_InsertText (default_cfg); return; }
 *     #endif
 *
 * Binds are the fork's exactly (WASD, MOUSE1 attack, SPACE jump, SHIFT run, CTRL swim down, F = rt_pfnswitch, no
 * arrow-key/keyboard-look/F10/F12/MOUSE2/INS/backslash binds). The zoom alias and the default cvar block follow
 * 1.36's Misc/vq_pak/default.cfg (the fork's copies of those were 1.20.3 leftovers: gamma, scr_*scale 1.6).
 */

#ifdef RT_RENDERER

static char default_cfg[] =
	"unbindall\n"

	"bind MOUSE1 +attack\n"

	"bind w +forward\n"
	"bind s +back\n"
	"bind a +moveleft\n"
	"bind d +moveright\n"

	"bind SPACE +jump\n"
	"bind SHIFT +speed\n"
	"bind CTRL +movedown\n"

	"bind TAB +showscores\n"

	"bind f rt_pfnswitch\n"

	"bind 1 \"impulse 1\"\n"
	"bind 2 \"impulse 2\"\n"
	"bind 3 \"impulse 3\"\n"
	"bind 4 \"impulse 4\"\n"
	"bind 5 \"impulse 5\"\n"
	"bind 6 \"impulse 6\"\n"
	"bind 7 \"impulse 7\"\n"
	"bind 8 \"impulse 8\"\n"

	"bind 0 \"impulse 0\"\n"

	"bind / \"impulse 10\"\n"
	"bind MWHEELDOWN \"impulse 10\"\n"
	"bind MWHEELUP \"impulse 12\"\n"

	// zoom
	"alias zoom_in \"togglezoom\"\n"
	"alias zoom_out \"togglezoom\"\n"
	"bind F11 zoom_in\n"

	"bind F1 \"help\"\n"
	"bind F2 \"menu_save\"\n"
	"bind F3 \"menu_load\"\n"
	"bind F4 \"menu_options\"\n"
	"bind F5 \"menu_multiplayer\"\n"
	"bind F6 \"echo Quicksaving...; wait; save quick\"\n"
	"bind F9 \"echo Quickloading...; wait; load quick\"\n"

	"bind PAUSE \"pause\"\n"
	"bind ESCAPE \"togglemenu\"\n"
	"bind ~ \"toggleconsole\"\n"
	"bind ` \"toggleconsole\"\n"

	"bind t \"messagemode\"\n"

	"bind + \"sizeup\"\n"
	"bind = \"sizeup\"\n"
	"bind - \"sizedown\"\n"

	"bind LSHOULDER \"impulse 12\"\n"
	"bind RSHOULDER \"impulse 10\"\n"
	"bind LTRIGGER +jump\n"
	"bind RTRIGGER +attack\n"

	// default cvars
	"volume 0.7\n"
	"sensitivity 3\n"

	"viewsize 110\n"

	// default to mouse-look enabled
	"+mlook\n"

	"crosshair 1\n";

#endif /* RT_RENDERER */
