/*
    OpenMCM2 - the PC side of the game object, ported from the game (PCGame.cpp in the original).
    Each function here replaces the game's at the address in its PORT line, and was checked
    against the original's instructions.
*/

#include "game.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

#define PCGAME_WINDOW     0x31C     /* HWND */
#define PCGAME_WINDOW_AT  0x308     /* RECT: where the window was before going full screen */
#define PCGAME_CARD       0xC       /* the video card */
#define PCGAME_DISPLAY    0x2D4     /* bit 1: full screen */
#define PCGAME_SOFTWARE   0x2D0     /* no hardware 3D */

BOOL __fastcall game_FUN_004ce3e0(BYTE* card);                          /* full screen */
BOOL __fastcall game_FUN_004ce470(BYTE* card, void* edx_unused, int x, int y, int width,
                                  int height);                          /* a window this big */

/* The card's vtable slot 2: the mode with this size and depth, set; FALSE if none. */
typedef BOOL (__fastcall* card_set_mode)(BYTE* card, void* edx_unused, int width, int height,
                                         int bits, DWORD a, DWORD b, BOOL hardware, DWORD c);

/*
    Vtable slot 32: the display brought up for the menus, at 640x480x16 full screen or in a
    window that size, the window's place remembered; then slot 33 makes the screen on it.
*/
PORT(0x004C53B0)
BOOL __fastcall pcgame_display_start(BYTE* self)
{
    BYTE* card;
    int width = 640;
    int height = 480;

#ifdef OPENMCM2_DEV
    /* The widescreen menus start in the races' mode, once the options have chosen one. */
    if (dev_settings.menus_widescreen) {
        BYTE* modes = FIELD(FIELD(self, PCGAME_CARD, BYTE*), 0x10, BYTE*);
        int chosen = FIELD(self, 0xFC4, int);

        if (modes && chosen >= 0 && chosen < FIELD(FIELD(self, PCGAME_CARD, BYTE*), 0x8, int)
            && FIELD(modes + chosen * 0x24, 0, int) >= 640) {
            width = FIELD(modes + chosen * 0x24, 0, int);
            height = FIELD(modes + chosen * 0x24, 4, int);
        }
    }
#endif
    GetWindowRect(FIELD(self, PCGAME_WINDOW, HWND), &FIELD(self, PCGAME_WINDOW_AT, RECT));
    if (FIELD(self, PCGAME_DISPLAY, BYTE) & 2) {
        if (!game_FUN_004ce3e0(FIELD(self, PCGAME_CARD, BYTE*)))
            return FALSE;
        while (ShowCursor(FALSE) >= 0)
            ;
    } else if (!game_FUN_004ce470(FIELD(self, PCGAME_CARD, BYTE*), NULL, 0, 0, width, height)) {
        return FALSE;
    }
    card = FIELD(self, PCGAME_CARD, BYTE*);
    if (!VIRTUAL(card, 0x8, card_set_mode)(card, NULL, width, height, 16, 2, 0,
                                          !FIELD(self, PCGAME_SOFTWARE, DWORD), 1))
        return FALSE;
    return VIRTUAL(self, 0x84, method_0)(self) != 0;
}
