/*
    OpenMCM2 dev build - MCM2:Reborn's race HUD, as its mockup (OpenMCM2 Mockups/race hud v2.png,
    hud_mock.py): the position, lap and race clock with the gaps and lap times at the top left,
    the track map at the top right, and a small speedometer at the bottom right.

    It is drawn by the renderer straight onto the window, after the game's frame is put there and
    before it is shown (ddraw.c, present), at the window's own resolution: the mockup's 2560x1440
    layout is scaled to the window's height and kept to its corners. Its shapes are drawn into a
    multisampled copy of the window, for smooth edges, and its words come from a picture of the
    menus' fonts that GDI draws once for each size.

    What it shows comes from hud_read (huddata.c); while that says no race is on, it draws nothing.
*/

#include "dev.h"
#include "gl.h"
#include "hud.h"
#include "maths.h"

#define BASE_H      1440.0f             /* the mockup's height, which its numbers are in */
#define MARGIN      40.0f

typedef struct colour { float r, g, b, a; } colour;

static const colour PAPER  = { 244 / 255.f, 240 / 255.f, 230 / 255.f, 1 };
static const colour GREY   = { 150 / 255.f, 150 / 255.f, 156 / 255.f, 1 };
static const colour ORANGE = { 1, 138 / 255.f, 0, 1 };
static const colour GREEN  = { 52 / 255.f, 199 / 255.f, 89 / 255.f, 1 };
static const colour RED    = { 1, 69 / 255.f, 58 / 255.f, 1 };
static const colour INK    = { 14 / 255.f, 14 / 255.f, 16 / 255.f, 1 };
static const colour DIM    = { 70 / 255.f, 72 / 255.f, 78 / 255.f, 1 };
static const colour EDGE   = { 64 / 255.f, 66 / 255.f, 72 / 255.f, 1 };
static const colour GLASS  = { 16 / 255.f, 17 / 255.f, 20 / 255.f, 0.78f };
static const colour TRACK  = { 190 / 255.f, 186 / 255.f, 178 / 255.f, 1 };
static const colour SHADOW = { 0, 0, 0, 0.63f };

static float g_scale;                   /* window pixels to a mockup pixel */
static float g_width;                   /* the window's, in mockup pixels */

/* ---- Shapes, in mockup pixels ---- */

#define MOST_POINTS 512

static GLfloat g_points[MOST_POINTS * 2][2];

static void tint(colour c)
{
    gl.Color4f(c.r, c.g, c.b, c.a);
}

static void draw(GLenum mode, int count)
{
    gl.VertexPointer(2, GL_FLOAT, sizeof g_points[0], g_points[0]);
    gl.DrawArrays(mode, 0, count);
}

static void put(int i, float x, float y)
{
    g_points[i][0] = x * g_scale;
    g_points[i][1] = y * g_scale;
}

/* A rounded rectangle's outline, `segments` points to a corner, into g_points from `at`. */
static int rounded_outline(int at, float x0, float y0, float x1, float y1, float r)
{
    static const float corners[4][3] = { { 1, 1, 0 }, { 0, 1, 1 }, { 0, 0, 2 }, { 1, 0, 3 } };
    const int segments = 6;
    int c, k, n = at;

    for (c = 0; c < 4; c++) {
        float cx = corners[c][0] ? x1 - r : x0 + r;
        float cy = corners[c][1] ? y1 - r : y0 + r;

        for (k = 0; k <= segments; k++) {
            float a = (corners[c][2] + (float) k / segments) * PI / 2;

            put(n++, cx + r * cosine(a), cy + r * sine(a));
        }
    }
    return n - at;
}

static void rounded(float x0, float y0, float x1, float y1, float r, colour c)
{
    int n;

    put(0, (x0 + x1) / 2, (y0 + y1) / 2);
    n = rounded_outline(1, x0, y0, x1, y1, r);
    g_points[n + 1][0] = g_points[1][0];
    g_points[n + 1][1] = g_points[1][1];
    tint(c);
    draw(GL_TRIANGLE_FAN, n + 2);
}

