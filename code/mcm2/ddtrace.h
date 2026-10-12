/*
    OpenMCM2 - a trace of what the game asks of DirectDraw and Direct3D.

    A research tool for the OpenGL renderer: it records which methods the game calls on every
    DirectDraw and Direct3D 7 object, and the distinct render states, primitive types and surface
    formats it uses, while the real DirectDraw does the work. It goes once the renderer replaces
    DirectDraw.
*/

#pragma once

/* Route the game's DirectDrawCreateEx through the trace. */
void ddtrace_install(void);
