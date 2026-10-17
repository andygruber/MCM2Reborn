/*
    OpenMCM2 - the player's controls, as the bike takes them each frame (Vehicle slot 63), and
    whether an action is held (the inputs' slot 2, which asks the keyboard, the joystick chosen
    and the mouse in turn).

    The bike's commands: +0x470 the throttle axis (0 at 0.2 or under, -1 without one), +0x508 the
    lean, +0x474 the steering, and +0x504 the steering it rides by - for a joystick that is not a
    game pad, the steering cubed (half the stick turns an eighth), else the steering itself.

    In the dev build a game pad read through XInput (code/dev/pad.c) steers and leans too,
    whichever is pushed further, and its buttons hold the actions given them; and an analog
    stick's steering goes through the response and sensitivity chosen in place of the cube, eased
    over [gamepad] smoothing_ms.
*/

#include "runtime.h"
#include "game.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

void __fastcall game_orig_vf_63_52E3E0(BYTE* self, void* edx_unused, DWORD unused);
BOOL __fastcall game_orig_vf_2_4C3CB0(BYTE* self, void* edx_unused, int action, DWORD devices);
BOOL __fastcall game_FUN_0048a770(void* device, void* edx_unused, int kind);   /* its subtype is `kind` */

#define axis_value(axis)    FIELD(axis, 0x24, float)
#define axis_device(axis)   FIELD(axis, 0x00, BYTE*)
#define device_type(d)      FIELD(d, 0x0C, int)         /* 2 a joystick, 3 one with force feedback */
#define GAMEPAD             4                           /* DIDEVTYPEJOYSTICK_GAMEPAD */

#ifdef OPENMCM2_DEV
static float size_of(float v)
{
    return v < 0 ? -v : v;
}
#endif

PORT(0x0052E3E0)
void __fastcall bike_controls(BYTE* self, void* edx_unused, DWORD unused)
{
    BYTE* throttle = FIELD(self, 0x500, BYTE*);
    BYTE* lean = FIELD(self, 0x4FC, BYTE*);
    BYTE* steer = FIELD(self, 0x4F8, BYTE*);
    BYTE* device = axis_device(steer);
    BOOL analog = device && !game_FUN_0048a770(device, NULL, GAMEPAD)
                  && (device_type(device) == 2 || device_type(device) == 3);
    float steering = axis_value(steer), leaning = axis_value(lean);

    (void) edx_unused;
    (void) unused;
    if (FIELD(self, 0x46C, int) && throttle)
        FIELD(self, 0x470, float) = axis_value(throttle) > 0.2f ? axis_value(throttle) : 0;
    else
        FIELD(self, 0x470, float) = -1.0f;
#ifdef OPENMCM2_DEV
    {
        float pad_steer, pad_lean;

        if (pad_sticks(&pad_steer, &pad_lean)) {
            if (size_of(pad_steer) > size_of(steering)) {
                steering = pad_steer;
                analog = TRUE;
            }
            if (size_of(pad_lean) > size_of(leaning))
                leaning = pad_lean;
        }
    }
    if (analog)
        steering = pad_smooth(steering);
#endif
    FIELD(self, 0x508, float) = -leaning;
    FIELD(self, 0x474, float) = -steering;
    FIELD(self, 0x50C, float) = 0;
    if (analog) {
        float s = FIELD(self, 0x474, float);

#ifdef OPENMCM2_DEV
        FIELD(self, 0x504, float) = pad_curve(s);
#else
        FIELD(self, 0x504, float) = s * s * s;
#endif
    } else {
        FIELD(self, 0x504, float) = FIELD(self, 0x474, float);
    }
}

PORT(0x004C3CB0)
BOOL __fastcall action_held(BYTE* self, void* edx_unused, int action, DWORD devices)
{
#ifdef OPENMCM2_DEV
    if (pad_held(action))
        return TRUE;
#endif
    return game_orig_vf_2_4C3CB0(self, edx_unused, action, devices);
}
