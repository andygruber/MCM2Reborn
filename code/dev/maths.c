/*
    OpenMCM2 dev build - maths without the CRT (maths.h).
*/

#include "maths.h"

float angle_wrap(float x)
{
    while (x > PI)
        x -= 2 * PI;
    while (x < -PI)
        x += 2 * PI;
    return x;
}

float sine(float x)
{
    float x2, term, sum;
    int k;

    x = angle_wrap(x);
    x2 = x * x;
    term = sum = x;
    for (k = 1; k < 8; k++) {
        term *= -x2 / ((2 * k) * (2 * k + 1));
        sum += term;
    }
    return sum;
}

float cosine(float x)
{
    return sine(x + PI / 2);
}

float square_root(float x)
{
    float r = x > 1 ? x : 1;
    int k;

    if (x <= 0)
        return 0;
    for (k = 0; k < 20; k++)
        r = (r + x / r) / 2;
    return r;
}

/* atan for |x| <= 1, by its series after halving the angle twice. */
static float arc_tangent_small(float x)
{
    float x2, term, sum;
    int k, halvings;

    for (halvings = 0; halvings < 2; halvings++)        /* atan x = 2 atan(x / (1 + sqrt(1 + x^2))) */
        x = x / (1 + square_root(1 + x * x));
    x2 = x * x;
    term = sum = x;
    for (k = 1; k < 8; k++) {
        term *= -x2;
        sum += term / (2 * k + 1);
    }
    return sum * 4;
}

float arc_tangent2(float y, float x)
{
    float a;

    if (x == 0 && y == 0)
        return 0;
    if ((x < 0 ? -x : x) >= (y < 0 ? -y : y)) {
        a = arc_tangent_small(y / x);
        if (x < 0)
            a += y < 0 ? -PI : PI;
    } else {
        a = PI / 2 - arc_tangent_small(x / y);
        if (y < 0)
            a -= PI;
    }
    return angle_wrap(a);
}

float exp_minus(float x)
{
    float term = 1, sum = 1;
    int k, halvings = 0;

    if (x <= 0)
        return 1;
    while (x > 0.5f && halvings < 30) {     /* e^-x = (e^-x/2)^2 */
        x *= 0.5f;
        halvings++;
    }
    for (k = 1; k < 10; k++) {
        term *= -x / k;
        sum += term;
    }
    for (; halvings > 0; halvings--)
        sum *= sum;
    return sum;
}

void spring(float* value, float* speed, float target, float omega, float dt)
{
    float off = *value - target;
    float push = *speed + omega * off;
    float decay = exp_minus(omega * dt);

    if (dt <= 0)
        return;
    *value = target + (off + push * dt) * decay;
    *speed = (*speed - omega * omega * dt * off - omega * dt * *speed) * decay;
}

void spring_angle(float* value, float* speed, float target, float omega, float dt)
{
    float near_target = *value + angle_wrap(target - *value);

    spring(value, speed, near_target, omega, dt);
    *value = angle_wrap(*value);
}
