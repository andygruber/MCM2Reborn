/*
    OpenMCM2 - LightEmitter, ported from the game (LightEmitter.cpp in the original).

    A LightEmitter is a GameObject holding a Direct3D light (D3DLIGHT7, from 0x30), with an
    optional sphere placed where it is and a lens flare given its colour and place. Each
    function here replaces the game's at the address in its PORT line, and was checked against
    the original's instructions.
*/

#include "game.h"

/* ---- Start-up ---- */

/* The file's copies of the header's unit vectors, set up by the runtime before main. */
#define light_zero (*(vector3*) 0x00682600)
#define light_x    (*(vector3*) 0x00682610)
#define light_y    (*(vector3*) 0x00682620)
#define light_z    (*(vector3*) 0x006825F0)

/* Out of line, so the entries below stay jumps to them as in the original. */
__declspec(noinline) void __cdecl light_init_zero(void);
__declspec(noinline) void __cdecl light_init_x(void);
__declspec(noinline) void __cdecl light_init_y(void);
__declspec(noinline) void __cdecl light_init_z(void);

PORT(0x0049F2E0)
void __cdecl light_init_zero_entry(void) { light_init_zero(); }
PORT(0x0049F2F0)
void __cdecl light_init_zero(void) { set_vector(&light_zero, 0.0f, 0.0f, 0.0f); }
PORT(0x0049F330)
void __cdecl light_init_x_entry(void) { light_init_x(); }
PORT(0x0049F340)
void __cdecl light_init_x(void) { set_vector(&light_x, 1.0f, 0.0f, 0.0f); }
PORT(0x0049F380)
void __cdecl light_init_y_entry(void) { light_init_y(); }
PORT(0x0049F390)
void __cdecl light_init_y(void) { set_vector(&light_y, 0.0f, 1.0f, 0.0f); }
PORT(0x0049F3D0)
void __cdecl light_init_z_entry(void) { light_init_z(); }
PORT(0x0049F3E0)
void __cdecl light_init_z(void) { set_vector(&light_z, 0.0f, 0.0f, 1.0f); }

/* ---- The light ---- */

typedef struct colour4 {
    float r, g, b, a;
} colour4;

typedef struct light_emitter {
    game_object base;
    DWORD   unknown_2c;
    int     type;
    colour4 diffuse;
    colour4 specular;
    colour4 ambient;
    vector3 position;
    vector3 direction;
    float   range;
    float   falloff;
    float   attenuation[3];
    float   theta;
    float   phi;
    BYTE*   sphere;         /* placed at the light's position */
    BYTE*   flare;          /* given its colour and position */
    int     unknown_a0;
    BYTE*   owner;          /* told of every change */
    DWORD   colour;         /* compared with a new colour, but never stored */
} light_emitter;

LAYOUT(light_emitter, type, 0x30);
LAYOUT(light_emitter, position, 0x64);
LAYOUT(light_emitter, range, 0x7C);
LAYOUT(light_emitter, sphere, 0x98);
LAYOUT(light_emitter, colour, 0xA8);
_Static_assert(sizeof(light_emitter) == 0xAC, "LightEmitter is 172 bytes");

#define LIGHT_VTABLE      ((void**) 0x0055A100)
#define LIGHT_DIRECTIONAL 3

/*
    The farthest range, sqrt(FLT_MAX) as the game works it out each time. That falls strictly
    between the float stored (0x5F7FFFFF) and 2^64, so this double compares with any float
    just as the game's does.
*/
#define RANGE_LIMIT 1.844674352395373e19
#define RANGE_MOST  18446742974197923840.0f
#define COLOUR_STEP (1.0f / 255)              /* 0x3B808081 */

void __fastcall game_FUN_00501470(BYTE* sphere, void* edx_unused, vector3 position);
void __fastcall game_FUN_0049fa40(BYTE* owner);

/* The x87 compares as the game tests them: C3 for equal, C0 for less; unordered is both. */
static BOOL x87_equal(double a, double b)
{
    return !(a < b) && !(a > b);
}

static BOOL x87_less(double a, double b)
{
    return !(a >= b);
}

static BOOL vector_same(const vector3* a, const vector3* b)
{
    return x87_equal(a->x, b->x) && x87_equal(a->y, b->y) && x87_equal(a->z, b->z);
}

static void light_changed(light_emitter* self)
{
    if (self->owner)
        game_FUN_0049fa40(self->owner);
}

/* The flare's own setters, which the compiler placed here. */
PORT(0x0049F420)
void __fastcall flare_set_colour(BYTE* self, void* edx_unused, DWORD colour)
{
    (void) edx_unused;
    FIELD(self, 0xD0, DWORD) = colour & 0xFFFFFF;
}