/* A rounded rectangle's edge, `width` thick, inside it. */
static void rounded_edge(float x0, float y0, float x1, float y1, float r, float width, colour c)
{
    static GLfloat outer[64][2], inner[64][2];
    int n, k;

    n = rounded_outline(0, x0, y0, x1, y1, r);
    for (k = 0; k < n; k++) {
        outer[k][0] = g_points[k][0];
        outer[k][1] = g_points[k][1];
    }
    rounded_outline(0, x0 + width, y0 + width, x1 - width, y1 - width, r > width ? r - width : 0);
    for (k = 0; k < n; k++) {
        inner[k][0] = g_points[k][0];
        inner[k][1] = g_points[k][1];
    }
    for (k = 0; k <= n; k++) {
        g_points[2 * k][0] = outer[k % n][0];
        g_points[2 * k][1] = outer[k % n][1];
        g_points[2 * k + 1][0] = inner[k % n][0];
        g_points[2 * k + 1][1] = inner[k % n][1];
    }
    tint(c);
    draw(GL_TRIANGLE_STRIP, 2 * n + 2);
}

/* A panel: glass, its edge, and (with `stripe`) the orange stripe down its left. */
static void panel(float x0, float y0, float x1, float y1, float r, BOOL stripe)
{
    rounded(x0, y0, x1, y1, r, GLASS);
    rounded_edge(x0, y0, x1, y1, r, 1.5f, EDGE);
    if (stripe)
        rounded(x0, y0 + 10, x0 + 6, y1 - 10, 3, ORANGE);
}

static void disc(float x, float y, float r, colour c)
{
    const int segments = 20;
    int k;

    put(0, x, y);
    for (k = 0; k <= segments; k++)
        put(k + 1, x + r * cosine(2 * PI * k / segments), y + r * sine(2 * PI * k / segments));
    tint(c);
    draw(GL_TRIANGLE_FAN, segments + 2);
}

/* A piece of a ring between radii r0 and r1, from angle a0 to a1 in degrees (clockwise from 3 o'clock). */
static void arc(float x, float y, float r0, float r1, float a0, float a1, colour c)
{
    const int segments = 6;
    int k;

    for (k = 0; k <= segments; k++) {
        float a = (a0 + (a1 - a0) * k / segments) * PI / 180;

        put(2 * k, x + r1 * cosine(a), y + r1 * sine(a));
        put(2 * k + 1, x + r0 * cosine(a), y + r0 * sine(a));
    }
    tint(c);
    draw(GL_TRIANGLE_STRIP, 2 * segments + 2);
}

/* A line through `count` points (mockup pixels), `width` wide, with round joins. */
static void polyline(const float (*points)[2], int count, float width, colour c)
{
    int k;

    for (k = 0; k + 1 < count; k++) {
        float dx = points[k + 1][0] - points[k][0], dy = points[k + 1][1] - points[k][1];
        float length = square_root(dx * dx + dy * dy);
        float nx, ny;

        if (length <= 0)
            continue;
        nx = -dy / length * width / 2;
        ny = dx / length * width / 2;
        put(0, points[k][0] + nx, points[k][1] + ny);
        put(1, points[k][0] - nx, points[k][1] - ny);
        put(2, points[k + 1][0] + nx, points[k + 1][1] + ny);
        put(3, points[k + 1][0] - nx, points[k + 1][1] - ny);
        tint(c);
        draw(GL_TRIANGLE_STRIP, 4);
    }
    for (k = 0; k < count; k++)
        disc(points[k][0], points[k][1], width / 2, c);
}

/* ---- Words ---- */

