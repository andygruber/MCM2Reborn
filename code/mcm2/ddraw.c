/*
    OpenMCM2 - DirectDraw 7 on OpenGL: the DirectDraw object, surfaces, palettes and gamma. See render.h.
*/

#include "render.h"

#include <intrin.h>

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"

#define BLUR_W 48       /* the side fill is the picture shrunk to this and stretched back */
#define BLUR_H 36

static GLuint g_blur_fbo, g_blur_colour;

/* Darken the window's part from `left` to `right` (black at `alpha`). */
static void darken(LONG left, LONG right, LONG width, LONG height, GLfloat alpha)
{
    GLfloat quad[4][2] = {
        { (GLfloat) left, 0 }, { (GLfloat) right, 0 },
        { (GLfloat) right, (GLfloat) height }, { (GLfloat) left, (GLfloat) height },
    };

    gl.ActiveTexture(GL_TEXTURE0 + 1);
    gl.Disable(GL_TEXTURE_2D);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.Disable(GL_TEXTURE_2D);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_FOG);
    gl.Disable(GL_ALPHA_TEST);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.Viewport(0, 0, width, height);
    gl.MatrixMode(GL_PROJECTION);
    gl.LoadIdentity();
    gl.Ortho(0, width, height, 0, -1, 1);
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadIdentity();
    gl.DisableClientState(GL_COLOR_ARRAY);
    gl.DisableClientState(GL_FOG_COORD_ARRAY);
    gl.ClientActiveTexture(GL_TEXTURE0 + 1);
    gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.ClientActiveTexture(GL_TEXTURE0);
    gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.EnableClientState(GL_VERTEX_ARRAY);
    gl.VertexPointer(2, GL_FLOAT, sizeof quad[0], quad[0]);
    gl.Color4f(0, 0, 0, alpha);
    gl.DrawArrays(GL_TRIANGLE_FAN, 0, 4);
    gl.Color4f(1, 1, 1, 1);
    gl.Disable(GL_BLEND);
}

/*
    Behind a picture narrower than the window: the picture covering the window, blurred by
    shrinking it to BLUR_W x BLUR_H and stretching it back, and the sides darkened.
*/
static void fill_sides(surface* target, LONG width, LONG height, LONG left, LONG right)
{
    LONG cover_h = width * (LONG) target->desc.dwHeight / (LONG) target->desc.dwWidth;
    LONG cover_y = (height - cover_h) / 2;

    if (!g_blur_fbo) {
        gl.GenFramebuffers(1, &g_blur_fbo);
        gl.GenRenderbuffers(1, &g_blur_colour);
        gl.BindRenderbuffer(GL_RENDERBUFFER, g_blur_colour);
        gl.RenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, BLUR_W, BLUR_H);
        gl.BindFramebuffer(GL_FRAMEBUFFER, g_blur_fbo);
        gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, g_blur_colour);
    }
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, target->fbo);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, g_blur_fbo);
    gl.BlitFramebuffer(0, 0, target->desc.dwWidth, target->desc.dwHeight, 0, 0, BLUR_W, BLUR_H,
                       GL_COLOR_BUFFER_BIT, GL_LINEAR);
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, g_blur_fbo);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.BlitFramebuffer(0, 0, BLUR_W, BLUR_H, 0, cover_y, width, cover_y + cover_h,
                       GL_COLOR_BUFFER_BIT, GL_LINEAR);
    darken(0, left, width, height, 0.55f);
    darken(right, width, width, height, 0.55f);
}
#endif

BOOL g_state_lost = TRUE;

/* ------------------------------------------------------------------ the display */

static HWND  g_window;
static BOOL  g_fullscreen;
static BOOL  g_gl;              /* the context exists */
static DWORD g_mode_w = 640, g_mode_h = 480;
static GLuint g_scratch;        /* a texture for drawing memory pixels onto a render target */

void display_size(DWORD* width, DWORD* height)
{
    *width = g_mode_w;
    *height = g_mode_h;
}

static BOOL ensure_gl(void)
{
    if (!g_gl && g_window)
    {
        g_gl = gl_start(g_window);
        if (g_gl)
            gl.GenTextures(1, &g_scratch);
    }
    return g_gl;
}

/* Cover the monitor the game's window is on, without changing the desktop's mode. */
static void cover_monitor(void)
{
    typedef HMONITOR (WINAPI *monitor_fn)(HWND, DWORD);
    typedef BOOL (WINAPI *info_fn)(HMONITOR, MONITORINFO*);
    typedef LONG (WINAPI *style_fn)(HWND, int, LONG);
    monitor_fn monitor = (monitor_fn) api("user32.dll", "MonitorFromWindow");
    info_fn info = (info_fn) api("user32.dll", "GetMonitorInfoA");
    style_fn style = (style_fn) api("user32.dll", "SetWindowLongA");
    MONITORINFO mi = { sizeof mi };

    if (!g_window || !monitor || !info || !style || !info(monitor(g_window, MONITOR_DEFAULTTOPRIMARY), &mi))
        return;

    style(g_window, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    SetWindowPos(g_window, HWND_TOP, mi.rcMonitor.left, mi.rcMonitor.top,
                 mi.rcMonitor.right - mi.rcMonitor.left, mi.rcMonitor.bottom - mi.rcMonitor.top,
                 SWP_FRAMECHANGED | SWP_SHOWWINDOW);
}

/* Show a render target in the window, scaled to fit and keeping its aspect. */
/*
    The modern main menu's photographs: a layer under the menu that crossfades between the
    pictures in ui\modern\photo1.tga.. (32-bit, bottom up), cover-scaled to the window whatever
    its size. The menu shows them where it draws HOLE, which the render target keeps with no
    alpha while the layer is on.

    The colours beside HOLE, red 29..31 with no green, are inks the photos show through: blue
    the amount of a light blue glow (red 31, blue 0..29), of blue (red 30) or of shadow (red 29).
    The target keeps them premultiplied, and they read back to the same colours. Only the
    modern menus' own pictures (render_inks) are read so, and a target since one was drawn on
    it, until it is cleared whole; with no photos they lie on MATTE.
*/
#define PHOTOS_MOST 8
#define PHOTO_HOLD  7000        /* milliseconds a photo shows */
#define PHOTO_FADE  1600        /* and fades into the next */

static struct
{
    BOOL   on, loaded;
    int    count;
    GLuint texture[PHOTOS_MOST];
    LONG   width[PHOTOS_MOST], height[PHOTOS_MOST];
    DWORD  start;
} g_photos;

static void unpack(int format, WORD p, BYTE* rgba);
static WORD pack(int format, const BYTE* rgba);

/* The inks of red 29, 30 and 31: shadow, blue, light blue. */
static const BYTE k_inks[3][3] = { { 0, 0, 0 }, { 34, 110, 255 }, { 110, 185, 255 } };

/* A menu pixel, as unpacked, as the target keeps it over the photos: HOLE clear, an ink
   premultiplied, anything else opaque. */
static void photo_pixel(BYTE* rgba)
{
    int red = rgba[0] >> 3, blue = rgba[2] >> 3, n;
    unsigned amount;

    rgba[3] = 255;
    if (red < 29 || rgba[1] >> 2)
        return;
    if (red == 31 && blue >= 30)
    {
        if (blue == 30)
            rgba[0] = rgba[1] = rgba[2] = rgba[3] = 0;
        return;
    }
    amount = red == 31 ? (blue + 1) * 255 / 30 : (blue + 1) * 255 / 32;
    for (n = 0; n < 3; ++n)
        rgba[n] = (BYTE) (k_inks[red - 29][n] * amount / 255);
    rgba[3] = (BYTE) amount;
}

/* An ink with no photos under it: on the dark the menus' art was drawn for. */
static void on_matte(BYTE* rgba)
{
    static const BYTE matte[3] = { 12, 13, 16 };
    int n;

    for (n = 0; n < 3; ++n)
        rgba[n] = (BYTE) (rgba[n] + matte[n] * (255 - rgba[3]) / 255);
    rgba[3] = 255;
}

/* A modern menu picture: its inks are read as inks. */
void render_inks(void* picture_surface)
{
    surface* s = surface_of((IDirectDrawSurface7*) picture_surface);

    if (s)
        s->inks = TRUE;
}

/* The other way, for a target read back: the 565 colour a pixel of the target came from. */
static WORD photo_code(const BYTE* rgba)
{
    int best = 0, n, i;
    long nearest = -1;

    if (!rgba[3])
        return 31 << 11 | 30;
    for (i = 0; i < 3; ++i)
    {
        long d = 0;

        for (n = 0; n < 3; ++n)
        {
            long e = (long) rgba[n] - (long) k_inks[i][n] * rgba[3] / 255;
            d += e * e;
        }
        if (nearest < 0 || d < nearest)
        {
            nearest = d;
            best = i;
        }
    }
    n = (rgba[3] * (best == 2 ? 30 : 32) + 127) / 255 - 1;
    return (WORD) ((29 + best) << 11 | (n < 0 ? 0 : n));
}

static BOOL is_photo_code(WORD p)
{
    return p >> 11 >= 29 && !(p >> 5 & 63) && p != (31 << 11 | 31);
}

/*
    Every 565 pixel as GDI should see it, with the photos off and on: a hole or ink as it shows
    with nothing under it, so the game's type blends with something dark rather than with the
    codes' magentas.
*/
static const WORD* photo_shown_table(void)
{
    static WORD* table[2];
    int on = g_photos.on ? 1 : 0;
    DWORD p;

    if (table[on])
        return table[on];
    table[on] = (WORD*) mem_alloc(65536 * sizeof(WORD));
    if (!table[on])
        return NULL;
    for (p = 0; p < 65536; ++p)
    {
        BYTE rgba[4];

        table[on][p] = (WORD) p;
        if (!is_photo_code((WORD) p))
            continue;
        unpack(PF_565, (WORD) p, rgba);
        photo_pixel(rgba);
        if (!on)
            on_matte(rgba);
        table[on][p] = pack(PF_565, rgba);
    }
    return table[on];
}

/*
    A target with inks keeps `shown`, its memory image as GDI should see it, brought up to date
    only where the memory has changed since (`stale`), so GetDC is a copy.
*/
static void shown_stale(surface* s, const RECT* r)
{
    RECT all = { 0, 0, (LONG) s->desc.dwWidth, (LONG) s->desc.dwHeight };

    if (!s->shown)
        return;
    if (!r)
        r = &all;
    if (s->stale.left >= s->stale.right)
        s->stale = *r;
    else
    {
        s->stale.left = min(s->stale.left, r->left);
        s->stale.top = min(s->stale.top, r->top);
        s->stale.right = max(s->stale.right, r->right);
        s->stale.bottom = max(s->stale.bottom, r->bottom);
    }
    s->stale.left = max(s->stale.left, 0);
    s->stale.top = max(s->stale.top, 0);
    s->stale.right = min(s->stale.right, all.right);
    s->stale.bottom = min(s->stale.bottom, all.bottom);
}

/* `shown` up to date, or NULL when it cannot be. */
static BYTE* shown_current(surface* s)
{
    const WORD* table = photo_shown_table();
    LONG pitch = surface_pitch(s), x, y;

    if (!table || !s->pixels)
        return NULL;
    if (!s->shown)
    {
        s->shown = (BYTE*) mem_alloc((SIZE_T) pitch * s->desc.dwHeight);
        if (!s->shown)
            return NULL;
        s->shown_photos = !g_photos.on;
    }
    if (s->shown_photos != g_photos.on)
    {
        s->shown_photos = g_photos.on;
        s->stale.right = 0;
        shown_stale(s, NULL);
    }
    for (y = s->stale.top; y < s->stale.bottom; ++y)
    {
        const WORD* from = (const WORD*) (s->pixels + (SIZE_T) y * pitch);
        WORD* to = (WORD*) (s->shown + (SIZE_T) y * pitch);

        for (x = s->stale.left; x < s->stale.right; ++x)
            to[x] = table[from[x]];
    }
    s->stale.left = s->stale.top = s->stale.right = s->stale.bottom = 0;
    return s->shown;
}

/* The time in milliseconds, through kernel32 (the game does not import GetTickCount). */
static DWORD photo_clock(void)
{
    typedef DWORD (WINAPI* tick_fn)(void);
    static tick_fn tick;

    if (!tick)
        tick = (tick_fn) api("kernel32.dll", "GetTickCount");
    return tick ? tick() : 0;
}

/* The photos on or off; they carry on from screen to screen rather than starting again. */
void render_photos(BOOL on)
{
    if (on && !g_photos.start)
        g_photos.start = photo_clock();
    g_photos.on = on;
}

static void load_photos(void)
{
    int n;

    g_photos.loaded = TRUE;
    for (n = 0; n < PHOTOS_MOST; ++n)
    {
        char name[64];
        BYTE header[18];
        HANDLE file;
        DWORD got, size;
        BYTE* pixels;
        LONG w, h;

        wsprintfA(name, "ui\\modern\\photo%d.tga", n + 1);
        file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        if (file == INVALID_HANDLE_VALUE)
            break;
        if (!ReadFile(file, header, sizeof header, &got, NULL) || got != sizeof header
            || header[2] != 2 || header[16] != 32)
        {
            CloseHandle(file);
            break;
        }
        w = header[12] | header[13] << 8;
        h = header[14] | header[15] << 8;
        size = (DWORD) w * h * 4;
        SetFilePointer(file, 18 + header[0], NULL, FILE_BEGIN);
        pixels = (BYTE*) mem_alloc(size);
        if (!pixels || !ReadFile(file, pixels, size, &got, NULL) || got != size)
        {
            mem_free(pixels);
            CloseHandle(file);
            break;
        }
        CloseHandle(file);

        gl.GenTextures(1, &g_photos.texture[n]);
        gl.BindTexture(GL_TEXTURE_2D, g_photos.texture[n]);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, pixels);
        mem_free(pixels);
        g_photos.width[n] = w;
        g_photos.height[n] = h;
        g_photos.count = n + 1;
    }
    log_line("render: %d menu photo(s)", g_photos.count);
}

