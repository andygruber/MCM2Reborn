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

#define CHASE       0
#define RIDER       3

/* KrustyBikeCamera. */
#define cam_mode(c)     FIELD(c, 0x244, int)
#define cam_event(c)    FIELD(c, 0x3B8, BYTE*)
#define cam_crashed(c)  FIELD(c, 0x276, BYTE)
#define cam_on_bike(c)  FIELD(c, 0x390, int)
#define cam_bike(c)     FIELD(c, 0x3B4, BYTE*)
#define cam_terrain(c)  FIELD(c, 0x240, BYTE*)
#define cam_roll(c)     FIELD(c, 0x194, float)
#define cam_heading(c)  ((float*) ((c) + 0x29C))    /* the game's own, smoothed */

/* KrustyBike. */
#define bike_pos(b)     ((float*) ((b) + 0x0C))
#define bike_vel(b)     ((float*) ((b) + 0x64))
#define bike_air(b)     FIELD(b, 0x108, BYTE)
#define bike_forward(b) ((float*) ((b) + 0x88))     /* the way it points (the game's heading when slow) */

#define FT_PER_S_TO_MPH 0.68181825f

/* Camera: eye, direction, up, roll, fov (NULL keeps one); the fov; its matrices made anew. */
void __fastcall game_FUN_0042f8d0(BYTE* camera, void* edx_unused, const float* eye, const float* dir,
                                  const float* up, const float* roll, const float* fov);
void __fastcall game_FUN_0042f850(BYTE* camera, void* edx_unused, float fov);
/* Terrain: the ground's height under pos's x and z, into its y. */
void __fastcall game_FUN_0050d3b0(BYTE* terrain, void* edx_unused, float* pos, float* normal, int a,
                                  char* material);
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
    float back, back_speed;         /* the chase distance */
    float roll, roll_speed;
    float head[3], head_speed[3];   /* the helmet relative to the bike */
    float fov, fov_speed;
} cam;

static float ground_under(BYTE* camera, float x, float z, float fallback)
{
    float pos[3] = { x, fallback, z }, normal[3];
    char material[64];

    if (!cam_terrain(camera))
        return fallback;
    game_FUN_0050d3b0(cam_terrain(camera), NULL, pos, normal, 0, material);
    return pos[1];
}

static float flat_speed(const float* v)
{
    return square_root(v[0] * v[0] + v[2] * v[2]);
}

/* ---- Chase ---- */

