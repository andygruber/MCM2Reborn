/*
    OpenMCM2 - a bike's shock (InlineShock the fork, RotatingShock the swingarm): its force for
    the step, the spring's and the damper's (Shock, called by the vehicle for each of its tyres).

    k rises from the spring's rate at full stretch (+0x9C) to its rate fully squeezed (+0xA0) with
    the squeeze (+0x70, 0 to 1), so the spring grows with the square of it, from nothing. The
    damper is `zeta` of critical for the tyre's part of the bike, its rate halved past a knee speed.
    In the dev build, with [reborn] physics, the Reborn shock instead (code/dev/physics.c).
*/

#include "menus.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"

#define INLINE_SHOCK_VTABLE 0x0055CE10      /* the fork's; the swingarm's is 0x0055CE1C */

void reborn_shock(BOOL front, float mass, const float* compression, const float* speed, float* force, float* damper);
#endif

#define shock_squeeze(s)    FIELD(s, 0x70, float)
#define shock_k_stretched(s) FIELD(s, 0x9C, float)
#define shock_k_squeezed(s) FIELD(s, 0xA0, float)
#define shock_zeta(s)       FIELD(s, 0xA4, float)
#define shock_knee(s)       FIELD(s, 0xA8, float)
#define shock_force(s)      ((float*) ((s) + 0x58))
#define shock_damper(s)     ((float*) ((s) + 0x64))
#define shock_compression(s) ((float*) ((s) + 0x74))
#define shock_speed(s)      ((float*) ((s) + 0xB0))

/* Returns the force (+0x58), left in eax as the game's did. */
PORT(0x004FEB20)
float* __fastcall shock_force_step(BYTE* self, void* edx_unused, float share, float mass, float dt, float* unused)
{
    const float* v = shock_speed(self);
    const float* x = shock_compression(self);
    float* damper = shock_damper(self);
    float* force = shock_force(self);
    float k = (shock_k_squeezed(self) - shock_k_stretched(self)) * shock_squeeze(self) + shock_k_stretched(self);
    float speed2 = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    float speed = x87_equal(speed2, 1.0f) ? 1.0f : game_FUN_00461ac0(speed2);
    float c;
    int i;

    (void) edx_unused;
    (void) dt;
    (void) unused;
#ifdef OPENMCM2_DEV
    if (g_reborn.physics) {
        reborn_shock(*(DWORD*) self == INLINE_SHOCK_VTABLE, mass, x, v, force, damper);
        return force;
    }
#endif
    c = game_FUN_00461ac0(share * mass * k) * shock_zeta(self) * -2.0f;
    if (speed > shock_knee(self))
        c *= ((speed - shock_knee(self)) * 0.5f + shock_knee(self)) / speed;
    for (i = 0; i < 3; i++)
        damper[i] = c * v[i];
    for (i = 0; i < 3; i++)
        force[i] = k * x[i] + damper[i];
    return force;
}