/*
    The menus' text face, Helvetica Oblique bold. Their heading face (AeromoveDemo) has no figures:
    the menus set figures in the text face at FIGURE_SCALE of the heading's size, as tall as its
    capitals (reborn_kit.heading_runs), and the HUD's numbers are set so too.
*/
enum face { FIGURES, TEXT };
#define FIGURE_SCALE 0.9f

#define ATLAS_W     1024
#define ATLAS_MOST  16
#define FIRST_CHAR  32
#define CHARS       95

typedef struct atlas {
    int     face, size;                 /* size in window pixels */
    GLuint  texture;
    int     height, ascent, rows;
    short   x[CHARS], y[CHARS], w[CHARS], advance[CHARS];
} atlas;

static atlas g_atlases[ATLAS_MOST];
static int   g_atlas_count;

typedef HFONT   (WINAPI* create_font_fn)(int, int, int, int, int, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                         DWORD, DWORD, LPCSTR);
typedef HDC     (WINAPI* create_dc_fn)(HDC);
typedef HBITMAP (WINAPI* create_dib_fn)(HDC, const BITMAPINFO*, UINT, void**, HANDLE, DWORD);
typedef HGDIOBJ (WINAPI* select_fn)(HDC, HGDIOBJ);
typedef COLORREF (WINAPI* text_colour_fn)(HDC, COLORREF);
typedef int     (WINAPI* bk_mode_fn)(HDC, int);
typedef BOOL    (WINAPI* text_out_fn)(HDC, int, int, LPCSTR, int);
typedef BOOL    (WINAPI* extent_fn)(HDC, LPCSTR, int, LPSIZE);
typedef BOOL    (WINAPI* metrics_fn)(HDC, LPTEXTMETRICA);
typedef BOOL    (WINAPI* delete_fn)(HGDIOBJ);
typedef BOOL    (WINAPI* delete_dc_fn)(HDC);

