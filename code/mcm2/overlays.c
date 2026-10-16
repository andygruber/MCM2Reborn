/*
    OpenMCM2 - the race HUD's overlays (QuarryStuntEvent makes them): the gauges, the race
    statistics and the overview radar, their draw passes (vtable slot 14). In the dev build with
    the modern menus, MCM2:Reborn's own HUD (code/dev/hud.c) is drawn in their place and they draw
    nothing; otherwise each is the game's own.
*/

#include "runtime.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

int __fastcall game_orig_vf_14_51DFC0(BYTE* self);     /* InstrumentOverlay */
int __fastcall game_orig_vf_14_51F000(BYTE* self);     /* StatsOverlay */
int __fastcall game_orig_vf_14_521370(BYTE* self);     /* RadarOverlay */

static BOOL reborn_hud(void)
{
#ifdef OPENMCM2_DEV
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

PORT(0x00521370)
int __fastcall radar_draw(BYTE* self)
{
    return reborn_hud() ? 1 : game_orig_vf_14_521370(self);
}
