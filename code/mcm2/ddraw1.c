/*
    OpenMCM2 - DirectDraw's version-1 interfaces, for DirectShow.

    IDirectDrawSurface versions 1, 2 and 3 share their methods' order, each adding to the last, so
    one adapter shaped like version 3 answers all three through the same pointer.

    The game plays its videos - the intro, and the menus' moving backgrounds - through DirectShow's
    multimedia streams, which ask the game's DirectDraw for the original IDirectDraw and create the
    surfaces frames are decoded into through it. These are thin adapters: each method translates the
    version-1 types (DDSURFACEDESC, DDSCAPS, version-1 surface pointers) and calls the version-7 one.
*/

#include "render.h"

#include <stddef.h>

#define DD_OF(self) ((ddraw*) ((BYTE*) (self) - offsetof(ddraw, iface1)))
#define DD7(self)   (&DD_OF(self)->iface)
#define S_OF(self)  ((surface*) ((BYTE*) (self) - offsetof(surface, iface1)))
#define S7(self)    (&S_OF(self)->iface)
#define CALL7(self, method, ...) (S7(self)->lpVtbl->method(S7(self), __VA_ARGS__))

surface* surface_of1(IDirectDrawSurface3* iface)
{
    return (iface && iface->lpVtbl == &k_surface1_vtbl) ? S_OF(iface) : NULL;
}

static IDirectDrawSurface7* to7(IDirectDrawSurface3* iface)
{
    surface* s = surface_of1(iface);
    return s ? &s->iface : NULL;
}

/* DDSURFACEDESC is DDSURFACEDESC2 up to the caps, which are one DWORD instead of four. */
static void desc_to2(const DDSURFACEDESC* from, DDSURFACEDESC2* to)
{
    __stosb((unsigned char*) to, 0, sizeof *to);
    __movsb((unsigned char*) to, (const unsigned char*) from, offsetof(DDSURFACEDESC, ddsCaps));
    to->dwSize = sizeof *to;
    to->ddsCaps.dwCaps = from->ddsCaps.dwCaps;
}

static void desc_to1(const DDSURFACEDESC2* from, DDSURFACEDESC* to)
{
    __movsb((unsigned char*) to, (const unsigned char*) from, offsetof(DDSURFACEDESC, ddsCaps));
    to->dwSize = sizeof *to;
    to->ddsCaps.dwCaps = from->ddsCaps.dwCaps;
}

/* ------------------------------------------------------------------ IDirectDraw */

static HRESULT WINAPI d1_QueryInterface(IDirectDraw* self, REFIID iid, void** out)
{
    return DD7(self)->lpVtbl->QueryInterface(DD7(self), iid, out);
}

static ULONG WINAPI d1_AddRef(IDirectDraw* self)
{
    return DD7(self)->lpVtbl->AddRef(DD7(self));
}

static ULONG WINAPI d1_Release(IDirectDraw* self)
{
    return DD7(self)->lpVtbl->Release(DD7(self));
}

