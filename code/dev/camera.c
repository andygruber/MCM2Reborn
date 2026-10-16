/*
    OpenMCM2 dev build - MCM2:Reborn's rider cameras, after MX vs ATV Unleashed's chase camera and
    the first-person views of MX Bikes and Open Unleashed: only where the camera is changes,
    never how the bike rides.

    The game's own camera update runs first (KrustyBikeCamera, 0x00498DD0); then, in its Chase Cam
    (mode 0) and Rider Cam (mode 3), this places the camera itself. Its other modes, the look-back,
    the crash and jump cameras, the intro flyby and the replays stay the game's.

    Chase: a level camera above and behind the rider that follows where the bike is going, its
    heading on a spring so it swings round in a turn and settles behind on the way out; its height
    on a slower spring in the air, so a jump is an arc and not a jolt; looking a little ahead and
    up or down a slope with the ground; held off the ground; its view widening with speed.

    Rider: from the rider's helmet, the head held as a neck holds it - its bumps softened, the
    horizon mostly level with some of the lean - looking where the bike points, a little towards
    where it is going and into the corner. Following the bike, not its path, keeps the bars and
    the rider's hands low in the view through whips, where a view kept on the path has the arms
    swing across it. The rider is drawn, as the game's Rider Cam would not.
*/

#include "dev.h"
#include "game.h"
#include "maths.h"

#include <mmsystem.h>

#define CHASE       0
#define RIDER       3

/* KrustyBikeCamera. */
#define cam_mode(c)     FIELD(c, 0x244, int)
#define cam_event(c)    FIELD(c, 0x3B8, BYTE*)
#define cam_crashed(c)  FIELD(c, 0x276, BYTE)
#define cam_on_bike(c)  FIELD(c, 0x390, int)
#define cam_bike(c)     FIELD(c, 0x3B4, BYTE*)
#define cam_eye(c)      ((float*) ((c) + 0x170))
#define cam_dir(c)      ((float*) ((c) + 0x17C))
#define cam_roll(c)     FIELD(c, 0x194, float)
#define cam_aspect(c)   FIELD(c, 0x19C, float)
#define cam_view_w(c)   FIELD(c, 0x1A8, int)        /* the viewport's: x, y, w, h from +0x1A0 */
#define cam_view_h(c)   FIELD(c, 0x1AC, int)
#define cam_heading(c)  ((float*) ((c) + 0x29C))    /* the game's own, smoothed */

/* KrustyBike. */
#define bike_pos(b)     ((float*) ((b) + 0x0C))
#define bike_vel(b)     ((float*) ((b) + 0x64))
#define bike_air(b)     FIELD(b, 0x108, BYTE)
#define bike_forward(b) ((float*) ((b) + 0x88))     /* the way it points (the game's heading when slow) */
#define bike_rider(b)   FIELD(b, 0x5C4, BYTE*)
#define rider_frame(r)  FIELD(r, 0x1A0, BYTE*)      /* the rider's model: its frames, Head among them */
#define frame_hidden(f) FIELD(f, 0x14C, int)        /* not drawn, nor the frames under it */

#define FT_PER_S_TO_MPH 0.68181825f

/* Camera: eye, direction, up, roll, fov (NULL keeps one); the fov; its matrices made anew. */
void __fastcall game_FUN_0042f8d0(BYTE* camera, void* edx_unused, const float* eye, const float* dir,
                                  const float* up, const float* roll, const float* fov);
void __fastcall game_FUN_0042f850(BYTE* camera, void* edx_unused, float fov);
/* A scene frame: the one of `name` among its children (found with the child's own). */
BYTE* __fastcall game_FUN_00502970(BYTE* frame, void* edx_unused, const char* name);
typedef void (__fastcall* camera_fn)(BYTE* camera, void* edx_unused);
typedef void (__fastcall* head_fn)(BYTE* camera, void* edx_unused, float* out);

#define cam_rebuild(c)      VIRTUAL(c, 0x70, camera_fn)(c, NULL)
#define cam_show_rider(c)   VIRTUAL(c, 0xD8, camera_fn)(c, NULL)    /* the rider and the bike, both */
#define cam_head(c, out)    VIRTUAL(c, 0xC8, head_fn)(c, NULL, out)  /* the rider's Head frame, in the world */

