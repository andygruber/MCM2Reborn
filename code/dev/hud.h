/*
    OpenMCM2 dev build - MCM2:Reborn's race HUD: what it shows (hud_read, huddata.c) and its
    drawing (hud_draw, hud.c).
*/

#pragma once

#include "runtime.h"

#define HUD_TRACK_MOST  512             /* the track map's points */
#define HUD_PARTS_MOST  32              /* ...in so many lines: the track file's sections */
#define HUD_RIDERS_MOST 16

typedef struct hud_rider {
    float x, z;                         /* where on the track, in the world */
    int   place;
    BOOL  player;
} hud_rider;

typedef struct hud_gap {
    BOOL known;
    char name[32];
    int  gap_ms;                        /* how far ahead or behind */
} hud_gap;

typedef struct hud_view {
    BOOL  show_race, show_map, show_speed;  /* the game's own HUD switches, which ours follow */

    int   place, riders;
    BOOL  timed;                        /* quarry and tag: points against the clock, no laps */
    int   lap, laps;                    /* laps 0 when there is no end to them */
    int   score;
    int   race_ms;                      /* timed, what is left of it */
    int   last_ms, best_ms;             /* 0 until there is one */
    hud_gap ahead, behind;

    float speed_mph;
    float revs;                         /* 0 to 1, or -1 when the game has none */
    int   gear;                         /* 0 when the game has none */

    char  track_name[48];
    float track[HUD_TRACK_MOST][2];     /* the track's lines, world x and z, one after another */
    int   track_count;
    int   part_start[HUD_PARTS_MOST];   /* where each line begins in track[] */
    int   parts;
    hud_rider rider[HUD_RIDERS_MOST];
    int   rider_count;
} hud_view;

/* The race as the HUD shows it: FALSE when no race is being driven, and nothing is drawn. */
BOOL hud_read(hud_view* view);

/* The HUD over the window's w x h, its frame just put there (ddraw.c, present). */
void hud_draw(int w, int h);