static HRESULT WINAPI d1_Compact(IDirectDraw* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI d1_CreateClipper(IDirectDraw* self, DWORD flags, LPDIRECTDRAWCLIPPER* out, IUnknown* outer)
{
    return DD7(self)->lpVtbl->CreateClipper(DD7(self), flags, out, outer);
}

static HRESULT WINAPI d1_CreatePalette(IDirectDraw* self, DWORD flags, LPPALETTEENTRY entries, LPDIRECTDRAWPALETTE* out,
                                       IUnknown* outer)
{
    return DD7(self)->lpVtbl->CreatePalette(DD7(self), flags, entries, out, outer);
}

static HRESULT WINAPI d1_CreateSurface(IDirectDraw* self, LPDDSURFACEDESC desc, LPDIRECTDRAWSURFACE* out, IUnknown* outer)
{
    DDSURFACEDESC2 d2;
    IDirectDrawSurface7* s7;
    HRESULT hr;

    desc_to2(desc, &d2);
    hr = DD7(self)->lpVtbl->CreateSurface(DD7(self), &d2, &s7, outer);
    *out = SUCCEEDED(hr) ? (LPDIRECTDRAWSURFACE) &surface_of(s7)->iface1 : NULL;
    return hr;
}

static HRESULT WINAPI d1_DuplicateSurface(IDirectDraw* self, LPDIRECTDRAWSURFACE from, LPDIRECTDRAWSURFACE* out)
{
    (void) self; (void) from; (void) out;
    return unsupported("DirectDraw::DuplicateSurface");
}

typedef struct mode_relay
{
    LPDDENUMMODESCALLBACK callback;
    LPVOID                context;
} mode_relay;

static HRESULT WINAPI relay_mode(LPDDSURFACEDESC2 d2, LPVOID context)
{
    mode_relay* r = (mode_relay*) context;
    DDSURFACEDESC d1;

    desc_to1(d2, &d1);
    return r->callback(&d1, r->context);
}

static HRESULT WINAPI d1_EnumDisplayModes(IDirectDraw* self, DWORD flags, LPDDSURFACEDESC filter, LPVOID context,
                                          LPDDENUMMODESCALLBACK callback)
{
    mode_relay r = { callback, context };
    DDSURFACEDESC2 f2;

    if (filter)
        desc_to2(filter, &f2);
    return DD7(self)->lpVtbl->EnumDisplayModes(DD7(self), flags, filter ? &f2 : NULL, &r, relay_mode);
}

static HRESULT WINAPI d1_EnumSurfaces(IDirectDraw* self, DWORD flags, LPDDSURFACEDESC desc, LPVOID context,
                                      LPDDENUMSURFACESCALLBACK callback)
{
    (void) self; (void) flags; (void) desc; (void) context; (void) callback;
    return unsupported("DirectDraw::EnumSurfaces");
}

static HRESULT WINAPI d1_FlipToGDISurface(IDirectDraw* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI d1_GetCaps(IDirectDraw* self, LPDDCAPS driver, LPDDCAPS emulation)
{
    return DD7(self)->lpVtbl->GetCaps(DD7(self), driver, emulation);
}

static HRESULT WINAPI d1_GetDisplayMode(IDirectDraw* self, LPDDSURFACEDESC desc)
{
    DDSURFACEDESC2 d2;
    HRESULT hr = DD7(self)->lpVtbl->GetDisplayMode(DD7(self), &d2);

    desc_to1(&d2, desc);
    return hr;
}

static HRESULT WINAPI d1_GetFourCCCodes(IDirectDraw* self, LPDWORD count, LPDWORD codes)
{
    return DD7(self)->lpVtbl->GetFourCCCodes(DD7(self), count, codes);
}

static HRESULT WINAPI d1_GetGDISurface(IDirectDraw* self, LPDIRECTDRAWSURFACE* out)
{
    (void) self; (void) out;
    return unsupported("DirectDraw::GetGDISurface");
}

static HRESULT WINAPI d1_GetMonitorFrequency(IDirectDraw* self, LPDWORD hz)
{
    return DD7(self)->lpVtbl->GetMonitorFrequency(DD7(self), hz);
}

static HRESULT WINAPI d1_GetScanLine(IDirectDraw* self, LPDWORD line)
{
    return DD7(self)->lpVtbl->GetScanLine(DD7(self), line);
}

static HRESULT WINAPI d1_GetVerticalBlankStatus(IDirectDraw* self, LPBOOL in_blank)
{
    return DD7(self)->lpVtbl->GetVerticalBlankStatus(DD7(self), in_blank);
}

static HRESULT WINAPI d1_Initialize(IDirectDraw* self, GUID* device)
{
    (void) self; (void) device;
    return DDERR_ALREADYINITIALIZED;
}

static HRESULT WINAPI d1_RestoreDisplayMode(IDirectDraw* self)
{
    (void) self;
    return DD_OK;
}

/* DirectShow streams into memory surfaces; it has no business with the window or the mode. */
static HRESULT WINAPI d1_SetCooperativeLevel(IDirectDraw* self, HWND window, DWORD flags)
{
    (void) self; (void) window; (void) flags;
    return DD_OK;
}

static HRESULT WINAPI d1_SetDisplayMode(IDirectDraw* self, DWORD w, DWORD h, DWORD bits)
{
    (void) self; (void) w; (void) h; (void) bits;
    return DD_OK;
}

static HRESULT WINAPI d1_WaitForVerticalBlank(IDirectDraw* self, DWORD flags, HANDLE event)
{
    (void) self; (void) flags; (void) event;
    return DD_OK;
}

IDirectDrawVtbl k_ddraw1_vtbl = {
    .QueryInterface = d1_QueryInterface,
    .AddRef = d1_AddRef,
    .Release = d1_Release,
    .Compact = d1_Compact,
    .CreateClipper = d1_CreateClipper,
    .CreatePalette = d1_CreatePalette,
    .CreateSurface = d1_CreateSurface,
    .DuplicateSurface = d1_DuplicateSurface,
    .EnumDisplayModes = d1_EnumDisplayModes,
    .EnumSurfaces = d1_EnumSurfaces,
    .FlipToGDISurface = d1_FlipToGDISurface,
    .GetCaps = d1_GetCaps,
    .GetDisplayMode = d1_GetDisplayMode,
    .GetFourCCCodes = d1_GetFourCCCodes,
    .GetGDISurface = d1_GetGDISurface,
    .GetMonitorFrequency = d1_GetMonitorFrequency,
    .GetScanLine = d1_GetScanLine,
    .GetVerticalBlankStatus = d1_GetVerticalBlankStatus,
    .Initialize = d1_Initialize,
    .RestoreDisplayMode = d1_RestoreDisplayMode,
    .SetCooperativeLevel = d1_SetCooperativeLevel,
    .SetDisplayMode = d1_SetDisplayMode,
    .WaitForVerticalBlank = d1_WaitForVerticalBlank,
};

/* ------------------------------------------------------------------ IDirectDrawSurface */

static HRESULT WINAPI s1_QueryInterface(IDirectDrawSurface3* self, REFIID iid, void** out)
{
    return CALL7(self, QueryInterface, iid, out);
}

static ULONG WINAPI s1_AddRef(IDirectDrawSurface3* self)
{
    return S7(self)->lpVtbl->AddRef(S7(self));
}

static ULONG WINAPI s1_Release(IDirectDrawSurface3* self)
{
    return S7(self)->lpVtbl->Release(S7(self));
}

static HRESULT WINAPI s1_AddAttachedSurface(IDirectDrawSurface3* self, LPDIRECTDRAWSURFACE3 other)
{
    return CALL7(self, AddAttachedSurface, to7(other));
}

static HRESULT WINAPI s1_AddOverlayDirtyRect(IDirectDrawSurface3* self, LPRECT rect)
{
    return CALL7(self, AddOverlayDirtyRect, rect);
}

static HRESULT WINAPI s1_Blt(IDirectDrawSurface3* self, LPRECT to, LPDIRECTDRAWSURFACE3 from, LPRECT from_rect,
                             DWORD flags, LPDDBLTFX fx)
{
    return CALL7(self, Blt, to, to7(from), from_rect, flags, fx);
}

static HRESULT WINAPI s1_BltBatch(IDirectDrawSurface3* self, LPDDBLTBATCH batch, DWORD count, DWORD flags)
{
    (void) self; (void) batch; (void) count; (void) flags;
    return unsupported("Surface::BltBatch");
}

static HRESULT WINAPI s1_BltFast(IDirectDrawSurface3* self, DWORD x, DWORD y, LPDIRECTDRAWSURFACE3 from, LPRECT rect, DWORD flags)
{
    return CALL7(self, BltFast, x, y, to7(from), rect, flags);
}

static HRESULT WINAPI s1_DeleteAttachedSurface(IDirectDrawSurface3* self, DWORD flags, LPDIRECTDRAWSURFACE3 other)
{
    return CALL7(self, DeleteAttachedSurface, flags, to7(other));
}

static HRESULT WINAPI s1_EnumAttachedSurfaces(IDirectDrawSurface3* self, LPVOID context, LPDDENUMSURFACESCALLBACK callback)
{
    (void) self; (void) context; (void) callback;
    return unsupported("Surface::EnumAttachedSurfaces");
}

static HRESULT WINAPI s1_EnumOverlayZOrders(IDirectDrawSurface3* self, DWORD flags, LPVOID context, LPDDENUMSURFACESCALLBACK callback)
{
    (void) self; (void) flags; (void) context; (void) callback;
    return unsupported("Surface::EnumOverlayZOrders");
}

static HRESULT WINAPI s1_Flip(IDirectDrawSurface3* self, LPDIRECTDRAWSURFACE3 target, DWORD flags)
{
    return CALL7(self, Flip, to7(target), flags);
}

static HRESULT WINAPI s1_GetAttachedSurface(IDirectDrawSurface3* self, LPDDSCAPS caps, LPDIRECTDRAWSURFACE3* out)
{
    DDSCAPS2 caps2 = { caps->dwCaps };
    IDirectDrawSurface7* found;
    HRESULT hr = CALL7(self, GetAttachedSurface, &caps2, &found);

    *out = SUCCEEDED(hr) ? &surface_of(found)->iface1 : NULL;
    return hr;
}

static HRESULT WINAPI s1_GetBltStatus(IDirectDrawSurface3* self, DWORD flags)
{
    return CALL7(self, GetBltStatus, flags);
}

static HRESULT WINAPI s1_GetCaps(IDirectDrawSurface3* self, LPDDSCAPS caps)
{
    caps->dwCaps = S_OF(self)->desc.ddsCaps.dwCaps;
    return DD_OK;
}

static HRESULT WINAPI s1_GetClipper(IDirectDrawSurface3* self, LPDIRECTDRAWCLIPPER* out)
{
    return CALL7(self, GetClipper, out);
}

static HRESULT WINAPI s1_GetColorKey(IDirectDrawSurface3* self, DWORD flags, LPDDCOLORKEY key)
{
    return CALL7(self, GetColorKey, flags, key);
}

static HRESULT WINAPI s1_GetDC(IDirectDrawSurface3* self, HDC* out)
{
    return CALL7(self, GetDC, out);
}

static HRESULT WINAPI s1_GetFlipStatus(IDirectDrawSurface3* self, DWORD flags)
{
    return CALL7(self, GetFlipStatus, flags);
}

static HRESULT WINAPI s1_GetOverlayPosition(IDirectDrawSurface3* self, LPLONG x, LPLONG y)
{
    return CALL7(self, GetOverlayPosition, x, y);
}

static HRESULT WINAPI s1_GetPalette(IDirectDrawSurface3* self, LPDIRECTDRAWPALETTE* out)
{
    return CALL7(self, GetPalette, out);
}

static HRESULT WINAPI s1_GetPixelFormat(IDirectDrawSurface3* self, LPDDPIXELFORMAT pf)
{
    return CALL7(self, GetPixelFormat, pf);
}

static HRESULT WINAPI s1_GetSurfaceDesc(IDirectDrawSurface3* self, LPDDSURFACEDESC desc)
{
    desc_to1(&S_OF(self)->desc, desc);
    desc->lpSurface = NULL;
    return DD_OK;
}

static HRESULT WINAPI s1_Initialize(IDirectDrawSurface3* self, LPDIRECTDRAW dd, LPDDSURFACEDESC desc)
{
    (void) self; (void) dd; (void) desc;
    return DDERR_ALREADYINITIALIZED;
}

static HRESULT WINAPI s1_IsLost(IDirectDrawSurface3* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI s1_Lock(IDirectDrawSurface3* self, LPRECT rect, LPDDSURFACEDESC desc, DWORD flags, HANDLE event)
{
    DDSURFACEDESC2 d2;
    HRESULT hr = CALL7(self, Lock, rect, &d2, flags, event);

    if (SUCCEEDED(hr))
        desc_to1(&d2, desc);
    return hr;
}

static HRESULT WINAPI s1_ReleaseDC(IDirectDrawSurface3* self, HDC dc)
{
    return CALL7(self, ReleaseDC, dc);
}

static HRESULT WINAPI s1_Restore(IDirectDrawSurface3* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI s1_SetClipper(IDirectDrawSurface3* self, LPDIRECTDRAWCLIPPER clipper)
{
    return CALL7(self, SetClipper, clipper);
}

static HRESULT WINAPI s1_SetColorKey(IDirectDrawSurface3* self, DWORD flags, LPDDCOLORKEY key)
{
    return CALL7(self, SetColorKey, flags, key);
}

static HRESULT WINAPI s1_SetOverlayPosition(IDirectDrawSurface3* self, LONG x, LONG y)
{
    return CALL7(self, SetOverlayPosition, x, y);
}

static HRESULT WINAPI s1_SetPalette(IDirectDrawSurface3* self, LPDIRECTDRAWPALETTE palette)
{
    return CALL7(self, SetPalette, palette);
}

/* Version 1 passes the pointer Lock returned; version 7 takes the locked rectangle. */
static HRESULT WINAPI s1_Unlock(IDirectDrawSurface3* self, LPVOID data)
{
    (void) data;
    return CALL7(self, Unlock, NULL);
}

static HRESULT WINAPI s1_UpdateOverlay(IDirectDrawSurface3* self, LPRECT from, LPDIRECTDRAWSURFACE3 to, LPRECT to_rect,
                                       DWORD flags, LPDDOVERLAYFX fx)
{
    return CALL7(self, UpdateOverlay, from, to7(to), to_rect, flags, fx);
}

static HRESULT WINAPI s1_UpdateOverlayDisplay(IDirectDrawSurface3* self, DWORD flags)
{
    return CALL7(self, UpdateOverlayDisplay, flags);
}

static HRESULT WINAPI s1_GetDDInterface(IDirectDrawSurface3* self, LPVOID* out)
{
    return CALL7(self, GetDDInterface, out);
}

static HRESULT WINAPI s1_PageLock(IDirectDrawSurface3* self, DWORD flags)
{
    (void) self; (void) flags;
    return DD_OK;
}

static HRESULT WINAPI s1_PageUnlock(IDirectDrawSurface3* self, DWORD flags)
{
    (void) self; (void) flags;
    return DD_OK;
}

static HRESULT WINAPI s1_SetSurfaceDesc(IDirectDrawSurface3* self, LPDDSURFACEDESC desc, DWORD flags)
{
    (void) self; (void) desc; (void) flags;
    return unsupported("Surface3::SetSurfaceDesc");
}

static HRESULT WINAPI s1_UpdateOverlayZOrder(IDirectDrawSurface3* self, DWORD flags, LPDIRECTDRAWSURFACE3 other)
{
    return CALL7(self, UpdateOverlayZOrder, flags, to7(other));
}

IDirectDrawSurface3Vtbl k_surface1_vtbl = {
    .QueryInterface = s1_QueryInterface,
    .AddRef = s1_AddRef,
    .Release = s1_Release,
    .AddAttachedSurface = s1_AddAttachedSurface,
    .AddOverlayDirtyRect = s1_AddOverlayDirtyRect,
    .Blt = s1_Blt,
    .BltBatch = s1_BltBatch,
    .BltFast = s1_BltFast,
    .DeleteAttachedSurface = s1_DeleteAttachedSurface,
    .EnumAttachedSurfaces = s1_EnumAttachedSurfaces,
    .EnumOverlayZOrders = s1_EnumOverlayZOrders,
    .Flip = s1_Flip,
    .GetAttachedSurface = s1_GetAttachedSurface,
    .GetBltStatus = s1_GetBltStatus,
    .GetCaps = s1_GetCaps,
    .GetClipper = s1_GetClipper,
    .GetColorKey = s1_GetColorKey,
    .GetDC = s1_GetDC,
    .GetFlipStatus = s1_GetFlipStatus,
    .GetOverlayPosition = s1_GetOverlayPosition,
    .GetPalette = s1_GetPalette,
    .GetPixelFormat = s1_GetPixelFormat,
    .GetSurfaceDesc = s1_GetSurfaceDesc,
    .Initialize = s1_Initialize,
    .IsLost = s1_IsLost,
    .Lock = s1_Lock,
    .ReleaseDC = s1_ReleaseDC,
    .Restore = s1_Restore,
    .SetClipper = s1_SetClipper,
    .SetColorKey = s1_SetColorKey,
    .SetOverlayPosition = s1_SetOverlayPosition,
    .SetPalette = s1_SetPalette,
    .Unlock = s1_Unlock,
    .UpdateOverlay = s1_UpdateOverlay,
    .UpdateOverlayDisplay = s1_UpdateOverlayDisplay,
    .UpdateOverlayZOrder = s1_UpdateOverlayZOrder,
    .GetDDInterface = s1_GetDDInterface,
    .PageLock = s1_PageLock,
    .PageUnlock = s1_PageUnlock,
    .SetSurfaceDesc = s1_SetSurfaceDesc,
};
