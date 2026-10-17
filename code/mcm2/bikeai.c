/*
    OpenMCM2 - a computer rider's bike, its step (KrustyBike::vf_49 calls it for a bike that is
    not bailed and has no player on it, in place of the full physics): its plan, its steering and
    a simple model of how it moves.

    In the dev build ([fixes] ai_rate) the step runs at a fixed rate, the frame's time made up
    of whole steps. The game made one step of each frame, and its rider leans a fifth of the way
    to its target each step while steering corrections go into that target divided by the step's
    time: a frame rate over 60 shook the computer riders side to side until they fell.

    Steps at a fixed rate land unevenly on the frames (none in one, two in the next), so the bike
    is drawn between its last two steps, by the time left over. It is drawn from two objects in
    the scene, each placed by the step as a remote bike's are (0x00494610): the bike's own
    (+0x3BC) and its rider's (+0x5C4, then +0x1A0). Each one's place (the matrix at +0xB8, which
    what hangs from it follows) is blended for the frame and put back before the next step. And
    the rider's animation (KrustyBike slot 102), which the step ends with, runs once a frame by
    the frame's time instead, as a remote bike's does (0x00494610): run by the steps, the rider
    moved at their rate alone and shook on the smooth bike.
*/

#include "runtime.h"
#include "game.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#include "../dev/maths.h"

#include <mmsystem.h>
#endif

void __fastcall game_orig_FUN_00414000(BYTE* self, void* edx_unused, float dt);

#ifdef OPENMCM2_DEV
#define BIKES_MOST  32                  /* a race's riders, 30 at most (riders.c), with room */
#define STEPS_MOST  12                  /* a frame of 0.2 s, the most the game gives, at 60 */

/* A scene object's place: side, up and forward (4 floats each), then its position. */
#define PLACE       0xB8
#define PLACE_SIZE  16
#define OBJECTS     2                   /* the bike's and its rider's */
#define JUMP_MOST   20.0f               /* feet in one step: further is a reset, not drawn between */

void __fastcall game_FUN_00500690(BYTE* object);    /* its place changed: what hangs from it follows */

#define ANIMATE         0x198           /* KrustyBike slot 102: the rider's animation, by a time */
#define ANIMATE_CALL    0x00414FFB      /* the step's call of it: call [edx + 198h] */

typedef void (__fastcall* animate_fn)(BYTE* bike, void* edx_unused, float dt);

static const BYTE animate_call_was[] = { 0xFF, 0x92, 0x98, 0x01, 0x00, 0x00 };
static BOOL g_animate_moved;            /* the step's call goes through step_animate */
static BOOL g_animate_later;            /* the steps leave the rider's animation to the frame */

/* At ANIMATE_CALL: ecx the bike, edx its vtable, the time pushed. Called by the steps, nothing. */
static __declspec(naked) void step_animate(void)
{
    __asm {
        cmp g_animate_later, 0
        jne later
        jmp dword ptr [edx + ANIMATE]
    later:
        ret 4
    }
}