PORT(0x0049F440)
void __fastcall flare_set_position(BYTE* self, void* edx_unused, const vector3* position)
{
    (void) edx_unused;
    FIELD(self, 0xB8, vector3) = *position;
}

/* A white directional light along x, as far as it goes, with no sphere, flare or owner. */
PORT(0x0049F460)
light_emitter* __fastcall light_construct(light_emitter* self, void* edx_unused, DWORD flag)
{
    static const colour4 white = { 1.0f, 1.0f, 1.0f, 0.0f };

    (void) edx_unused;
    object_construct(self, NULL, flag);
    self->base.vtable = LIGHT_VTABLE;
    self->unknown_2c = 0;
    self->type = LIGHT_DIRECTIONAL;
    self->diffuse = white;
    self->specular = white;
    self->ambient = white;
    set_vector(&self->position, 0.0f, 0.0f, 0.0f);
    set_vector(&self->direction, 1.0f, 0.0f, 0.0f);
    self->range = RANGE_MOST;
    self->falloff = 1.0f;
    self->attenuation[0] = 1.0f;
    self->attenuation[1] = 0.0f;
    self->attenuation[2] = 0.0f;
    self->theta = 0.785398185f;                     /* pi/4 */
    self->phi = 1.57079637f;                        /* pi/2 */
    self->sphere = NULL;
    self->flare = NULL;
    self->unknown_a0 = -1;
    self->owner = NULL;
    self->colour = 0;
    return self;
}

__declspec(noinline) void __fastcall light_destruct(light_emitter* self);

PORT(0x0049F550)
light_emitter* __fastcall light_delete(light_emitter* self, void* edx_unused, DWORD flags)
{
    (void) edx_unused;
    light_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

PORT(0x0049F570)
void __fastcall light_destruct(light_emitter* self)
{
    self->base.vtable = LIGHT_VTABLE;
    object_destruct(self);
}

/* The diffuse colour as ARGB, each part times 255 and cut, as __ftol does. */
PORT(0x0049F580)
DWORD __fastcall light_colour(light_emitter* self)
{
    DWORD colour = (DWORD) (int) (self->diffuse.a * 255.0f) << 8;

    colour = (colour | (DWORD) (int) (self->diffuse.r * 255.0f)) << 8;
    colour = (colour | (DWORD) (int) (self->diffuse.g * 255.0f)) << 8;
    return colour | (DWORD) (int) (self->diffuse.b * 255.0f);
}

/* Diffuse, specular and ambient all become the colour, alpha 0, and the flare is told. */
PORT(0x0049F5D0)
void __fastcall light_set_colour(light_emitter* self, void* edx_unused, DWORD argb)
{
    float red, green, blue;

    (void) edx_unused;
    if (argb == self->colour)
        return;
    red = (float) (int) (argb >> 16 & 0xFF) * COLOUR_STEP;
    green = (float) (int) (argb >> 8 & 0xFF) * COLOUR_STEP;
    blue = (float) (int) (argb & 0xFF) * COLOUR_STEP;
    self->diffuse.r = self->specular.r = self->ambient.r = red;
    self->diffuse.g = self->specular.g = self->ambient.g = green;
    self->diffuse.b = self->specular.b = self->ambient.b = blue;
    self->diffuse.a = self->specular.a = self->ambient.a = 0.0f;
    if (self->flare)
        flare_set_colour(self->flare, NULL, argb);
    light_changed(self);
}

/* No range (0), or one past the limit, is the farthest. */
PORT(0x0049F6A0)
void __fastcall light_set_range(light_emitter* self, void* edx_unused, float range)
{
    (void) edx_unused;
    if (x87_equal(range, 0.0f) || x87_less(RANGE_LIMIT, range))
        range = RANGE_MOST;
    if (x87_equal(range, self->range))
        return;
    self->range = range;
    light_changed(self);
}

PORT(0x0049F700)
void __fastcall light_set_position(light_emitter* self, void* edx_unused, const vector3* position)
{
    (void) edx_unused;
    if (vector_same(position, &self->position))
        return;
    if (self->sphere)
        game_FUN_00501470(self->sphere, NULL, *position);
    self->position = *position;
    if (self->flare)
        flare_set_position(self->flare, NULL, position);
    light_changed(self);
}

PORT(0x0049F790)
void __fastcall light_set_direction(light_emitter* self, void* edx_unused,
                                    const vector3* direction)
{
    (void) edx_unused;
    if (vector_same(direction, &self->direction))
        return;
    self->direction = *direction;
    light_changed(self);
}
