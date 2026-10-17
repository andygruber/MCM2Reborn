/*
    OpenMCM2 dev build - the few maths functions the dev code needs, without the CRT's (the game
    links its own 1999 one, which the new code cannot rely on): near enough for drawing and for
    the cameras, not for physics.
*/

#pragma once

#define PI 3.14159265f

float sine(float radians);
float cosine(float radians);
float square_root(float x);
float arc_tangent2(float y, float x);       /* atan2: -pi..pi */
float exp_minus(float x);                   /* e^-x, for x >= 0 */

/* `radians` brought to -pi..pi. */
float angle_wrap(float radians);

/*
    A critically damped spring towards `target`, as fast as `omega` (1/s): *value and *speed
    carried from frame to frame, `dt` seconds on. Exact for any dt, so the frame rate does not
    change how it moves.
*/
void spring(float* value, float* speed, float target, float omega, float dt);

/* The same for an angle, the short way round. */
void spring_angle(float* value, float* speed, float target, float omega, float dt);