/* A textured quad over x0..x1, y0..y1 of the window (y up), its texture's u0..u1, v0..v1. */
static void quad(GLfloat x0, GLfloat y0, GLfloat x1, GLfloat y1, GLfloat u0, GLfloat v0, GLfloat u1,
                 GLfloat v1)
{
    GLfloat q[4][4] = {
        { x0, y0, u0, v0 }, { x1, y0, u1, v0 }, { x1, y1, u1, v1 }, { x0, y1, u0, v1 },
    };

    gl.VertexPointer(2, GL_FLOAT, sizeof q[0], q[0]);
    gl.TexCoordPointer(2, GL_FLOAT, sizeof q[0], &q[0][2]);
    gl.DrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

/* Photo `n` covering the w x h window, cropped about its middle, at `alpha`. */
static void draw_photo(int n, LONG w, LONG h, GLfloat alpha)
{
    GLfloat pw = (GLfloat) g_photos.width[n], ph = (GLfloat) g_photos.height[n];
    GLfloat u = 0, v = 0;

    if (w * ph > h * pw)
        v = (1 - (pw * h / w) / ph) / 2;       /* the window is wider: crop top and bottom */
    else
        u = (1 - (ph * w / h) / pw) / 2;
    gl.BindTexture(GL_TEXTURE_2D, g_photos.texture[n]);
    gl.Color4f(1, 1, 1, alpha);
    quad(0, 0, (GLfloat) w, (GLfloat) h, u, v, 1 - u, 1 - v);
}

/* The photos, then the menu over them where it is not see-through. */
static void present_over_photos(surface* target, LONG w, LONG h, LONG x, LONG y, LONG fit_w, LONG fit_h)
{
    DWORD cycle = (DWORD) g_photos.count * (PHOTO_HOLD + PHOTO_FADE);
    DWORD at = (photo_clock() - g_photos.start) % cycle;
    int shown = (int) (at / (PHOTO_HOLD + PHOTO_FADE));
    DWORD into = at % (PHOTO_HOLD + PHOTO_FADE);

    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_FOG);
    gl.DepthMask(GL_FALSE);
    gl.ActiveTexture(GL_TEXTURE0 + 1);
    gl.Disable(GL_TEXTURE_2D);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.Enable(GL_TEXTURE_2D);
    gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    gl.MatrixMode(GL_PROJECTION);
    gl.LoadIdentity();
    gl.Ortho(0, w, 0, h, -1, 1);
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadIdentity();
    gl.DisableClientState(GL_COLOR_ARRAY);
    gl.DisableClientState(GL_FOG_COORD_ARRAY);
    gl.ClientActiveTexture(GL_TEXTURE0 + 1);
    gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.ClientActiveTexture(GL_TEXTURE0);
    gl.EnableClientState(GL_VERTEX_ARRAY);
    gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);

    gl.Disable(GL_ALPHA_TEST);
    gl.Disable(GL_BLEND);
    draw_photo(shown, w, h, 1);
    if (into > PHOTO_HOLD && g_photos.count > 1)
    {
        GLfloat t = (GLfloat) (into - PHOTO_HOLD) / PHOTO_FADE;

        gl.Enable(GL_BLEND);
        gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        draw_photo((shown + 1) % g_photos.count, w, h, t * t * (3 - 2 * t));
        gl.Disable(GL_BLEND);
    }

    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);      /* the target is premultiplied */
    gl.BindTexture(GL_TEXTURE_2D, target->colour);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.Color4f(1, 1, 1, 1);
    quad((GLfloat) x, (GLfloat) y, (GLfloat) (x + fit_w), (GLfloat) (y + fit_h), 0, 0, 1, 1);
    gl.Disable(GL_BLEND);
}

static void present(surface* target)
{
    RECT client;
    LONG w, h, fit_w, fit_h, x, y;

    if (!target || !target->fbo || !GetClientRect(g_window, &client))
        return;

    w = client.right;
    h = client.bottom;
    fit_w = w;
    fit_h = w * (LONG) target->desc.dwHeight / (LONG) target->desc.dwWidth;   /* both fit 32 bits easily */
    if (fit_h > h)
    {
        fit_h = h;
        fit_w = h * (LONG) target->desc.dwWidth / (LONG) target->desc.dwHeight;
    }
    x = (w - fit_w) / 2;
    y = (h - fit_h) / 2;

    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.Disable(GL_SCISSOR_TEST);
    gl.Viewport(0, 0, w, h);
    gl.ClearColor(0, 0, 0, 1);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    if (g_photos.on && !g_photos.loaded)
        load_photos();
    if (g_photos.on && g_photos.count)
    {
        present_over_photos(target, w, h, x, y, fit_w, fit_h);
        gl_present();
        {
            BOOL current = target->pixels_current;

            target_bind(target);
            target->pixels_current = current;
        }
        g_state_lost = TRUE;
        return;
    }
#ifdef OPENMCM2_DEV
    if (dev_settings.menus_fill_sides && fit_w < w)
        fill_sides(target, w, h, x, x + fit_w);
#endif
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, target->fbo);
    gl.BlitFramebuffer(0, 0, target->desc.dwWidth, target->desc.dwHeight, x, y, x + fit_w, y + fit_h,
                       GL_COLOR_BUFFER_BIT, GL_LINEAR);
    gl_present();

    {
        BOOL current = target->pixels_current;

        target_bind(target);
        target->pixels_current = current;
    }
    g_state_lost = TRUE;
}

/* ------------------------------------------------------------------ pixel formats */

typedef struct format_info
{
    DWORD  flags, r, g, b, a;
    GLenum gl_internal, gl_format, gl_type;
} format_info;

static const format_info k_format[PF_FORMATS] = {
    { DDPF_RGB, 0xF800, 0x07E0, 0x001F, 0x0000, GL_RGB, GL_RGB, GL_UNSIGNED_SHORT_5_6_5 },
    { DDPF_RGB | DDPF_ALPHAPIXELS, 0x7C00, 0x03E0, 0x001F, 0x8000, GL_RGBA, GL_BGRA, GL_UNSIGNED_SHORT_1_5_5_5_REV },
    { DDPF_RGB | DDPF_ALPHAPIXELS, 0x0F00, 0x00F0, 0x000F, 0xF000, GL_RGBA, GL_BGRA, GL_UNSIGNED_SHORT_4_4_4_4_REV },
};

void describe_format(int format, DDPIXELFORMAT* pf)
{
    const format_info* f = &k_format[format];

    __stosb((unsigned char*) pf, 0, sizeof *pf);
    pf->dwSize = sizeof *pf;
    pf->dwFlags = f->flags;
    pf->dwRGBBitCount = 16;
    pf->dwRBitMask = f->r;
    pf->dwGBitMask = f->g;
    pf->dwBBitMask = f->b;
    pf->dwRGBAlphaBitMask = f->a;
}

static int format_of(const DDPIXELFORMAT* pf)
{
    int i;

    for (i = 0; i < PF_FORMATS; ++i)
    {
        if (pf->dwRBitMask == k_format[i].r && pf->dwGBitMask == k_format[i].g &&
            pf->dwBBitMask == k_format[i].b && pf->dwRGBAlphaBitMask == k_format[i].a)
            return i;
    }
    return PF_565;
}

