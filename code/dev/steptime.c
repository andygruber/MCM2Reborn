/*
    OpenMCM2 dev build - the bike's physics the same at any frame rate ([reborn] physics, PHYSICS
    on the Game page).

    A physics step is as long as its share of the frame (the vehicle's +0x13C, h): 0.02 s at 50
    frames a second and slower, as the game was tuned, a seventh of that at 144. Most of a step
    goes by h, but some of it is so much per step: the spin damped by 0.85 on the ground with the
    heading turned and by 0.9 in the air, by 0.999 every step; the lean's 1.5 checks of an
    opposite spin; the thresholds that stop a slow bike, skip a small turn or choose its pivot; the
    rider's pump capped as a force for one step; a power slide counted in steps; the bail test's
    floor. At 144 a second the spin was damped near three times over. Here each takes this step's
    length into account, as the game's 0.02 s step would: the factors to the power h / 0.02, the
    per-step amounts times h / 0.02; the power slide by h; the bail test from a whole 0.02 s; and
    the shocks take this step's length, not the last one's. At 0.02 s steps it is the game.

    The constants are shared with the rest of the game, so the instructions that use them here are
    pointed at values of our own (found by their bytes, as the audit of the step lists them:
    scratchpad physics/dt/audit.md), set again for each step's length. The crashed rider's spin
    and the wheels' look in the air stay per step: they do not change how the bike rides.
*/

#include "dev.h"
#include "game.h"
#include "maths.h"

#define TUNED_STEP      0.02f

#define vehicle_step(v)         FIELD(v, 0x13C, float)
#define vehicle_tuned(v)        FIELD(v, 0x1E4, float)
#define vehicle_shock_step(v)   FIELD(v, 0x1E8, float)

/* Our values, in the order of the groups below. */
static struct {
    float ground_spin;      /* 0.85 */
    float spin_decay;       /* 0.999 */
    float air_spin;         /* 0.9 */
    float rest;             /* 0.001 ft/s */
    float turn_gate;        /* 1e-4 rad */
    float pivot;            /* 0.003 rad */
    float bail_floor;       /* 0.8: a step shorter than 0.016 s tested as one that long */
    float lean_check;       /* 1.5 */
    float pump_most;        /* 10000 */
} g_step = { 0.85f, 0.999f, 0.9f, 0.001f, 1e-4f, 0.003f, 0.8f, 1.5f, 10000.0f };

/* Each group: the code to search, the constant its instructions use, how many there are, ours. */
static const struct group {
    DWORD from, to, constant;
    int count;
    float* ours;
} groups[] = {
    { 0x0053098D, 0x005309BB, 0x0055DFA4, 3, &g_step.ground_spin },  /* the vehicle step, heading turned */
    { 0x00497640, 0x00497680, 0x005597FC, 3, &g_step.spin_decay },   /* KrustyBike's integrator */
    { 0x00506F50, 0x00506F90, 0x005597FC, 3, &g_step.spin_decay },   /* the base integrator */
    { 0x0040C0D0, 0x0040C104, 0x005558D8, 3, &g_step.air_spin },     /* in the air */
    { 0x0040C1A0, 0x0040C1B4, 0x005558D8, 1, &g_step.air_spin },     /* ...its roll's base */
    { 0x005301F7, 0x00530248, 0x00555888, 1, &g_step.rest },         /* a slow bike stopped */
    { 0x00530603, 0x00530648, 0x00555888, 1, &g_step.rest },         /* ...after a collision */
    { 0x00530825, 0x0053085A, 0x00555BC0, 2, &g_step.turn_gate },    /* the heading turned at all */
    { 0x005308C8, 0x005308D8, 0x0055DFA8, 1, &g_step.pivot },        /* ...about which point */
    { 0x0052FDA0, 0x0052FDB0, 0x00555BC0, 1, &g_step.turn_gate },    /* the body turned at all */
    { 0x0040C020, 0x0040C030, 0x00555BC0, 1, &g_step.turn_gate },    /* the air input turned at all */
    { 0x0040AC99, 0x0040AC9F, 0x00555850, 1, &g_step.bail_floor },   /* the bail test */
    { 0x0040CB60, 0x0040CCB0, 0x00555840, 6, &g_step.lean_check },   /* the lean against a spin */
    { 0x0040D488, 0x0040D4A0, 0x00555BFC, 2, &g_step.pump_most },    /* the rider's pump */
};
#define GROUPS ((int) (sizeof groups / sizeof groups[0]))

