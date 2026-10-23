/*
    OpenMCM2 - the race HUD's overlays (QuarryStuntEvent makes them): the gauges, the race
    statistics and the overview radar, their draw passes (vtable slot 14). In the dev build with
    the modern menus, MCM2:Reborn's own HUD (code/dev/hud.c) is drawn in their place and they draw
    nothing; otherwise each is the game's own. Their passes also tell the HUD the race is on the
    screen (hud_overlays_drawn), once the loading screen has gone: the race loads with its bikes
    already made, and is drawn under the loading screen for its last moments.
*/

#include "runtime.h"
#include "game.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

int __fastcall game_orig_vf_14_51DFC0(BYTE* self);     /* InstrumentOverlay */
int __fastcall game_orig_vf_14_51F000(BYTE* self);     /* StatsOverlay */
int __fastcall game_orig_vf_14_521370(BYTE* self);     /* RadarOverlay */
void __fastcall game_orig_FUN_00521520(BYTE* self, void* edx_unused, int kind);

#ifdef OPENMCM2_DEV
/* The loading screen open: it puts itself in the session (game_main + 0x56C, at +0x490). */
static BOOL loading_shown(void)
{
    BYTE* session = FIELD(game_main, 0x56C, BYTE*);

    return session && FIELD(session, 0x490, BYTE*);
}
#endif

static BOOL reborn_hud(void)
{
#ifdef OPENMCM2_DEV
    if (!loading_shown())
        hud_overlays_drawn();
    return dev_settings.menus_modern && g_reborn.hud;
#else
    return FALSE;
#endif
}

PORT(0x0051DFC0)
int __fastcall instruments_draw(BYTE* self)
{
    return reborn_hud() ? 1 : game_orig_vf_14_51DFC0(self);
}

PORT(0x0051F000)
int __fastcall race_stats_draw(BYTE* self)
{
    return reborn_hud() ? 1 : game_orig_vf_14_51F000(self);
}

#ifdef OPENMCM2_DEV
/*
    The radar keeps its bikes in a list made bigger for 30 (riders.c), counted at +0x160; but the
    markers it moves for them, in a table at +0x2D8 of the race's overview ([+0x12C]+0x19C),
    have room for the game's 11 (FUN_00521520). With more on the gate the classic radar read past
    them and crashed, so every marker update is told of the first 11, including initialization.
*/
#define RADAR_BIKES     0x160
#define RADAR_MARKERS   11
#endif

/* Also called while the radar is initialized, outside its draw pass. Limit the
   actual marker update so every caller respects the overview's eleven markers. */
PORT(0x00521520)
void __fastcall radar_update_markers(BYTE* self, void* edx_unused, int kind)
{
#ifdef OPENMCM2_DEV
    int bikes = FIELD(self, RADAR_BIKES, int);

    if (bikes > RADAR_MARKERS)
        FIELD(self, RADAR_BIKES, int) = RADAR_MARKERS;
    game_orig_FUN_00521520(self, edx_unused, kind);
    FIELD(self, RADAR_BIKES, int) = bikes;
#else
    game_orig_FUN_00521520(self, edx_unused, kind);
#endif
}

PORT(0x00521370)
int __fastcall radar_draw(BYTE* self)
{
#ifdef OPENMCM2_DEV
    int bikes, drawn;

    if (reborn_hud())
        return 1;
    bikes = FIELD(self, RADAR_BIKES, int);
    if (bikes > RADAR_MARKERS)
        FIELD(self, RADAR_BIKES, int) = RADAR_MARKERS;
    drawn = game_orig_vf_14_521370(self);
    FIELD(self, RADAR_BIKES, int) = bikes;
    return drawn;
#else
    return reborn_hud() ? 1 : game_orig_vf_14_521370(self);
#endif
}