/* One pixel as 8-bit RGBA, and back. */
static void unpack(int format, WORD p, BYTE* rgba)
{
    switch (format)
    {
    case PF_565:
        rgba[0] = (BYTE) ((p >> 8 & 0xF8) | p >> 13);
        rgba[1] = (BYTE) ((p >> 3 & 0xFC) | (p >> 9 & 3));
        rgba[2] = (BYTE) ((p << 3 & 0xF8) | (p >> 2 & 7));
        rgba[3] = 255;
        break;
    case PF_1555:
        rgba[0] = (BYTE) ((p >> 7 & 0xF8) | (p >> 12 & 7));
        rgba[1] = (BYTE) ((p >> 2 & 0xF8) | (p >> 7 & 7));
        rgba[2] = (BYTE) ((p << 3 & 0xF8) | (p >> 2 & 7));
        rgba[3] = (p & 0x8000) ? 255 : 0;
        break;
    default:
        rgba[0] = (BYTE) ((p >> 8 & 15) * 17);
        rgba[1] = (BYTE) ((p >> 4 & 15) * 17);
        rgba[2] = (BYTE) ((p & 15) * 17);
        rgba[3] = (BYTE) ((p >> 12 & 15) * 17);
        break;
    }
}

static WORD pack(int format, const BYTE* rgba)
{
    switch (format)
    {
    case PF_565:
        return (WORD) ((rgba[0] & 0xF8) << 8 | (rgba[1] & 0xFC) << 3 | rgba[2] >> 3);
    case PF_1555:
        return (WORD) ((rgba[3] >= 128) << 15 | (rgba[0] & 0xF8) << 7 | (rgba[1] & 0xF8) << 2 | rgba[2] >> 3);
    default:
        return (WORD) ((rgba[3] >> 4) << 12 | (rgba[0] >> 4) << 8 | (rgba[1] >> 4) << 4 | rgba[2] >> 4);
    }
}

/* ------------------------------------------------------------------ surfaces: memory and GL */

static IDirectDrawSurface7Vtbl k_surface_vtbl;
static IDirectDrawGammaControlVtbl k_gamma_vtbl;

surface* surface_of(IDirectDrawSurface7* iface)
{
    return (iface && iface->lpVtbl == &k_surface_vtbl) ? (surface*) iface : NULL;
}

LONG surface_pitch(const surface* s)
{
    return (LONG) (s->desc.dwWidth * 2 + 3) & ~3;
}

static BOOL is_target(const surface* s)
{
    return (s->desc.ddsCaps.dwCaps & DDSCAPS_3DDEVICE) != 0;
}

static BOOL is_primary(const surface* s)
{
    return (s->desc.ddsCaps.dwCaps & DDSCAPS_PRIMARYSURFACE) != 0;
}

static BOOL is_zbuffer(const surface* s)
{
    return (s->desc.ddsCaps.dwCaps & DDSCAPS_ZBUFFER) != 0;
}

/* A surface's memory image, allocated when first needed. */
static BYTE* pixels(surface* s)
{
    if (!s->pixels)
        s->pixels = (BYTE*) mem_alloc((SIZE_T) surface_pitch(s) * s->desc.dwHeight);
    return s->pixels;
}

void target_bind(surface* target)
{
    if (!ensure_gl() || !target)
        return;
    target->pixels_current = FALSE;

    if (!target->fbo)
    {
        gl.GenFramebuffers(1, &target->fbo);
        gl.GenTextures(1, &target->colour);
        gl.GenRenderbuffers(1, &target->depth);

        gl.BindTexture(GL_TEXTURE_2D, target->colour);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, target->desc.dwWidth, target->desc.dwHeight, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        gl.BindRenderbuffer(GL_RENDERBUFFER, target->depth);
        gl.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, target->desc.dwWidth, target->desc.dwHeight);

        gl.BindFramebuffer(GL_FRAMEBUFFER, target->fbo);
        gl.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target->colour, 0);
        gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, target->depth);
        if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            log_line("render: the %ux%u render target is incomplete", target->desc.dwWidth, target->desc.dwHeight);

        gl.Disable(GL_SCISSOR_TEST);
        gl.ClearColor(0, 0, 0, 1);
        gl.ClearDepth(1);
        gl.DepthMask(GL_TRUE);
        gl.Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        g_state_lost = TRUE;
    }

    gl.BindFramebuffer(GL_FRAMEBUFFER, target->fbo);
}

void texture_bind(surface* s)
{
    surface* level;
    int n;

    if (!s)
    {
        gl.BindTexture(GL_TEXTURE_2D, 0);
        return;
    }

    s = s->top;
    if (!s->texture)
    {
        gl.GenTextures(1, &s->texture);
        s->dirty = TRUE;
    }
    gl.BindTexture(GL_TEXTURE_2D, s->texture);

    if (!s->dirty)
        return;

    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    for (level = s, n = 0; level; level = level->mip, ++n)
    {
        const format_info* f = &k_format[level->format];
        gl.TexImage2D(GL_TEXTURE_2D, n, f->gl_internal, level->desc.dwWidth, level->desc.dwHeight, 0,
                      f->gl_format, f->gl_type, pixels(level));
    }
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, n - 1);
    s->dirty = FALSE;
}

/* Copy a render target's pixels into its memory image, top row first, as 16-bit 565. */
static void target_read(surface* s)
{
    LONG pitch = surface_pitch(s);
    BYTE* row = pixels(s);
    BYTE* rgba = (BYTE*) mem_alloc((SIZE_T) s->desc.dwWidth * s->desc.dwHeight * 4);
    DWORD x, y;

    if (!rgba || !row || !ensure_gl())
    {
        mem_free(rgba);
        return;
    }

    target_bind(s);
    gl.PixelStorei(GL_PACK_ALIGNMENT, 4);
    gl.ReadPixels(0, 0, s->desc.dwWidth, s->desc.dwHeight, GL_RGBA, GL_UNSIGNED_BYTE, rgba);

    for (y = 0; y < s->desc.dwHeight; ++y)
    {
        const BYTE* from = rgba + (SIZE_T) (s->desc.dwHeight - 1 - y) * s->desc.dwWidth * 4;
        WORD* to = (WORD*) (row + (SIZE_T) y * pitch);

        for (x = 0; x < s->desc.dwWidth; ++x)
            to[x] = s->inks && from[x * 4 + 3] < 255 ? photo_code(from + x * 4)
                                                      : pack(PF_565, from + x * 4);
    }
    mem_free(rgba);
    s->pixels_current = TRUE;
    shown_stale(s, NULL);
}

/* Every pixel of `format` as draw_pixels shows it from a picture with inks, photos off or on
   as they are now: 8-bit RGBA. NULL when there is no memory for it. */
static const BYTE* ink_table(int format)
{
    static BYTE* table[PF_FORMATS][2];
    BYTE** made = &table[format][g_photos.on ? 1 : 0];
    DWORD p;

    if (*made)
        return *made;
    *made = (BYTE*) mem_alloc(65536 * 4);
    if (!*made)
        return NULL;
    for (p = 0; p < 65536; ++p)
    {
        BYTE* out = *made + p * 4;

        unpack(format, (WORD) p, out);
        photo_pixel(out);
        if (!g_photos.on)
            on_matte(out);
    }
    return *made;
}

/* Draw a rectangle of 16-bit memory pixels onto the bound render target, scaled to `to`. With
   `keyed`, pixels matching `key` are left out. */
static void draw_pixels(surface* target, const BYTE* from, LONG pitch, int format, const RECT* part,
                        const RECT* to, BOOL keyed, DWORD key, BOOL inks)
{
    LONG w = part->right - part->left, h = part->bottom - part->top, x, y;
    BYTE* rgba = (BYTE*) mem_alloc((SIZE_T) w * h * 4);
    const BYTE* inked = inks ? ink_table(format) : NULL;

    if (!rgba || w <= 0 || h <= 0)
    {
        mem_free(rgba);
        return;
    }

    for (y = 0; y < h; ++y)
    {
        const WORD* row = (const WORD*) (from + (SIZE_T) (part->top + y) * pitch) + part->left;

        for (x = 0; x < w; ++x)
        {
            BYTE* out = rgba + ((SIZE_T) y * w + x) * 4;

            if (inked)
                *(DWORD*) out = *(const DWORD*) (inked + row[x] * 4);
            else
            {
                unpack(format, row[x], out);
                if (inks)
                {
                    photo_pixel(out);
                    if (!g_photos.on)
                        on_matte(out);
                }
                else if (g_photos.on || format == PF_565 || keyed)
                    out[3] = 255;
            }
            if (keyed && row[x] == key)
                out[3] = 0;
        }
    }

    target_bind(target);
    gl.ActiveTexture(GL_TEXTURE0 + 1);
    gl.Disable(GL_TEXTURE_2D);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.Enable(GL_TEXTURE_2D);
    gl.BindTexture(GL_TEXTURE_2D, g_scratch);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 1);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    mem_free(rgba);

    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_BLEND);
    gl.Disable(GL_FOG);
    gl.Disable(GL_SCISSOR_TEST);
    gl.DepthMask(GL_FALSE);
    if (keyed)
    {
        gl.Enable(GL_ALPHA_TEST);
        gl.AlphaFunc(GL_NEVER + 4, 0.5f);       /* GL_GREATER */
    }
    else
    {
        gl.Disable(GL_ALPHA_TEST);
    }

    gl.Viewport(0, 0, target->desc.dwWidth, target->desc.dwHeight);
    gl.MatrixMode(GL_PROJECTION);
    gl.LoadIdentity();
    gl.Ortho(0, target->desc.dwWidth, target->desc.dwHeight, 0, -1, 1);
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadIdentity();

    {
        GLfloat quad[4][4] = {
            { (GLfloat) to->left, (GLfloat) to->top, 0, 0 },
            { (GLfloat) to->right, (GLfloat) to->top, 1, 0 },
            { (GLfloat) to->right, (GLfloat) to->bottom, 1, 1 },
            { (GLfloat) to->left, (GLfloat) to->bottom, 0, 1 },
        };

        /* Every array the device's draws enable is turned off: OpenGL reads all enabled arrays,
           and theirs point at the device's vertex memory, which may have been freed since. */
        gl.DisableClientState(GL_COLOR_ARRAY);
        gl.DisableClientState(GL_FOG_COORD_ARRAY);
        gl.ClientActiveTexture(GL_TEXTURE0 + 1);
        gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
        gl.ClientActiveTexture(GL_TEXTURE0);
        gl.EnableClientState(GL_VERTEX_ARRAY);
        gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
        gl.VertexPointer(2, GL_FLOAT, sizeof quad[0], quad[0]);
        gl.TexCoordPointer(2, GL_FLOAT, sizeof quad[0], &quad[0][2]);
        gl.DrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    g_state_lost = TRUE;
}

/* Put a render target's memory image back after the game wrote to it. */
static void target_write(surface* s)
{
    RECT all = { 0, 0, (LONG) s->desc.dwWidth, (LONG) s->desc.dwHeight };

    draw_pixels(s, s->pixels, surface_pitch(s), PF_565, &all, &all, FALSE, 0, s->inks);
    s->pixels_current = TRUE;
    shown_stale(s, NULL);
}

