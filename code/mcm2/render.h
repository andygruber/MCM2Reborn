/*
    OpenMCM2 - the renderer: DirectDraw 7 and Direct3D 7, as much of them as the game uses, on OpenGL.

    The game gets these objects from DirectDrawCreateEx, which is taken over at start-up, so its
    own rendering code runs unchanged. What it uses was measured, not guessed: a hardware device,
    16-bit textures, no lights, TL and LVERTEX geometry with fog in specular alpha, GDI text, Blt
    and BltFast for 2D, and Flip.
    Anything else answers DDERR_UNSUPPORTED and is logged the first time.

    Render targets (the back buffer, and any 3D offscreen surface) are framebuffer objects at the
    game's own resolution, so 2D work on them - Lock, GetDC, Blt - is pixel-exact. Flip scales the
    back buffer into the window, keeping its aspect. Every other surface lives in memory as the game
    writes it, and a texture is uploaded when it changes.
*/

#pragma once

#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION   0x0700
#define CINTERFACE

#include "gl.h"

#include <ddraw.h>
#include <d3d.h>

/* The 16-bit formats the renderer offers; the game picks its textures from these. */
enum pixel_format { PF_565, PF_1555, PF_4444, PF_FORMATS };

typedef struct surface
{
    IDirectDrawSurface7  iface;
    IDirectDrawSurface3  iface1;        /* versions 1-3, for DirectShow's video streams: see ddraw1.c */
    IDirectDrawGammaControl gamma;
    LONG                 refs;
    DDSURFACEDESC2       desc;          /* as the game sees it */
    int                  format;        /* pixel_format, for colour surfaces */

    BYTE*                pixels;        /* the surface in memory: textures, offscreen, a locked target */
    BOOL                 locked_target; /* a render target's pixels hold a Lock or GetDC readback */
    BOOL                 pixels_current;    /* a render target's pixels are what it shows */

    GLuint               fbo, colour, depth;    /* a render target */
    GLuint               texture;       /* a texture, uploaded when `dirty` */
    BOOL                 dirty;

    struct surface*      back;          /* the primary's back buffer */
    struct surface*      mip;           /* the next smaller mipmap level */
    struct surface*      zbuffer;       /* attached with AddAttachedSurface */
    struct surface*      top;           /* the level that owns the mip chain, or itself */

    DDCOLORKEY           key;           /* source colour key, if `keyed` */
    BOOL                 keyed;
    BOOL                 inks;          /* a modern menu picture, or a target one is on: see render_inks */

    HDC                  dc;            /* GetDC: a DIB section over a copy of the pixels */
    HBITMAP              dib, old_bitmap;
    void*                dib_bits;

    IDirectDrawPalette*  palette;
} surface;

typedef struct ddraw
{
    IDirectDraw7 iface;
    IDirectDraw  iface1;                /* for DirectShow's video streams: see ddraw1.c */
    LONG         refs;
    IDirect3D7*  d3d;
} ddraw;

/* ------------------------------------------------------------------ ddraw.c */

/* The surface behind an IDirectDrawSurface7 the game hands back, or NULL for one that is not ours. */
surface* surface_of(IDirectDrawSurface7* iface);

/* ------------------------------------------------------------------ ddraw1.c */

/* The version-1 interfaces, which forward to the version-7 ones. */
extern IDirectDrawVtbl        k_ddraw1_vtbl;
extern IDirectDrawSurface3Vtbl k_surface1_vtbl;

/* The surface behind a version 1-3 interface, or NULL. */
surface* surface_of1(IDirectDrawSurface3* iface);

/* Bytes per row of a surface's pixels. */
LONG surface_pitch(const surface* s);

/* The DDPIXELFORMAT of one of the renderer's formats. */
void describe_format(int format, DDPIXELFORMAT* pf);

/* Make a render target the one OpenGL draws into, creating its framebuffer on first use. */
void target_bind(surface* target);

/* Bind a texture surface to the active texture unit, uploading it if the game changed it. */
void texture_bind(surface* texture);

/* The game's display mode, as it set it - the size its full-screen render targets have. */
void display_size(DWORD* width, DWORD* height);

/* 2D work outside the device changed OpenGL's state: the device must set its own again. */
extern BOOL g_state_lost;

/* ------------------------------------------------------------------ device.c */

/* Hand the game this renderer instead of DirectDraw. */
void render_install(void);

/* The Direct3D7 object that belongs to a DirectDraw object, created on first QueryInterface. */
IDirect3D7* direct3d_create(IDirectDraw7* owner);

/* The log line for a method nobody has implemented, once per method. */
HRESULT unsupported(const char* method);

/* A QueryInterface the renderer turns down: logged, so a missing interface shows in the log. */
HRESULT no_interface(const char* object, REFIID iid, void** out);
