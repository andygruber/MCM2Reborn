/*
    OpenMCM2 - a physics object's frame split into steps (SoultreePhysicsBaseObject vtable slot 9):
    the frame's time and what was left over, in steps of +0x1E4 (0.02 s), at most +0x1EC of them,
    the rest a shorter step after (the vehicle's step loop); a frame shorter than one step is one
    step of its length, and a frame of no time one whole step. And a bike's trick score for the
    step (KrustyBike slot 29), which the step loop calls at the end of each step.

    In the dev build both set the per-step constants for the step's length (code/dev/steptime.c);
    with [reborn] physics a frame of no time takes no step.
*/

#include "runtime.h"

void __fastcall game_orig_vf_9_506B20(BYTE* self, void* edx_unused, float dt, int* steps);
void __fastcall game_orig_vf_29_496E00(BYTE* self, void* edx_unused, int last);

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"

void steptime_update(BYTE* vehicle);
#endif

PORT(0x00506B20)
void __fastcall physics_split_frame(BYTE* self, void* edx_unused, float dt, int* steps)
{
    game_orig_vf_9_506B20(self, edx_unused, dt, steps);
#ifdef OPENMCM2_DEV
    if (g_reborn.physics && dt == 0)
        *steps = 0;
    steptime_update(self);
#endif
}

PORT(0x00496E00)
void __fastcall bike_trick_step(BYTE* self, void* edx_unused, int last)
{
#ifdef OPENMCM2_DEV
    steptime_update(self);
#endif
    game_orig_vf_29_496E00(self, edx_unused, last);
}
