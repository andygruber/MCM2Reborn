/*
    OpenMCM2 dev build - MCM2:Reborn's rider cameras, after MX vs ATV Unleashed's chase camera and
    the first-person views of MX Bikes and Open Unleashed: only where the camera is changes,
    never how the bike rides.

    The game's own camera update runs first (KrustyBikeCamera, 0x00498DD0); then, in its Rider Cam
    (mode 3), this places the camera itself. Its other modes stay the game's: the chase camera
    turning by time and not by frames (chase_heading_rate), the look-back, the crash and jump
    cameras, the intro flyby and the replays.

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
#define NEAR_PLANE      0.1f            /* feet: the game's 1, set again every frame, cut the bars, hands and legs */

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
    float head[3], head_speed[3];   /* the helmet relative to the bike */
} cam;

/*
    In the rider's view the rider's arms and hands are drawn on the bars, and the legs when the
    view takes them in, but not the head, helmet and torso, which the view is in. The rider is one
    mesh whose vertices follow its frames (PELVIS with the legs under it; LWRTORSO, UPRTORSO with
    the arms and the HEAD under it), and no frame holds a mesh of its own, so hiding one did
    nothing: for the rider's draw alone these frames are made to a point - their axes zeroed, their
    places kept - and put back straight after (bikecamera.c, soul_mesh_draw). The frames under them
    keep their own axes, so the arms are drawn whole.
*/
#define frame_child(f)      FIELD(f, 0x140, BYTE*)
#define frame_sibling(f)    FIELD(f, 0x144, BYTE*)
#define mesh_dirty(m)       FIELD(m, 0x18C, int)

static const char* const unseen_names[] = { "HEAD", "UPRTORSO", "LWRTORSO" };
#define UNSEEN_COUNT ((int) (sizeof unseen_names / sizeof unseen_names[0]))

static BYTE* g_unseen_model;            /* the rider's model, while its view is the rider's */
static BYTE* g_unseen[UNSEEN_COUNT];
static float g_unseen_axes[UNSEEN_COUNT][12];
static BYTE* g_near_camera;             /* the camera whose near plane is the rider view's */

static void head_hide(BYTE* bike)
{
    BYTE* rider = bike_rider(bike);
    BYTE* model = rider ? rider_frame(rider) : NULL;
    int i;

    g_unseen_model = model;
    for (i = 0; i < UNSEEN_COUNT; i++)
        g_unseen[i] = model ? game_FUN_00502970(model, NULL, unseen_names[i]) : NULL;
}

static void head_show(BYTE* bike)
{
    (void) bike;
    g_unseen_model = NULL;
}

BOOL rider_head_collapse(BYTE* mesh)
{
    int i, k;

    if (!g_unseen_model || mesh != g_unseen_model)
        return FALSE;
    for (i = 0; i < UNSEEN_COUNT; i++) {
        float* axes = g_unseen[i] ? (float*) (g_unseen[i] + 0xF8) : NULL;

        for (k = 0; axes && k < 12; k++) {
            g_unseen_axes[i][k] = axes[k];
            axes[k] = 0;
        }
    }
    mesh_dirty(mesh) = 1;
    return TRUE;
}

void rider_head_restore(BYTE* mesh)
{
    int i, k;

    for (i = 0; i < UNSEEN_COUNT; i++) {
        float* axes = g_unseen[i] ? (float*) (g_unseen[i] + 0xF8) : NULL;

        for (k = 0; axes && k < 12; k++)
            axes[k] = g_unseen_axes[i][k];
    }
    mesh_dirty(mesh) = 1;
}

float camera_near_plane(BYTE* camera)
{
    return camera && camera == g_near_camera ? NEAR_PLANE : 0;
}

/*
    Each frame the camera's aspect is the picture's own, its viewport's width over height (it was
    so already in the game's, as the log showed; kept as a guard for other screens).
*/
static void aspect_true(BYTE* camera)
{
    int w = cam_view_w(camera), h = cam_view_h(camera);
    float aspect;

    if (w <= 0 || h <= 0)
        return;
    aspect = (float) w / h;
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
    g_near_camera = camera;
    FIELD(camera, 0x1BC, float) = NEAR_PLANE;
    cam_rebuild(camera);
    cam_show_rider(camera);
    head_hide(bike);
}

/* ---- Chase ---- */

/*
    The chase camera is the game's own, but for how fast it turns. Each frame VehicleCamera's
    heading (slot 41, 0x005316A0) went 0.82 of the old way and 0.18 of the new, with no time in
    it: at the 30 frames a second the game was made for the heading caught up in about a sixth of
    a second, at 144 in a thirtieth, and every turn snapped the camera round. Before each update
    the two weights (used by that function alone) are made the ones that turn it as far in this
    frame's time as 30 a second would: the old's 0.82 to the power of 30 x dt.
*/
#define HEADING_KEPT    ((float*) 0x0055E0EC)   /* 0.82 */
#define HEADING_TAKEN   ((float*) 0x0055E0E8)   /* 0.18 */
#define DESIGN_RATE     30.0f
#define LN_KEPT         0.19845094f             /* -ln 0.82 */

void chase_heading_rate(float dt)
{
    float w[2];

    if (dt <= 0)
        return;
    if (dt > 0.1f)
        dt = 0.1f;
    w[0] = exp_minus(LN_KEPT * DESIGN_RATE * dt);
    w[1] = 1 - w[0];
    if (*HEADING_KEPT != w[0])
        write_protected(HEADING_KEPT, &w[0], sizeof w[0]);
    if (*HEADING_TAKEN != w[1])
        write_protected(HEADING_TAKEN, &w[1], sizeof w[1]);
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
        for (child = frame_child(f); child && n < 64; child = frame_sibling(child)) {
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
    if (!dev_settings.menus_modern || !g_reborn.cameras || mode != RIDER || !bike || !cam_on_bike(camera)
        || cam_crashed(camera) || (event && FIELD(event, 0x18E, BYTE)) || dt <= 0) {
        head_show(bike);
        if (g_near_camera == camera)
            FIELD(camera, 0x1BC, float) = 1;
        g_near_camera = NULL;
        cam.camera = NULL;              /* the game's camera: ours begins anew after */
        record_update(camera, bike, 0, dt);
        return;
    }
    begin = camera != cam.camera || bike != cam.bike || mode != cam.mode;
    cam.camera = camera;
    cam.bike = bike;
    cam.mode = mode;
    if (dt > 0.1f)
        dt = 0.1f;
    rider(camera, bike, dt, begin);
    rider_frames(bike);
    record_update(camera, bike, 1, dt);
}
