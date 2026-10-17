/*
    OpenMCM2 dev build - modern game pads, through XInput ([gamepad], the GAMEPAD page).

    The game knows DirectInput's devices alone: an Xbox pad is a joystick to it whose two triggers
    share one axis (so a trigger bound to the throttle holds it on), and a pad of DirectInput's
    "gamepad" kind steers by a keyboard's ramp, not by its stick (scratchpad input/input.md). So
    the first XInput pad connected is read here as well: its left stick steers (the right or the
    left one leans), and each of the game's actions may be given any of its buttons, triggers or
    directions. The game's own devices go on as they were; whichever is pushed further steers.
*/

#include "dev.h"
#include "maths.h"

#include <mmsystem.h>

pad_options g_pad = { 1, 15, 15, 0, 1,
                      /* steer left, right, lean forward, back: the sticks */
                      { 0, 0, 0, 0,
                        PAD_RT, PAD_LT, PAD_A, PAD_B, PAD_Y, PAD_UP, PAD_BACK, PAD_LB, PAD_RB, PAD_X } };

typedef struct xinput_state {           /* XINPUT_STATE */
    DWORD packet;
    WORD  buttons;
    BYTE  left_trigger, right_trigger;
    SHORT left_x, left_y, right_x, right_y;
} xinput_state;

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
        log_line("pad: XInput %s", g_get ? "found" : "not found; DirectInput's devices alone");
    }
    if (!g_get)
        return FALSE;
    if (now == g_read_at)
        return g_index >= 0;
    g_read_at = now;
    if (g_index >= 0 && g_get((DWORD) g_index, &g_state) != ERROR_SUCCESS) {
        log_line("pad: pad %d gone", g_index);
        g_index = -1;
    }
    if (g_index < 0 && now - g_looked_at >= LOOK_EVERY_MS) {
        g_looked_at = now;
        for (i = 0; i < PADS_MOST && g_index < 0; i++)
            if (g_get((DWORD) i, &g_state) == ERROR_SUCCESS) {
                g_index = i;
                log_line("pad: pad %d connected", i);
            }
    }
    return g_index >= 0;
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

BOOL pad_connected(void)
{
    return pad_read();
}

BOOL pad_start_down(void)
{
    return pad_read() && (g_state.buttons & 0x0010) != 0;
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
    return v < 0 ? -curved : curved;
}