/* The face's characters at `size` window pixels, drawn white by GDI and kept as a texture. */
static atlas* atlas_of(int face, int size)
{
    create_font_fn create_font = (create_font_fn) api("gdi32.dll", "CreateFontA");
    create_dc_fn create_dc = (create_dc_fn) api("gdi32.dll", "CreateCompatibleDC");
    create_dib_fn create_dib = (create_dib_fn) api("gdi32.dll", "CreateDIBSection");
    select_fn select = (select_fn) api("gdi32.dll", "SelectObject");
    text_colour_fn text_colour = (text_colour_fn) api("gdi32.dll", "SetTextColor");
    bk_mode_fn bk_mode = (bk_mode_fn) api("gdi32.dll", "SetBkMode");
    text_out_fn text_out = (text_out_fn) api("gdi32.dll", "TextOutA");
    extent_fn extent = (extent_fn) api("gdi32.dll", "GetTextExtentPoint32A");
    metrics_fn metrics = (metrics_fn) api("gdi32.dll", "GetTextMetricsA");
    delete_fn delete_object = (delete_fn) api("gdi32.dll", "DeleteObject");
    delete_dc_fn delete_dc = (delete_dc_fn) api("gdi32.dll", "DeleteDC");
    BITMAPINFO info = { { sizeof(BITMAPINFOHEADER) } };
    TEXTMETRICA tm;
    atlas* a;
    HFONT font, old_font;
    HBITMAP bitmap, old_bitmap;
    HDC dc;
    BYTE* bits;
    int i, x = 0, y = 0, rows, pad = size / 3 + 2;

    for (i = 0; i < g_atlas_count; i++)
        if (g_atlases[i].face == face && g_atlases[i].size == size)
            return &g_atlases[i];
    if (g_atlas_count == ATLAS_MOST || !create_font || !create_dc || !create_dib || !select || !text_colour
        || !bk_mode || !text_out || !extent || !metrics || !delete_object || !delete_dc)
        return NULL;
    a = &g_atlases[g_atlas_count];
    a->face = face;
    a->size = size;

    font = create_font(-size, 0, 0, 0, FW_BOLD, TRUE, FALSE, FALSE, DEFAULT_CHARSET, OUT_TT_PRECIS,
                       CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY, DEFAULT_PITCH, "Helvetica");
    dc = create_dc(NULL);
    old_font = (HFONT) select(dc, font);
    metrics(dc, &tm);
    a->height = tm.tmHeight + 2;
    a->ascent = tm.tmAscent + 1;
    for (i = 0; i < CHARS; i++) {           /* where each goes, row by row */
        char c = (char) (FIRST_CHAR + i);
        SIZE s;

        extent(dc, &c, 1, &s);
        if (x + s.cx + pad > ATLAS_W) {
            x = 0;
            y += a->height;
        }
        a->x[i] = (short) x;
        a->y[i] = (short) y;
        a->w[i] = (short) (s.cx + pad);
        a->advance[i] = (short) s.cx;
        x += s.cx + pad;
    }
    rows = y + a->height;
    a->rows = rows;

    info.bmiHeader.biWidth = ATLAS_W;
    info.bmiHeader.biHeight = -rows;        /* top row first */
    info.bmiHeader.biPlanes = 1;
    info.bmiHeader.biBitCount = 32;
    bitmap = create_dib(dc, &info, DIB_RGB_COLORS, (void**) &bits, NULL, 0);
    if (!bitmap) {
        select(dc, old_font);
        delete_object(font);
        delete_dc(dc);
        return NULL;
    }
    old_bitmap = (HBITMAP) select(dc, bitmap);
    text_colour(dc, RGB(255, 255, 255));
    bk_mode(dc, TRANSPARENT);
    for (i = 0; i < CHARS; i++) {
        char c = (char) (FIRST_CHAR + i);

        text_out(dc, a->x[i] + 1, a->y[i] + 1, &c, 1);
    }
    /* White, its coverage as alpha: GDI drew grey levels on black. */
    for (i = 0; i < ATLAS_W * rows; i++) {
        BYTE* p = bits + 4 * i;
        BYTE level = p[0] > p[1] ? (p[0] > p[2] ? p[0] : p[2]) : (p[1] > p[2] ? p[1] : p[2]);

        p[0] = p[1] = p[2] = 255;
        p[3] = level;
    }
    gl.GenTextures(1, &a->texture);
    gl.BindTexture(GL_TEXTURE_2D, a->texture);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl.TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    gl.PixelStorei(GL_UNPACK_ALIGNMENT, 4);
    gl.TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, ATLAS_W, rows, 0, GL_BGRA, GL_UNSIGNED_BYTE, bits);

    select(dc, old_bitmap);
    select(dc, old_font);
    delete_object(bitmap);
    delete_object(font);
    delete_dc(dc);
    g_atlas_count++;
    return a;
}

static void atlases_free(void)
{
    int i;

    for (i = 0; i < g_atlas_count; i++)
        gl.DeleteTextures(1, &g_atlases[i].texture);
    g_atlas_count = 0;
}

/* How wide `words` are at `size` mockup pixels, in mockup pixels. */
static float words_width(int face, float size, const char* words)
{
    atlas* a = atlas_of(face, (int) (size * (face == FIGURES ? FIGURE_SCALE : 1) * g_scale + 0.5f));
    float w = 0;

    if (!a)
        return 0;
    for (; *words; words++)
        if ((BYTE) *words >= FIRST_CHAR && (BYTE) *words < FIRST_CHAR + CHARS)
            w += a->advance[(BYTE) *words - FIRST_CHAR];
    return w / g_scale;
}

enum align { LEFT, MIDDLE, RIGHT };

