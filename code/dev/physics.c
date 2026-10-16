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

    Suspension (shock.c). Unleashed's springs are straight, as stiff as so many times the bike's
    weight a foot (front 1.65, rear 2.55), damped at 0.3 and 0.4 of critical for the whole bike,
    the shock's speed counted to 20 ft/s at most. The game's sprang harder the further in, from
    nothing at full stretch, damped at 0.7 of critical for half the bike.
*/

#include "dev.h"
#include "game.h"
#include "maths.h"

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
    The Reborn shock's force: its spring as stiff as `stiffness` times the bike's weight a foot,
    damped at `damping` of critical for the whole bike; `compression` the shock's squeeze as a
    vector (its length the squeeze in feet), `speed` its rate. Writes the spring and damper's
    force and the damper's alone.
*/
void reborn_shock(BOOL front, float mass, const float* compression, const float* speed, float* force, float* damper)
{
    float stiffness = front ? 1.65f : 2.55f, damping = front ? 0.3f : 0.4f;
    float k = stiffness * mass * G;
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