/* What the springs carry between frames; begun again when the camera or its mode changes. */
static struct {
    BYTE* camera;
    BYTE* bike;
    int   mode;
    float yaw, yaw_speed;
    float pitch, pitch_speed;
    float height, height_speed;     /* the chase eye's, above the world's zero */
    float roll, roll_speed;
    float head[3], head_speed[3];   /* the helmet relative to the bike */
    float eye[3], look[3];          /* the chase's, filtered */
    float fov;
} cam;

static float flat_speed(const float* v)
{
    return square_root(v[0] * v[0] + v[2] * v[2]);
}

/*
    In the rider's view the rider is drawn - the arms and hands on the bars, the bike under them -
    but not the head (and the helmet with it): the view is from where the face is, and the helmet
    showed from inside. It is shown again whenever the view is not the rider's, and one hidden on a
    bike since gone is let be.
*/
static BYTE* g_head;                    /* the Head frame hidden, and the bike it is on */
static BYTE* g_head_bike;

static void head_hide(BYTE* bike)
{
    BYTE* rider = bike_rider(bike);
    BYTE* model = rider ? rider_frame(rider) : NULL;
    BYTE* head = model ? game_FUN_00502970(model, NULL, "Head") : NULL;

    if (!head)
        return;
    frame_hidden(head) = 1;
    g_head = head;
    g_head_bike = bike;
}

static void head_show(BYTE* bike)
{
    if (g_head && bike == g_head_bike)
        frame_hidden(g_head) = 0;
    g_head = g_head_bike = NULL;
}

/*
    The view's width across, in degrees, for a height of `high` degrees on this camera's screen.
    The game's projection takes its field of view across and stretches it by the aspect.
*/
static float across_for_high(BYTE* camera, float high)
{
    float half = high * (PI / 360);

    return 2 * arc_tangent2(sine(half) / cosine(half) * cam_aspect(camera), 1) * (180 / PI);
}

/*
    The game made its 3D for a 4:3 screen, and on a wider one the renderer stretched it: a view
    meant for 4:3 was far too tall (the first-person view at 140 degrees bent from the roof to the
    bars). Each frame the camera's aspect is the picture's own, its viewport's width over height.
*/
static void aspect_true(BYTE* camera)
{
    static BOOL told;
    int w = cam_view_w(camera), h = cam_view_h(camera);
    float aspect;

    if (w <= 0 || h <= 0)
        return;
    aspect = (float) w / h;
    if (!told) {
        log_line("camera: the game's aspect %d/1000, the picture's %dx%d (%d/1000)",
                 (int) (cam_aspect(camera) * 1000), w, h, (int) (aspect * 1000));
        told = TRUE;
    }
    if (cam_aspect(camera) != aspect) {
        cam_aspect(camera) = aspect;
        cam_rebuild(camera);
    }
}

/* ---- Rider ---- */

/*
    The first-person view, ported from Open Unleashed's (MxaOnboardCamera.Apply, its "dynamic"
    head mode, with its option meanings): from just in front of the rider's head, the eye's place
    relative to the bike on a critically damped spring of 0.035 s; looking along the bike's going
    - its velocity over the last 0.1 s laid flat to its "up" over the last 0.5 s (the bike's up on
    the ground, the world's in the air), the bike's facing below 1 m/s and blended into the going
    by 5 m/s - so through a whip the view keeps the line and the bike swings under it. Then rolled
    by the lean times Tilt, turned by the corner anticipation, pitched down Pitch x 20 + 15
    degrees. The rider is drawn but for the head, which the view is in.
*/
#define FT_TO_M         0.3048f
#define STEP            0.002f          /* the averages' sample spacing, OU's Box */
#define UP_SAMPLES      250             /* 0.5 s */
#define VEL_SAMPLES     50              /* 0.1 s */
#define HEAD_SMOOTHING  0.035f
#define NEAR_PLANE      0.25f           /* feet: the game's 1 cut the bars and hands */

static const float eye_offset[3] = { 0, 0.52f, 0.3f };     /* in the head's own axes */

typedef struct box {                    /* OU's Box: the mean of a vector over a window */
    float buf[UP_SAMPLES][3];
    int   size, head;
    float out[3];
} box;

static box   g_up = { { { 0 } }, UP_SAMPLES }, g_vel = { { { 0 } }, VEL_SAMPLES };
static float g_up_src[3], g_vel_src[3], g_step_acc;

