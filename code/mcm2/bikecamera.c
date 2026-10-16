/*
    OpenMCM2 - the race's rider camera (KrustyBikeCamera), its per-frame update (vtable slot 10):
    the game's own, then in the dev build MCM2:Reborn's chase and rider cameras in its place
    (code/dev/camera.c).
*/

#include "runtime.h"

int __fastcall game_orig_vf_10_498DD0(BYTE* self, void* edx_unused, float dt);

#ifdef OPENMCM2_DEV
void camera_after_update(BYTE* camera, float dt);
#endif

PORT(0x00498DD0)
int __fastcall bike_camera_update(BYTE* self, void* edx_unused, float dt)
{
    int result = game_orig_vf_10_498DD0(self, edx_unused, dt);

#ifdef OPENMCM2_DEV
    camera_after_update(self, dt);
#endif
    return result;
}
