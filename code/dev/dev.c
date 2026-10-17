/*
    OpenMCM2 dev build - its settings and its trace log (see dev.h).
*/

#include "dev.h"
#include "gl.h"

#include <stdarg.h>

/*
    The settings file is named after the game's exe, beside it: OpenMCM2-Dev.ini for the dev build,
    MCM2Reborn.ini for a release, so the two keep their own settings in one folder.
*/
static const char* settings_file(void)
{
    typedef DWORD (WINAPI* module_name_fn)(HMODULE, LPSTR, DWORD);
    static char path[MAX_PATH];
    module_name_fn module_name;
    int n, dot = -1, i;

    if (path[0])
        return path;
    module_name = (module_name_fn) api("kernel32.dll", "GetModuleFileNameA");
    n = module_name ? (int) module_name(NULL, path, MAX_PATH - 4) : 0;
    for (i = 0; i < n; i++) {
        if (path[i] == '.')
            dot = i;
        else if (path[i] == '\\')
            dot = -1;
    }
    if (n <= 0 || dot < 0)
        lstrcpyA(path, ".\\OpenMCM2-Dev.ini");
    else
        lstrcpyA(path + dot, ".ini");
    return path;
}
#define SETTINGS_FILE settings_file()

dev_switches dev_settings = { 0 };     /* initialised, or it is a common symbol and lands in .data */
camera_options g_camera_options = { 140, 0, 70, 0, 352 };
reborn_options g_reborn = { 1, 1, 1, 30, 1 };

typedef UINT (WINAPI* read_int_fn)(LPCSTR section, LPCSTR key, INT fallback, LPCSTR file);

/* The game's actions given pad controls, by their keys in [gamepad]: actions 4 to 13. */
static const char* const pad_keys[PAD_ACTIONS] = {
    NULL, NULL, NULL, NULL, "accelerate", "brake", "stunts_1_8", "stunts_9_16", "kahuna_dumpster",
    "taunt", "thrill_cam", "reverse_cam", "clutch", "back_up",
};

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
        dev_settings.trace_camera = read_switch(read, "trace", "camera");
        dev_settings.trace_input = read_switch(read, "trace", "input");
        dev_settings.trace_physics = read_switch(read, "trace", "physics");
        dev_settings.trace_ai = read_switch(read, "trace", "ai");
        dev_settings.menus_widescreen = read_switch(read, "menus", "widescreen");
        dev_settings.menus_fill_sides = read_switch(read, "menus", "fill_sides");
        dev_settings.menus_modern = read_switch(read, "menus", "modern");
        dev_settings.menus_live = read_switch(read, "menus", "live_background");
        /* The modern menus are drawn at the screen's size. */
        dev_settings.menus_widescreen |= dev_settings.menus_modern;
        dev_settings.fix_last_pick = read_switch(read, "fixes", "last_pick");
        /* 30 or the game's own, 0: 30 moves them as the game did at 30 frames a second. */
        dev_settings.fix_ai_rate = read("fixes", "ai_rate", 30, SETTINGS_FILE) ? 30 : 0;
        dev_settings.online_presence = read("online", "presence", 1, SETTINGS_FILE) != 0;
        dev_settings.online_rate = (int) read("online", "packets", 20, SETTINGS_FILE);
        {
            typedef DWORD (WINAPI* read_text_fn)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
            read_text_fn read_text = (read_text_fn) api("kernel32.dll", "GetPrivateProfileStringA");

            if (read_text)
                read_text("leaderboard", "url", "", g_leaders_url, sizeof g_leaders_url, SETTINGS_FILE);
        }
        if (dev_settings.online_rate > 60)
            dev_settings.online_rate = 60;
    }
    log_line("%s %s", GAME_TITLE, GAME_VERSION);
    log_line("dev build: trace screens %d, events %d, files %d, display %d; "
             "menus widescreen %d, fill sides %d, modern %d, live %d; fixes: last pick %d, ai rate %d",
             dev_settings.trace_screens, dev_settings.trace_events, dev_settings.trace_files,
             dev_settings.trace_display, dev_settings.menus_widescreen,
             dev_settings.menus_fill_sides, dev_settings.menus_modern, dev_settings.menus_live,
             dev_settings.fix_last_pick, dev_settings.fix_ai_rate);
    if (dev_settings.menus_modern)
        load_menu_fonts();
    if (read) {
        g_render.vsync = read("graphics", "vsync", 1, SETTINGS_FILE) != 0;
        g_render.samples = (int) read("graphics", "antialiasing", 4, SETTINGS_FILE);
        g_render.filtering = (int) read("graphics", "filtering", 4, SETTINGS_FILE);
        g_render.frame_limit = (int) read("graphics", "frame_limit", 0, SETTINGS_FILE);
        g_render.fxaa = (int) read("graphics", "fxaa", 0, SETTINGS_FILE);
        g_render.sharpen = (int) read("graphics", "sharpen", 0, SETTINGS_FILE);
        g_render.vibrance = (int) read("graphics", "vibrance", 0, SETTINGS_FILE);
        g_render.brightness = (int) read("graphics", "brightness", 100, SETTINGS_FILE);
        g_render.bloom = (int) read("graphics", "bloom", 0, SETTINGS_FILE);
    }
    log_line("dev build: graphics: vsync %d, anti-aliasing %dx, filtering %d, frame limit %d; "
             "fxaa %d, sharpen %d, vibrance %d, brightness %d, bloom %d", g_render.vsync, g_render.samples,
             g_render.filtering, g_render.frame_limit, g_render.fxaa, g_render.sharpen, g_render.vibrance,
             g_render.brightness, g_render.bloom);
    if (read) {
        camera_options* c = &g_camera_options;

        c->fov = (int) read("camera", "fov", c->fov, SETTINGS_FILE);
        c->tilt = (int) read("camera", "tilt", c->tilt, SETTINGS_FILE);
        c->pitch = (int) read("camera", "pitch", c->pitch, SETTINGS_FILE);
        c->corner = (int) read("camera", "corner_anticipation", c->corner, SETTINGS_FILE);
        c->lean_heading = (int) read("camera", "lean_heading", c->lean_heading, SETTINGS_FILE);
        g_reborn.hud = (int) read("reborn", "hud", g_reborn.hud, SETTINGS_FILE);
        g_reborn.cameras = (int) read("reborn", "cameras", g_reborn.cameras, SETTINGS_FILE);
        g_reborn.physics = (int) read("reborn", "physics", g_reborn.physics, SETTINGS_FILE);
        g_reborn.riders = (int) read("reborn", "riders", g_reborn.riders, SETTINGS_FILE);
        g_reborn.gear = (int) read("reborn", "gear", g_reborn.gear, SETTINGS_FILE);
    }
    if (read) {
        int a;

        g_pad.on = (int) read("gamepad", "xinput", g_pad.on, SETTINGS_FILE);
        g_pad.response = (int) read("gamepad", "response", g_pad.response, SETTINGS_FILE);
        g_pad.deadzone = (int) read("gamepad", "deadzone", g_pad.deadzone, SETTINGS_FILE);
        g_pad.lean_stick = (int) read("gamepad", "lean_stick", g_pad.lean_stick, SETTINGS_FILE);
        g_pad.analog_di = (int) read("gamepad", "directinput_pads_analog", g_pad.analog_di, SETTINGS_FILE);
        g_pad.sensitivity = (int) read("gamepad", "sensitivity", g_pad.sensitivity, SETTINGS_FILE);
        g_pad.smoothing = (int) read("gamepad", "smoothing_ms", -1, SETTINGS_FILE);
        if (g_pad.smoothing < 0) {              /* saved before the eased steering: its tuning too */
            g_pad.smoothing = 60;
            g_pad.response = 20;
            g_pad.sensitivity = 90;
        }
        for (a = 0; a < PAD_ACTIONS; a++)
            if (pad_keys[a]) {
                int control = (int) read("gamepad", pad_keys[a], g_pad.bind[a], SETTINGS_FILE);

                g_pad.bind[a] = control >= 0 && control < PAD_CONTROLS ? control : PAD_NONE;
            }
    }
    eos_install();
    inputfix_install();
    gridfix_install();
    ai_install();
    steptime_install();
    riders_install();
}