/*
    The part of a render target that changed in its memory image, from `before` (a copy taken
    when it was handed out): drawn back, the rest left alone. Returns whether anything changed.
*/
static BOOL target_write_changes(surface* s, const BYTE* changed, const BYTE* before)
{
    LONG pitch = surface_pitch(s), w = (LONG) s->desc.dwWidth, h = (LONG) s->desc.dwHeight;
    RECT part = { w, h, 0, 0 };
    const WORD* table = before == s->shown ? photo_shown_table() : NULL;
    LONG x, y;

    for (y = 0; y < h; ++y)
    {
        const WORD* a = (const WORD*) (changed + (SIZE_T) y * pitch);
        const WORD* b = (const WORD*) (before + (SIZE_T) y * pitch);

        if (!memcmp(a, b, (SIZE_T) w * 2))
            continue;
        if (y < part.top)
            part.top = y;
        part.bottom = y + 1;
        for (x = 0; x < part.left && a[x] == b[x]; ++x)
            ;
        part.left = x;
        for (x = w - 1; x >= part.right && a[x] == b[x]; --x)
            ;
        if (x + 1 > part.right)
            part.right = x + 1;
    }
    if (part.left >= part.right)
        return FALSE;
    /* Only what GDI changed goes into the memory image: the rest keeps its holes and inks. */
    for (y = part.top; y < part.bottom; ++y)
    {
        const WORD* a = (const WORD*) (changed + (SIZE_T) y * pitch);
        WORD* memory = (WORD*) (s->pixels + (SIZE_T) y * pitch);
        WORD* seen = table ? (WORD*) (s->shown + (SIZE_T) y * pitch) : NULL;

        for (x = part.left; x < part.right; ++x)
            if (a[x] != (seen ? seen[x] : memory[x]))
            {
                memory[x] = a[x];
                if (seen)
                    seen[x] = table[a[x]];
            }
    }
    draw_pixels(s, s->pixels, pitch, PF_565, &part, &part, FALSE, 0, s->inks);
    s->pixels_current = TRUE;
    return TRUE;
}

/* ------------------------------------------------------------------ creating surfaces */

static surface* new_surface(const DDSURFACEDESC2* request)
{
    surface* s = (surface*) mem_alloc(sizeof *s);

    if (!s)
        return NULL;

    s->iface.lpVtbl = &k_surface_vtbl;
    s->iface1.lpVtbl = &k_surface1_vtbl;
    s->gamma.lpVtbl = &k_gamma_vtbl;
    s->refs = 1;
    s->top = s;
    s->desc = *request;
    s->desc.dwSize = sizeof s->desc;
    return s;
}

static void free_surface(surface* s)
{
    while (s)
    {
        surface* next = s->mip;

        if (s->back)
            free_surface(s->back);
        if (s->zbuffer)
            s->zbuffer->iface.lpVtbl->Release(&s->zbuffer->iface);
        if (g_gl)
        {
            if (s->texture) gl.DeleteTextures(1, &s->texture);
            if (s->colour) gl.DeleteTextures(1, &s->colour);
            if (s->fbo) gl.DeleteFramebuffers(1, &s->fbo);
            if (s->depth) gl.DeleteRenderbuffers(1, &s->depth);
        }
        if (s->palette)
            s->palette->lpVtbl->Release(s->palette);
        mem_free(s->pixels);
        mem_free(s->shown);
        mem_free(s);
        s = next;
    }
}

/* Fill in the description the game will read back: size, pitch, format and memory caps. */
static void complete(surface* s, int format)
{
    DDSURFACEDESC2* d = &s->desc;

    if (is_primary(s) || (!(d->dwFlags & DDSD_WIDTH) && is_target(s)))
    {
        d->dwWidth = g_mode_w;
        d->dwHeight = g_mode_h;
    }

    d->dwFlags |= DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_CAPS;
    d->lPitch = surface_pitch(s);
    if (!(d->ddsCaps.dwCaps & DDSCAPS_SYSTEMMEMORY))
        d->ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM;

    if (is_zbuffer(s))
    {
        d->ddpfPixelFormat.dwSize = sizeof d->ddpfPixelFormat;
        d->ddpfPixelFormat.dwFlags = DDPF_ZBUFFER;
        if (!d->ddpfPixelFormat.dwZBufferBitDepth)
            d->ddpfPixelFormat.dwZBufferBitDepth = 16;
        d->ddpfPixelFormat.dwZBitMask = d->ddpfPixelFormat.dwZBufferBitDepth == 16 ? 0xFFFF : 0xFFFFFF;
    }
    else
    {
        s->format = format;
        describe_format(format, &d->ddpfPixelFormat);
    }
}

/* The levels below a mipmapped texture, each half the size of the one above it. */
static BOOL add_mips(surface* top, DWORD count)
{
    surface* above = top;
    DWORD i;

    for (i = 1; i < count && (above->desc.dwWidth > 1 || above->desc.dwHeight > 1); ++i)
    {
        surface* level = new_surface(&above->desc);

        if (!level)
            return FALSE;

        level->desc.dwWidth = above->desc.dwWidth > 1 ? above->desc.dwWidth / 2 : 1;
        level->desc.dwHeight = above->desc.dwHeight > 1 ? above->desc.dwHeight / 2 : 1;
        level->desc.lPitch = surface_pitch(level);
        level->desc.dwMipMapCount = count - i;
        level->desc.ddsCaps.dwCaps2 |= DDSCAPS2_MIPMAPSUBLEVEL;
        level->format = top->format;
        level->top = top;
        above->mip = level;
        above = level;
    }
    return TRUE;
}

static HRESULT create_surface(DDSURFACEDESC2* request, surface** out)
{
    surface* s;
    DWORD caps = request->ddsCaps.dwCaps;
    int format = (request->dwFlags & DDSD_PIXELFORMAT) ? format_of(&request->ddpfPixelFormat) : PF_565;

    s = new_surface(request);
    if (!s)
        return DDERR_OUTOFMEMORY;

    complete(s, format);

    if (is_primary(s) && (request->dwFlags & DDSD_BACKBUFFERCOUNT) && request->dwBackBufferCount)
    {
        DDSURFACEDESC2 back = s->desc;

        back.dwFlags &= ~DDSD_BACKBUFFERCOUNT;
        back.ddsCaps.dwCaps = (caps & ~(DDSCAPS_PRIMARYSURFACE | DDSCAPS_FRONTBUFFER)) | DDSCAPS_BACKBUFFER;
        s->back = new_surface(&back);
        if (!s->back)
        {
            free_surface(s);
            return DDERR_OUTOFMEMORY;
        }
        complete(s->back, PF_565);
        s->back->top = s;       /* attached: it lives as long as the primary, sharing its count */
        s->desc.ddsCaps.dwCaps |= DDSCAPS_FRONTBUFFER;
    }

    if (caps & DDSCAPS_MIPMAP)
    {
        DWORD count = (request->dwFlags & DDSD_MIPMAPCOUNT) ? request->dwMipMapCount : 32;

        s->desc.dwFlags |= DDSD_MIPMAPCOUNT;
        if (!add_mips(s, count))
        {
            free_surface(s);
            return DDERR_OUTOFMEMORY;
        }
        {
            DWORD levels = 0;
            surface* level;

            for (level = s; level; level = level->mip)
                ++levels;
            s->desc.dwMipMapCount = levels;
        }
    }

    *out = s;
    return DD_OK;
}

/* ------------------------------------------------------------------ IDirectDrawSurface7 */

#define SELF ((surface*) self)

static HRESULT WINAPI s_QueryInterface(IDirectDrawSurface7* self, REFIID iid, void** out)
{
    /* Version 4 is version 7's first 45 methods with the same types, so the same pointer serves. */
    if (IsEqualGUID(iid, &IID_IDirectDrawSurface7) || IsEqualGUID(iid, &IID_IDirectDrawSurface4) ||
        IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = self;
        ++SELF->refs;
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IDirectDrawGammaControl))
    {
        *out = &SELF->gamma;
        ++SELF->refs;
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IDirectDrawSurface) || IsEqualGUID(iid, &IID_IDirectDrawSurface2) ||
        IsEqualGUID(iid, &IID_IDirectDrawSurface3))
    {
        *out = &SELF->iface1;
        ++SELF->top->refs;
        return S_OK;
    }
    return no_interface("a surface", iid, out);
}

static ULONG WINAPI s_AddRef(IDirectDrawSurface7* self)
{
    return (ULONG) ++SELF->top->refs;
}

static ULONG WINAPI s_Release(IDirectDrawSurface7* self)
{
    surface* top = SELF->top;
    LONG refs = --top->refs;

    if (refs == 0)
        free_surface(top);
    return (ULONG) (refs < 0 ? 0 : refs);
}

static HRESULT WINAPI s_AddAttachedSurface(IDirectDrawSurface7* self, IDirectDrawSurface7* other)
{
    surface* z = surface_of(other);

    if (!z || !is_zbuffer(z))
        return unsupported("Surface7::AddAttachedSurface");
    SELF->zbuffer = z;
    ++z->refs;
    return DD_OK;
}

/* The rectangle a Blt names, or the whole surface. */
static RECT whole_or(const surface* s, const RECT* r)
{
    RECT all = { 0, 0, (LONG) s->desc.dwWidth, (LONG) s->desc.dwHeight };

    return r ? *r : all;
}

/* Memory to memory, scaled by nearest neighbour, with an optional source key. */
static void blit_memory(surface* to, const RECT* to_rect, surface* from, const RECT* from_rect, BOOL keyed)
{
    LONG w = to_rect->right - to_rect->left, h = to_rect->bottom - to_rect->top;
    LONG sw = from_rect->right - from_rect->left, sh = from_rect->bottom - from_rect->top;
    LONG to_pitch = surface_pitch(to), from_pitch = surface_pitch(from), x, y;
    DWORD key = from->key.dwColorSpaceLowValue;
    BYTE* dst = pixels(to);
    const BYTE* src = pixels(from);

    if (!dst || !src || w <= 0 || h <= 0)
        return;

    for (y = 0; y < h; ++y)
    {
        LONG ty = to_rect->top + y, sy = from_rect->top + y * sh / h;
        WORD* drow;
        const WORD* srow;

        if (ty < 0 || ty >= (LONG) to->desc.dwHeight || sy < 0 || sy >= (LONG) from->desc.dwHeight)
            continue;

        drow = (WORD*) (dst + (SIZE_T) ty * to_pitch);
        srow = (const WORD*) (src + (SIZE_T) sy * from_pitch);
        for (x = 0; x < w; ++x)
        {
            LONG tx = to_rect->left + x, sx = from_rect->left + x * sw / w;
            WORD p;

            if (tx < 0 || tx >= (LONG) to->desc.dwWidth || sx < 0 || sx >= (LONG) from->desc.dwWidth)
                continue;

            p = srow[sx];
            if (keyed && p == key)
                continue;

            if (from->format != to->format)
            {
                BYTE rgba[4];
                unpack(from->format, p, rgba);
                p = pack(to->format, rgba);
            }
            drow[tx] = p;
        }
    }
    to->top->dirty = TRUE;
    shown_stale(to, to_rect);
}