static void chase(BYTE* camera, BYTE* bike, float dt, BOOL begin)
{
    const float* p = bike_pos(bike);
    const float* v = bike_vel(bike);
    float speed = flat_speed(v), mph = speed * FT_PER_S_TO_MPH;
    float target[3] = { p[0], p[1] + 3.2f, p[2] };
    float heading = speed > 10 ? arc_tangent2(v[0], v[2]) : arc_tangent2(cam_heading(camera)[0], cam_heading(camera)[2]);
    float ahead = ground_under(camera, p[0] + sine(heading) * 10, p[2] + cosine(heading) * 10, p[1]);
    float behind = ground_under(camera, p[0] - sine(heading) * 10, p[2] - cosine(heading) * 10, p[1]);
    float slope = arc_tangent2(ahead - behind, 20);
    float back = 13 + (mph < 60 ? mph : 60) * 0.06f;
    float eye[3], look[3], dir[3], up[3] = { 0, 1, 0 }, length, floor, roll = 0, fov;

    if (begin) {
        cam.yaw = heading;
        cam.pitch = slope;
        cam.height = target[1] + 4.5f;
        cam.back = back;
        cam.fov = 75;
        cam.yaw_speed = cam.pitch_speed = cam.height_speed = cam.back_speed = cam.fov_speed = 0;
    }
    spring_angle(&cam.yaw, &cam.yaw_speed, heading, bike_air(bike) ? 1.8f : 3.2f, dt);
    spring(&cam.pitch, &cam.pitch_speed, bike_air(bike) ? cam.pitch : slope, 2.0f, dt);
    spring(&cam.height, &cam.height_speed, target[1] + 4.5f - cam.pitch * 6, bike_air(bike) ? 2.5f : 6.0f, dt);
    spring(&cam.back, &cam.back_speed, back, 3.0f, dt);

    eye[0] = target[0] - sine(cam.yaw) * cam.back;
    eye[2] = target[2] - cosine(cam.yaw) * cam.back;
    eye[1] = cam.height;
    floor = ground_under(camera, eye[0], eye[2], eye[1] - 3) + 3.0f;
    if (eye[1] < floor) {               /* never under the ground: lifted, and the spring with it */
        eye[1] = floor;
        cam.height = floor;
        cam.height_speed = 0;
    }
    look[0] = target[0] + sine(cam.yaw) * 6;
    look[2] = target[2] + cosine(cam.yaw) * 6;
    look[1] = target[1] + 0.5f + sine(cam.pitch) * 6;
    dir[0] = look[0] - eye[0];
    dir[1] = look[1] - eye[1];
    dir[2] = look[2] - eye[2];
    length = square_root(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (length <= 0)
        return;
    dir[0] /= length;
    dir[1] /= length;
    dir[2] /= length;

    spring(&cam.fov, &cam.fov_speed, 75 + (mph < 60 ? mph : 60) / 60 * 10, 2.0f, dt);
    fov = cam.fov;
    game_FUN_0042f8d0(camera, NULL, eye, dir, up, &roll, NULL);
    game_FUN_0042f850(camera, NULL, fov);
    cam_rebuild(camera);
}

/* ---- Rider ---- */

static void rider(BYTE* camera, BYTE* bike, float dt, BOOL begin)
{
    const float* p = bike_pos(bike);
    const float* v = bike_vel(bike);
    const float* bike_dir = bike_forward(bike);
    float raw[3];
    float speed = flat_speed(v), mph = speed * FT_PER_S_TO_MPH;
    float yaw_bike = arc_tangent2(bike_dir[0], bike_dir[2]);
    float pitch_bike = arc_tangent2(bike_dir[1], flat_speed(bike_dir));
    float yaw = yaw_bike, pitch, roll, fov, eye[3], dir[3], up[3] = { 0, 1, 0 };
    int k;

    cam_head(camera, raw);                      /* the helmet, unsmoothed */
    if (speed > 10)                             /* a little towards where it is going */
        yaw = yaw_bike + angle_wrap(arc_tangent2(v[0], v[2]) - yaw_bike) * 0.25f;
    pitch = pitch_bike * 0.4f - 6 * PI / 180;   /* some of the bike's, a little down at the bars */
    roll = cam_roll(camera) * 0.35f;            /* some of the lean */

    if (begin) {
        for (k = 0; k < 3; k++) {
            cam.head[k] = raw[k] - p[k];
            cam.head_speed[k] = 0;
        }
        cam.yaw = yaw;
        cam.pitch = pitch;
        cam.roll = roll;
        cam.fov = 92;
        cam.yaw_speed = cam.pitch_speed = cam.roll_speed = cam.fov_speed = 0;
    }
    for (k = 0; k < 3; k++)                     /* the neck: the head's jolts softened */
        spring(&cam.head[k], &cam.head_speed[k], raw[k] - p[k], 28, dt);
    spring_angle(&cam.yaw, &cam.yaw_speed, yaw, 12, dt);
    spring(&cam.pitch, &cam.pitch_speed, pitch, 9, dt);
    spring(&cam.roll, &cam.roll_speed, roll, 8, dt);

    dir[0] = sine(cam.yaw) * cosine(cam.pitch);
    dir[1] = sine(cam.pitch);
    dir[2] = cosine(cam.yaw) * cosine(cam.pitch);
    for (k = 0; k < 3; k++)                     /* just out of the helmet, a touch above */
        eye[k] = p[k] + cam.head[k] + dir[k] * 0.45f;
    eye[1] += 0.2f;

    spring(&cam.fov, &cam.fov_speed, 92 + (mph < 60 ? mph : 60) / 60 * 8, 2.0f, dt);
    fov = cam.fov;
    game_FUN_0042f8d0(camera, NULL, eye, dir, up, &cam.roll, NULL);
    game_FUN_0042f850(camera, NULL, fov);
    cam_rebuild(camera);
    cam_show_rider(camera);
}

/* ---- Each frame ---- */

void camera_after_update(BYTE* camera, float dt)
{
    BYTE* event = cam_event(camera);
    BYTE* bike = cam_bike(camera);
    int mode = cam_mode(camera);
    BOOL begin;

    if (!dev_settings.menus_modern || (mode != CHASE && mode != RIDER) || !bike || !cam_on_bike(camera)
        || cam_crashed(camera) || (event && FIELD(event, 0x18E, BYTE)) || dt <= 0) {
        cam.camera = NULL;              /* the game's camera: ours begins anew after */
        return;
    }
    begin = camera != cam.camera || bike != cam.bike || mode != cam.mode;
    cam.camera = camera;
    cam.bike = bike;
    cam.mode = mode;
    if (dt > 0.1f)
        dt = 0.1f;
    if (mode == CHASE)
        chase(camera, bike, dt, begin);
    else
        rider(camera, bike, dt, begin);
}
