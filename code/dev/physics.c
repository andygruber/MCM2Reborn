/*
    OpenMCM2 dev build - MCM2:Reborn's bike physics ([reborn] physics=1, PHYSICS on the Game page):
    the tyres' sideways grip and the suspension's spring and damper of MX vs ATV Unleashed, ported
    from its stock bike (mx_Pro250P.phy) and mxacore.dll. Only the player's bikes ride on them; the
    computer riders' simple model (bikeai.c) is the game's own.

    Grip. The game has no sideways grip of its own: its tyres' friction only works while the bars
    turn, and a turn bends the bike's path only with both wheels down. A bike landing a little
    sideways kept sliding the way it flew. Here each tyre on the ground pushes against the slide
    with Unleashed's curve, a damper at small slides that softens as the slide grows,

        a = g x share x scale x rate x v / (1 + |round x rate x v| ^ drop)

    for v the slide's speed sideways to the bike (ft/s), share the tyre's part of the bike's weight;
    at low speed the slide counts for up to five times as much, as in Unleashed. The slide is
    never pushed past nothing in one step.

    Berms (vehicleturn.c): a bank turned into holds the bike round, below.

    Suspension (shock.c). The game's springs, which stiffen the further in, damped as Unleashed
    damps: at 0.3 (fork) and 0.4 (shock) of critical for the whole bike, the shock's speed counted
    to 20 ft/s at most; the game damped at 0.7 of critical for half the bike. (Unleashed's straight
    springs, 1.65 and 2.55 times the bike's weight a foot, were made for a bike whose weight sits
    evenly on its wheels; on MCM2's, the fork carrying nearly three fifths of it, the fork
    bottomed in every hard turn and the bike tipped over the front, the rear wheel up.)
*/

#include "dev.h"
#include "game.h"
#include "maths.h"

#include <mmsystem.h>

#define G               32.174f         /* ft/s^2 */
#define LOW_SPEED       10.0f           /* ft/s: slower, the slide counts for more */
#define LOW_SPEED_MOST  5.0f            /* ...up to so many times, standing */

#define bike_bailed(b)  FIELD(b, 0x444, int)
#define bike_dt(b)      FIELD(b, 0x13C, float)
#define bike_vel(b)     ((float*) ((b) + 0x64))
#define bike_forward(b) ((float*) ((b) + 0x88))
#define bike_front(b)   FIELD(b, 0x5F0, BYTE*)
#define bike_rear(b)    FIELD(b, 0x5F4, BYTE*)
#define tyre_down(t)    FIELD(t, 0x260, int)
#define tyre_normal(t)  ((float*) ((t) + 0xE4))

/* Unleashed's lateral curve, dirt: Scale, SpeedScale, RoundOff, DropOff. */
typedef struct grip_curve {
    float scale, rate, round, drop;
} grip_curve;

static const grip_curve front_grip = { 38.2f, 0.005f, 4.0f, 1.5f };
static const grip_curve rear_grip = { 37.0f, 0.0045f, 4.0f, 1.6f };

/* The curve's push for a slide of `v` ft/s, in parts of the tyre's load (g). */
static float grip(const grip_curve* c, float v)
{
    float x = c->rate * (v < 0 ? -v : v);

    return c->scale * x / (1 + power(c->round * x, c->drop));
}