static void fill_memory(surface* to, const RECT* r, WORD colour)
{
    LONG pitch = surface_pitch(to), x, y;
    BYTE* dst = pixels(to);

    if (!dst)
        return;

    for (y = r->top < 0 ? 0 : r->top; y < r->bottom && y < (LONG) to->desc.dwHeight; ++y)
    {
        WORD* row = (WORD*) (dst + (SIZE_T) y * pitch);

        for (x = r->left < 0 ? 0 : r->left; x < r->right && x < (LONG) to->desc.dwWidth; ++x)
            row[x] = colour;
    }
    to->top->dirty = TRUE;
    shown_stale(to, r);
}

static void fill_target(surface* to, const RECT* r, WORD colour)
{
    BYTE rgba[4];

    if (r->left <= 0 && r->top <= 0 && r->right >= (LONG) to->desc.dwWidth
        && r->bottom >= (LONG) to->desc.dwHeight)
        to->inks = FALSE;
    unpack(PF_565, colour, rgba);
    target_bind(to);
    gl.Enable(GL_SCISSOR_TEST);
    gl.Scissor(r->left, (GLint) to->desc.dwHeight - r->bottom, r->right - r->left, r->bottom - r->top);
    gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl.ClearColor(rgba[0] / 255.0f, rgba[1] / 255.0f, rgba[2] / 255.0f, 1);
    gl.Clear(GL_COLOR_BUFFER_BIT);
    gl.Disable(GL_SCISSOR_TEST);
    g_state_lost = TRUE;
}

/* The same on a target, kept in its memory image when that is current. */
static void fill_target_and_memory(surface* to, const RECT* r, WORD colour)
{
    BOOL current = to->pixels_current;

    fill_target(to, r, colour);
    if (current)
    {
        fill_memory(to, r, colour);
        to->pixels_current = TRUE;
    }
}

/* One render target onto another, through the framebuffers. */
static void blit_targets(surface* to, const RECT* t, surface* from, const RECT* f)
{
    target_bind(to);
    target_bind(from);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, to->fbo);
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, from->fbo);
    gl.BlitFramebuffer(f->left, (GLint) from->desc.dwHeight - f->bottom, f->right, (GLint) from->desc.dwHeight - f->top,
                       t->left, (GLint) to->desc.dwHeight - t->bottom, t->right, (GLint) to->desc.dwHeight - t->top,
                       GL_COLOR_BUFFER_BIT, GL_NEAREST);
    target_bind(to);
    g_state_lost = TRUE;
}

static HRESULT blit(surface* to, const RECT* to_rect, surface* from, const RECT* from_rect, BOOL keyed)
{
    RECT t = whole_or(to, to_rect), f = whole_or(from, from_rect);

    /* DirectDraw draws nothing from a source rectangle that leaves its surface. */
    if (f.left < 0 || f.top < 0 || f.left >= f.right || f.top >= f.bottom
        || f.right > (LONG) from->desc.dwWidth || f.bottom > (LONG) from->desc.dwHeight)
        return DDERR_INVALIDRECT;

    /* The primary is the window: drawing a render target onto it presents it. */
    if (is_primary(to))
    {
        if (is_target(from))
            present(from);
        return DD_OK;
    }

    if (is_target(to) && is_target(from))
        blit_targets(to, &t, from, &f);
    else if (is_target(to))
    {
        BOOL current = to->pixels_current;

        draw_pixels(to, pixels(from), surface_pitch(from), from->format, &f, &t, keyed,
                    from->key.dwColorSpaceLowValue, from->top->inks);
        to->inks |= from->top->inks;
        if (current)
        {
            blit_memory(to, &t, from, &f, keyed);
            to->pixels_current = TRUE;
        }
    }
    else
    {
        if (is_target(from))
            target_read(from);
        blit_memory(to, &t, from, &f, keyed);
        to->top->inks |= from->top->inks;
    }
    return DD_OK;
}

static HRESULT WINAPI s_Blt(IDirectDrawSurface7* self, LPRECT to_rect, IDirectDrawSurface7* other, LPRECT from_rect,
                            DWORD flags, LPDDBLTFX fx)
{
    surface* from = surface_of(other);

    if (flags & DDBLT_COLORFILL)
    {
        RECT r = whole_or(SELF, to_rect);
        WORD colour = fx ? (WORD) fx->dwFillColor : 0;

        if (is_target(SELF))
            fill_target_and_memory(SELF, &r, colour);
        else if (!is_primary(SELF))
            fill_memory(SELF, &r, colour);
        return DD_OK;
    }

    if (flags & DDBLT_DEPTHFILL)
        return DD_OK;       /* render targets clear their own depth through Device7::Clear */

    if (!from)
        return DDERR_INVALIDPARAMS;
    return blit(SELF, to_rect, from, from_rect, (flags & DDBLT_KEYSRC) && from->keyed);
}

static HRESULT WINAPI s_AddOverlayDirtyRect(IDirectDrawSurface7* self, LPRECT rect)
{
    (void) self; (void) rect;
    return DDERR_NOTAOVERLAYSURFACE;
}

static HRESULT WINAPI s_BltBatch(IDirectDrawSurface7* self, LPDDBLTBATCH batch, DWORD count, DWORD flags)
{
    (void) self; (void) batch; (void) count; (void) flags;
    return unsupported("Surface7::BltBatch");
}

static HRESULT WINAPI s_BltFast(IDirectDrawSurface7* self, DWORD x, DWORD y, IDirectDrawSurface7* other,
                                LPRECT from_rect, DWORD flags)
{
    surface* from = surface_of(other);
    RECT f, t;

    if (!from)
        return DDERR_INVALIDPARAMS;

    f = whole_or(from, from_rect);
    t.left = (LONG) x;
    t.top = (LONG) y;
    t.right = t.left + (f.right - f.left);
    t.bottom = t.top + (f.bottom - f.top);
    return blit(SELF, &t, from, &f, (flags & DDBLTFAST_SRCCOLORKEY) && from->keyed);
}

static HRESULT WINAPI s_DeleteAttachedSurface(IDirectDrawSurface7* self, DWORD flags, IDirectDrawSurface7* other)
{
    surface* z = surface_of(other);

    (void) flags;
    if (z && SELF->zbuffer == z)
    {
        SELF->zbuffer = NULL;
        s_Release(other);
    }
    return DD_OK;
}

static HRESULT WINAPI s_EnumAttachedSurfaces(IDirectDrawSurface7* self, LPVOID context, LPDDENUMSURFACESCALLBACK7 callback)
{
    (void) self; (void) context; (void) callback;
    return unsupported("Surface7::EnumAttachedSurfaces");
}

static HRESULT WINAPI s_EnumOverlayZOrders(IDirectDrawSurface7* self, DWORD flags, LPVOID context, LPDDENUMSURFACESCALLBACK7 callback)
{
    (void) self; (void) flags; (void) context; (void) callback;
    return unsupported("Surface7::EnumOverlayZOrders");
}

static HRESULT WINAPI s_Flip(IDirectDrawSurface7* self, IDirectDrawSurface7* target, DWORD flags)
{
    (void) target; (void) flags;
    if (SELF->back)
        present(SELF->back);
    return DD_OK;
}

static HRESULT WINAPI s_GetAttachedSurface(IDirectDrawSurface7* self, LPDDSCAPS2 caps, IDirectDrawSurface7** out)
{
    surface* found = NULL;

    if (caps->dwCaps & DDSCAPS_BACKBUFFER)
        found = SELF->back;
    else if (caps->dwCaps & DDSCAPS_MIPMAP)
        found = SELF->mip;
    else if (caps->dwCaps & DDSCAPS_ZBUFFER)
        found = SELF->zbuffer;

    if (!found)
    {
        *out = NULL;
        return DDERR_NOTFOUND;      /* the end of a mip chain, which the game walks to find */
    }
    ++found->top->refs;
    *out = &found->iface;
    return DD_OK;
}

static HRESULT WINAPI s_GetBltStatus(IDirectDrawSurface7* self, DWORD flags)
{
    (void) self; (void) flags;
    return DD_OK;
}

static HRESULT WINAPI s_GetCaps(IDirectDrawSurface7* self, LPDDSCAPS2 caps)
{
    *caps = SELF->desc.ddsCaps;
    return DD_OK;
}

static HRESULT WINAPI s_GetClipper(IDirectDrawSurface7* self, LPDIRECTDRAWCLIPPER* out)
{
    (void) self;
    *out = NULL;
    return DDERR_NOCLIPPERATTACHED;
}

static HRESULT WINAPI s_GetColorKey(IDirectDrawSurface7* self, DWORD flags, LPDDCOLORKEY key)
{
    (void) flags;
    if (!SELF->keyed)
        return DDERR_NOCOLORKEY;
    *key = SELF->key;
    return DD_OK;
}

typedef HBITMAP (WINAPI *create_dib_fn)(HDC, const BITMAPINFO*, UINT, void**, HANDLE, DWORD);
typedef HDC     (WINAPI *create_dc_fn)(HDC);
typedef HGDIOBJ (WINAPI *select_fn)(HDC, HGDIOBJ);
typedef BOOL    (WINAPI *delete_fn)(HGDIOBJ);
typedef BOOL    (WINAPI *delete_dc_fn)(HDC);
typedef BOOL    (WINAPI *flush_fn)(void);

static HRESULT WINAPI s_GetDC(IDirectDrawSurface7* self, HDC* out)
{
    struct { BITMAPINFOHEADER h; DWORD masks[3]; } bmi = { { sizeof(BITMAPINFOHEADER) } };
    surface* s = SELF;
    const format_info* f = &k_format[s->format];
    create_dib_fn create_dib = (create_dib_fn) api("gdi32.dll", "CreateDIBSection");
    create_dc_fn create_dc = (create_dc_fn) api("gdi32.dll", "CreateCompatibleDC");
    select_fn select = (select_fn) api("gdi32.dll", "SelectObject");

    if (s->dc)
        return DDERR_DCALREADYCREATED;
    if (!create_dib || !create_dc || !select || !pixels(s))
        return DDERR_GENERIC;

    if (is_target(s) && !s->pixels_current)
        target_read(s);

    bmi.h.biWidth = (LONG) s->desc.dwWidth;
    bmi.h.biHeight = -(LONG) s->desc.dwHeight;      /* top row first, like the surface */
    bmi.h.biPlanes = 1;
    bmi.h.biBitCount = 16;
    bmi.h.biCompression = BI_BITFIELDS;
    bmi.masks[0] = f->r;
    bmi.masks[1] = f->g;
    bmi.masks[2] = f->b;

    s->dc = create_dc(NULL);
    s->dib = create_dib(s->dc, (const BITMAPINFO*) &bmi, DIB_RGB_COLORS, &s->dib_bits, NULL, 0);
    if (!s->dc || !s->dib)
        return DDERR_GENERIC;

    s->dib_from = is_target(s) && s->inks ? shown_current(s) : NULL;
    if (!s->dib_from)
        s->dib_from = s->pixels;
    __movsb((unsigned char*) s->dib_bits, s->dib_from, (SIZE_T) surface_pitch(s) * s->desc.dwHeight);
    s->old_bitmap = (HBITMAP) select(s->dc, s->dib);
    *out = s->dc;
    return DD_OK;
}

