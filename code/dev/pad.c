/*
    OpenMCM2 dev build - modern Xbox and PlayStation pads ([gamepad], the GAMEPAD page).

    The game knows DirectInput's devices alone: an Xbox pad is a joystick to it whose two triggers
    share one axis (so a trigger bound to the throttle holds it on), and a pad of DirectInput's
    "gamepad" kind steers by a keyboard's ramp, not by its stick (scratchpad input/input.md). So
    the first XInput pad connected is read here, or a native Sony pad (padsony.c): its left stick steers (the right or the
    left one leans), and each of the game's actions may be given any of its buttons, triggers or
    directions. Keyboard input remains available alongside it. Both readers discover newly
    connected pads without the game's legacy joystick picker or bindings.
*/

#include "dev.h"
#include "maths.h"
#include "padstate.h"

#include <mmsystem.h>

pad_options g_pad = { 1, 20, 15, 0, 1,
                      /* steer left, right, lean forward, back: the sticks */
                      { 0, 0, 0, 0,
                        PAD_RT, PAD_LT, PAD_A, PAD_B, PAD_Y, PAD_UP, PAD_BACK, PAD_LB, PAD_RB, PAD_X },
                      90, 60 };

typedef DWORD (WINAPI* xinput_get_fn)(DWORD user, xinput_state* state);

#define PADS_MOST       4
#define LOOK_EVERY_MS   1000            /* a pad not connected is costly to ask: once a second */
#define TRIGGER_DOWN    30              /* XINPUT_GAMEPAD_TRIGGER_THRESHOLD */
#define STICK_DOWN      0.5f            /* a stick's direction as a button: half way */

/* XINPUT_GAMEPAD's buttons, by PAD_*. */
static const WORD k_buttons[PAD_CONTROLS] = {
    0, 0x1000, 0x2000, 0x4000, 0x8000, 0x0100, 0x0200, 0, 0, 0x0020, 0x0010, 0x0040, 0x0080,
    0x0001, 0x0002, 0x0004, 0x0008,
};

static xinput_get_fn g_get;
static BOOL          g_loaded;
static int           g_index = -1;      /* the pad read, or -1 */
static DWORD         g_looked_at, g_read_at;
static xinput_state  g_state;
static BOOL          g_sony;

/* The pad's state, read once a millisecond at most; FALSE with none connected. */
static BOOL pad_read(void)
{
    static const char* const dlls[] = { "xinput1_4.dll", "xinput1_3.dll", "xinput9_1_0.dll" };
    DWORD now = timeGetTime();
    int i;

    if (!g_pad.on)
        return FALSE;
    if (!g_loaded) {
        g_loaded = TRUE;
        for (i = 0; i < 3 && !g_get; i++)
            g_get = (xinput_get_fn) api(dlls[i], "XInputGetState");
        log_line("pad: XInput %s", g_get ? "found" : "unavailable; checking native PlayStation input");
    }
    if (now == g_read_at)
        return g_index >= 0 || g_sony;
    g_read_at = now;
    if (g_index >= 0 && g_get((DWORD) g_index, &g_state) != ERROR_SUCCESS) {
        log_line("pad: pad %d gone", g_index);
        g_index = -1;
    }
    if (g_get && g_index < 0 && (!g_looked_at || now - g_looked_at >= LOOK_EVERY_MS)) {
        g_looked_at = now;
        for (i = 0; i < PADS_MOST && g_index < 0; i++)
            if (g_get((DWORD) i, &g_state) == ERROR_SUCCESS) {
                g_index = i;
                log_line("pad: pad %d connected", i);
            }
    }
    g_sony = g_index < 0 && sony_pad_read(&g_state, TRUE);
    if (g_index < 0 && !g_sony)
        ZeroMemory(&g_state, sizeof g_state);
    return g_index >= 0 || g_sony;
}

/* A stick's two axes, -1..1, with its dead zone taken out round its middle (radially) and the
   rest stretched to the edge. */
static void stick(SHORT raw_x, SHORT raw_y, float* x, float* y)
{
    float fx = raw_x / 32767.0f, fy = raw_y / 32767.0f;
    float length = square_root(fx * fx + fy * fy), dead = g_pad.deadzone / 100.0f, scale;

    if (fx < -1)
        fx = -1;
    if (fy < -1)
        fy = -1;
    if (length <= dead || dead >= 1) {
        *x = *y = 0;
        return;
    }
    scale = (length > 1 ? 1 : length) - dead;
    scale = scale / (1 - dead) / length;
    *x = fx * scale;
    *y = fy * scale;
}

BOOL pad_sticks(float* steer, float* lean)
{
    float lx, ly, rx, ry;

    if (!pad_read())
        return FALSE;
    stick(g_state.left_x, g_state.left_y, &lx, &ly);
    stick(g_state.right_x, g_state.right_y, &rx, &ry);
    *steer = lx;                                /* the game's: -1 left, as its Steer Left key */
    *lean = -(g_pad.lean_stick ? ry : ly);      /* -1 forward, as its Lean Forward key */
    return TRUE;
}