/* `words` at `size` mockup pixels, their baseline at y, from x (or centred or ended on it). */
static void say(int face, float size, float x, float y, enum align align, colour c, const char* words)
{
    static GLfloat quads[128 * 6][4];
    atlas* a = atlas_of(face, (int) (size * (face == FIGURES ? FIGURE_SCALE : 1) * g_scale + 0.5f));
    float px, py;
    int n = 0;

    if (!a)
        return;
    if (align != LEFT)
        x -= words_width(face, size, words) / (align == MIDDLE ? 2 : 1);
    px = x * g_scale;
    py = y * g_scale - a->ascent;
    for (; *words && n < 128 * 6; words++) {
        int i = (BYTE) *words - FIRST_CHAR;
        float u0, v0, u1, v1, x1, y1;

        if (i < 0 || i >= CHARS)
            continue;
        u0 = (float) a->x[i] / ATLAS_W;
        v0 = (float) a->y[i] / a->rows;
        u1 = (float) (a->x[i] + a->w[i]) / ATLAS_W;
        v1 = (float) (a->y[i] + a->height) / a->rows;
        x1 = px + a->w[i];
        y1 = py + a->height;
        {
            GLfloat corners[6][4] = { { px, py, u0, v0 }, { x1, py, u1, v0 }, { x1, y1, u1, v1 },
                                      { px, py, u0, v0 }, { x1, y1, u1, v1 }, { px, y1, u0, v1 } };
            int k;

            for (k = 0; k < 6; k++, n++) {
                quads[n][0] = corners[k][0];
                quads[n][1] = corners[k][1];
                quads[n][2] = corners[k][2];
                quads[n][3] = corners[k][3];
            }
        }
        px += a->advance[i];
    }
    gl.Enable(GL_TEXTURE_2D);
    gl.BindTexture(GL_TEXTURE_2D, a->texture);
    gl.EnableClientState(GL_TEXTURE_COORD_ARRAY);
    tint(c);
    gl.VertexPointer(2, GL_FLOAT, sizeof quads[0], quads[0]);
    gl.TexCoordPointer(2, GL_FLOAT, sizeof quads[0], &quads[0][2]);
    gl.DrawArrays(GL_TRIANGLES, 0, n);
    gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.Disable(GL_TEXTURE_2D);
}

/* ---- Times ---- */

/* `ms` as m:ss.mmm, or with `sign` a gap as +s.cc / -s.cc. */
static void time_words(char* to, int ms, BOOL sign)
{
    int whole = ms < 0 ? -ms : ms;

    if (sign)
        wsprintfA(to, "%c%d.%02d", ms < 0 ? '-' : '+', whole / 1000, whole % 1000 / 10);
    else
        wsprintfA(to, "%d:%02d.%03d", whole / 60000, whole / 1000 % 60, whole % 1000);
}

/* ---- The HUD's parts ---- */

static void race_strip(const hud_view* v)
{
    float x = MARGIN, y = MARGIN, cx;
    char big[24], small[16];

    panel(x, y, x + 600, y + 104, 14, TRUE);
    cx = x + 30;
    say(TEXT, 18, cx, y + 34, LEFT, GREY, "POS");
    wsprintfA(big, "%d", v->place);
    wsprintfA(small, "/%d", v->riders);
    say(FIGURES, 46, cx, y + 84, LEFT, ORANGE, big);
    say(TEXT, 24, cx + words_width(FIGURES, 46, big) + 6, y + 84, LEFT, GREY, small);
    cx += 132;
    rounded(cx - 23, y + 22, cx - 21, y + 82, 0, EDGE);
    say(TEXT, 18, cx, y + 34, LEFT, GREY, v->timed ? "PTS" : "LAP");
    wsprintfA(big, "%d", v->timed ? v->score : v->lap);
    say(FIGURES, 46, cx, y + 84, LEFT, PAPER, big);
    if (!v->timed && v->laps > 0) {
        wsprintfA(small, "/%d", v->laps);
        say(TEXT, 24, cx + words_width(FIGURES, 46, big) + 6, y + 84, LEFT, GREY, small);
    }
    cx += v->timed ? 160 : 132;
    rounded(cx - 23, y + 22, cx - 21, y + 82, 0, EDGE);
    say(TEXT, 18, cx, y + 34, LEFT, GREY, v->timed ? "LEFT" : "TIME");
    time_words(big, v->race_ms, FALSE);
    say(FIGURES, 46, cx, y + 84, LEFT, PAPER, big);
}