/* The power slide's length and its slowing, from the tuned step (+0x1E4) or the step (+0x13C). */
static const DWORD slide_by_step[] = { 0x0040D0B2, 0x0040D0D9 };
static const BYTE slide_was[2][4] = { { 0xD9, 0x86, 0xE4, 0x01 }, { 0xD8, 0xB6, 0xE4, 0x01 } };

static BOOL g_installed;
static BOOL g_slide_by_step;

/* The instructions in from..to that read `constant` (x87, [disp32]). */
static int find_uses(const struct group* g, BYTE** found, int most)
{
    int n = 0;
    DWORD at;

    for (at = g->from; at < g->to && n <= most; at++) {
        const BYTE* code = (const BYTE*) at;

        if ((code[0] == 0xD8 || code[0] == 0xD9) && (code[1] & 0xC7) == 0x05 && *(const DWORD*) (code + 2) == g->constant)
            found[n++] = (BYTE*) at;
    }
    return n;
}

void steptime_install(void)
{
    BYTE* uses[GROUPS][8];
    int i, k, j;

    for (i = 0; i < GROUPS; i++)
        if (find_uses(&groups[i], uses[i], 7) != groups[i].count) {
            log_line("dev build: the physics step is not the game's as known (group %d): left as it is", i);
            return;
        }
    for (k = 0; k < 2; k++)
        for (j = 0; j < 4; j++)
            if (((const BYTE*) slide_by_step[k])[j] != slide_was[k][j]) {
                log_line("dev build: the power slide is not the game's as known: the physics step left as it is");
                return;
            }
    for (i = 0; i < GROUPS; i++)
        for (k = 0; k < groups[i].count; k++) {
            DWORD ours = (DWORD) groups[i].ours;

            write_protected(uses[i][k] + 2, &ours, sizeof ours);
        }
    g_installed = TRUE;
    log_line("dev build: the physics step's per-step constants are now the step's own");
}

static void slide_by(BOOL step)
{
    BYTE field[2] = { step ? 0x3C : 0xE4, 0x01 };
    int k;

    if (step == g_slide_by_step)
        return;
    for (k = 0; k < 2; k++)
        write_protected((BYTE*) slide_by_step[k] + 2, field, sizeof field);
    g_slide_by_step = step;
}

/*
    The values for the vehicle's step now (vf_9 has just set its length, or the step loop has set
    the last, shorter one): the game's own on Classic.
*/
void steptime_update(BYTE* vehicle)
{
    float h = vehicle_step(vehicle), tuned = vehicle_tuned(vehicle), r;

    if (!g_installed)
        return;
    slide_by(g_reborn.physics);
    if (!g_reborn.physics || h <= 0 || tuned <= 0 || h == tuned) {
        g_step.ground_spin = 0.85f;
        g_step.spin_decay = 0.999f;
        g_step.air_spin = 0.9f;
        g_step.rest = 0.001f;
        g_step.turn_gate = 1e-4f;
        g_step.pivot = 0.003f;
        g_step.bail_floor = g_reborn.physics ? 1.0f : 0.8f;
        g_step.lean_check = 1.5f;
        g_step.pump_most = 10000.0f;
        if (g_reborn.physics && h > 0)
            vehicle_shock_step(vehicle) = h;
        return;
    }
    r = h / tuned;
    vehicle_shock_step(vehicle) = h;
    g_step.ground_spin = exp_minus(0.16251893f * r);  /* 0.85^r */
    g_step.spin_decay = exp_minus(0.0010005f * r);    /* 0.999^r */
    g_step.air_spin = exp_minus(0.10536052f * r);     /* 0.9^r */
    g_step.rest = 0.001f * r;
    g_step.turn_gate = 1e-4f * r;
    g_step.pivot = 0.003f * r;
    g_step.bail_floor = g_reborn.physics ? 1.0f : 0.8f;
    g_step.lean_check = 1.5f * r;
    g_step.pump_most = 10000.0f / r;
}
