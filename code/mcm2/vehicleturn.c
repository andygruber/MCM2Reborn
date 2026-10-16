/*
    OpenMCM2 - a vehicle's turn on the ground for the physics step (Vehicle vtable slot 74), from
    the bike's steering (Bike slot 72): its path bent to the arc its bars and lean make (slot 73,
    times `scale`), by `grip` at most, and its speed kept; returns the angle turned.

    In the dev build, with [reborn] physics, a berm turned into tightens the arc first
    (code/dev/physics.c, reborn_berm).
*/

#include "runtime.h"
#include "game.h"

typedef float (__fastcall* arc_fn)(BYTE* self, void* edx_unused, float* axle, float* side);

float __fastcall game_orig_vf_74_52D800(BYTE* self, void* edx_unused, float* axle, float* side, float grip, float scale);

#ifdef OPENMCM2_DEV
float reborn_berm(BYTE* vehicle, const float* side, float radius);
#endif

PORT(0x0052D800)
float __fastcall vehicle_turn(BYTE* self, void* edx_unused, float* axle, float* side, float grip, float scale)
{
#ifdef OPENMCM2_DEV
    scale *= reborn_berm(self, side, VIRTUAL(self, 0x124, arc_fn)(self, NULL, axle, side) * scale);
#endif
    return game_orig_vf_74_52D800(self, edx_unused, axle, side, grip, scale);
}
