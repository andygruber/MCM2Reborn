/*
    OpenMCM2 - the race's rider camera (KrustyBikeCamera), its per-frame update (vtable slot 10):
    the game's own, then in the dev build MCM2:Reborn's chase and rider cameras in its place
    (code/dev/camera.c). Also a camera's near and far planes, and a soul mesh's draw, which the
    dev build's rider camera needs a say in.
*/

#include "runtime.h"
#include "game.h"

int  __fastcall game_orig_vf_10_498DD0(BYTE* self, void* edx_unused, float dt);
void __fastcall game_orig_FUN_00441ed0(BYTE* self);

#define NEAR_LEAST  (*(const float*) 0x00556024)    /* 1.0 */
#define FAR_MOST    (*(const float*) 0x00556028)

#ifdef OPENMCM2_DEV
void  camera_after_update(BYTE* camera, float dt);
void  chase_heading_rate(float dt);
float camera_near_plane(BYTE* camera);
BOOL  rider_head_collapse(BYTE* mesh);
void  rider_head_restore(BYTE* mesh);
#endif

/*
    A camera's near and far planes, the near no nearer than NEAR_LEAST, the far no further than
    FAR_MOST. The world pass sets them every frame; in the dev build the rider camera's own near
    plane holds while it is the view, for the bars and hands an inch or two from the eye.
*/
PORT(0x0042F880)
void __fastcall camera_set_planes(BYTE* self, void* edx_unused, float near_plane, float far_plane)
{
    (void) edx_unused;
    FIELD(self, 0x1BC, float) = NEAR_LEAST <= near_plane ? near_plane : NEAR_LEAST;
    FIELD(self, 0x1C0, float) = FAR_MOST < far_plane ? FAR_MOST : far_plane;
#ifdef OPENMCM2_DEV
    if (camera_near_plane(self) > 0)
        FIELD(self, 0x1BC, float) = camera_near_plane(self);
#endif
}

/* A soul mesh drawn: in the dev build the rider's, in the rider camera, without its head. */
PORT(0x00441ED0)
void __fastcall soul_mesh_draw(BYTE* self)
{
#ifdef OPENMCM2_DEV
    if (rider_head_collapse(self)) {
        game_orig_FUN_00441ed0(self);
        rider_head_restore(self);
        return;
    }
#endif
    game_orig_FUN_00441ed0(self);
}

PORT(0x00498DD0)
int __fastcall bike_camera_update(BYTE* self, void* edx_unused, float dt)
{
    int result;

#ifdef OPENMCM2_DEV
    chase_heading_rate(dt);
#endif
    result = game_orig_vf_10_498DD0(self, edx_unused, dt);

#ifdef OPENMCM2_DEV
    camera_after_update(self, dt);
#endif
    return result;
}
