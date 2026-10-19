/* The legacy controller picker selects one startup-only DirectInput joystick.
   Reborn reads modern controllers separately, before menus and throughout play. */
#include "runtime.h"
#include "game.h"
#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

int __cdecl game_orig_FUN_004d1d60(DWORD use_last);

PORT(0x004D1D60)
int __cdecl controller_pick(DWORD use_last)
{
#ifdef OPENMCM2_DEV
    BYTE* inputs = FIELD(game_main, 0x14, BYTE*);
    (void) use_last;
    if (inputs) {
        FIELD(inputs, 8, int) = -1;
        FIELD(inputs, 0xC, BYTE*) = NULL;
    }
    pad_connected();
    return -1;                         /* keyboard, plus the modern pad reader */
#else
    return game_orig_FUN_004d1d60(use_last);
#endif
}
