/*
    OpenMCM2 - a trace of what the game asks of DirectDraw and Direct3D. See ddtrace.h.

    Every object the game gets from DirectDraw has its vtable rewritten in place: each slot points
    at a stub that counts the call and jumps on to the original. The methods that hand back new
    objects are wrapped instead, so those objects are traced in turn, and a few more are wrapped
    to record the distinct values they are given. Totals go to openmcm2.log every ten seconds.
*/

#include "ddtrace.h"
#include "runtime.h"

#include <intrin.h>

enum kind { K_DDRAW, K_SURFACE, K_D3D, K_DEVICE, K_BUFFER, K_CLIPPER, K_PALETTE, K_GAMMA, K_KINDS };

#define MAX_SLOTS 49

static const char* const k_kind[K_KINDS] = {
    "DirectDraw7", "Surface7", "Direct3D7", "Device7", "VertexBuffer7", "Clipper", "Palette", "Gamma"
};

static const int k_slots[K_KINDS] = { 30, 49, 8, 49, 9, 9, 7, 5 };

static const char* const k_method[K_KINDS][MAX_SLOTS] = {
    { "QueryInterface", "AddRef", "Release", "Compact", "CreateClipper", "CreatePalette",
      "CreateSurface", "DuplicateSurface", "EnumDisplayModes", "EnumSurfaces", "FlipToGDISurface",
      "GetCaps", "GetDisplayMode", "GetFourCCCodes", "GetGDISurface", "GetMonitorFrequency",
      "GetScanLine", "GetVerticalBlankStatus", "Initialize", "RestoreDisplayMode",
      "SetCooperativeLevel", "SetDisplayMode", "WaitForVerticalBlank", "GetAvailableVidMem",
      "GetSurfaceFromDC", "RestoreAllSurfaces", "TestCooperativeLevel", "GetDeviceIdentifier",
      "StartModeTest", "EvaluateMode" },
    { "QueryInterface", "AddRef", "Release", "AddAttachedSurface", "AddOverlayDirtyRect", "Blt",
      "BltBatch", "BltFast", "DeleteAttachedSurface", "EnumAttachedSurfaces", "EnumOverlayZOrders",
      "Flip", "GetAttachedSurface", "GetBltStatus", "GetCaps", "GetClipper", "GetColorKey", "GetDC",
      "GetFlipStatus", "GetOverlayPosition", "GetPalette", "GetPixelFormat", "GetSurfaceDesc",
      "Initialize", "IsLost", "Lock", "ReleaseDC", "Restore", "SetClipper", "SetColorKey",
      "SetOverlayPosition", "SetPalette", "Unlock", "UpdateOverlay", "UpdateOverlayDisplay",
      "UpdateOverlayZOrder", "GetDDInterface", "PageLock", "PageUnlock", "SetSurfaceDesc",
      "SetPrivateData", "GetPrivateData", "FreePrivateData", "GetUniquenessValue",
      "ChangeUniquenessValue", "SetPriority", "GetPriority", "SetLOD", "GetLOD" },
    { "QueryInterface", "AddRef", "Release", "EnumDevices", "CreateDevice", "CreateVertexBuffer",
      "EnumZBufferFormats", "EvictManagedTextures" },
    { "QueryInterface", "AddRef", "Release", "GetCaps", "EnumTextureFormats", "BeginScene",
      "EndScene", "GetDirect3D", "SetRenderTarget", "GetRenderTarget", "Clear", "SetTransform",
      "GetTransform", "SetViewport", "MultiplyTransform", "GetViewport", "SetMaterial",
      "GetMaterial", "SetLight", "GetLight", "SetRenderState", "GetRenderState", "BeginStateBlock",
      "EndStateBlock", "PreLoad", "DrawPrimitive", "DrawIndexedPrimitive", "SetClipStatus",
      "GetClipStatus", "DrawPrimitiveStrided", "DrawIndexedPrimitiveStrided", "DrawPrimitiveVB",
      "DrawIndexedPrimitiveVB", "ComputeSphereVisibility", "GetTexture", "SetTexture",
      "GetTextureStageState", "SetTextureStageState", "ValidateDevice", "ApplyStateBlock",
      "CaptureStateBlock", "DeleteStateBlock", "CreateStateBlock", "Load", "LightEnable",
      "GetLightEnable", "SetClipPlane", "GetClipPlane", "GetInfo" },
    { "QueryInterface", "AddRef", "Release", "Lock", "Unlock", "ProcessVertices",
      "GetVertexBufferDesc", "Optimize", "ProcessVerticesStrided" },
    { "QueryInterface", "AddRef", "Release", "GetClipList", "GetHWnd", "Initialize",
      "IsClipListChanged", "SetClipList", "SetHWnd" },
    { "QueryInterface", "AddRef", "Release", "GetCaps", "GetEntries", "Initialize", "SetEntries" },
    { "QueryInterface", "AddRef", "Release", "GetGammaRamp", "SetGammaRamp" },
};

