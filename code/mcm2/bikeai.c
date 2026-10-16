/*
    OpenMCM2 - a computer rider's bike, its step (KrustyBike::vf_49 calls it for a bike that is
    not bailed and has no player on it, in place of the full physics): its plan, its steering and
    a simple model of how it moves.

    In the dev build ([fixes] ai_rate) the step runs at a fixed rate, the frame's time made up
    of whole steps. The game made one step of each frame, and its rider leans a fifth of the way
    to its target each step while steering corrections go into that target divided by the step's
    time: a frame rate over 60 shook the computer riders side to side until they fell.
*/

#include "runtime.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

void __fastcall game_orig_FUN_00414000(BYTE* self, void* edx_unused, float dt);

#ifdef OPENMCM2_DEV
#define BIKES_MOST  16
#define STEPS_MOST  12                  /* a frame of 0.2 s, the most the game gives, at 60 */

/* Each bike's time not yet stepped, by the bike. */
static struct {
    BYTE* bike;
    float owed;
} owing[BIKES_MOST];

static float* owed_by(BYTE* bike)
{
    int i, free_slot = -1;

    for (i = 0; i < BIKES_MOST; i++) {
        if (owing[i].bike == bike)
            return &owing[i].owed;
        if (!owing[i].bike && free_slot < 0)
            free_slot = i;
    }
    if (free_slot < 0)
        free_slot = (int) (((UINT_PTR) bike >> 4) % BIKES_MOST);    /* more bikes than ever race */
    owing[free_slot].bike = bike;
    owing[free_slot].owed = 0;
    return &owing[free_slot].owed;
}
#endif

PORT(0x00414000)
void __fastcall ai_bike_step(BYTE* self, void* edx_unused, float dt)
{
#ifdef OPENMCM2_DEV
    if (dev_settings.fix_ai_rate > 0) {
        float step = 1.0f / dev_settings.fix_ai_rate;
        float* owed = owed_by(self);
        int steps = 0;

        *owed += dt;
        while (*owed >= step && steps < STEPS_MOST) {
            game_orig_FUN_00414000(self, edx_unused, step);
            *owed -= step;
            steps++;
        }
        if (steps == STEPS_MOST)
            *owed = 0;
        return;
    }
#endif
    game_orig_FUN_00414000(self, edx_unused, dt);
}