static void write_number(const char* section, const char* key, int value)
{
    typedef BOOL (WINAPI* write_fn)(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR file);
    write_fn write = (write_fn) api("kernel32.dll", "WritePrivateProfileStringA");
    char text[16];

    if (!write)
        return;
    wsprintfA(text, "%d", value);
    write(section, key, text, SETTINGS_FILE);
}

void dev_camera_save(void)
{
    const camera_options* c = &g_camera_options;

    write_number("camera", "fov", c->fov);
    write_number("camera", "tilt", c->tilt);
    write_number("camera", "pitch", c->pitch);
    write_number("camera", "corner_anticipation", c->corner);
    write_number("camera", "lean_heading", c->lean_heading);
}

void dev_reborn_save(void)
{
    write_number("reborn", "hud", g_reborn.hud);
    write_number("reborn", "cameras", g_reborn.cameras);
    write_number("reborn", "physics", g_reborn.physics);
    write_number("reborn", "riders", g_reborn.riders);
    write_number("reborn", "gear", g_reborn.gear);
}

BOOL dev_modern_has(const char* file)
{
    typedef DWORD (WINAPI* attributes_fn)(LPCSTR);
    attributes_fn attributes = (attributes_fn) api("kernel32.dll", "GetFileAttributesA");
    char path[MAX_PATH];

    wsprintfA(path, "UI\\modern\\%s", file);
    return dev_settings.menus_modern && attributes && attributes(path) != INVALID_FILE_ATTRIBUTES;
}

void dev_pad_save(void)
{
    int a;

    write_number("gamepad", "xinput", g_pad.on);
    write_number("gamepad", "response", g_pad.response);
    write_number("gamepad", "deadzone", g_pad.deadzone);
    write_number("gamepad", "lean_stick", g_pad.lean_stick);
    write_number("gamepad", "directinput_pads_analog", g_pad.analog_di);
    write_number("gamepad", "sensitivity", g_pad.sensitivity);
    write_number("gamepad", "smoothing_ms", g_pad.smoothing);
    for (a = 0; a < PAD_ACTIONS; a++)
        if (pad_keys[a])
            write_number("gamepad", pad_keys[a], g_pad.bind[a]);
}

void dev_fixes_save(void)
{
    write_number("fixes", "ai_rate", dev_settings.fix_ai_rate);
}

void dev_graphics_save(void)
{
    write_number("graphics", "vsync", g_render.vsync);
    write_number("graphics", "antialiasing", g_render.samples);
    write_number("graphics", "filtering", g_render.filtering);
    write_number("graphics", "frame_limit", g_render.frame_limit);
    write_number("graphics", "fxaa", g_render.fxaa);
    write_number("graphics", "sharpen", g_render.sharpen);
    write_number("graphics", "vibrance", g_render.vibrance);
    write_number("graphics", "brightness", g_render.brightness);
    write_number("graphics", "bloom", g_render.bloom);
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
