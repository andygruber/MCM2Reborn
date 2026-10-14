/*
    OpenMCM2 - the game's own, ported from the game (Game.cpp in the original). Each function here
    replaces the game's at the address in its PORT line, and was checked against the original's
    instructions.
*/

#include "gui.h"

#include <imm.h>

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

#define game_session  FIELD(game_main, 0x56C, BYTE*)
#define session_gui   FIELD(game_session, 0x2C, gui_manager*)
#define game_window   FIELD(game_main, 0x31C, HWND)
#define video_card    FIELD(game_main, 0xC, BYTE*)

/* The display modes the card offers, 36 bytes each: width, height, bits first. */
#define card_mode(card, index) (FIELD(card, 0x10, BYTE*) + (index) * 0x24)
#define race_mode_index        FIELD(game_main, 0xFC4, int)     /* the one the options chose */

void      __fastcall gui_manager_make_background(gui_manager* self);
void      __fastcall gui_manager_show_cursors(gui_manager* self, void* edx_unused, BOOL show);
BOOL      __fastcall gui_manager_set_palette(gui_manager* self, void* edx_unused, DWORD palette);
void      __fastcall session_record_race(BYTE* self);
void      __fastcall session_build_scene(BYTE* self, void* edx_unused, int stage);
void      __fastcall session_go_to(BYTE* self, void* edx_unused, int screen);
void      __fastcall game_FUN_00527090(BYTE* game);
int       __fastcall game_FUN_005328c0(BYTE* card, void* edx_unused, int width, int height,
                                       int bits, DWORD a, DWORD b);    /* a mode's index, or -1 */
void      __fastcall game_FUN_004c3100(BYTE* self, void* edx_unused, DWORD a);

typedef BOOL (__fastcall* set_display)(BYTE* game, void* edx_unused, int mode);

/*
    Back to the menus after a race, at `screen`: the input context and palette the menus had,
    the menus' 640x480x16 display mode with a new background for it, the race just recorded
    offered for saving, and the scene behind the menus.
*/
PORT(0x0045F720)
void __fastcall game_menus_return(BYTE* self, void* edx_unused, int screen)
{
    BYTE* mode;

    (void) self;
    (void) edx_unused;
    if (!game_session)
        return;
    if (FIELD(game_session, 0x494, HIMC))
        ImmAssociateContext(game_window, FIELD(game_session, 0x494, HIMC));
    if (FIELD(game_session, 0x498, DWORD))
        gui_manager_set_palette(session_gui, NULL, 1);
    FIELD(game_main, 0x3430, DWORD) = 0;
    FIELD(game_main, 0x3438, DWORD) = 0;
    FIELD(game_main, 0x3434, DWORD) = 0;
    if (!FIELD(game_main, 0xF98, DWORD))
        game_FUN_00527090(game_main);

    mode = card_mode(video_card, race_mode_index);
#ifdef OPENMCM2_DEV
    if (!dev_settings.menus_widescreen)         /* the widescreen menus stay in the races' mode */
#endif
    if (FIELD(mode, 0, int) != 640 || FIELD(mode, 4, int) != 480 || FIELD(mode, 8, int) != 16) {
        int menus = game_FUN_005328c0(video_card, NULL, 640, 480, 16, 0, 0);

        VIRTUAL(game_main, 0x4C, set_display)(game_main, NULL, menus);
    }

    gui_manager_make_background(session_gui);
    FIELD(FIELD(gui_manager_user(session_gui, NULL, 0), 0xC0, BYTE*), 0x5C, DWORD) =
        FIELD(game_main, 0xC4C, DWORD);
    if (!FIELD(game_main, 0x3428, DWORD) && !FIELD(game_session, 0x4A8, DWORD)
        && FIELD(game_main, 0x2D70, DWORD))
        session_record_race(game_session);
    session_build_scene(game_session, NULL, -1);
    gui_manager_show_cursors(session_gui, NULL, TRUE);
    session_go_to(game_session, NULL, screen);
    game_FUN_004c3100(FIELD(game_main, 0x4, BYTE*), NULL, 0);
}