typedef struct ai_bike {
    BYTE* bike;
    float owed;                         /* time not yet stepped */
    BYTE* objects[OBJECTS];             /* the scene's objects for it when last stepped */
    float before[OBJECTS][PLACE_SIZE];  /* each one's place before the last step, and after it */
    float after[OBJECTS][PLACE_SIZE];
    float drawn[OBJECTS][PLACE_SIZE];   /* the places written for this frame */
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

/* The bike's object `k` in the scene, or NULL. */
static BYTE* object_of(BYTE* bike, int k)
{
    BYTE* rider;

    if (k == 0)
        return FIELD(bike, 0x3BC, BYTE*);
    rider = FIELD(bike, 0x5C4, BYTE*);
    return rider ? FIELD(rider, 0x1A0, BYTE*) : NULL;
}

static float* place_of(BYTE* object)
{
    return (float*) (object + PLACE);
}

static void place_set(BYTE* object, const float* place)
{
    __movsd((unsigned long*) place_of(object), (const unsigned long*) place, PLACE_SIZE);
    game_FUN_00500690(object);
}

static void places_read(ai_bike* ai, float (*to)[PLACE_SIZE])
{
    int k;

    for (k = 0; k < OBJECTS; k++)
        if (ai->objects[k])
            __movsd((unsigned long*) to[k], (const unsigned long*) place_of(ai->objects[k]), PLACE_SIZE);
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

/*
    [trace] ai=1: the first computer rider, each frame, to ai-record.csv: the frame's time, its
    steps and the time left over; each scene object's place as drawn (position, up) and the
    bike's own position; its steering and lean commands; each wheel's place (+0xD8). Lengths
    in hundredths, directions in thousandths.
*/
static void ai_record(BYTE* bike, const ai_bike* ai, float dt, int steps)
{
    static HANDLE file = INVALID_HANDLE_VALUE;
    static BOOL tried;
    char line[640];
    DWORD done;
    int n, k;

    if (!dev_settings.trace_ai || ai != &g_ai_bikes[0])
        return;
    if (file == INVALID_HANDLE_VALUE) {
        static const char header[] =
            "ms,dt,steps,owed,blended,bike x,bike y,bike z,bike up x,bike up y,bike up z,"
            "rider x,rider y,rider z,rider up x,rider up y,rider up z,pos x,pos y,pos z,steer,lean,"
            "front x,front y,front z,rear x,rear y,rear z\r\n";

        if (tried)
            return;
        tried = TRUE;
        file = CreateFileA("ai-record.csv", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file == INVALID_HANDLE_VALUE)
            return;
        WriteFile(file, header, sizeof header - 1, &done, NULL);
    }
    n = wsprintfA(line, "%lu,%d,%d,%d,%d", timeGetTime(), (int) (dt * 100000), steps, (int) (ai->owed * 100000),
                  ai->blended);
    for (k = 0; k < OBJECTS; k++) {
        const float* place = ai->objects[k] ? place_of(ai->objects[k]) : NULL;

        if (place)
            n += wsprintfA(line + n, ",%d,%d,%d,%d,%d,%d", (int) (place[12] * 100), (int) (place[13] * 100),
                           (int) (place[14] * 100), (int) (place[4] * 1000), (int) (place[5] * 1000),
                           (int) (place[6] * 1000));
        else
            n += wsprintfA(line + n, ",,,,,,");
    }
    n += wsprintfA(line + n, ",%d,%d,%d,%d,%d", (int) (FIELD(bike, 0x0C, float) * 100),
                   (int) (FIELD(bike, 0x10, float) * 100), (int) (FIELD(bike, 0x14, float) * 100),
                   (int) (FIELD(bike, 0x504, float) * 1000), (int) (FIELD(bike, 0x508, float) * 1000));
    for (k = 0; k < 2; k++) {
        BYTE* wheel = FIELD(bike, k ? 0x5F4 : 0x5F0, BYTE*);

        if (wheel)
            n += wsprintfA(line + n, ",%d,%d,%d", (int) (FIELD(wheel, 0xD8, float) * 100),
                           (int) (FIELD(wheel, 0xDC, float) * 100), (int) (FIELD(wheel, 0xE0, float) * 100));
        else
            n += wsprintfA(line + n, ",,,");
    }
    n += wsprintfA(line + n, "\r\n");
    WriteFile(file, line, n, &done, NULL);
}

PORT(0x00414000)
void __fastcall ai_bike_step(BYTE* self, void* edx_unused, float dt)
{
#ifdef OPENMCM2_DEV
    if (dev_settings.fix_ai_rate > 0) {
        float step = 1.0f / dev_settings.fix_ai_rate;
        ai_bike* ai = ai_bike_of(self);
        int steps = 0, k;

        g_animate_later = g_animate_moved;

        /* The true places back for the step, unless something else has moved them since. */
        for (k = 0; ai->blended && k < OBJECTS; k++)
            if (ai->objects[k] && ai->objects[k] == object_of(self, k)
                && place_same(place_of(ai->objects[k]), ai->drawn[k]))
                place_set(ai->objects[k], ai->after[k]);
        ai->blended = FALSE;

        ai->owed += dt;
        while (ai->owed >= step && steps < STEPS_MOST) {
            for (k = 0; k < OBJECTS; k++)
                ai->objects[k] = object_of(self, k);
            places_read(ai, ai->before);
            game_orig_FUN_00414000(self, edx_unused, step);
            places_read(ai, ai->after);
            ai->owed -= step;
            steps++;
        }
        if (steps == STEPS_MOST)
            ai->owed = 0;
        if (g_animate_later) {
            g_animate_later = FALSE;
            VIRTUAL(self, ANIMATE, animate_fn)(self, NULL, dt);
        }

        /* Drawn between its last two steps; not across a reset, nor before it has stepped. */
        for (k = 0; k < OBJECTS; k++) {
            BYTE* object = ai->objects[k];

            if (!object || object != object_of(self, k) || !place_same(place_of(object), ai->after[k])
                || distance_squared(ai->before[k], ai->after[k]) >= JUMP_MOST * JUMP_MOST
                || place_same(ai->before[k], ai->after[k]))
                continue;
            place_between(ai->drawn[k], ai->before[k], ai->after[k], ai->owed / step);
            place_set(object, ai->drawn[k]);
            ai->blended = TRUE;
        }
        ai_record(self, ai, dt, steps);
        return;
    }
#endif
    game_orig_FUN_00414000(self, edx_unused, dt);
}

#ifdef OPENMCM2_DEV
/* The step's call of the rider's animation goes through step_animate, if it is the game's. */
void ai_install(void)
{
    BYTE call[6] = { 0xE8, 0, 0, 0, 0, 0x90 };
    DWORD to = (DWORD) step_animate - (ANIMATE_CALL + 5);
    BOOL known = !memcmp((const void*) ANIMATE_CALL, animate_call_was, sizeof animate_call_was);

    __movsb(call + 1, (const BYTE*) &to, 4);
    if (known)
        g_animate_moved = write_protected((void*) ANIMATE_CALL, call, sizeof call);
    log_line("dev build: the computer riders' animation %s", known ? "once a frame" : "left in their steps");
}
#endif
