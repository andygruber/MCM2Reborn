/*
    OpenMCM2 dev build - its settings and its trace log (see dev.h).
*/

#include "dev.h"
#include "gl.h"

#include <stdarg.h>

#define SETTINGS_FILE ".\\OpenMCM2-Dev.ini"

dev_switches dev_settings = { 0 };     /* initialised, or it is a common symbol and lands in .data */

typedef UINT (WINAPI* read_int_fn)(LPCSTR section, LPCSTR key, INT fallback, LPCSTR file);

static BOOL read_switch(read_int_fn read, const char* section, const char* key)
{
    return read(section, key, 0, SETTINGS_FILE) != 0;
}

#define FONTS_FOLDER "ui\\modern\\fonts\\"

typedef HANDLE (WINAPI* find_first_fn)(LPCSTR pattern, WIN32_FIND_DATAA* found);
typedef BOOL   (WINAPI* find_next_fn)(HANDLE search, WIN32_FIND_DATAA* found);
typedef BOOL   (WINAPI* find_close_fn)(HANDLE search);
typedef int    (WINAPI* add_font_fn)(LPCSTR path, DWORD flags, void* reserved);

/*
    The modern menus' fonts, from the game's ui\modern\fonts folder: made available to this
    process alone (nothing is installed), for the menus' FontName to find by name.
*/
static void load_menu_fonts(void)
{
    find_first_fn find_first = (find_first_fn) api("kernel32.dll", "FindFirstFileA");
    find_next_fn find_next = (find_next_fn) api("kernel32.dll", "FindNextFileA");
    find_close_fn find_close = (find_close_fn) api("kernel32.dll", "FindClose");
    add_font_fn add_font = (add_font_fn) api("gdi32.dll", "AddFontResourceExA");
    WIN32_FIND_DATAA found;
    HANDLE search;
    int loaded = 0;

    if (!find_first || !find_next || !find_close || !add_font)
        return;
    search = find_first(FONTS_FOLDER "*.ttf", &found);
    if (search == INVALID_HANDLE_VALUE)
        return;
    do {
        char path[MAX_PATH];

        wsprintfA(path, "%s%s", FONTS_FOLDER, found.cFileName);
        if (add_font(path, FR_PRIVATE, NULL))
            loaded++;
        else
            log_line("dev build: the font %s would not load", path);
    } while (find_next(search, &found));
    find_close(search);
    log_line("dev build: %d menu font(s) from " FONTS_FOLDER, loaded);
}

void dev_install(void)
{
    read_int_fn read = (read_int_fn) api("kernel32.dll", "GetPrivateProfileIntA");

    if (read) {
        dev_settings.trace_screens = read_switch(read, "trace", "screens");
        dev_settings.trace_events = read_switch(read, "trace", "events");
        dev_settings.trace_files = read_switch(read, "trace", "files");
        dev_settings.trace_display = read_switch(read, "trace", "display");
        dev_settings.menus_widescreen = read_switch(read, "menus", "widescreen");
        dev_settings.menus_fill_sides = read_switch(read, "menus", "fill_sides");
        dev_settings.menus_modern = read_switch(read, "menus", "modern");
        dev_settings.menus_live = read_switch(read, "menus", "live_background");
        /* The modern menus are drawn at the screen's size. */
        dev_settings.menus_widescreen |= dev_settings.menus_modern;
        dev_settings.fix_last_pick = read_switch(read, "fixes", "last_pick");
    }
    log_line("dev build: trace screens %d, events %d, files %d, display %d; "
             "menus widescreen %d, fill sides %d, modern %d, live %d; fixes: last pick %d",
             dev_settings.trace_screens, dev_settings.trace_events, dev_settings.trace_files,
             dev_settings.trace_display, dev_settings.menus_widescreen,
             dev_settings.menus_fill_sides, dev_settings.menus_modern, dev_settings.menus_live,
             dev_settings.fix_last_pick);
    if (dev_settings.menus_modern)
        load_menu_fonts();
    if (read) {
        g_render.vsync = read("graphics", "vsync", 1, SETTINGS_FILE) != 0;
        g_render.samples = (int) read("graphics", "antialiasing", 4, SETTINGS_FILE);
        g_render.filtering = (int) read("graphics", "filtering", 4, SETTINGS_FILE);
    }
    log_line("dev build: graphics: vsync %d, anti-aliasing %dx, filtering %d", g_render.vsync,
             g_render.samples, g_render.filtering);
    eos_install();
    inputfix_install();
}

void dev_graphics_save(void)
{
    typedef BOOL (WINAPI* write_fn)(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR file);
    write_fn write = (write_fn) api("kernel32.dll", "WritePrivateProfileStringA");
    char value[16];

    if (!write)
        return;
    wsprintfA(value, "%d", g_render.vsync);
    write("graphics", "vsync", value, SETTINGS_FILE);
    wsprintfA(value, "%d", g_render.samples);
    write("graphics", "antialiasing", value, SETTINGS_FILE);
    wsprintfA(value, "%d", g_render.filtering);
    write("graphics", "filtering", value, SETTINGS_FILE);
}

/* A clock for timing: milliseconds since it was first read. */
double dev_milliseconds(void)
{
    typedef BOOL (WINAPI* counter_fn)(LARGE_INTEGER* value);
    static counter_fn counter;
    static LARGE_INTEGER start;
    static double per_ms;
    LARGE_INTEGER now;

    if (!counter) {
        counter_fn frequency = (counter_fn) api("kernel32.dll", "QueryPerformanceFrequency");
        LARGE_INTEGER rate;

        counter = (counter_fn) api("kernel32.dll", "QueryPerformanceCounter");
        if (!counter || !frequency || !frequency(&rate) || !rate.QuadPart || !counter(&start)) {
            counter = NULL;
            return 0;
        }
        per_ms = (double) rate.QuadPart / 1000;
    }
    counter(&now);
    return (double) (now.QuadPart - start.QuadPart) / per_ms;
}

void dev_trace(const char* area, const char* fmt, ...)
{
    char text[900];
    va_list args;

    va_start(args, fmt);
    wvsprintfA(text, fmt, args);
    va_end(args);
    log_line("[%s] %s", area, text);
}

/* The events' names, as the ports number them. */
static const char* event_name(DWORD type)
{
    static const char* const names[] = {
        "?", "click", "select", "slide", "?", "open", "close", "timer", "?", "child ended",
        "edit done", "key", "key up", "?", "?", "?", "slide end", "slide hold", "?", "edit changed",
    };

    return type < sizeof names / sizeof *names ? names[type] : "?";
}

void dev_trace_event(DWORD type, const char* control, const void* dialog)
{
    if (dev_settings.trace_events)
        dev_trace("event", "%s (%u) from %s to dialog %08X", event_name(type), type,
                  control ? control : "(none)", (DWORD) dialog);
}