static void box_fill(box* b, const float* v)
{
    int i, k;

    for (i = 0; i < b->size; i++)
        for (k = 0; k < 3; k++)
            b->buf[i][k] = v[k];
    b->head = 0;
    for (k = 0; k < 3; k++)
        b->out[k] = v[k];
}

/* n samples more, from a to b over `elapsed`, as OU's Box.Push. */
static void box_push(box* b, const float* a, const float* to, int n, float acc0, float elapsed, float frac)
{
    float sum[3] = { 0, 0, 0 };
    int i, k;

    for (i = n - b->size + 1 > 1 ? n - b->size + 1 : 1; i <= n; i++) {
        float t = (i * STEP - acc0) / elapsed;

        t = t < 0 ? 0 : t > 1 ? 1 : t;
        for (k = 0; k < 3; k++)
            b->buf[b->head][k] = a[k] + (to[k] - a[k]) * t;
        b->head = (b->head + 1) % b->size;
    }
    for (i = 0; i < b->size; i++)
        for (k = 0; k < 3; k++)
            sum[k] += b->buf[i][k];
    for (k = 0; k < 3; k++)
        b->out[k] = (sum[k] + frac * (to[k] - b->buf[b->head][k])) / b->size;
}

static float dot3(const float* a, const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static void cross3(const float* a, const float* b, float* out)
{
    float c[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };

    out[0] = c[0];
    out[1] = c[1];
    out[2] = c[2];
}

static float normalise(float* v)
{
    float length = square_root(dot3(v, v));

    if (length > 1e-9f) {
        v[0] /= length;
        v[1] /= length;
        v[2] /= length;
    }
    return length;
}

/* The bike's body frame's world axes (scene frame rows at +0xF8: x, y, z). */
#define bike_body(b)    FIELD(b, 0x3BC, BYTE*)
#define frame_axis(f, n) ((const float*) ((f) + 0xF8 + (n) * 0x10))

/* The averages, fed with the time since the last frame (OU's UpdateFilters). */
static void filters_update(const float* up_now, const float* velocity, float dt, BOOL begin)
{
    float acc0 = g_step_acc;
    int n, k;

    if (begin) {
        box_fill(&g_up, up_now);
        box_fill(&g_vel, velocity);
        for (k = 0; k < 3; k++) {
            g_up_src[k] = up_now[k];
            g_vel_src[k] = velocity[k];
        }
        g_step_acc = 0;
        return;
    }
    g_step_acc += dt;
    n = (int) ((g_step_acc + 1e-6f) / STEP);
    g_step_acc = g_step_acc - n * STEP > 0 ? g_step_acc - n * STEP : 0;
    box_push(&g_up, g_up_src, up_now, n, acc0, dt, g_step_acc / STEP);
    box_push(&g_vel, g_vel_src, velocity, n, acc0, dt, g_step_acc / STEP);
    for (k = 0; k < 3; k++) {
        g_up_src[k] = up_now[k];
        g_vel_src[k] = velocity[k];
    }
}

static void rider(BYTE* camera, BYTE* bike, float dt, BOOL begin)
{
    const camera_options* o = &g_camera_options;
    const float* p = bike_pos(bike);
    const float* v = bike_vel(bike);
    BYTE* body = bike_body(bike);
    float world_up[3] = { 0, 1, 0 };
    float bike_right[3], bike_up[3], bike_fwd[3], head[3], head_x[3], head_y[3], head_z[3];
    float up[3], forward[3], right[3], flat[3], eye_world[3], rel[3], eye[3], dir[3], view_up[3];
    float speed_m, lean_deg, yaw, pitch, roll, fov, cy, sy, cp, sp, cr, sr, local[3];
    BYTE* rider_model = bike_rider(bike) ? rider_frame(bike_rider(bike)) : NULL;
    BYTE* head_frame = rider_model ? game_FUN_00502970(rider_model, NULL, "Head") : NULL;
    int k;

    if (!body || !head_frame)
        return;
    for (k = 0; k < 3; k++) {
        bike_right[k] = frame_axis(body, 0)[k];
        bike_up[k] = frame_axis(body, 1)[k];
        bike_fwd[k] = bike_forward(bike)[k];
        head_x[k] = frame_axis(head_frame, 0)[k];
        head_y[k] = frame_axis(head_frame, 1)[k];
        head_z[k] = frame_axis(head_frame, 2)[k];
    }
    normalise(bike_up);
    normalise(bike_fwd);
    cam_head(camera, head);

    /* The eye: just in front of the head, in its own axes; its place relative to the bike, in
       the bike's axes, on the 0.035 s spring. */
    for (k = 0; k < 3; k++)
        eye_world[k] = head[k] + head_x[k] * eye_offset[0] + head_y[k] * eye_offset[1] + head_z[k] * eye_offset[2]
                       - p[k];
    rel[0] = dot3(eye_world, bike_right);
    rel[1] = dot3(eye_world, bike_up);
    rel[2] = dot3(eye_world, bike_fwd);
    filters_update(bike_air(bike) ? world_up : bike_up, v, dt, begin);
    if (begin)
        for (k = 0; k < 3; k++) {
            cam.head[k] = rel[k];
            cam.head_speed[k] = 0;
        }
    else
        for (k = 0; k < 3; k++)
            spring(&cam.head[k], &cam.head_speed[k], rel[k], 1 / HEAD_SMOOTHING, dt);

    /* Its look: the going, flat to the averaged up. */
    for (k = 0; k < 3; k++)
        up[k] = g_up.out[k];
    if (normalise(up) < 1e-3f)
        up[0] = up[2] = 0, up[1] = 1;
    for (k = 0; k < 3; k++)
        flat[k] = g_vel.out[k] - up[k] * dot3(g_vel.out, up);
    speed_m = square_root(dot3(flat, flat)) * FT_TO_M;
    for (k = 0; k < 3; k++)
        forward[k] = bike_fwd[k];
    if (speed_m >= 5) {
        for (k = 0; k < 3; k++)
            forward[k] = flat[k];
        normalise(forward);
    } else if (speed_m > 1) {
        float t = (speed_m - 1) * 0.25f;

        normalise(flat);
        for (k = 0; k < 3; k++)
            forward[k] = bike_fwd[k] * (1 - t) + flat[k] * t;
        normalise(forward);
    }
    cross3(up, forward, right);
    right[1] = 0;
    if (dot3(right, right) < 1e-8f)
        cross3(bike_up, forward, right);
    normalise(right);
    cross3(forward, right, up);
    normalise(up);
    if (dot3(bike_up, up) < -0.5f)
        for (k = 0; k < 3; k++) {
            up[k] = -up[k];
            right[k] = -right[k];
        }
    cross3(right, up, forward);

    /* The bike's lean, seen from the view: by Tilt, the roll; by the lean, the corner's turn. */
    lean_deg = arc_tangent2(dot3(bike_up, right), dot3(bike_up, up)) * (180 / PI);
    yaw = 0;
    if (o->corner == 1) {
        float into = lean_deg / 50;

        yaw = (into < -1 ? -1 : into > 1 ? 1 : into) * o->lean_heading / 10.0f;
        yaw = yaw < -60 ? -60 : yaw > 60 ? 60 : yaw;
    }
    if (begin) {
        cam.yaw = yaw;
        cam.yaw_speed = 0;
    } else {
        spring(&cam.yaw, &cam.yaw_speed, yaw, 8.333334f, dt);
    }
    pitch = (o->pitch * 20 / 100.0f + 15) * (PI / 180);    /* down */
    roll = lean_deg * o->tilt / 100.0f * (PI / 180);
    cy = cosine(cam.yaw * (PI / 180));
    sy = sine(cam.yaw * (PI / 180));
    cp = cosine(pitch);
    sp = sine(pitch);
    cr = cosine(-roll);
    sr = sine(-roll);

    /* forward' = basis * Rz(-roll) * Ry(yaw) * Rx(pitch) * (0, 0, 1); up' likewise of (0, 1, 0). */
    local[0] = cp * sy;
    local[1] = -sp;
    local[2] = cp * cy;
    {
        float x = local[0] * cr - local[1] * sr, y = local[0] * sr + local[1] * cr;

        for (k = 0; k < 3; k++)
            dir[k] = right[k] * x + up[k] * y + forward[k] * local[2];
    }
    local[0] = sp * sy;
    local[1] = cp;
    local[2] = sp * cy;
    {
        float x = local[0] * cr - local[1] * sr, y = local[0] * sr + local[1] * cr;

        for (k = 0; k < 3; k++)
            view_up[k] = right[k] * x + up[k] * y + forward[k] * local[2];
    }
    for (k = 0; k < 3; k++)
        eye[k] = p[k] + bike_right[k] * cam.head[0] + bike_up[k] * cam.head[1] + bike_fwd[k] * cam.head[2];

    fov = (float) (o->fov < 60 ? 60 : o->fov > 160 ? 160 : o->fov);
    {
        float none = 0;

        game_FUN_0042f8d0(camera, NULL, eye, dir, view_up, &none, NULL);
    }
    game_FUN_0042f850(camera, NULL, fov);
    FIELD(camera, 0x1BC, float) = NEAR_PLANE;
    cam_rebuild(camera);
    cam_show_rider(camera);
    head_hide(bike);
}

/* ---- Chase ---- */

/*
    MX vs ATV Unleashed's follow camera, "Race Camera 1" (mxacore.dll follow_cam_update, its preset
    from Data\Cam\FC_MXDefault.pb, 16x9): a boom 15.5 feet long raised 42 degrees behind the way
    the bike points, from 0.35 feet above it, looking at a point 3 feet over it pushed ahead along
    its line as it goes faster; the look and the camera each on per-axis lag filters (0.30/0.23/0.30
    and 0.15/0.25/0.15 s); the view 85 degrees across, eased in from 80; flatter at a walk. Its
    heading is the bike's facing (the velocity heading sits behind a flag of its not identified,
    and the facing keeps the way ahead in view), its yaw rate the facing's turn; the terrain lift of
    the look is not ported (the camera sits ten feet up).
*/
#define FC_ANGLE_X      (42 * PI / 180)
#define FC_DISTANCE     15.5f
#define FC_FOV          85.0f
#define FC_FOV_MAX      110.0f
#define FC_FOV_START    80.0f
#define FC_FOV_FILTER   0.55f
#define FC_LOOK_AHEAD   15.0f
#define FC_LOOK_SWING   0.6f
#define FC_BODY_UP      0.35f
#define FC_LOOK_UP      3.0f
#define FC_SLOW_CUTOFF  30.0f           /* ft/s: below it the boom flattens */
#define FC_SLOW_SCALE   0.5f
#define FC_TOP_SPEED    117.33f         /* ft/s, 80 mph */

static const float fc_look_filter[3] = { 0.30f, 0.23f, 0.30f };
static const float fc_pos_filter[3] = { 0.15f, 0.25f, 0.15f };

/* lerpc: x from x0..x1 onto y0..y1, held within y0..y1. */
static float lerpc(float x, float x0, float y0, float x1, float y1)
{
    float y;

    if (x1 == x0)
        return y0;
    y = y0 + (x - x0) / (x1 - x0) * (y1 - y0);
    if (y0 < y1)
        return y < y0 ? y0 : y > y1 ? y1 : y;
    return y < y1 ? y1 : y > y0 ? y0 : y;
}

/* The game's filter: linear towards the target, its share of the way min(dt, tau) / tau. */
static void lag(float* value, float target, float tau, float dt)
{
    *value += (target - *value) * ((dt < tau ? dt : tau) / tau);
}

static void chase(BYTE* camera, BYTE* bike, float dt, BOOL begin)
{
    const float* p = bike_pos(bike);
    const float* v = bike_vel(bike);
    const float* f = bike_forward(bike);
    BYTE* body = bike_body(bike);
    float body_up[3] = { 0, 1, 0 }, base[3], look_base[3], look[3], eye[3], dir[3], up[3] = { 0, 1, 0 };
    float flat[3] = { f[0], 0, f[2] }, look_dir[3], speed, ratio, pitch, k, ahead, heading, yaw_rate, length;
    float swing, roll = 0;
    int i;

    if (body)
        for (i = 0; i < 3; i++)
            body_up[i] = frame_axis(body, 1)[i];
    normalise(flat);
    speed = square_root(dot3(v, v));
    for (i = 0; i < 3; i++)
        look_base[i] = p[i] + body_up[i] * FC_BODY_UP;

    /* Reversing: as if still. */
    if (speed > 4 && dot3(flat, v) < 0)
        speed = 0;

    heading = arc_tangent2(flat[0], flat[2]);
    yaw_rate = begin || dt <= 0 ? 0 : angle_wrap(heading - cam.yaw) / dt;
    cam.yaw = heading;

    if (begin)
        cam.fov = FC_FOV_START;
    lag(&cam.fov, FC_FOV, FC_FOV_FILTER, dt);
    ratio = FC_FOV / FC_FOV_MAX;

    pitch = FC_ANGLE_X;
    if (speed < FC_SLOW_CUTOFF) {
        pitch *= (1 - FC_SLOW_SCALE) * (speed / FC_SLOW_CUTOFF) + FC_SLOW_SCALE;
        pitch -= (f[1] < 0 ? -f[1] : f[1]) * FC_SLOW_SCALE;
    }

    if (speed < 1e-4f) {
        for (i = 0; i < 3; i++) {
            base[i] = look_base[i];
            look[i] = look_base[i];
        }
        look[1] += FC_LOOK_UP;
    } else {
        for (i = 0; i < 3; i++)
            base[i] = look_base[i] + v[i] * (dt < 0.2f ? dt : 0.2f);
        k = 1.8f * (speed / FC_TOP_SPEED) * (1 - FC_DISTANCE / 600)
            * (1 + ratio * FC_DISTANCE / 90) * (1 + ratio * FC_DISTANCE / 90);
        if (v[1] < -9)                  /* falling fast: less look ahead */
            k *= 1 + (v[1] / speed) * (5.0f / 9);
        ahead = k * FC_LOOK_AHEAD > 0 ? k * FC_LOOK_AHEAD : 0;
        swing = 0.147f * FC_LOOK_SWING * ratio * yaw_rate;
        look_dir[0] = flat[0] * cosine(swing) + flat[2] * sine(swing);
        look_dir[1] = 0;
        look_dir[2] = -flat[0] * sine(swing) + flat[2] * cosine(swing);
        for (i = 0; i < 3; i++)
            look[i] = base[i] + look_dir[i] * ahead;
        look[1] += 0.25f * v[1] * (1 - ratio) + FC_LOOK_UP;
    }

    /* The boom: the flat heading raised by the pitch, turned round behind (yaw 180). */
    eye[0] = base[0] - flat[0] * cosine(pitch) * FC_DISTANCE;
    eye[1] = base[1] + sine(pitch) * FC_DISTANCE;
    eye[2] = base[2] - flat[2] * cosine(pitch) * FC_DISTANCE;
    if (speed < 1e-4f)
        for (i = 0; i < 3; i++)
            eye[i] += v[i] * dt * 10;
    if (eye[1] < p[1] + 2)              /* the ground clamp's floor: two feet, here over the bike */
        eye[1] = p[1] + 2;

    if (begin) {
        for (i = 0; i < 3; i++) {
            cam.look[i] = look[i];
            cam.eye[i] = eye[i];
        }
    } else {
        for (i = 0; i < 3; i++) {
            lag(&cam.look[i], look[i], fc_look_filter[i], dt);
            lag(&cam.eye[i], eye[i], fc_pos_filter[i], dt);
        }
    }

    for (i = 0; i < 3; i++)
        dir[i] = cam.look[i] - cam.eye[i];
    length = normalise(dir);
    if (length <= 0)
        return;
    game_FUN_0042f8d0(camera, NULL, cam.eye, dir, up, &roll, NULL);
    game_FUN_0042f850(camera, NULL, cam.fov);
    cam_rebuild(camera);
}

/*
    [trace] camera=1: a frame-by-frame record, camera-record.csv in the game's folder, to find
    what jumps or flickers when the view warps. Each camera update writes a C line (the mode,
    whether ours placed it, the bike and the camera as it was left), and each view or projection
    the game then hands the renderer a T line, what is truly drawn with. Numbers are hundredths.
*/
static HANDLE g_record = INVALID_HANDLE_VALUE;

static void record(const char* line)
{
    DWORD done;

    if (g_record == INVALID_HANDLE_VALUE) {
        g_record = CreateFileA("camera-record.csv", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_record == INVALID_HANDLE_VALUE)
            return;
        record("kind,ms,mode,ours,air,bike x,bike y,bike z,eye x,eye y,eye z,dir x,dir y,dir z,fov,aspect,dt,"
               "fwd88 x,fwd88 y,fwd88 z,body up x,body up y,body up z,body z x,body z y,body z z\r\n");
    }
    WriteFile(g_record, line, lstrlenA(line), &done, NULL);
}

static int hundredths(float x)
{
    return (int) (x * 100);
}

static void record_update(BYTE* camera, BYTE* bike, int ours, float dt)
{
    char line[384];
    const float* p = bike ? bike_pos(bike) : NULL;
    BYTE* body = bike ? bike_body(bike) : NULL;
    const float* f = bike ? bike_forward(bike) : NULL;
    const float zero[3] = { 0, 0, 0 };
    const float* bu = body ? frame_axis(body, 1) : zero;
    const float* bz = body ? frame_axis(body, 2) : zero;
    int n;
    const float* e = cam_eye(camera);
    const float* d = cam_dir(camera);

    if (!dev_settings.trace_camera)
        return;
    n = wsprintfA(line, "C,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", timeGetTime(), cam_mode(camera), ours,
                  bike ? bike_air(bike) : 0, p ? hundredths(p[0]) : 0, p ? hundredths(p[1]) : 0, p ? hundredths(p[2]) : 0,
                  hundredths(e[0]), hundredths(e[1]), hundredths(e[2]), hundredths(d[0]), hundredths(d[1]), hundredths(d[2]),
                  hundredths(FIELD(camera, 0x16C, float)), hundredths(cam_aspect(camera)), hundredths(dt * 1000));
    wsprintfA(line + n, ",%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", f ? hundredths(f[0]) : 0, f ? hundredths(f[1]) : 0,
              f ? hundredths(f[2]) : 0, hundredths(bu[0]), hundredths(bu[1]), hundredths(bu[2]), hundredths(bz[0]),
              hundredths(bz[1]), hundredths(bz[2]));
    record(line);
}

void camera_record_transform(int type, const float* m)
{
    char line[256];

    if (!dev_settings.trace_camera || (type != 2 && type != 3))     /* VIEW, PROJECTION */
        return;
    /* A view: its rows' z (the look) and the translation; a projection: its x and y scales. */
    wsprintfA(line, "T,%lu,%d,%d,%d,%d,%d,%d,%d,%d,%d\r\n", timeGetTime(), type,
              hundredths(m[2]), hundredths(m[6]), hundredths(m[10]), hundredths(m[12]), hundredths(m[13]),
              hundredths(m[14]), hundredths(m[0]), hundredths(m[5]));
    record(line);
}

/* The rider's model, every frame in it by name and depth, once: what there is to hide. */
static void rider_frames(BYTE* bike)
{
    static BOOL told;
    BYTE* rider = bike_rider(bike);
    BYTE* stack[64];
    int depth[64], n = 0;

    if (told || !dev_settings.trace_camera || !rider || !rider_frame(rider))
        return;
    told = TRUE;
    stack[n] = rider_frame(rider);
    depth[n++] = 0;
    while (n > 0) {
        BYTE* f = stack[--n];
        int d = depth[n];
        BYTE* child;

        char indent[2 * 64 + 1];
        int i;

        for (i = 0; i < d * 2 && i < 2 * 64; i++)       /* wsprintf has no %*s */
            indent[i] = ' ';
        indent[i] = 0;
        log_line("camera: rider frame %s%s%s", indent, (const char*) f + 0x38, frame_hidden(f) ? " (hidden)" : "");
        for (child = FIELD(f, 0x144, BYTE*); child && n < 64; child = FIELD(child, 0x140, BYTE*)) {
            stack[n] = child;
            depth[n++] = d + 1;
        }
    }
}

/* ---- Each frame ---- */

void camera_after_update(BYTE* camera, float dt)
{
    BYTE* event = cam_event(camera);
    BYTE* bike = cam_bike(camera);
    int mode = cam_mode(camera);
    BOOL begin;

    if (dev_settings.menus_modern)
        aspect_true(camera);
    if (!dev_settings.menus_modern || (mode != CHASE && mode != RIDER) || !bike || !cam_on_bike(camera)
        || cam_crashed(camera) || (event && FIELD(event, 0x18E, BYTE)) || dt <= 0) {
        head_show(bike);
        if (FIELD(camera, 0x1BC, float) == NEAR_PLANE)
            FIELD(camera, 0x1BC, float) = 1;
        cam.camera = NULL;              /* the game's camera: ours begins anew after */
        record_update(camera, bike, 0, dt);
        return;
    }
    if (mode != RIDER) {
        head_show(bike);
        FIELD(camera, 0x1BC, float) = 1;    /* the game's near plane, where the depth is fine */
    }
    begin = camera != cam.camera || bike != cam.bike || mode != cam.mode;
    cam.camera = camera;
    cam.bike = bike;
    cam.mode = mode;
    if (dt > 0.1f)
        dt = 0.1f;
    if (mode == CHASE) {
        chase(camera, bike, dt, begin);
    } else {
        rider(camera, bike, dt, begin);
        rider_frames(bike);
    }
    record_update(camera, bike, 1, dt);
}
