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
    BOOL fix_last_pick;
} dev_switches;

extern dev_switches dev_settings;

/* Read the settings; start-up calls this before the game runs. */
void dev_install(void);

/* A line in the log, prefixed with its area ("screen", "event", ...). */
void dev_trace(const char* area, const char* fmt, ...);

/* An event a control is about to send its dialog (when events are traced). */
void dev_trace_event(DWORD type, const char* control, const void* dialog);
