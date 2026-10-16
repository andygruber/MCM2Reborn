/*
    OpenMCM2 - a bike's steering for the physics step (Bike vtable slot 72): with both wheels
    near the ground, its path bent by the bars' turn and the lean (Vehicle slot 74), the turn
    written to +0x4B8 and its rate to +0x43C; in the air or with the front up, nothing.

    "Near" is each tyre down, or within 0.2 ft of the ground (its depth, +0x150, above -0.2).
    In a berm or across a slope, the bike leant and a shock stretched, one tyre is often a little
    further off, and the bike could not turn at all: it ran straight on until it fell or found
    flat ground. In the dev build, with [reborn] physics, the bike steers with either wheel down,
    as Unleashed's does, the other tyre counted near for the game's test alone; then MCM2:Reborn's
    sideways grip (code/dev/physics.c).
*/

#include "runtime.h"
#include "game.h"

void __fastcall game_orig_vf_72_406BD0(BYTE* self, void* edx_unused, float* ground, int unused);

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"

#define bike_bailed(b)  FIELD(b, 0x444, int)
#define bike_wheelie(b) FIELD(b, 0x700, int)
#define bike_front(b)   FIELD(b, 0x5F0, BYTE*)
#define bike_rear(b)    FIELD(b, 0x5F4, BYTE*)
#define tyre_down(t)    FIELD(t, 0x260, int)
#define tyre_depth(t)   FIELD(t, 0x150, float)

void reborn_grip(BYTE* bike);
void physics_record(BYTE* bike);

/* The tyre off the ground while the other is down, or NULL. */
static BYTE* lifted_tyre(BYTE* bike)
{
    BYTE* front = bike_front(bike);
    BYTE* rear = bike_rear(bike);

    if (!g_reborn.physics || bike_bailed(bike) || bike_wheelie(bike) || !front || !rear)
        return NULL;
    if (tyre_down(front) && !tyre_down(rear))
        return rear;
    if (tyre_down(rear) && !tyre_down(front))
        return front;
    return NULL;
}
#endif

PORT(0x00406BD0)
void __fastcall bike_steer(BYTE* self, void* edx_unused, float* ground, int unused)
{
#ifdef OPENMCM2_DEV
    BYTE* lifted = lifted_tyre(self);
    float depth = lifted ? tyre_depth(lifted) : 0;

    if (lifted)
        tyre_depth(lifted) = 0;
    game_orig_vf_72_406BD0(self, edx_unused, ground, unused);
    if (lifted)
        tyre_depth(lifted) = depth;
    reborn_grip(self);
    physics_record(self);
#else
    game_orig_vf_72_406BD0(self, edx_unused, ground, unused);
#endif
}
