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

static HDC     g_dc;
static swap_fn g_swap;

static BOOL load(HMODULE opengl, get_proc_fn get_proc)
{
    BOOL ok = TRUE;

#define LOAD_CORE(name, ret, args) \
    if (!(*(FARPROC*) &gl.name = GetProcAddress(opengl, "gl" #name))) { log_line("gl: no gl" #name); ok = FALSE; }
#define LOAD_LATER(name, ret, args) \
    if (!(*(PROC*) &gl.name = get_proc("gl" #name))) { log_line("gl: no gl" #name); ok = FALSE; }

    GL_FUNCTIONS(LOAD_CORE)
    GL_LATER_FUNCTIONS(LOAD_LATER)
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

    /* Present on vertical blank. The game's physics assume a few dozen frames a second, as its
       original drivers gave it; uncapped, the bikes jitter and the AI can fall apart. */
    {
        typedef BOOL (WINAPI *interval_fn)(int);
        interval_fn interval = (interval_fn) get_proc("wglSwapIntervalEXT");

        if (interval && interval(1))
            log_line("gl: vsync on");
        else
            log_line("gl: vsync is not available - the frame rate is not capped");
    }
    return TRUE;
}

void gl_present(void)
{
    g_swap(g_dc);
}