static HRESULT WINAPI s_ReleaseDC(IDirectDrawSurface7* self, HDC dc)
{
    surface* s = SELF;
    select_fn select = (select_fn) api("gdi32.dll", "SelectObject");
    delete_fn delete_object = (delete_fn) api("gdi32.dll", "DeleteObject");
    delete_dc_fn delete_dc = (delete_dc_fn) api("gdi32.dll", "DeleteDC");
    flush_fn flush = (flush_fn) api("gdi32.dll", "GdiFlush");

    if (!s->dc || dc != s->dc)
        return DDERR_NODC;

    if (flush)
        flush();
    if (is_target(s))
        target_write_changes(s, (const BYTE*) s->dib_bits, s->dib_from);
    else
    {
        __movsb(s->pixels, (const unsigned char*) s->dib_bits, (SIZE_T) surface_pitch(s) * s->desc.dwHeight);
        s->top->dirty = TRUE;
    }
    select(s->dc, s->old_bitmap);
    delete_object(s->dib);
    delete_dc(s->dc);
    s->dc = NULL;
    s->dib = NULL;
    return DD_OK;
}

static HRESULT WINAPI s_GetFlipStatus(IDirectDrawSurface7* self, DWORD flags)
{
    (void) self; (void) flags;
    return DD_OK;
}

static HRESULT WINAPI s_GetOverlayPosition(IDirectDrawSurface7* self, LPLONG x, LPLONG y)
{
    (void) self; (void) x; (void) y;
    return DDERR_NOTAOVERLAYSURFACE;
}

static HRESULT WINAPI s_GetPalette(IDirectDrawSurface7* self, LPDIRECTDRAWPALETTE* out)
{
    *out = SELF->palette;
    if (!SELF->palette)
        return DDERR_NOPALETTEATTACHED;
    SELF->palette->lpVtbl->AddRef(SELF->palette);
    return DD_OK;
}

static HRESULT WINAPI s_GetPixelFormat(IDirectDrawSurface7* self, LPDDPIXELFORMAT pf)
{
    *pf = SELF->desc.ddpfPixelFormat;
    return DD_OK;
}

static HRESULT WINAPI s_GetSurfaceDesc(IDirectDrawSurface7* self, LPDDSURFACEDESC2 desc)
{
    *desc = SELF->desc;
    desc->lpSurface = NULL;
    return DD_OK;
}

static HRESULT WINAPI s_Initialize(IDirectDrawSurface7* self, LPDIRECTDRAW dd, LPDDSURFACEDESC2 desc)
{
    (void) self; (void) dd; (void) desc;
    return DDERR_ALREADYINITIALIZED;
}

