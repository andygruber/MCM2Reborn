/* BikeRace::LoadBikes: discard the previous event's AI frame state before any
   new bikes can reuse its addresses. All five stack arguments retain their bits. */
#include "runtime.h"

int __fastcall game_orig_FUN_0041a770(BYTE* self, void* edx_unused,
                                    DWORD a, DWORD b, DWORD c, DWORD d, DWORD e);

#ifdef OPENMCM2_DEV
void ai_race_reset(void);
#endif

PORT(0x0041A770)
int __fastcall race_load_bikes(BYTE* self, void* edx_unused,
                              DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{
#ifdef OPENMCM2_DEV
    ai_race_reset();
#endif
    return game_orig_FUN_0041a770(self, edx_unused, a, b, c, d, e);
}
