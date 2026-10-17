/*
    OpenMCM2 - a rider's place on the start (BikeRace::GetStartPos, bikerace.cpp): slot 1 to the
    race's riders (BikeRace + 0x158) across one row, spread over the gate's width by the riders'
    count, facing the way the race starts; slot 0 the row's middle.

    The dev build may have more riders than the game's 11 (riders.c). One row for 30 puts the bikes
    on top of each other, so past 11 the riders stand in rows of 11, each row spread as the game
    spreads 11, the next row behind it by twice the gate's own set-back (4.66) and half a place
    across, so that no bike stands right behind another.
*/

#include "runtime.h"
#include "game.h"

void __fastcall game_orig_FUN_00422030(BYTE* self, void* edx_unused, float* pos, float* dir,
                                       void* track, int slot);

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"

#define ROW             11
#define ROW_GAP         (2 * 4.66f)

#define race_riders(r)  FIELD(r, 0x158, int)
#define start_cached(r) FIELD(r, 0x188, BYTE)
#define start_step(r)   ((float*) ((r) + 0x170))
#endif

PORT(0x00422030)
void __fastcall race_start_place(BYTE* self, void* edx_unused, float* pos, float* dir, void* track,
                                 int slot)
{
#ifdef OPENMCM2_DEV
    int riders = race_riders(self);

    if (riders > ROW && slot > 0) {
        int row = (slot - 1) / ROW;
        float across = (row & 1) ? 0.5f : 0;
        int k;

        /* The game's row of 11, worked out afresh: the place in it, then moved back by the row. */
        race_riders(self) = ROW;
        start_cached(self) = 0;
        game_orig_FUN_00422030(self, edx_unused, pos, dir, track, (slot - 1) % ROW + 1);
        race_riders(self) = riders;
        for (k = 0; k < 3; k++)
            pos[k] += start_step(self)[k] * across - dir[k] * ROW_GAP * row;
        return;
    }
#endif
    game_orig_FUN_00422030(self, edx_unused, pos, dir, track, slot);
}