/* A vtable slot. */
typedef void (*code)(void);

/* One traced vtable: which interface it is, and what its slots held before the stubs. */
typedef struct record
{
    void** vtable;
    int    kind;
    code   original[MAX_SLOTS];
} record;

#define MAX_RECORDS 64

static record        g_record[MAX_RECORDS];
static volatile long g_records;
static volatile long g_lock;
static unsigned      g_calls[K_KINDS][MAX_SLOTS];
static code          g_wrapper[K_KINDS][MAX_SLOTS];

static record* find(void** vtable)
{
    long i, n = g_records;

    for (i = 0; i < n; ++i)
    {
        if (g_record[i].vtable == vtable)
            return &g_record[i];
    }
    return NULL;
}

static code original(void* self, int slot)
{
    return find(*(void***) self)->original[slot];
}

/* ------------------------------------------------------------------ distinct values */

/* Has this combination been seen before? A 32-bit hash: a collision only loses a log line. */
static int first_time(unsigned a, unsigned b, unsigned c, unsigned d)
{
    static unsigned seen[8192];
    unsigned values[4] = { a, b, c, d };
    unsigned h = 2166136261u, i;

    for (i = 0; i < 4; ++i)
        h = (h ^ values[i]) * 16777619u;

    h |= 1;     /* 0 marks an empty slot */

    for (i = h & 8191; seen[i]; i = (i + 1) & 8191)
    {
        if (seen[i] == h)
            return 0;
    }
    seen[i] = h;
    return 1;
}

/* ------------------------------------------------------------------ totals */

static void report(void)
{
    int k, s;

    log_line("--- calls so far");
    for (k = 0; k < K_KINDS; ++k)
    {
        for (s = 0; s < k_slots[k]; ++s)
        {
            if (g_calls[k][s])
                log_line("  %s::%s %u", k_kind[k], k_method[k][s], g_calls[k][s]);
        }
    }
}

static void report_now_and_then(void)
{
    static LARGE_INTEGER frequency, last;
    static unsigned ticks;
    LARGE_INTEGER now;

    if (++ticks & 4095)
        return;

    QueryPerformanceCounter(&now);
    if (!frequency.QuadPart)
    {
        QueryPerformanceFrequency(&frequency);
        last = now;
    }
    else if (now.QuadPart - last.QuadPart > frequency.QuadPart * 10)
    {
        last = now;
        report();
    }
}

/* ------------------------------------------------------------------ the stubs */

/* Every traced slot lands here: count the call, then go on to the wrapper or the original. */
static code __cdecl trace_hit(int slot, void* self)
{
    record* r = find(*(void***) self);
    code wrapper = g_wrapper[r->kind][slot];

    if (g_calls[r->kind][slot]++ == 0)
        log_line("first call: %s::%s", k_kind[r->kind], k_method[r->kind][slot]);

    report_now_and_then();
    return wrapper ? wrapper : r->original[slot];
}