static HRESULT WINAPI s_IsLost(IDirectDrawSurface7* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI s_Lock(IDirectDrawSurface7* self, LPRECT rect, LPDDSURFACEDESC2 desc, DWORD flags, HANDLE event)
{
    surface* s = SELF;
    BYTE* p;

    (void) event;
    if (is_zbuffer(s))
        return unsupported("Surface7::Lock of the z-buffer");

    /* The primary is what was last shown, which is still the back buffer's contents - the game
       reads it for screenshots. Its pixels are the back buffer's; writes to it are not shown. */
    if (is_primary(s))
    {
        if (!s->back)
            return unsupported("Surface7::Lock of a windowed primary");
        target_read(s->back);
        if (!s->back->pixels)
            return DDERR_OUTOFMEMORY;
        *desc = s->desc;
        desc->dwFlags |= DDSD_LPSURFACE;
        desc->lpSurface = s->back->pixels + (rect ? rect->top * surface_pitch(s) + rect->left * 2 : 0);
        return DD_OK;
    }

    if (is_target(s))
    {
        if (!s->pixels_current)
            target_read(s);
        s->locked_target = !(flags & DDLOCK_READONLY);
    }

    p = pixels(s);
    if (!p)
        return DDERR_OUTOFMEMORY;

    *desc = s->desc;
    desc->dwFlags |= DDSD_LPSURFACE;
    desc->lpSurface = rect ? p + rect->top * surface_pitch(s) + rect->left * 2 : p;
    return DD_OK;
}

static HRESULT WINAPI s_Unlock(IDirectDrawSurface7* self, LPRECT rect)
{
    (void) rect;
    if (SELF->locked_target)
    {
        target_write(SELF);
        SELF->locked_target = FALSE;
    }
    else if (!is_target(SELF) && !is_primary(SELF))
    {
        SELF->top->dirty = TRUE;
    }
    return DD_OK;
}

static HRESULT WINAPI s_Restore(IDirectDrawSurface7* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI s_SetClipper(IDirectDrawSurface7* self, LPDIRECTDRAWCLIPPER clipper)
{
    (void) self; (void) clipper;
    return DD_OK;
}

static HRESULT WINAPI s_SetColorKey(IDirectDrawSurface7* self, DWORD flags, LPDDCOLORKEY key)
{
    if (!(flags & DDCKEY_SRCBLT))
        return DD_OK;       /* destination keys are not used by the game */

    SELF->keyed = key != NULL;
    if (key)
        SELF->key = *key;
    return DD_OK;
}

static HRESULT WINAPI s_SetOverlayPosition(IDirectDrawSurface7* self, LONG x, LONG y)
{
    (void) self; (void) x; (void) y;
    return DDERR_NOTAOVERLAYSURFACE;
}

static HRESULT WINAPI s_SetPalette(IDirectDrawSurface7* self, LPDIRECTDRAWPALETTE palette)
{
    if (palette)
        palette->lpVtbl->AddRef(palette);
    if (SELF->palette)
        SELF->palette->lpVtbl->Release(SELF->palette);
    SELF->palette = palette;
    return DD_OK;
}

static HRESULT WINAPI s_UpdateOverlay(IDirectDrawSurface7* self, LPRECT from, IDirectDrawSurface7* to, LPRECT to_rect,
                                      DWORD flags, LPDDOVERLAYFX fx)
{
    (void) self; (void) from; (void) to; (void) to_rect; (void) flags; (void) fx;
    return DDERR_NOTAOVERLAYSURFACE;
}

static HRESULT WINAPI s_UpdateOverlayDisplay(IDirectDrawSurface7* self, DWORD flags)
{
    (void) self; (void) flags;
    return DDERR_NOTAOVERLAYSURFACE;
}

static HRESULT WINAPI s_UpdateOverlayZOrder(IDirectDrawSurface7* self, DWORD flags, IDirectDrawSurface7* other)
{
    (void) self; (void) flags; (void) other;
    return DDERR_NOTAOVERLAYSURFACE;
}

static IDirectDraw7* g_ddraw;   /* the last DirectDraw object made: every surface's GetDDInterface */

static HRESULT WINAPI s_GetDDInterface(IDirectDrawSurface7* self, LPVOID* out)
{
    (void) self;
    *out = g_ddraw;
    if (!g_ddraw)
        return DDERR_GENERIC;
    g_ddraw->lpVtbl->AddRef(g_ddraw);
    return DD_OK;
}

static HRESULT WINAPI s_PageLock(IDirectDrawSurface7* self, DWORD flags)
{
    (void) self; (void) flags;
    return DD_OK;
}

static HRESULT WINAPI s_PageUnlock(IDirectDrawSurface7* self, DWORD flags)
{
    (void) self; (void) flags;
    return DD_OK;
}

static HRESULT WINAPI s_SetSurfaceDesc(IDirectDrawSurface7* self, LPDDSURFACEDESC2 desc, DWORD flags)
{
    (void) self; (void) desc; (void) flags;
    return unsupported("Surface7::SetSurfaceDesc");
}

static HRESULT WINAPI s_SetPrivateData(IDirectDrawSurface7* self, REFGUID tag, LPVOID data, DWORD size, DWORD flags)
{
    (void) self; (void) tag; (void) data; (void) size; (void) flags;
    return unsupported("Surface7::SetPrivateData");
}

static HRESULT WINAPI s_GetPrivateData(IDirectDrawSurface7* self, REFGUID tag, LPVOID data, LPDWORD size)
{
    (void) self; (void) tag; (void) data; (void) size;
    return DDERR_NOTFOUND;
}

static HRESULT WINAPI s_FreePrivateData(IDirectDrawSurface7* self, REFGUID tag)
{
    (void) self; (void) tag;
    return DDERR_NOTFOUND;
}

static HRESULT WINAPI s_GetUniquenessValue(IDirectDrawSurface7* self, LPDWORD value)
{
    (void) self;
    *value = 1;
    return DD_OK;
}

static HRESULT WINAPI s_ChangeUniquenessValue(IDirectDrawSurface7* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI s_SetPriority(IDirectDrawSurface7* self, DWORD priority)
{
    (void) self; (void) priority;
    return DD_OK;
}

static HRESULT WINAPI s_GetPriority(IDirectDrawSurface7* self, LPDWORD priority)
{
    (void) self;
    *priority = 0;
    return DD_OK;
}

static HRESULT WINAPI s_SetLOD(IDirectDrawSurface7* self, DWORD lod)
{
    (void) self; (void) lod;
    return DD_OK;
}

static HRESULT WINAPI s_GetLOD(IDirectDrawSurface7* self, LPDWORD lod)
{
    (void) self;
    *lod = 0;
    return DD_OK;
}

#undef SELF

static IDirectDrawSurface7Vtbl k_surface_vtbl = {
    .QueryInterface = s_QueryInterface,
    .AddRef = s_AddRef,
    .Release = s_Release,
    .AddAttachedSurface = s_AddAttachedSurface,
    .AddOverlayDirtyRect = s_AddOverlayDirtyRect,
    .Blt = s_Blt,
    .BltBatch = s_BltBatch,
    .BltFast = s_BltFast,
    .DeleteAttachedSurface = s_DeleteAttachedSurface,
    .EnumAttachedSurfaces = s_EnumAttachedSurfaces,
    .EnumOverlayZOrders = s_EnumOverlayZOrders,
    .Flip = s_Flip,
    .GetAttachedSurface = s_GetAttachedSurface,
    .GetBltStatus = s_GetBltStatus,
    .GetCaps = s_GetCaps,
    .GetClipper = s_GetClipper,
    .GetColorKey = s_GetColorKey,
    .GetDC = s_GetDC,
    .GetFlipStatus = s_GetFlipStatus,
    .GetOverlayPosition = s_GetOverlayPosition,
    .GetPalette = s_GetPalette,
    .GetPixelFormat = s_GetPixelFormat,
    .GetSurfaceDesc = s_GetSurfaceDesc,
    .Initialize = s_Initialize,
    .IsLost = s_IsLost,
    .Lock = s_Lock,
    .ReleaseDC = s_ReleaseDC,
    .Restore = s_Restore,
    .SetClipper = s_SetClipper,
    .SetColorKey = s_SetColorKey,
    .SetOverlayPosition = s_SetOverlayPosition,
    .SetPalette = s_SetPalette,
    .Unlock = s_Unlock,
    .UpdateOverlay = s_UpdateOverlay,
    .UpdateOverlayDisplay = s_UpdateOverlayDisplay,
    .UpdateOverlayZOrder = s_UpdateOverlayZOrder,
    .GetDDInterface = s_GetDDInterface,
    .PageLock = s_PageLock,
    .PageUnlock = s_PageUnlock,
    .SetSurfaceDesc = s_SetSurfaceDesc,
    .SetPrivateData = s_SetPrivateData,
    .GetPrivateData = s_GetPrivateData,
    .FreePrivateData = s_FreePrivateData,
    .GetUniquenessValue = s_GetUniquenessValue,
    .ChangeUniquenessValue = s_ChangeUniquenessValue,
    .SetPriority = s_SetPriority,
    .GetPriority = s_GetPriority,
    .SetLOD = s_SetLOD,
    .GetLOD = s_GetLOD,
};

/* ------------------------------------------------------------------ IDirectDrawGammaControl */

#define GAMMA_SURFACE(self) ((surface*) ((BYTE*) (self) - (SIZE_T) &((surface*) 0)->gamma))

static HRESULT WINAPI g_QueryInterface(IDirectDrawGammaControl* self, REFIID iid, void** out)
{
    return s_QueryInterface(&GAMMA_SURFACE(self)->iface, iid, out);
}

static ULONG WINAPI g_AddRef(IDirectDrawGammaControl* self)
{
    return s_AddRef(&GAMMA_SURFACE(self)->iface);
}

static ULONG WINAPI g_Release(IDirectDrawGammaControl* self)
{
    return s_Release(&GAMMA_SURFACE(self)->iface);
}

static HRESULT WINAPI g_GetGammaRamp(IDirectDrawGammaControl* self, DWORD flags, LPDDGAMMARAMP ramp)
{
    int i;

    (void) self; (void) flags;
    for (i = 0; i < 256; ++i)
        ramp->red[i] = ramp->green[i] = ramp->blue[i] = (WORD) (i * 257);
    return DD_OK;
}

static HRESULT WINAPI g_SetGammaRamp(IDirectDrawGammaControl* self, DWORD flags, LPDDGAMMARAMP ramp)
{
    (void) self; (void) flags; (void) ramp;
    return DD_OK;       /* brightness is left to the display */
}

static IDirectDrawGammaControlVtbl k_gamma_vtbl = {
    .QueryInterface = g_QueryInterface,
    .AddRef = g_AddRef,
    .Release = g_Release,
    .GetGammaRamp = g_GetGammaRamp,
    .SetGammaRamp = g_SetGammaRamp,
};

/* ------------------------------------------------------------------ IDirectDrawPalette */

typedef struct palette
{
    IDirectDrawPalette iface;
    LONG               refs;
    PALETTEENTRY       entries[256];
} palette;

#define PAL ((palette*) self)

static HRESULT WINAPI p_QueryInterface(IDirectDrawPalette* self, REFIID iid, void** out)
{
    if (IsEqualGUID(iid, &IID_IDirectDrawPalette) || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = self;
        ++PAL->refs;
        return S_OK;
    }
    return no_interface("a palette", iid, out);
}

static ULONG WINAPI p_AddRef(IDirectDrawPalette* self)
{
    return (ULONG) ++PAL->refs;
}

static ULONG WINAPI p_Release(IDirectDrawPalette* self)
{
    LONG refs = --PAL->refs;

    if (refs == 0)
        mem_free(self);
    return (ULONG) refs;
}

static HRESULT WINAPI p_GetCaps(IDirectDrawPalette* self, LPDWORD caps)
{
    (void) self;
    *caps = DDPCAPS_8BIT | DDPCAPS_ALLOW256;
    return DD_OK;
}

static HRESULT WINAPI p_GetEntries(IDirectDrawPalette* self, DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY out)
{
    (void) flags;
    if (start + count > 256)
        return DDERR_INVALIDPARAMS;
    __movsb((unsigned char*) out, (const unsigned char*) &PAL->entries[start], count * sizeof *out);
    return DD_OK;
}

static HRESULT WINAPI p_Initialize(IDirectDrawPalette* self, LPDIRECTDRAW dd, DWORD flags, LPPALETTEENTRY entries)
{
    (void) self; (void) dd; (void) flags; (void) entries;
    return DDERR_ALREADYINITIALIZED;
}

static HRESULT WINAPI p_SetEntries(IDirectDrawPalette* self, DWORD flags, DWORD start, DWORD count, LPPALETTEENTRY in)
{
    (void) flags;
    if (start + count > 256)
        return DDERR_INVALIDPARAMS;
    __movsb((unsigned char*) &PAL->entries[start], (const unsigned char*) in, count * sizeof *in);
    return DD_OK;
}

#undef PAL

static IDirectDrawPaletteVtbl k_palette_vtbl = {
    .QueryInterface = p_QueryInterface,
    .AddRef = p_AddRef,
    .Release = p_Release,
    .GetCaps = p_GetCaps,
    .GetEntries = p_GetEntries,
    .Initialize = p_Initialize,
    .SetEntries = p_SetEntries,
};

/* ------------------------------------------------------------------ IDirectDraw7 */

#define DD ((ddraw*) self)

/* The modes offered: the common ones that fit the desktop, and the desktop itself. All 16-bit. */
static const WORD k_modes[][2] = {
    { 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 }, { 1280, 720 }, { 1280, 960 },
    { 1280, 1024 }, { 1366, 768 }, { 1600, 900 }, { 1600, 1200 }, { 1680, 1050 }, { 1920, 1080 },
    { 1920, 1200 }, { 2560, 1080 }, { 2560, 1440 }, { 3440, 1440 }, { 3840, 2160 },
};

static void mode_desc(DWORD w, DWORD h, DDSURFACEDESC2* d)
{
    __stosb((unsigned char*) d, 0, sizeof *d);
    d->dwSize = sizeof *d;
    d->dwFlags = DDSD_WIDTH | DDSD_HEIGHT | DDSD_PITCH | DDSD_PIXELFORMAT | DDSD_REFRESHRATE;
    d->dwWidth = w;
    d->dwHeight = h;
    d->lPitch = (LONG) w * 2;
    d->dwRefreshRate = 60;
    describe_format(PF_565, &d->ddpfPixelFormat);
}

static HRESULT WINAPI d_QueryInterface(IDirectDraw7* self, REFIID iid, void** out)
{
    if (IsEqualGUID(iid, &IID_IDirectDraw7) || IsEqualGUID(iid, &IID_IUnknown))
    {
        *out = self;
        ++DD->refs;
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IDirectDraw))
    {
        *out = &DD->iface1;
        ++DD->refs;
        return S_OK;
    }
    if (IsEqualGUID(iid, &IID_IDirect3D7))
    {
        if (!DD->d3d)
            DD->d3d = direct3d_create(self);
        *out = DD->d3d;
        if (!DD->d3d)
            return E_NOINTERFACE;
        DD->d3d->lpVtbl->AddRef(DD->d3d);
        return S_OK;
    }
    return no_interface("DirectDraw", iid, out);
}

static ULONG WINAPI d_AddRef(IDirectDraw7* self)
{
    return (ULONG) ++DD->refs;
}

static ULONG WINAPI d_Release(IDirectDraw7* self)
{
    LONG refs = --DD->refs;

    if (refs == 0)
    {
        if (g_ddraw == self)
            g_ddraw = NULL;
        mem_free(self);
    }
    return (ULONG) refs;
}

static HRESULT WINAPI d_Compact(IDirectDraw7* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI d_CreateClipper(IDirectDraw7* self, DWORD flags, LPDIRECTDRAWCLIPPER* out, IUnknown* outer)
{
    (void) self; (void) flags; (void) out; (void) outer;
    return unsupported("DirectDraw7::CreateClipper");
}

static HRESULT WINAPI d_CreatePalette(IDirectDraw7* self, DWORD flags, LPPALETTEENTRY entries, LPDIRECTDRAWPALETTE* out,
                                      IUnknown* outer)
{
    palette* p = (palette*) mem_alloc(sizeof *p);

    (void) self; (void) flags; (void) outer;
    if (!p)
        return DDERR_OUTOFMEMORY;
    p->iface.lpVtbl = &k_palette_vtbl;
    p->refs = 1;
    if (entries)
        __movsb((unsigned char*) p->entries, (const unsigned char*) entries, sizeof p->entries);
    *out = &p->iface;
    return DD_OK;
}

static HRESULT WINAPI d_CreateSurface(IDirectDraw7* self, LPDDSURFACEDESC2 desc, LPDIRECTDRAWSURFACE7* out, IUnknown* outer)
{
    surface* s;
    HRESULT hr;

    (void) self; (void) outer;
    hr = create_surface(desc, &s);
    *out = SUCCEEDED(hr) ? &s->iface : NULL;
#ifdef OPENMCM2_DEV
    /* The size it got: a primary's request has none. */
    if (dev_settings.trace_display && SUCCEEDED(hr) && !(desc->ddsCaps.dwCaps & DDSCAPS_TEXTURE))
        dev_trace("display", "surface %ux%u, caps %08X, for %08X", s->desc.dwWidth,
                  s->desc.dwHeight, s->desc.ddsCaps.dwCaps, (DWORD) _ReturnAddress());
#endif
    return hr;
}

static HRESULT WINAPI d_DuplicateSurface(IDirectDraw7* self, LPDIRECTDRAWSURFACE7 from, LPDIRECTDRAWSURFACE7* out)
{
    (void) self; (void) from; (void) out;
    return unsupported("DirectDraw7::DuplicateSurface");
}

static HRESULT WINAPI d_EnumDisplayModes(IDirectDraw7* self, DWORD flags, LPDDSURFACEDESC2 filter, LPVOID context,
                                         LPDDENUMMODESCALLBACK2 callback)
{
    DWORD desktop_w = (DWORD) GetSystemMetrics(SM_CXSCREEN), desktop_h = (DWORD) GetSystemMetrics(SM_CYSCREEN);
    BOOL desktop_listed = FALSE;
    DDSURFACEDESC2 d;
    int i;

    (void) self; (void) flags;
    for (i = 0; i < (int) (sizeof k_modes / sizeof k_modes[0]); ++i)
    {
        if (k_modes[i][0] > desktop_w || k_modes[i][1] > desktop_h)
            continue;
        if (filter && (filter->dwFlags & DDSD_WIDTH) && filter->dwWidth != k_modes[i][0])
            continue;
        desktop_listed |= k_modes[i][0] == desktop_w && k_modes[i][1] == desktop_h;
        mode_desc(k_modes[i][0], k_modes[i][1], &d);
        if (callback(&d, context) == DDENUMRET_CANCEL)
            return DD_OK;
    }
    if (!desktop_listed && !(filter && (filter->dwFlags & DDSD_WIDTH)))
    {
        mode_desc(desktop_w, desktop_h, &d);
        callback(&d, context);
    }
    return DD_OK;
}

static HRESULT WINAPI d_EnumSurfaces(IDirectDraw7* self, DWORD flags, LPDDSURFACEDESC2 desc, LPVOID context,
                                     LPDDENUMSURFACESCALLBACK7 callback)
{
    (void) self; (void) flags; (void) desc; (void) context; (void) callback;
    return unsupported("DirectDraw7::EnumSurfaces");
}

static HRESULT WINAPI d_FlipToGDISurface(IDirectDraw7* self)
{
    (void) self;
    return DD_OK;
}

static void fill_caps(LPDDCAPS caps)
{
    DWORD size;

    if (!caps)
        return;
    size = caps->dwSize;
    __stosb((unsigned char*) caps, 0, size);
    caps->dwSize = size;
    caps->dwCaps = DDCAPS_3D | DDCAPS_BLT | DDCAPS_BLTCOLORFILL | DDCAPS_BLTSTRETCH | DDCAPS_COLORKEY |
                   DDCAPS_CANBLTSYSMEM | DDCAPS_GDI | DDCAPS_PALETTE;
    caps->dwCaps2 = DDCAPS2_WIDESURFACES | DDCAPS2_CANRENDERWINDOWED | DDCAPS2_FLIPNOVSYNC;
    caps->dwCKeyCaps = DDCKEYCAPS_SRCBLT;
    caps->dwZBufferBitDepths = DDBD_16 | DDBD_24;
    caps->dwVidMemTotal = 256u << 20;
    caps->dwVidMemFree = 256u << 20;
    caps->ddsCaps.dwCaps = DDSCAPS_3DDEVICE | DDSCAPS_TEXTURE | DDSCAPS_ZBUFFER | DDSCAPS_FLIP | DDSCAPS_MIPMAP |
                           DDSCAPS_PRIMARYSURFACE | DDSCAPS_BACKBUFFER | DDSCAPS_OFFSCREENPLAIN | DDSCAPS_COMPLEX |
                           DDSCAPS_VIDEOMEMORY | DDSCAPS_LOCALVIDMEM | DDSCAPS_SYSTEMMEMORY;
}

static HRESULT WINAPI d_GetCaps(IDirectDraw7* self, LPDDCAPS driver, LPDDCAPS emulation)
{
    (void) self;
    fill_caps(driver);
    fill_caps(emulation);
    return DD_OK;
}

static HRESULT WINAPI d_GetDisplayMode(IDirectDraw7* self, LPDDSURFACEDESC2 desc)
{
    (void) self;
    mode_desc(g_mode_w, g_mode_h, desc);
    return DD_OK;
}

static HRESULT WINAPI d_GetFourCCCodes(IDirectDraw7* self, LPDWORD count, LPDWORD codes)
{
    (void) self; (void) codes;
    *count = 0;
    return DD_OK;
}

static HRESULT WINAPI d_GetGDISurface(IDirectDraw7* self, LPDIRECTDRAWSURFACE7* out)
{
    (void) self; (void) out;
    return unsupported("DirectDraw7::GetGDISurface");
}

static HRESULT WINAPI d_GetMonitorFrequency(IDirectDraw7* self, LPDWORD hz)
{
    (void) self;
    *hz = 60;
    return DD_OK;
}

static HRESULT WINAPI d_GetScanLine(IDirectDraw7* self, LPDWORD line)
{
    (void) self; (void) line;
    return DDERR_UNSUPPORTED;
}

static HRESULT WINAPI d_GetVerticalBlankStatus(IDirectDraw7* self, LPBOOL in_blank)
{
    (void) self;
    *in_blank = TRUE;
    return DD_OK;
}

static HRESULT WINAPI d_Initialize(IDirectDraw7* self, GUID* device)
{
    (void) self; (void) device;
    return DDERR_ALREADYINITIALIZED;
}

static HRESULT WINAPI d_RestoreDisplayMode(IDirectDraw7* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI d_SetCooperativeLevel(IDirectDraw7* self, HWND window, DWORD flags)
{
    (void) self;
    if (window)
        g_window = window;
    g_fullscreen = (flags & DDSCL_FULLSCREEN) != 0;
    if (g_fullscreen)
        cover_monitor();
    ensure_gl();
    return DD_OK;
}

static HRESULT WINAPI d_SetDisplayMode(IDirectDraw7* self, DWORD w, DWORD h, DWORD bits, DWORD refresh, DWORD flags)
{
    (void) self; (void) bits; (void) refresh; (void) flags;
    g_mode_w = w;
    g_mode_h = h;
#ifdef OPENMCM2_DEV
    if (dev_settings.trace_display)
        dev_trace("display", "mode %ux%u, %u bits, for %08X", w, h, bits, (DWORD) _ReturnAddress());
#endif
    if (g_fullscreen)
        cover_monitor();
    log_line("render: the game's display mode is %ux%u", w, h);
    return DD_OK;
}

static HRESULT WINAPI d_WaitForVerticalBlank(IDirectDraw7* self, DWORD flags, HANDLE event)
{
    (void) self; (void) flags; (void) event;
    return DD_OK;
}

static HRESULT WINAPI d_GetAvailableVidMem(IDirectDraw7* self, LPDDSCAPS2 caps, LPDWORD total, LPDWORD free)
{
    (void) self; (void) caps;
    if (total) *total = 256u << 20;
    if (free) *free = 256u << 20;
    return DD_OK;
}

static HRESULT WINAPI d_GetSurfaceFromDC(IDirectDraw7* self, HDC dc, LPDIRECTDRAWSURFACE7* out)
{
    (void) self; (void) dc; (void) out;
    return unsupported("DirectDraw7::GetSurfaceFromDC");
}

static HRESULT WINAPI d_RestoreAllSurfaces(IDirectDraw7* self)
{
    (void) self;
    return DD_OK;
}

static HRESULT WINAPI d_TestCooperativeLevel(IDirectDraw7* self)
{
    (void) self;
    return DD_OK;
}

/* {4F50454E-4D43-4D32-4F47-4C0000000001} - "OPEN MCM2 OGL" */
static const GUID k_device_id = { 0x4F50454E, 0x4D43, 0x4D32, { 0x4F, 0x47, 0x4C, 0, 0, 0, 0, 1 } };

static HRESULT WINAPI d_GetDeviceIdentifier(IDirectDraw7* self, LPDDDEVICEIDENTIFIER2 id, DWORD flags)
{
    (void) self; (void) flags;
    __stosb((unsigned char*) id, 0, sizeof *id);
    lstrcpyA(id->szDriver, "openmcm2");
    lstrcpyA(id->szDescription, "OpenMCM2 OpenGL renderer");
    id->guidDeviceIdentifier = k_device_id;
    id->dwWHQLLevel = 1;
    return DD_OK;
}

static HRESULT WINAPI d_StartModeTest(IDirectDraw7* self, LPSIZE modes, DWORD count, DWORD flags)
{
    (void) self; (void) modes; (void) count; (void) flags;
    return DDERR_TESTFINISHED;
}

static HRESULT WINAPI d_EvaluateMode(IDirectDraw7* self, DWORD flags, DWORD* seconds)
{
    (void) self; (void) flags; (void) seconds;
    return DDERR_TESTFINISHED;
}

#undef DD

static IDirectDraw7Vtbl k_ddraw_vtbl = {
    .QueryInterface = d_QueryInterface,
    .AddRef = d_AddRef,
    .Release = d_Release,
    .Compact = d_Compact,
    .CreateClipper = d_CreateClipper,
    .CreatePalette = d_CreatePalette,
    .CreateSurface = d_CreateSurface,
    .DuplicateSurface = d_DuplicateSurface,
    .EnumDisplayModes = d_EnumDisplayModes,
    .EnumSurfaces = d_EnumSurfaces,
    .FlipToGDISurface = d_FlipToGDISurface,
    .GetCaps = d_GetCaps,
    .GetDisplayMode = d_GetDisplayMode,
    .GetFourCCCodes = d_GetFourCCCodes,
    .GetGDISurface = d_GetGDISurface,
    .GetMonitorFrequency = d_GetMonitorFrequency,
    .GetScanLine = d_GetScanLine,
    .GetVerticalBlankStatus = d_GetVerticalBlankStatus,
    .Initialize = d_Initialize,
    .RestoreDisplayMode = d_RestoreDisplayMode,
    .SetCooperativeLevel = d_SetCooperativeLevel,
    .SetDisplayMode = d_SetDisplayMode,
    .WaitForVerticalBlank = d_WaitForVerticalBlank,
    .GetAvailableVidMem = d_GetAvailableVidMem,
    .GetSurfaceFromDC = d_GetSurfaceFromDC,
    .RestoreAllSurfaces = d_RestoreAllSurfaces,
    .TestCooperativeLevel = d_TestCooperativeLevel,
    .GetDeviceIdentifier = d_GetDeviceIdentifier,
    .StartModeTest = d_StartModeTest,
    .EvaluateMode = d_EvaluateMode,
};

/* ------------------------------------------------------------------ the game's entry points */

typedef HRESULT (WINAPI *create_fn)(GUID*, void**, REFIID, IUnknown*);
typedef HRESULT (WINAPI *enumerate_fn)(LPDDENUMCALLBACKA, LPVOID);

extern create_fn    _imp__DirectDrawCreateEx;      /* the game's own import slots */
extern enumerate_fn _imp__DirectDrawEnumerateA;

static HRESULT WINAPI create_ddraw(GUID* device, void** out, REFIID iid, IUnknown* outer)
{
    ddraw* dd;

    (void) device; (void) outer;
    if (!IsEqualGUID(iid, &IID_IDirectDraw7))
        return DDERR_INVALIDPARAMS;

    dd = (ddraw*) mem_alloc(sizeof *dd);
    if (!dd)
        return DDERR_OUTOFMEMORY;
    dd->iface.lpVtbl = &k_ddraw_vtbl;
    dd->iface1.lpVtbl = &k_ddraw1_vtbl;
    dd->refs = 1;
    g_ddraw = &dd->iface;
    *out = &dd->iface;
    return DD_OK;
}

/* One device: the renderer itself, as the primary display driver. */
static HRESULT WINAPI enumerate_ddraw(LPDDENUMCALLBACKA callback, LPVOID context)
{
    callback(NULL, "OpenMCM2 OpenGL renderer", "display", context);
    return DD_OK;
}

void render_install(void)
{
    create_fn create = create_ddraw;
    enumerate_fn enumerate = enumerate_ddraw;

    if (write_protected(&_imp__DirectDrawCreateEx, &create, sizeof create) &&
        write_protected(&_imp__DirectDrawEnumerateA, &enumerate, sizeof enumerate))
        log_line("render: the game draws through OpenGL");
    else
        log_line("render: could not take over DirectDraw");
}
