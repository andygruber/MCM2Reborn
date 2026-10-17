/*
    OpenMCM2 - OpenGL, loaded at runtime. See gl.h.
*/

#include "gl.h"

gl_api gl = { 0 };     /* initialised, or it becomes a common symbol the section move cannot see */

typedef HGLRC (WINAPI *create_context_fn)(HDC);
typedef BOOL  (WINAPI *make_current_fn)(HDC, HGLRC);
typedef PROC  (WINAPI *get_proc_fn)(LPCSTR);
typedef int   (WINAPI *choose_format_fn)(HDC, const PIXELFORMATDESCRIPTOR*);
typedef BOOL  (WINAPI *set_format_fn)(HDC, int, const PIXELFORMATDESCRIPTOR*);
typedef BOOL  (WINAPI *swap_fn)(HDC);

typedef BOOL  (WINAPI *interval_fn)(int);
typedef BOOL  (WINAPI *counter_fn)(LARGE_INTEGER*);
typedef int   (WINAPI *device_caps_fn)(HDC, int);
typedef UINT  (WINAPI *period_fn)(UINT);

static HDC         g_dc;
static swap_fn     g_swap;
static interval_fn g_interval;

render_options g_render = { TRUE, 0, 0, 0, 0, 0, 0, 100, 0 };   /* initialised, or it is a common symbol */

static BOOL load(HMODULE opengl, get_proc_fn get_proc)
{
    BOOL ok = TRUE;

#define LOAD_CORE(name, ret, args) \
    if (!(*(FARPROC*) &gl.name = GetProcAddress(opengl, "gl" #name))) { log_line("gl: no gl" #name); ok = FALSE; }
#define LOAD_LATER(name, ret, args) \
    if (!(*(PROC*) &gl.name = get_proc("gl" #name))) { log_line("gl: no gl" #name); ok = FALSE; }

#define LOAD_SHADER(name, ret, args)     if (!(*(PROC*) &gl.name = get_proc("gl" #name))) gl.shaders = FALSE;

    GL_FUNCTIONS(LOAD_CORE)
    GL_LATER_FUNCTIONS(LOAD_LATER)
    gl.shaders = TRUE;
    GL_SHADER_FUNCTIONS(LOAD_SHADER)
    if (!gl.shaders)
        log_line("gl: no shaders, the picture is drawn as it is");
    return ok;
}

BOOL gl_start(HWND window)
{
    PIXELFORMATDESCRIPTOR pfd = { sizeof pfd, 1 };
    HMODULE opengl = LoadLibraryA("opengl32.dll");
    create_context_fn create;
    make_current_fn make_current;
    get_proc_fn get_proc;
    choose_format_fn choose;
    set_format_fn set;
    HGLRC context;
    int format;

    if (!opengl)
    {
        log_line("gl: no opengl32.dll");
        return FALSE;
    }

    create = (create_context_fn) GetProcAddress(opengl, "wglCreateContext");
    make_current = (make_current_fn) GetProcAddress(opengl, "wglMakeCurrent");
    get_proc = (get_proc_fn) GetProcAddress(opengl, "wglGetProcAddress");
    choose = (choose_format_fn) api("gdi32.dll", "ChoosePixelFormat");
    set = (set_format_fn) api("gdi32.dll", "SetPixelFormat");
    g_swap = (swap_fn) api("gdi32.dll", "SwapBuffers");
    if (!create || !make_current || !get_proc || !choose || !set || !g_swap)
        return FALSE;

    /* The window only ever shows the finished frame; depth lives in the back buffer's FBO. */
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;

    g_dc = GetDC(window);
    format = choose(g_dc, &pfd);
    if (!format || !set(g_dc, format, &pfd) || !(context = create(g_dc)) || !make_current(g_dc, context))
    {
        log_line("gl: could not create a context on the game window");
        return FALSE;
    }

    if (!load(opengl, get_proc))
        return FALSE;

    log_line("gl: %s, %s, OpenGL %s", gl.GetString(GL_VENDOR), gl.GetString(GL_RENDERER), gl.GetString(GL_VERSION));

    g_interval = (interval_fn) get_proc("wglSwapIntervalEXT");
    gl_vsync_apply();
    return TRUE;
}

/* Present on vertical blank, or not. The game's physics assume a few dozen frames a second, as its
   original drivers gave it; uncapped, the bikes jitter and the AI can fall apart, so without
   vsync gl_present holds the frames to the refresh rate itself. */
void gl_vsync_apply(void)
{
    if (g_interval && g_interval(g_render.vsync ? 1 : 0))
        log_line("gl: vsync %s", g_render.vsync ? "on" : "off, the frames held to the refresh rate");
    else
        log_line("gl: vsync cannot be set - the frame rate is held to the refresh rate");
}

/* Without vsync, or with a frame limit, the frame waits for its turn at the limit or else the
   display's refresh rate: slept most of the way, then spun, so it is neither late nor early. */
static void frame_hold(void)
{
    static counter_fn counter;
    static double frequency, next;
    static int refresh;
    LARGE_INTEGER now;
    double at, period;

    if (!counter) {
        device_caps_fn caps = (device_caps_fn) api("gdi32.dll", "GetDeviceCaps");
        period_fn period_begin = (period_fn) api("winmm.dll", "timeBeginPeriod");
        LARGE_INTEGER ticks;
        counter_fn ticks_per_second = (counter_fn) api("kernel32.dll", "QueryPerformanceFrequency");

        counter = (counter_fn) api("kernel32.dll", "QueryPerformanceCounter");
        if (!counter || !ticks_per_second || !ticks_per_second(&ticks))
            return;
        frequency = (double) ticks.QuadPart;
        refresh = caps ? caps(g_dc, VREFRESH) : 0;
        if (refresh < 30)                       /* 0 and 1 are the card's own default */
            refresh = 60;
        if (period_begin)
            period_begin(1);                    /* Sleep(1) sleeps a millisecond, not a tick */
    }
    if (!frequency)
        return;
    period = 1000.0 / (g_render.frame_limit > 0 ? g_render.frame_limit : refresh);
    counter(&now);
    at = (double) now.QuadPart * 1000.0 / frequency;
    if (next < at - period)                     /* far behind, or the first frame: start now */
        next = at;
    while (at < next) {
        if (next - at > 2.0)
            Sleep(1);
        counter(&now);
        at = (double) now.QuadPart * 1000.0 / frequency;
    }
    next += period;
}

void gl_present(void)
{
    if (!g_render.vsync || !g_interval || g_render.frame_limit > 0)
        frame_hold();
    g_swap(g_dc);
}