static void gaps_bar(const hud_view* v)
{
    float x = MARGIN, y = MARGIN + 114;
    char words[48];

    if (!v->ahead.known && !v->behind.known)
        return;
    panel(x, y, x + 600, y + 48, 10, FALSE);
    if (v->ahead.known) {
        wsprintfA(words, "P%d %s", v->place - 1, v->ahead.name);
        say(TEXT, 20, x + 24, y + 33, LEFT, PAPER, words);
        time_words(words, v->ahead.gap_ms, TRUE);
        say(TEXT, 22, x + 270, y + 33, RIGHT, RED, words);
    }
    if (v->behind.known) {
        wsprintfA(words, "P%d %s", v->place + 1, v->behind.name);
        say(TEXT, 20, x + 318, y + 33, LEFT, PAPER, words);
        time_words(words, -v->behind.gap_ms, TRUE);
        say(TEXT, 22, x + 576, y + 33, RIGHT, GREEN, words);
    }
}

static void laps_box(const hud_view* v)
{
    float x = MARGIN, y = MARGIN + 172;
    char words[24];

    if (v->last_ms <= 0 && v->best_ms <= 0)
        return;
    panel(x, y, x + 380, y + 82, 10, FALSE);
    say(TEXT, 18, x + 24, y + 34, LEFT, GREY, "LAST");
    if (v->last_ms > 0) {
        time_words(words, v->last_ms, FALSE);
        say(TEXT, 24, x + 110, y + 34, LEFT, PAPER, words);
        if (v->best_ms > 0 && v->last_ms != v->best_ms) {
            time_words(words, v->last_ms - v->best_ms, TRUE);
            say(TEXT, 22, x + 356, y + 34, RIGHT, v->last_ms < v->best_ms ? GREEN : RED, words);
        }
    }
    say(TEXT, 18, x + 24, y + 68, LEFT, GREY, "BEST");
    if (v->best_ms > 0) {
        time_words(words, v->best_ms, FALSE);
        say(TEXT, 24, x + 110, y + 68, LEFT, ORANGE, words);
    }
}

/* The track fitted into the map's room, north up, its aspect kept. */
static void map_fit(const hud_view* v, const float room[4], float* scale, float* ox, float* oy)
{
    float x0 = v->track[0][0], x1 = x0, z0 = v->track[0][1], z1 = z0, sx, sz;
    int k;

    for (k = 1; k < v->track_count; k++) {
        x0 = v->track[k][0] < x0 ? v->track[k][0] : x0;
        x1 = v->track[k][0] > x1 ? v->track[k][0] : x1;
        z0 = v->track[k][1] < z0 ? v->track[k][1] : z0;
        z1 = v->track[k][1] > z1 ? v->track[k][1] : z1;
    }
    sx = (room[2] - room[0]) / (x1 - x0 > 1 ? x1 - x0 : 1);
    sz = (room[3] - room[1]) / (z1 - z0 > 1 ? z1 - z0 : 1);
    *scale = sx < sz ? sx : sz;
    *ox = (room[0] + room[2]) / 2 - (x0 + x1) / 2 * *scale;
    *oy = (room[1] + room[3]) / 2 + (z0 + z1) / 2 * *scale;       /* world z grows up the map */
}

/* A rider's place on the map, kept inside its room when off the track's extent. */
static void map_place(float wx, float wz, const float room[4], float scale, float ox, float oy, float* x, float* y)
{
    *x = ox + wx * scale;
    *y = oy - wz * scale;
    *x = *x < room[0] ? room[0] : *x > room[2] ? room[2] : *x;
    *y = *y < room[1] ? room[1] : *y > room[3] ? room[3] : *y;
}

