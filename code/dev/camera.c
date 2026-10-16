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
} cam;

static float flat_speed(const float* v)
{
    return square_root(v[0] * v[0] + v[2] * v[2]);
}

/* ---- Chase ---- */

/*
    As MX vs ATV Unleashed's: well back and high, the rider small and a little below the middle
    with the track ahead in view; straight behind the line ridden, level, barely swinging; the
    view's width fixed (widening with speed, it warped on every jump). The heading is where the
    bike is going, on one gentle spring; the height follows the bike's arc.
*/
#define CHASE_BACK      30.0f           /* feet behind the bike */
#define CHASE_UP        11.0f           /* ...and above it */
#define CHASE_AHEAD     25.0f           /* the camera looks this far past the bike */
#define CHASE_FOV       80.0f

static void chase(BYTE* camera, BYTE* bike, float dt, BOOL begin)
{
    const float* p = bike_pos(bike);
    const float* v = bike_vel(bike);
    float speed = flat_speed(v);
    float heading = speed > 8 ? arc_tangent2(v[0], v[2]) : arc_tangent2(bike_forward(bike)[0], bike_forward(bike)[2]);
    float eye[3], look[3], dir[3], up[3] = { 0, 1, 0 }, length, roll = 0;

    if (begin) {
        cam.yaw = heading;
        cam.height = p[1] + CHASE_UP;
        cam.yaw_speed = cam.height_speed = 0;
    }
    spring_angle(&cam.yaw, &cam.yaw_speed, heading, 4.0f, dt);
    spring(&cam.height, &cam.height_speed, p[1] + CHASE_UP, 6.0f, dt);

    eye[0] = p[0] - sine(cam.yaw) * CHASE_BACK;
    eye[2] = p[2] - cosine(cam.yaw) * CHASE_BACK;
    eye[1] = cam.height > p[1] + 4 ? cam.height : p[1] + 4;    /* never below the rider's head */
    look[0] = p[0] + sine(cam.yaw) * CHASE_AHEAD;
    look[2] = p[2] + cosine(cam.yaw) * CHASE_AHEAD;
    look[1] = p[1] + 2;
    dir[0] = look[0] - eye[0];
    dir[1] = look[1] - eye[1];
    dir[2] = look[2] - eye[2];
    length = square_root(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    if (length <= 0)
        return;
    dir[0] /= length;
    dir[1] /= length;
    dir[2] /= length;
    game_FUN_0042f8d0(camera, NULL, eye, dir, up, &roll, NULL);
    game_FUN_0042f850(camera, NULL, CHASE_FOV);
    cam_rebuild(camera);
}

/* ---- Rider ---- */

/*
    From just in front of the rider's face (from behind it, the helmet showed), looking the way
    the bike points - so through whips the bars and the hands stay low in the view - a little
    towards where it is going and a little down, at the bars. The head is held as a neck holds it:
    its jolts softened, the horizon with only a quarter of the bike's lean.
*/
#define RIDER_FOV       100.0f
#define RIDER_FORWARD   0.8f            /* feet in front of the head */
#define RIDER_UP        0.35f

static void rider(BYTE* camera, BYTE* bike, float dt, BOOL begin)
{
    const float* p = bike_pos(bike);
    const float* v = bike_vel(bike);
    const float* f = bike_forward(bike);
    float speed = flat_speed(v);
    float yaw_bike = arc_tangent2(f[0], f[2]);
    float yaw = yaw_bike, pitch, roll, eye[3], dir[3], up[3] = { 0, 1, 0 }, head[3];
    int k;

    cam_head(camera, head);
    if (speed > 10)
        yaw = yaw_bike + angle_wrap(arc_tangent2(v[0], v[2]) - yaw_bike) * 0.3f;
    pitch = arc_tangent2(f[1], flat_speed(f)) * 0.3f - 10 * PI / 180;
    roll = cam_roll(camera) * 0.25f;

    if (begin) {
        for (k = 0; k < 3; k++) {
            cam.head[k] = head[k] - p[k];
            cam.head_speed[k] = 0;
        }
        cam.yaw = yaw;
        cam.pitch = pitch;
        cam.roll = roll;
        cam.yaw_speed = cam.pitch_speed = cam.roll_speed = 0;
    }
    for (k = 0; k < 3; k++)                     /* the neck: the head's jolts softened */
        spring(&cam.head[k], &cam.head_speed[k], head[k] - p[k], 20, dt);
    spring_angle(&cam.yaw, &cam.yaw_speed, yaw, 10, dt);
    spring(&cam.pitch, &cam.pitch_speed, pitch, 8, dt);
    spring(&cam.roll, &cam.roll_speed, roll, 6, dt);

    dir[0] = sine(cam.yaw) * cosine(cam.pitch);
    dir[1] = sine(cam.pitch);
    dir[2] = cosine(cam.yaw) * cosine(cam.pitch);
    eye[0] = p[0] + cam.head[0] + sine(cam.yaw) * RIDER_FORWARD;
    eye[1] = p[1] + cam.head[1] + RIDER_UP;
    eye[2] = p[2] + cam.head[2] + cosine(cam.yaw) * RIDER_FORWARD;
    game_FUN_0042f8d0(camera, NULL, eye, dir, up, &cam.roll, NULL);
    game_FUN_0042f850(camera, NULL, RIDER_FOV);
    cam_rebuild(camera);
    cam_show_rider(camera);
}

/* What the camera reads, twice a second ([trace] camera=1), to see which is wrong. */
static void trace(BYTE* camera, BYTE* bike, float dt)
{
    static DWORD last;
    const float* p = bike_pos(bike);
    const float* v = bike_vel(bike);
    const float* f = bike_forward(bike);
    const float* e = cam_eye(camera);
    const float* d = cam_dir(camera);
    float head[3];

    if (!dev_settings.trace_camera || timeGetTime() - last < 500)
        return;
    last = timeGetTime();
    cam_head(camera, head);
    dev_trace("camera", "mode %d dt %d ms air %d | bike %d %d %d | vel %d %d %d | fwd88 %d %d %d | head %d %d %d"
              " | game eye %d %d %d dir %d %d %d roll %d", cam_mode(camera), (int) (dt * 1000), bike_air(bike),
              (int) p[0], (int) p[1], (int) p[2], (int) v[0], (int) v[1], (int) v[2],
              (int) (f[0] * 100), (int) (f[1] * 100), (int) (f[2] * 100),
              (int) head[0], (int) head[1], (int) head[2], (int) e[0], (int) e[1], (int) e[2],
              (int) (d[0] * 100), (int) (d[1] * 100), (int) (d[2] * 100), (int) (cam_roll(camera) * 100));
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
    trace(camera, bike, dt);
    if (mode == CHASE)
        chase(camera, bike, dt, begin);
    else
        rider(camera, bike, dt, begin);
}
