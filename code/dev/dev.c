/*
    OpenMCM2 dev build - its settings and its trace log (see dev.h).
*/

#include "dev.h"

#include <stdarg.h>

#define SETTINGS_FILE ".\\OpenMCM2-Dev.ini"

dev_switches dev_settings = { 0 };     /* initialised, or it is a common symbol and lands in .data */

typedef UINT (WINAPI* read_int_fn)(LPCSTR section, LPCSTR key, INT fallback, LPCSTR file);

static BOOL read_switch(read_int_fn read, const char* section, const char* key)
{
    return read(section, key, 0, SETTINGS_FILE) != 0;
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
        dev_settings.fix_last_pick = read_switch(read, "fixes", "last_pick");
    }
    log_line("dev build: trace screens %d, events %d, files %d, display %d; "
             "menus widescreen %d, fill sides %d; fixes: last pick %d",
             dev_settings.trace_screens, dev_settings.trace_events, dev_settings.trace_files,
             dev_settings.trace_display, dev_settings.menus_widescreen,
             dev_settings.menus_fill_sides, dev_settings.fix_last_pick);
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