/* `this` is the first argument of every COM method, at [esp + 4] on entry. */
#define STUB(n) static __declspec(naked) void stub_##n(void) \
    { __asm mov eax, [esp + 4] __asm push eax __asm push n __asm call trace_hit __asm add esp, 8 __asm jmp eax }
#define STUB10(t) STUB(t##0) STUB(t##1) STUB(t##2) STUB(t##3) STUB(t##4) \
                  STUB(t##5) STUB(t##6) STUB(t##7) STUB(t##8) STUB(t##9)

STUB10() STUB10(1) STUB10(2) STUB10(3)
STUB(40) STUB(41) STUB(42) STUB(43) STUB(44) STUB(45) STUB(46) STUB(47) STUB(48)

#define REF(n) stub_##n,
#define REF10(t) REF(t##0) REF(t##1) REF(t##2) REF(t##3) REF(t##4) \
                 REF(t##5) REF(t##6) REF(t##7) REF(t##8) REF(t##9)

static const code g_stub[MAX_SLOTS] = {
    REF10() REF10(1) REF10(2) REF10(3)
    REF(40) REF(41) REF(42) REF(43) REF(44) REF(45) REF(46) REF(47) REF(48)
};

/* Start tracing an object, unless its vtable already is. */
static void trace(int kind, void* object)
{
    void** vtable;
    record* r;

    if (!object || kind < 0)
        return;

    vtable = *(void***) object;

    while (_InterlockedCompareExchange(&g_lock, 1, 0))
        _mm_pause();

    if (!find(vtable) && g_records < MAX_RECORDS)
    {
        r = &g_record[g_records];
        r->vtable = vtable;
        r->kind = kind;
        __movsb((unsigned char*) r->original, (const unsigned char*) vtable, k_slots[kind] * sizeof(void*));

        if (write_protected(vtable, g_stub, k_slots[kind] * sizeof(void*)))
        {
            _InterlockedIncrement(&g_records);
            log_line("tracing a %s (vtable %08X)", k_kind[kind], (unsigned) (UINT_PTR) vtable);
        }
    }

    g_lock = 0;
}

/* ------------------------------------------------------------------ wrappers */

static int same_guid(const GUID* a, unsigned data1, unsigned short data2, unsigned short data3,
                     const unsigned char* data4)
{
    int i;

    if (a->Data1 != data1 || a->Data2 != data2 || a->Data3 != data3)
        return 0;

    for (i = 0; i < 8; ++i)
    {
        if (a->Data4[i] != data4[i])
            return 0;
    }
    return 1;
}

/* Which interface an IID names, or -1 for one the trace does not follow. */
static int kind_of(const GUID* iid)
{
    static const unsigned char ddraw[8] = { 0xb9, 0x2f, 0x00, 0x60, 0x97, 0x97, 0xea, 0x5b };
    static const unsigned char d3d[8] = { 0xa4, 0x07, 0x00, 0xa0, 0xc9, 0x06, 0x29, 0xa8 };
    static const unsigned char gamma[8] = { 0xad, 0x7a, 0x00, 0xc0, 0x4f, 0xc2, 0x9b, 0x4e };

    if (same_guid(iid, 0x15e65ec0, 0x3b9c, 0x11d2, ddraw))
        return K_DDRAW;
    if (same_guid(iid, 0x06675a80, 0x3b9b, 0x11d2, ddraw))
        return K_SURFACE;
    if (same_guid(iid, 0xf5049e77, 0x4861, 0x11d2, d3d))
        return K_D3D;
    if (same_guid(iid, 0xf5049e79, 0x4861, 0x11d2, d3d))
        return K_DEVICE;
    if (same_guid(iid, 0x69c11c3e, 0xb46b, 0x11d1, gamma))
        return K_GAMMA;
    return -1;
}

typedef HRESULT (WINAPI *method2)(void*, void*);
typedef HRESULT (WINAPI *method3)(void*, void*, void*);
typedef HRESULT (WINAPI *method4)(void*, void*, void*, void*);
typedef HRESULT (WINAPI *method5)(void*, void*, void*, void*, void*);
typedef HRESULT (WINAPI *method6)(void*, void*, void*, void*, void*, void*);
typedef HRESULT (WINAPI *method7)(void*, void*, void*, void*, void*, void*, void*);
typedef HRESULT (WINAPI *method8)(void*, void*, void*, void*, void*, void*, void*, void*);

#define U(x) ((unsigned) (UINT_PTR) (x))

static HRESULT WINAPI query_interface(void* self, GUID* iid, void** out)
{
    HRESULT hr = ((method3) original(self, 0))(self, iid, out);

    if (first_time(0xC0, iid->Data1, iid->Data2, iid->Data3))
        log_line("QueryInterface {%08X-%04X-%04X} -> %08X", U(iid->Data1), iid->Data2, iid->Data3, U(hr));

    if (SUCCEEDED(hr))
        trace(kind_of(iid), *out);
    return hr;
}

/* Methods whose last-but-n argument receives a new object of a known kind. */
static HRESULT WINAPI create_clipper(void* self, void* flags, void** out, void* outer)
{
    HRESULT hr = ((method4) original(self, 4))(self, flags, out, outer);
    if (SUCCEEDED(hr)) trace(K_CLIPPER, *out);
    return hr;
}

static HRESULT WINAPI create_palette(void* self, void* flags, void* entries, void** out, void* outer)
{
    HRESULT hr = ((method5) original(self, 5))(self, flags, entries, out, outer);
    if (SUCCEEDED(hr)) trace(K_PALETTE, *out);
    return hr;
}

/* DDSURFACEDESC2: +4 flags, +8 height, +C width, +48 pixel format (+4 flags, +8 FourCC,
   +C bit count), +68 caps, +6C caps2. */
static HRESULT WINAPI create_surface(void* self, unsigned char* desc, void** out, void* outer)
{
    HRESULT hr = ((method4) original(self, 6))(self, desc, out, outer);
    unsigned flags = *(unsigned*) (desc + 4), caps = *(unsigned*) (desc + 0x68);
    unsigned pf = *(unsigned*) (desc + 0x4C), bits = *(unsigned*) (desc + 0x54);

    if (first_time(0x51, flags ^ caps, *(unsigned*) (desc + 0x6C) ^ pf, bits))
        log_line("CreateSurface flags %08X caps %08X/%08X %ux%u pixel format %08X %u bits fourcc %08X -> %08X",
                 flags, caps, *(unsigned*) (desc + 0x6C), *(unsigned*) (desc + 0xC), *(unsigned*) (desc + 8),
                 pf, bits, *(unsigned*) (desc + 0x50), U(hr));

    if (SUCCEEDED(hr))
        trace(K_SURFACE, *out);
    return hr;
}

static HRESULT WINAPI get_object(int kind, int slot, void* self, void** out)
{
    HRESULT hr = ((method2) original(self, slot))(self, out);
    if (SUCCEEDED(hr)) trace(kind, *out);
    return hr;
}

static HRESULT WINAPI get_gdi_surface(void* self, void** out) { return get_object(K_SURFACE, 14, self, out); }
static HRESULT WINAPI get_dd_interface(void* self, void** out) { return get_object(K_DDRAW, 36, self, out); }
static HRESULT WINAPI get_direct3d(void* self, void** out) { return get_object(K_D3D, 7, self, out); }
static HRESULT WINAPI get_render_target(void* self, void** out) { return get_object(K_SURFACE, 9, self, out); }

static HRESULT WINAPI get_attached_surface(void* self, unsigned* caps, void** out)
{
    HRESULT hr = ((method3) original(self, 12))(self, caps, out);
    if (SUCCEEDED(hr)) trace(K_SURFACE, *out);
    return hr;
}

static HRESULT WINAPI set_cooperative_level(void* self, void* window, void* flags)
{
    log_line("SetCooperativeLevel flags %08X", U(flags));
    return ((method3) original(self, 20))(self, window, flags);
}

static HRESULT WINAPI set_display_mode(void* self, void* w, void* h, void* bits, void* refresh, void* flags)
{
    HRESULT hr = ((method6) original(self, 21))(self, w, h, bits, refresh, flags);
    log_line("SetDisplayMode %ux%u %u bits %u Hz -> %08X", U(w), U(h), U(bits), U(refresh), U(hr));
    return hr;
}

static HRESULT WINAPI blt(void* self, void* to, void* from, void* rect, void* flags, void* fx)
{
    if (first_time(0x55, U(flags), 0, 0))
        log_line("Blt flags %08X", U(flags));
    return ((method6) original(self, 5))(self, to, from, rect, flags, fx);
}

static HRESULT WINAPI blt_fast(void* self, void* x, void* y, void* from, void* rect, void* how)
{
    if (first_time(0x57, U(how), 0, 0))
        log_line("BltFast flags %08X", U(how));
    return ((method6) original(self, 7))(self, x, y, from, rect, how);
}

static HRESULT WINAPI flip(void* self, void* target, void* flags)
{
    if (first_time(0x5B, U(flags), 0, 0))
        log_line("Flip flags %08X", U(flags));
    return ((method3) original(self, 11))(self, target, flags);
}

static HRESULT WINAPI lock(void* self, void* rect, void* desc, void* flags, void* event)
{
    if (first_time(0x59, U(flags), 0, 0))
        log_line("Lock flags %08X", U(flags));
    return ((method5) original(self, 25))(self, rect, desc, flags, event);
}

static HRESULT WINAPI create_device(void* self, GUID* type, void* surface, void** out)
{
    HRESULT hr = ((method4) original(self, 4))(self, type, surface, out);
    log_line("CreateDevice {%08X-%04X-%04X} -> %08X", U(type->Data1), type->Data2, type->Data3, U(hr));
    if (SUCCEEDED(hr)) trace(K_DEVICE, *out);
    return hr;
}

/* D3DVERTEXBUFFERDESC: +4 caps, +8 FVF, +C vertex count. */
static HRESULT WINAPI create_vertex_buffer(void* self, unsigned* desc, void** out, void* flags)
{
    HRESULT hr = ((method4) original(self, 5))(self, desc, out, flags);
    if (first_time(0xB0, desc[1], desc[2], U(flags)))
        log_line("CreateVertexBuffer caps %08X fvf %08X %u vertices flags %08X -> %08X",
                 desc[1], desc[2], desc[3], U(flags), U(hr));
    if (SUCCEEDED(hr)) trace(K_BUFFER, *out);
    return hr;
}

static HRESULT WINAPI clear(void* self, void* count, void* rects, void* flags, void* colour, void* z, void* stencil)
{
    if (first_time(0xD10, U(flags), 0, 0))
        log_line("Clear flags %08X", U(flags));
    return ((method7) original(self, 10))(self, count, rects, flags, colour, z, stencil);
}

static HRESULT WINAPI set_transform(void* self, void* type, void* matrix)
{
    if (first_time(0xD11, U(type), 0, 0))
        log_line("SetTransform %u", U(type));
    return ((method3) original(self, 11))(self, type, matrix);
}

static HRESULT WINAPI set_render_state(void* self, void* state, void* value)
{
    if (first_time(0xD20, U(state), U(value), 0))
        log_line("SetRenderState %u = %08X", U(state), U(value));
    return ((method3) original(self, 20))(self, state, value);
}

static HRESULT WINAPI draw_primitive(void* self, void* type, void* fvf, void* vertices, void* count, void* flags)
{
    if (first_time(0xD25, U(type), U(fvf), U(flags)))
        log_line("DrawPrimitive type %u fvf %08X flags %08X", U(type), U(fvf), U(flags));
    return ((method6) original(self, 25))(self, type, fvf, vertices, count, flags);
}

static HRESULT WINAPI draw_indexed_primitive(void* self, void* type, void* fvf, void* vertices, void* count,
                                             void* indices, void* index_count, void* flags)
{
    if (first_time(0xD26, U(type), U(fvf), U(flags)))
        log_line("DrawIndexedPrimitive type %u fvf %08X flags %08X", U(type), U(fvf), U(flags));
    return ((method8) original(self, 26))(self, type, fvf, vertices, count, indices, index_count, flags);
}

static HRESULT WINAPI set_texture(void* self, void* stage, void* surface)
{
    if (first_time(0xD35, U(stage), 0, 0))
        log_line("SetTexture stage %u", U(stage));
    return ((method3) original(self, 35))(self, stage, surface);
}

static HRESULT WINAPI set_texture_stage_state(void* self, void* stage, void* type, void* value)
{
    if (first_time(0xD37, U(stage), U(type), U(value)))
        log_line("SetTextureStageState stage %u %u = %08X", U(stage), U(type), U(value));
    return ((method4) original(self, 37))(self, stage, type, value);
}

/* ------------------------------------------------------------------ installing */

typedef HRESULT (WINAPI *create_fn)(GUID*, void**, const GUID*, void*);

extern create_fn _imp__DirectDrawCreateEx;      /* the game's own import slot */
static create_fn g_create;

static HRESULT WINAPI traced_create(GUID* device, void** out, const GUID* iid, void* outer)
{
    HRESULT hr = g_create(device, out, iid, outer);

    log_line("DirectDrawCreateEx(%s) -> %08X", device ? "a device" : "the primary display", U(hr));
    if (SUCCEEDED(hr))
        trace(K_DDRAW, *out);
    return hr;
}

#define WRAP(kind, slot, fn) (g_wrapper[kind][slot] = (code) (fn))

void ddtrace_install(void)
{
    create_fn mine = traced_create;
    int k;

    for (k = 0; k < K_KINDS; ++k)
        g_wrapper[k][0] = (code) query_interface;

    WRAP(K_DDRAW, 4, create_clipper);
    WRAP(K_DDRAW, 5, create_palette);
    WRAP(K_DDRAW, 6, create_surface);
    WRAP(K_DDRAW, 14, get_gdi_surface);
    WRAP(K_DDRAW, 20, set_cooperative_level);
    WRAP(K_DDRAW, 21, set_display_mode);
    WRAP(K_SURFACE, 5, blt);
    WRAP(K_SURFACE, 7, blt_fast);
    WRAP(K_SURFACE, 11, flip);
    WRAP(K_SURFACE, 12, get_attached_surface);
    WRAP(K_SURFACE, 25, lock);
    WRAP(K_SURFACE, 36, get_dd_interface);
    WRAP(K_D3D, 4, create_device);
    WRAP(K_D3D, 5, create_vertex_buffer);
    WRAP(K_DEVICE, 7, get_direct3d);
    WRAP(K_DEVICE, 9, get_render_target);
    WRAP(K_DEVICE, 10, clear);
    WRAP(K_DEVICE, 11, set_transform);
    WRAP(K_DEVICE, 20, set_render_state);
    WRAP(K_DEVICE, 25, draw_primitive);
    WRAP(K_DEVICE, 26, draw_indexed_primitive);
    WRAP(K_DEVICE, 35, set_texture);
    WRAP(K_DEVICE, 37, set_texture_stage_state);

    g_create = _imp__DirectDrawCreateEx;
    if (write_protected(&_imp__DirectDrawCreateEx, &mine, sizeof mine))
        log_line("tracing DirectDraw");
    else
        log_line("could not take over DirectDrawCreateEx - no trace");
}