static BOOL control_down(int control)
{
    float lx, ly, rx, ry;

    if (control <= PAD_NONE || control >= PAD_CONTROLS)
        return FALSE;
    if (control == PAD_LT)
        return g_state.left_trigger > TRIGGER_DOWN;
    if (control == PAD_RT)
        return g_state.right_trigger > TRIGGER_DOWN;
    if (control >= PAD_RIGHT_UP) {
        stick(g_state.right_x, g_state.right_y, &rx, &ry);
        return control == PAD_RIGHT_UP ? ry > STICK_DOWN : control == PAD_RIGHT_DOWN ? ry < -STICK_DOWN
             : control == PAD_RIGHT_LEFT ? rx < -STICK_DOWN : rx > STICK_DOWN;
    }
    if (control >= PAD_LEFT_UP) {
        stick(g_state.left_x, g_state.left_y, &lx, &ly);
        return control == PAD_LEFT_UP ? ly > STICK_DOWN : control == PAD_LEFT_DOWN ? ly < -STICK_DOWN
             : control == PAD_LEFT_LEFT ? lx < -STICK_DOWN : lx > STICK_DOWN;
    }
    return (g_state.buttons & k_buttons[control]) != 0;
}

BOOL pad_held(int action)
{
    float steer, lean;

    if (action < 0 || action >= PAD_ACTIONS || !pad_read())
        return FALSE;
    if (action < 4) {                           /* the sticks' directions, as the game's keys */
        pad_sticks(&steer, &lean);
        return action == 0 ? steer < -STICK_DOWN : action == 1 ? steer > STICK_DOWN
             : action == 2 ? lean < -STICK_DOWN : lean > STICK_DOWN;
    }
    return control_down(g_pad.bind[action]);
}

/* The first of the controls an action may have that is down: a button, a trigger or the right
   stick's way; PAD_NONE when none is (or there is no pad). START pauses and the left stick
   steers, so neither is one. */
int pad_pressed(void)
{
    int control;

    if (!pad_read())
        return PAD_NONE;
    for (control = PAD_A; control < PAD_CONTROLS; control++)
        if (control != PAD_START && (control < PAD_LEFT_UP || control > PAD_LEFT_RIGHT) && control_down(control))
            return control;
    return PAD_NONE;
}

BOOL pad_connected(void)
{
    return pad_read();
}

BOOL pad_start_down(void)
{
    return pad_read() && (g_state.buttons & 0x0010) != 0;
}

BOOL pad_back_down(void)
{
    return pad_read() && (g_state.buttons & (0x0020 | 0x2000)) != 0;
}

BOOL pad_accept_down(void)
{
    return pad_read() && (g_state.buttons & 0x1000) != 0;
}

/* The way the d-pad or left stick points in the menus: 1 up, 2 down, 3 left, 4 right, 0 none,
   -1 with no pad. The stick goes by its stronger axis, so a rough diagonal is not read as up. */
int pad_menu_direction(void)
{
    WORD buttons;
    float x, y;

    if (!pad_read())
        return -1;
    buttons = g_state.buttons;
    if (buttons & 0x000F)
        return buttons & 0x0001 ? 1 : buttons & 0x0002 ? 2 : buttons & 0x0004 ? 3 : 4;
    stick(g_state.left_x, g_state.left_y, &x, &y);
    if ((x < 0 ? -x : x) > (y < 0 ? -y : y))
        return x < -STICK_DOWN ? 3 : x > STICK_DOWN ? 4 : 0;
    return y > STICK_DOWN ? 1 : y < -STICK_DOWN ? 2 : 0;
}

/* LB -1, RB 1, neither (or both) 0: they step through a screen's tabs. */
int pad_menu_tab(void)
{
    if (!pad_read())
        return 0;
    return ((g_state.buttons & 0x0200) != 0) - ((g_state.buttons & 0x0100) != 0);
}

/* An analog stick's steering through the chosen response: its size to the power 1, 1.5, 2 or 3
   (the game's own, for its joysticks), its sign kept. */
float pad_curve(float v)
{
    float size = v < 0 ? -v : v, curved;

    switch (g_pad.response) {
    case 15: curved = size * square_root(size); break;
    case 20: curved = size * size; break;
    case 30: curved = size * size * size; break;
    default: curved = size; break;
    }
    curved *= g_pad.sensitivity / 100.0f;
    return v < 0 ? -curved : curved;
}

/* A first-order lag of g_pad.smoothing ms, by time, so the same at any frame rate: it takes a
   stick's small shakes out and follows a whole turn within a few tenths of a second. */
float pad_smooth(float v)
{
    static float now;
    static DWORD at;
    DWORD time = timeGetTime();
    float seconds = at ? (time - at) / 1000.0f : 0;

    at = time;
    if (g_pad.smoothing <= 0 || seconds <= 0 || seconds > 0.25f)
        return now = v;                     /* none, or the first frame after a pause */
    now += (v - now) * seconds / (seconds + g_pad.smoothing / 1000.0f);
    return now;
}