static void track_map(const hud_view* v)
{
    static float line[HUD_TRACK_MOST][2];
    float x1 = g_width - MARGIN, x0 = x1 - 380, y0 = MARGIN, y1 = MARGIN + 300;
    float room[4] = { x0 + 34, y0 + 56, x1 - 30, y1 - 26 };
    float scale, ox, oy;
    int k, me = -1, pass, part;

    if (v->track_count < 2)
        return;
    panel(x0, y0, x1, y1, 14, TRUE);
    say(TEXT, 18, x0 + 26, y0 + 34, LEFT, GREY, v->track_name);
    map_fit(v, room, &scale, &ox, &oy);
    for (k = 0; k < v->track_count; k++) {
        line[k][0] = ox + v->track[k][0] * scale;
        line[k][1] = oy - v->track[k][1] * scale;
    }
    for (pass = 0; pass < 2; pass++) {  /* all the shadow first, then the lines over it */
        for (part = 0; part < v->parts; part++) {
            int from = v->part_start[part];
            int to = part + 1 < v->parts ? v->part_start[part + 1] : v->track_count;

            polyline((const float (*)[2]) line + from, to - from, pass ? 6.0f : 14.0f, pass ? TRACK : SHADOW);
        }
    }
    for (k = 0; k < v->rider_count; k++) {
        float x, y;
        char number[8];

        map_place(v->rider[k].x, v->rider[k].z, room, scale, ox, oy, &x, &y);

        if (v->rider[k].player) {
            me = k;
            continue;
        }
        disc(x, y, 12, INK);
        disc(x, y, 10, PAPER);
        wsprintfA(number, "%d", v->rider[k].place);
        say(TEXT, 15, x, y + 6, MIDDLE, INK, number);
    }
    if (me >= 0) {                      /* the player last, over the others */
        float x, y;
        char number[8];

        map_place(v->rider[me].x, v->rider[me].z, room, scale, ox, oy, &x, &y);
        disc(x, y, 15, INK);
        disc(x, y, 13, ORANGE);
        wsprintfA(number, "%d", v->rider[me].place);
        say(TEXT, 17, x, y + 6, MIDDLE, INK, number);
    }
}

static void speedometer(const hud_view* v)
{
    const float r = 110, start = 150, sweep = 240;
    const int segments = 24;
    float cx = g_width - MARGIN - 130, cy = BASE_H - MARGIN - 120;
    int lit = v->revs >= 0 ? (int) (v->revs * segments + 0.5f) : (int) (v->speed_mph / 80 * segments + 0.5f);
    char words[16];
    int k;

    panel(cx - r - 18, cy - r - 18, cx + r + 18, cy + r - 6, 20, FALSE);
    for (k = 0; k < segments; k++) {
        float a0 = start + k * sweep / segments + 1.5f, a1 = start + (k + 1) * sweep / segments - 1.5f;
        colour c = k < lit ? (k >= segments - 3 ? RED : ORANGE) : DIM;

        arc(cx, cy, r - 24, r - 10, a0, a1, c);
    }
    wsprintfA(words, "%d", (int) (v->speed_mph + 0.5f));
    say(FIGURES, 80, cx, cy + 26, MIDDLE, PAPER, words);
    say(TEXT, 18, cx, cy + 56, MIDDLE, GREY, "MPH");
    if (v->gear > 0) {
        wsprintfA(words, "%d", v->gear);
        say(TEXT, 26, cx, cy + r - 24, MIDDLE, ORANGE, words);
        say(TEXT, 14, cx, cy + r - 2, MIDDLE, GREY, "GEAR");
    }
}

/* ---- Drawing it ---- */

static GLuint g_fbo, g_colour;
static int    g_fbo_w, g_fbo_h;

