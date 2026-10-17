/*
    OpenMCM2 - a computer rider's bike, its step (KrustyBike::vf_49 calls it for a bike that is
    not bailed and has no player on it, in place of the full physics): its plan, its steering and
    a simple model of how it moves.

    In the dev build ([fixes] ai_rate) the step runs at a fixed rate, the frame's time made up
    of whole steps. The game made one step of each frame, and its rider leans a fifth of the way
    to its target each step while steering corrections go into that target divided by the step's
    time: a frame rate over 60 shook the computer riders side to side until they fell.

    Steps at a fixed rate land unevenly on the frames (none in one, two in the next), so the bike
    is drawn between its last two steps, by the time left over: its place in the scene (the
    matrix at +0xB8, which the rider and everything else on the bike hang from) is blended for
    the frame and put back before the next step.
*/

#include "runtime.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#include "../dev/maths.h"
#endif

void __fastcall game_orig_FUN_00414000(BYTE* self, void* edx_unused, float dt);

#ifdef OPENMCM2_DEV
#define BIKES_MOST  32                  /* a race's riders, 30 at most (riders.c), with room */
#define STEPS_MOST  12                  /* a frame of 0.2 s, the most the game gives, at 60 */

/* The bike's place in the scene: side, up and forward (4 floats each), then its position. */
#define PLACE       0xB8
#define PLACE_SIZE  16
#define JUMP_MOST   20.0f               /* feet in one step: further is a reset, not drawn between */

void __fastcall game_FUN_00500690(BYTE* object);    /* its place changed: what hangs from it follows */

typedef struct ai_bike {
    BYTE* bike;
    float owed;                         /* time not yet stepped */
    float before[PLACE_SIZE];           /* its place before the last step, and after it */
    float after[PLACE_SIZE];
    float drawn[PLACE_SIZE];            /* the place written for this frame */
    BOOL  blended;
} ai_bike;

static ai_bike g_ai_bikes[BIKES_MOST];

static ai_bike* ai_bike_of(BYTE* bike)
{
    int i, free_slot = -1;

    for (i = 0; i < BIKES_MOST; i++) {
        if (g_ai_bikes[i].bike == bike)
            return &g_ai_bikes[i];
        if (!g_ai_bikes[i].bike && free_slot < 0)
            free_slot = i;
    }
    if (free_slot < 0)
        free_slot = (int) (((UINT_PTR) bike >> 4) % BIKES_MOST);    /* more bikes than ever race */
    __stosd((unsigned long*) &g_ai_bikes[free_slot], 0, sizeof g_ai_bikes[0] / 4);
    g_ai_bikes[free_slot].bike = bike;
    return &g_ai_bikes[free_slot];
}

static float* place_of(BYTE* bike)
{
    return (float*) (bike + PLACE);
}

static void place_set(BYTE* bike, const float* place)
{
    __movsd((unsigned long*) place_of(bike), (const unsigned long*) place, PLACE_SIZE);
    game_FUN_00500690(bike);
}

static BOOL place_same(const float* a, const float* b)
{
    int i;

    for (i = 0; i < PLACE_SIZE; i++)
        if (a[i] != b[i])
            return FALSE;
    return TRUE;
}

static float dot(const float* a, const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void cross(float* to, const float* a, const float* b)
{
    to[0] = a[1] * b[2] - a[2] * b[1];
    to[1] = a[2] * b[0] - a[0] * b[2];
    to[2] = a[0] * b[1] - a[1] * b[0];
}

static void unit(float* v)
{
    float length = square_root(dot(v, v));
    int k;

    if (length > 0)
        for (k = 0; k < 3; k++)
            v[k] /= length;
}

/* The place `t` of the way from `a` to `b`: the position along the line, the axes turned and
   kept square, the side axis on the same hand as b's. */
static void place_between(float* to, const float* a, const float* b, float t)
{
    float* side = to;
    float* up = to + 4;
    float* forward = to + 8;
    float hand[3];
    int k;

    for (k = 0; k < PLACE_SIZE; k++)
        to[k] = a[k] + (b[k] - a[k]) * t;
    unit(forward);
    for (k = 0; k < 3; k++)
        up[k] -= forward[k] * dot(up, forward);
    unit(up);
    cross(hand, b + 4, b + 8);
    if (dot(hand, b) >= 0)
        cross(side, up, forward);
    else
        cross(side, forward, up);
}

static float distance_squared(const float* a, const float* b)
{
    float d[3] = { a[12] - b[12], a[13] - b[13], a[14] - b[14] };

    return dot(d, d);
}
#endif

PORT(0x00414000)
void __fastcall ai_bike_step(BYTE* self, void* edx_unused, float dt)
{
#ifdef OPENMCM2_DEV
    if (dev_settings.fix_ai_rate > 0) {
        float step = 1.0f / dev_settings.fix_ai_rate;
        ai_bike* ai = ai_bike_of(self);
        int steps = 0;

        /* The true place back for the step, unless something else has moved the bike since. */
        if (ai->blended && place_same(place_of(self), ai->drawn))
            place_set(self, ai->after);
        ai->blended = FALSE;

        ai->owed += dt;
        while (ai->owed >= step && steps < STEPS_MOST) {
            __movsd((unsigned long*) ai->before, (const unsigned long*) place_of(self), PLACE_SIZE);
            game_orig_FUN_00414000(self, edx_unused, step);
            __movsd((unsigned long*) ai->after, (const unsigned long*) place_of(self), PLACE_SIZE);
            ai->owed -= step;
            steps++;
        }
        if (steps == STEPS_MOST)
            ai->owed = 0;

        /* Drawn between its last two steps; not across a reset, nor before it has stepped. */
        if (place_same(place_of(self), ai->after)
            && distance_squared(ai->before, ai->after) < JUMP_MOST * JUMP_MOST
            && !place_same(ai->before, ai->after)) {
            place_between(ai->drawn, ai->before, ai->after, ai->owed / step);
            place_set(self, ai->drawn);
            ai->blended = TRUE;
        }
        return;
    }
#endif
    game_orig_FUN_00414000(self, edx_unused, dt);
}
