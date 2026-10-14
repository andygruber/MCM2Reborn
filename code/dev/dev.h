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
