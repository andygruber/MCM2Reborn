/*
    OpenMCM2 dev build - debugging tools and changes to the game.

    Only the dev build (OPENMCM2_DEV, the dev branch) compiles this. Its switches come from
    OpenMCM2-Dev.ini in the game folder; each is off unless the file turns it on, so the dev build
    plays as the normal one until asked otherwise.

        [trace]           what goes to openmcm2-dev.log
        screens=1         every menu screen opened, by number
        events=1          every dialog event the ported handlers see
        files=1           every file the profile code looks for
        display=1         every display mode set and every surface made that is not a
                          texture, with the address of the game code that asked

        [menus]
        widescreen=1      the menus at the display mode the races use, their 640x480
                          scaled evenly to fit and centred, the backgrounds filling the screen
                          (experimental: the start of the modern menus)
        fill_sides=1      a picture narrower than the screen, such as the 640x480 menus, gets
                          a blurred, darkened copy of itself behind its side bars
        modern=1          the modern menus: widescreen, and each screen's layout taken from the
                          folder's modern\ when one is there (ui\modern\mainmenu.dat, or a loose
                          mainmenu.dtm), else the original's, scaled
        live_background=1 the main menu over a replay - the newest in Record, else the last
                          race's tape - its sound off, looping; a button that leaves the main
                          menu stops it and goes on in the plain menus (a first version)

        [fixes]           the original's bugs, mended
        last_pick=1       the computer riders' random picks can land on the last bike or rider

    A change to the game goes in the ported function it changes, between #ifdef OPENMCM2_DEV and
    #endif, reading its switch from dev_settings; the code outside stays the faithful port.
*/

#pragma once

#include "runtime.h"

/* The game's name in the modern menus, the window's title and wherever else it is shown. */
#define GAME_TITLE "MCM2:Reborn"

typedef struct dev_switches {
    BOOL trace_screens;
    BOOL trace_events;
    BOOL trace_files;
    BOOL trace_display;
    BOOL menus_widescreen;
    BOOL menus_fill_sides;
    BOOL menus_modern;
    BOOL menus_live;
    BOOL fix_last_pick;
} dev_switches;

extern dev_switches dev_settings;

/* Read the settings; start-up calls this before the game runs. */
void dev_install(void);

/* A line in the log, prefixed with its area ("screen", "event", ...). */
void dev_trace(const char* area, const char* fmt, ...);

/* Milliseconds since the clock was first read (dev.c). */
double dev_milliseconds(void);

/* The mouse wheel moves a bike preview's camera (at 0x7F70 of its page) in or out: TRUE when
   it did, for the page to place its camera again (dlgprocs.c). */
BOOL preview_zoom(unsigned char* page);

/* The mouse wheel's notches turned since the last frame, away from you positive: the same to
   every caller in a frame. `user` is the gui_user, `object` anything on the screen (ui.c). */
int menu_wheel(void* user, void* object);

/* Whether a dialog's layout is MCM2:Reborn's, which draws its words in its pictures (ui.c). */
BOOL menu_reborn(unsigned char* dialog);

/* A MCM2:Reborn screen with pages: the title of page `page` shown (dlgprocs.c). */
void page_title_show(unsigned char* screen, int page);

/* Whether a modern screen has the photos under it (ui.c). */
BOOL menu_photos_shown(void);

/* A rectangle on the 640x480 menu screen, where the scaled menus put it (ui.c). */
void menu_place_view(unsigned char* dialog, RECT* view);

/* An event a control is about to send its dialog (when events are traced). */
void dev_trace_event(DWORD type, const char* control, const void* dialog);

/*
    The live background ([menus] live_background), in dlgprocs.c. live_menu_start starts it
    instead of opening the main menu (TRUE when it did); live_menu_opening sees each dialog
    opened, for the race's own; live_menu_tick runs each frame.
*/
BOOL live_menu_start(void);
void live_menu_opening(BYTE* dialog);
void live_menu_tick(void);

/*
    Online play through Epic Online Services (eosplay.c). eos_install takes over the game's
    CoCreateInstance at start-up; eos_online makes the network again, its DirectPlay EOS's own
    or the real one. FALSE when the network could not be made, or EOS's helper not started.
*/
void eos_install(void);
BOOL eos_online(BOOL on);
/* 1 signed in to EOS, 0 signing in, -1 not (the helper did not start, or failed). */
int  eos_state(void);

/* What a lobby found shows: its race is the host's settings, kind -1 until it has published them. */
typedef struct online_lobby {
    int  players, most;
    int  kind, laps, trees;                 /* race_kind's numbers (menus.h) */
    BOOL closed;                            /* racing: joining is closed */
    char host[32];
    char track[48];
    char riders[8][32];                     /* the host's first; empty ones are open */
    struct { char name[24], text[76]; } chat[4];    /* the host's lobby chat, the oldest first */
} online_lobby;

/* The lobby a session's instance GUID stands for: FALSE when it is not one of EOS's. */
BOOL eos_lobby(const GUID* instance, online_lobby* lobby);

/* A line of the lobby's chat as the game shows it: hosting, it goes on the lobby for searches. */
void eos_chat(const char* name, const char* text);

/* Seconds since the lobbies were last found, or -1 before they ever were. */
int  eos_found_ago(void);