static float dot(const float* a, const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void reborn_grip(BYTE* bike)
{
    BYTE* front = bike_front(bike);
    BYTE* rear = bike_rear(bike);
    BOOL front_down = front && tyre_down(front), rear_down = rear && tyre_down(rear);
    float dt = bike_dt(bike), n[3] = { 0, 0, 0 }, side[3], length, along, slide, felt, push, share;
    float* v = bike_vel(bike);
    const float* f = bike_forward(bike);
    int i;

    if (!g_reborn.physics || bike_bailed(bike) || dt <= 0 || (!front_down && !rear_down))
        return;
    for (i = 0; i < 3; i++)
        n[i] = (front_down ? tyre_normal(front)[i] : 0) + (rear_down ? tyre_normal(rear)[i] : 0);

    /* Sideways to the bike, along the ground: n x forward. */
    side[0] = n[1] * f[2] - n[2] * f[1];
    side[1] = n[2] * f[0] - n[0] * f[2];
    side[2] = n[0] * f[1] - n[1] * f[0];
    length = square_root(dot(side, side));
    if (length < 0.001f)
        return;
    for (i = 0; i < 3; i++)
        side[i] /= length;

    slide = dot(v, side);
    along = dot(v, f);
    if (along < 0)
        along = -along;
    felt = slide * (along < LOW_SPEED ? LOW_SPEED_MOST - (LOW_SPEED_MOST - 1) * along / LOW_SPEED : 1);
    share = front_down && rear_down ? 0.5f : 1.0f;
    push = G * share * ((front_down ? grip(&front_grip, felt) : 0) + (rear_down ? grip(&rear_grip, felt) : 0)) * dt;
    if (push > (slide < 0 ? -slide : slide))
        push = slide < 0 ? -slide : slide;
    if (slide < 0)
        push = -push;
    for (i = 0; i < 3; i++)
        v[i] -= push * side[i];
}

/*
    Berms. The game turns a bike on the ground by bending its path to the arc its bars and lean
    make, the lean measured from the ground: in a berm, the bike square to its face, the arc is
    the flat ground's, and the bank's push never reaches the bike (only the up and down of the
    ground's forces do). A bank tilted at angle t towards the inside of the turn holds a turn of
    g tan t across it with no grip at all, so turning into one the arc's curve is that much
    more: 1 / R' = 1 / R + g tan t / v^2. As Unleashed hands the bike's upright over from the
    world to the ground, the berm's hold comes in from 10 to 30 ft/s; a bank past 60 degrees
    holds no more than one at 60; an off camber is the game's own.

    `side` is the turn's sideways axis on the ground, `radius` the game's arc (its sign the turn's
    way: the path bends against `side` for a positive one). Returns what the arc is to be
    multiplied by.
*/
#define BERM_FROM       10.0f           /* ft/s */
#define BERM_FULL       30.0f
#define BERM_STEEPEST   1.7320508f      /* tan 60 degrees */

void __fastcall game_FUN_0052da50(BYTE* vehicle, void* edx_unused, float* normal);   /* the tyres' ground, averaged */

float reborn_berm(BYTE* vehicle, const float* side, float radius)
{
    float speed = FIELD(vehicle, 0x434, float), n[3], inward[3], flat, lean_in, hold, ease;
    float sign = radius > 0 ? -1.0f : 1.0f;
    float r = radius < 0 ? -radius : radius;

    if (!g_reborn.physics || r < 0.001f)
        return 1;
    if (speed < 0)
        speed = -speed;
    if (speed <= BERM_FROM)
        return 1;
    ease = speed >= BERM_FULL ? 1 : (speed - BERM_FROM) / (BERM_FULL - BERM_FROM);

    game_FUN_0052da50(vehicle, NULL, n);
    inward[0] = sign * side[0];         /* across the turn, inwards, level */
    inward[1] = 0;
    inward[2] = sign * side[2];
    flat = square_root(inward[0] * inward[0] + inward[2] * inward[2]);
    if (flat < 0.001f || n[1] < 0.1f)
        return 1;
    lean_in = (n[0] * inward[0] + n[2] * inward[2]) / flat / n[1];     /* tan t */
    if (lean_in <= 0)
        return 1;
    if (lean_in > BERM_STEEPEST)
        lean_in = BERM_STEEPEST;
    hold = ease * G * lean_in / (speed * speed);
    return 1 / (1 + r * hold);
}

/*
    [trace] physics=1: each physics step of each bike, physics-record.csv in the game's folder,
    to see why a bike locks up or skips across a slope. Per tyre: down (+0x260), its depth to the
    ground (+0x150), whether it was handed to the rigid collision (+0x15C), its shock's squeeze
    (0 to 1) and whether it bottomed. Angles in thousandths of a radian, the rest in hundredths.
*/
#define tyre_rigid(t)       FIELD(t, 0x15C, int)
#define tyre_shock(t, f)    FIELD(t, (f) ? 0x2B0 : 0x2AC, BYTE*)

static HANDLE g_record = INVALID_HANDLE_VALUE;

static int hundredths(float x)
{
    return (int) (x * 100);
}

static int tyre_fields(char* line, BYTE* tyre, BOOL front)
{
    BYTE* shock = tyre ? tyre_shock(tyre, front) : NULL;

    if (!tyre)
        return wsprintfA(line, ",0,0,0,0,0");
    return wsprintfA(line, ",%d,%d,%d,%d,%d", tyre_down(tyre) != 0, hundredths(FIELD(tyre, 0x150, float)),
                     tyre_rigid(tyre) != 0, shock ? hundredths(FIELD(shock, 0x70, float)) : -1,
                     shock ? FIELD(shock, 0x50, int) != 0 : -1);
}

void physics_record(BYTE* bike)
{
    char line[512];
    const float* v = bike_vel(bike);
    const float* f = bike_forward(bike);
    DWORD done;
    int n;

    if (!dev_settings.trace_physics)
        return;
    if (g_record == INVALID_HANDLE_VALUE) {
        static const char header[] =
            "ms,bike,dt,speed,steer,turn,lean,bailed,wheelie,"
            "front down,front depth,front rigid,front squeeze,front bottomed,"
            "rear down,rear depth,rear rigid,rear squeeze,rear bottomed,vel x,vel y,vel z,fwd x,fwd y,fwd z\r\n";

        g_record = CreateFileA("physics-record.csv", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_record == INVALID_HANDLE_VALUE)
            return;
        WriteFile(g_record, header, sizeof header - 1, &done, NULL);
    }
    n = wsprintfA(line, "%lu,%04X,%d,%d,%d,%d,%d,%d,%d", timeGetTime(), (unsigned) ((UINT_PTR) bike & 0xFFFF),
                  hundredths(bike_dt(bike) * 1000), hundredths(FIELD(bike, 0x434, float)),
                  (int) (FIELD(FIELD(bike, 0x47C, BYTE*), 4, float) * 1000), (int) (FIELD(bike, 0x4B8, float) * 1000),
                  (int) (FIELD(bike, 0x4AC, float) * 1000), bike_bailed(bike) != 0, FIELD(bike, 0x700, int) != 0);
    n += tyre_fields(line + n, bike_front(bike), TRUE);
    n += tyre_fields(line + n, bike_rear(bike), FALSE);
    n += wsprintfA(line + n, ",%d,%d,%d,%d,%d,%d\r\n", hundredths(v[0]), hundredths(v[1]), hundredths(v[2]),
                   hundredths(f[0]), hundredths(f[1]), hundredths(f[2]));
    WriteFile(g_record, line, n, &done, NULL);
}

/*
    The Reborn shock's force: the game's spring `k` (lb/ft, its stiffness at this squeeze), damped
    at Unleashed's part of critical for the whole bike; `compression` the shock's squeeze as a
    vector (its length the squeeze in feet), `speed` its rate. Writes the spring and damper's
    force and the damper's alone.
*/
void reborn_shock(BOOL front, float mass, float k, const float* compression, const float* speed, float* force,
                  float* damper)
{
    float damping = front ? 0.3f : 0.4f;
    float c = -2 * damping * square_root(k * mass);
    float s = square_root(dot(speed, speed));
    int i;

    if (s > 20)
        c *= 20 / s;
    for (i = 0; i < 3; i++) {
        damper[i] = c * speed[i];
        force[i] = k * compression[i] + damper[i];
    }
}
