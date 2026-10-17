/*
    OpenMCM2 dev build - a way out of the game whatever it is doing. The game covers the screen
    and hides the cursor; if it stops (a box it waits on hidden under it, a stuck start), the
    player could not get out. A thread of its own hears the keyboard through a low-level hook,
    which works while the game's own thread is stuck:

      Alt+F4    ends the game at once, its settings saved (profile.c's exit)
      Alt+Tab   when the game has shown no frame for two seconds, minimises it and lets the
                cursor go, so Windows can switch away; while it is running, as ever
*/

#include "dev.h"
#include "gl.h"

#include <mmsystem.h>

#define STUCK_MS 2000

typedef HHOOK   (WINAPI* set_hook_fn)(int, HOOKPROC, HINSTANCE, DWORD);
typedef LRESULT (WINAPI* next_hook_fn)(HHOOK, int, WPARAM, LPARAM);
typedef BOOL    (WINAPI* get_message_fn)(MSG*, HWND, UINT, UINT);
typedef BOOL    (WINAPI* show_async_fn)(HWND, int);
typedef BOOL    (WINAPI* clip_fn)(const RECT*);
typedef HWND    (WINAPI* foreground_fn)(void);
typedef DWORD   (WINAPI* window_process_fn)(HWND, DWORD*);
typedef DWORD   (WINAPI* process_id_fn)(void);

static next_hook_fn g_next;
static HHOOK g_hook;

/* The foreground window, when it is the game's. */
static HWND game_in_front_window(void)
{
    foreground_fn foreground = (foreground_fn) api("user32.dll", "GetForegroundWindow");
    window_process_fn window_process = (window_process_fn) api("user32.dll", "GetWindowThreadProcessId");
    process_id_fn process_id = (process_id_fn) api("kernel32.dll", "GetCurrentProcessId");
    HWND window = foreground ? foreground() : NULL;
    DWORD process = 0;

    if (window && window_process)
        window_process(window, &process);
    return window && process_id && process == process_id() ? window : NULL;
}

static LRESULT CALLBACK keyboard(int code, WPARAM message, LPARAM data)
{
    const KBDLLHOOKSTRUCT* key = (const KBDLLHOOKSTRUCT*) data;
    BOOL alt = code == HC_ACTION && (key->flags & LLKHF_ALTDOWN) && message == WM_SYSKEYDOWN;
    HWND game = alt ? game_in_front_window() : NULL;

    if (game && key->vkCode == VK_F4) {
        log_line("escape: Alt+F4, the game ended");
        ExitProcess(0);
    }
    if (game && key->vkCode == VK_TAB && timeGetTime() - g_frame_shown > STUCK_MS) {
        show_async_fn show_async = (show_async_fn) api("user32.dll", "ShowWindowAsync");
        clip_fn clip = (clip_fn) api("user32.dll", "ClipCursor");

        log_line("escape: Alt+Tab with no frame for %lu ms, the game minimised", timeGetTime() - g_frame_shown);
        if (clip)
            clip(NULL);
        if (show_async)
            show_async(game, SW_MINIMIZE);
    }
    return g_next(g_hook, code, message, data);
}

static DWORD WINAPI escape_thread(void* unused)
{
    set_hook_fn set_hook = (set_hook_fn) api("user32.dll", "SetWindowsHookExA");
    get_message_fn get_message = (get_message_fn) api("user32.dll", "GetMessageA");
    MSG message;

    (void) unused;
    g_next = (next_hook_fn) api("user32.dll", "CallNextHookEx");
    g_hook = set_hook && g_next && get_message ? set_hook(WH_KEYBOARD_LL, keyboard, GetModuleHandleA(NULL), 0) : NULL;
    if (!g_hook) {
        log_line("escape: no keyboard hook - Alt+F4 and Alt+Tab are the game's alone");
        return 0;
    }
    while (get_message(&message, NULL, 0, 0) > 0)
        ;                                   /* the hook is called from this thread's messages */
    return 0;
}

void escape_install(void)
{
    g_frame_shown = timeGetTime();
    CloseHandle(CreateThread(NULL, 0, escape_thread, NULL, 0, NULL));
}
