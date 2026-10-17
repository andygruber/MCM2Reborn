/*
    OpenMCM2 - a bike's steering for the physics step (Bike vtable slot 72): with both wheels
    near the ground (each down, or within 0.2 ft: its depth, +0x150, above -0.2), its path bent by
    the bars' turn and the lean (Vehicle slot 74), the turn written to +0x4B8 and its rate to
    +0x43C; in the air or with a wheel up, nothing. (Turning with one wheel down, as Unleashed
    does, pitched the bike over that wheel: MCM2 turns a bike by turning it, and on one wheel it
    turned about the wheel.)

    In the dev build MCM2:Reborn's sideways grip (code/dev/physics.c) goes first: the turn bends
    the path now and the bike's heading only later in the step, so after it the turn itself looked
    like a slide, and the grip undid part of every turn and bled its speed.
*/

#include "runtime.h"

void __fastcall game_orig_vf_72_406BD0(BYTE* self, void* edx_unused, float* ground, int unused);

#ifdef OPENMCM2_DEV
void reborn_grip(BYTE* bike);
void physics_record(BYTE* bike);
#endif

PORT(0x00406BD0)
void __fastcall bike_steer(BYTE* self, void* edx_unused, float* ground, int unused)
{
#ifdef OPENMCM2_DEV
    reborn_grip(self);
#endif
    game_orig_vf_72_406BD0(self, edx_unused, ground, unused);
#ifdef OPENMCM2_DEV
    physics_record(self);
#endif
}