/* A multisampled copy of the window to draw on, made from it: FALSE without one. */
static BOOL smooth_begin(int w, int h)
{
    GLint most = 0;

    gl.GetIntegerv(GL_MAX_SAMPLES, &most);
    if (most < 2)
        return FALSE;
    if (!g_fbo || g_fbo_w != w || g_fbo_h != h) {
        if (!g_fbo) {
            gl.GenFramebuffers(1, &g_fbo);
            gl.GenRenderbuffers(1, &g_colour);
        }
        gl.BindRenderbuffer(GL_RENDERBUFFER, g_colour);
        gl.RenderbufferStorageMultisample(GL_RENDERBUFFER, most < 4 ? most : 4, GL_RGBA8, w, h);
        gl.BindFramebuffer(GL_FRAMEBUFFER, g_fbo);
        gl.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, g_colour);
        g_fbo_w = w;
        g_fbo_h = h;
        if (gl.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
            return FALSE;
        }
    }
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, g_fbo);
    gl.BlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    gl.BindFramebuffer(GL_FRAMEBUFFER, g_fbo);
    return TRUE;
}

static void smooth_end(int w, int h)
{
    gl.BindFramebuffer(GL_READ_FRAMEBUFFER, g_fbo);
    gl.BindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    gl.BlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    gl.BindFramebuffer(GL_FRAMEBUFFER, 0);
}

/* The state the HUD draws with: 2D, blended, untextured unless saying words, no 3D left over. */
static void state_2d(int w, int h)
{
    gl.Disable(GL_SCISSOR_TEST);
    gl.Disable(GL_DEPTH_TEST);
    gl.Disable(GL_CULL_FACE);
    gl.Disable(GL_FOG);
    gl.Disable(GL_ALPHA_TEST);
    gl.Disable(GL_COLOR_SUM);
    gl.Disable(GL_SAMPLE_ALPHA_TO_COVERAGE);
    gl.DepthMask(GL_FALSE);
    gl.ColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    gl.Enable(GL_BLEND);
    gl.BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl.ActiveTexture(GL_TEXTURE0 + 1);
    gl.Disable(GL_TEXTURE_2D);
    gl.ActiveTexture(GL_TEXTURE0);
    gl.Disable(GL_TEXTURE_2D);
    gl.TexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    gl.Viewport(0, 0, w, h);
    gl.MatrixMode(GL_PROJECTION);
    gl.LoadIdentity();
    gl.Ortho(0, w, h, 0, -1, 1);
    gl.MatrixMode(GL_MODELVIEW);
    gl.LoadIdentity();
    gl.DisableClientState(GL_COLOR_ARRAY);
    gl.DisableClientState(GL_FOG_COORD_ARRAY);
    gl.DisableClientState(GL_SECONDARY_COLOR_ARRAY);
    gl.ClientActiveTexture(GL_TEXTURE0 + 1);
    gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.ClientActiveTexture(GL_TEXTURE0);
    gl.DisableClientState(GL_TEXTURE_COORD_ARRAY);
    gl.EnableClientState(GL_VERTEX_ARRAY);
}

void hud_draw(int w, int h)
{
    static hud_view v;
    static float last_scale;
    BOOL smooth;

    if (h <= 0 || !dev_settings.menus_modern || !g_reborn.hud || !hud_read(&v))
        return;
    g_scale = h / BASE_H;
    g_width = w / g_scale;
    if (g_scale != last_scale) {        /* the words' pictures are made for one size of window */
        atlases_free();
        last_scale = g_scale;
    }
    smooth = smooth_begin(w, h);
    state_2d(w, h);
    if (v.show_race) {
        race_strip(&v);
        gaps_bar(&v);
        laps_box(&v);
    }
    if (v.show_map)
        track_map(&v);
    if (v.show_speed)
        speedometer(&v);
    gl.Disable(GL_BLEND);
    if (smooth)
        smooth_end(w, h);
}
