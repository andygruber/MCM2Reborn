/*
    OpenMCM2 - the in-race menus, ported from the game (InGameProcs.cpp in the original): the
    menu over a race, a notice, and the replay's video-recorder controls. Each function here
    replaces the game's at the address in its PORT line, and was checked against the original's
    instructions.
*/

#include "gui.h"
#include <float.h>

BYTE* __fastcall dialog_find_control(BYTE* self, void* edx_unused, const char* name, int type);
int   __fastcall control_set_text(BYTE* self, void* edx_unused, const char* text);
int   __fastcall control_load_text(BYTE* self, void* edx_unused, HINSTANCE module, UINT id);
void  __fastcall control_show(BYTE* self, void* edx_unused, DWORD shown, BOOL animate);
void  __fastcall dialog_end(BYTE* self, void* edx_unused, DWORD sound);
BYTE* __fastcall dialog_construct(BYTE* self, void* edx_unused, DWORD flag, const char* name);
int   __fastcall dialog_tick(BYTE* self, void* edx_unused, float seconds);
BYTE* __fastcall gui_manager_open(gui_manager* self, void* edx_unused, BYTE* dialog, DWORD a,
                                  DWORD flags, DWORD area, BYTE* parent, DWORD arg, DWORD c,
                                  DWORD wait);
void  __fastcall gui_manager_show_cursors(gui_manager* self, void* edx_unused, BOOL show);
void  __fastcall prompt_setup_ids(BYTE* self, void* edx_unused, const char* title, UINT title_id,
                                  const char* prompt, UINT prompt_id, const char* left,
                                  UINT left_id, const char* middle, UINT middle_id,
                                  const char* right, UINT right_id);
void  __cdecl    result_go(int code, ui_event* event);
void  __cdecl    result_back(int code, ui_event* event);
void  __cdecl    result_confirm(int code, ui_event* event);
int   __fastcall option_select(BYTE* self, void* edx_unused, int index);
int   __fastcall option_current_index(BYTE* self);
int   __fastcall slider_set_range(BYTE* self, void* edx_unused, int range);
BOOL  __fastcall slider_set_value(BYTE* self, void* edx_unused, int value, unsigned range);
int   __fastcall slider_get_value(BYTE* self);

int   __cdecl    game_FUN_0053b6e0(const char* a, const char* b);           /* _stricmp */
int   __cdecl    game_FUN_0053a11f(char* to, const char* format, ...);      /* sprintf */
BYTE* __fastcall game_FUN_0045e300(BYTE* events);       /* the race under way, or NULL */
BYTE* __fastcall game_FUN_0045e2c0(BYTE* events);       /* its view: camera 0x3C, riders 0x34 */
void  __fastcall game_FUN_00421590(BYTE* race, void* edx_unused, DWORD a, DWORD b, DWORD c);
void  __fastcall game_FUN_00420350(BYTE* race, void* edx_unused, DWORD value);   /* pause */
void  __fastcall game_FUN_0041ffd0(BYTE* riders, void* edx_unused, DWORD back, DWORD a, DWORD b);
void  __fastcall game_FUN_0045ddd0(BYTE* events, void* edx_unused, DWORD a);
void  __fastcall game_FUN_0045f720(BYTE* events, void* edx_unused, DWORD a);
void  __fastcall game_FUN_00526eb0(BYTE* game, void* edx_unused, DWORD a, DWORD b, DWORD c);
void  __fastcall game_FUN_00469760(BYTE* game);
void  __fastcall game_FUN_004b0e10(BYTE* net, void* edx_unused, DWORD player, char* name);
void  __fastcall game_FUN_004b0f20(BYTE* net, void* edx_unused, DWORD type, void* data,
                                   DWORD size, DWORD to, DWORD flags);

typedef DWORD (__fastcall* setting_number)(void* game, void* edx_unused, const char* name,
                                           DWORD fallback);
#define GAME_SETTING_NUMBER 0x58

#define NAME(address)    ((const char*) (address))
#define INGAMEPROCS_CPP  NAME(0x0057264C)    /* "D:\aardvark\VC\krusty2\InGameProcs.cpp" */
#define MESSAGE_BOX      NAME(0x0056F734)    /* "messbox2.dtm" */
#define THREE_WAY_VTABLE ((void*) 0x005575AC) /* three_way_handle's: 0x65, 0x66, 0x67 */

#define BUTTON_LEFT    NAME(0x0056F888)     /* "ButLeft" */
#define BUTTON_MIDDLE  NAME(0x0056F87C)     /* "ButMiddle" */
#define BUTTON_RIGHT   NAME(0x0056F870)     /* "ButRight" */
#define TITLE_TEXT     NAME(0x0056F864)     /* "TitleText" */

#define NO_TIME        NAME(0x00572674)     /* "00:00:00.00" */
#define TIME_FORMAT    NAME(0x00572680)     /* "%02d:%02d:%05.2f" */
#define BUT_EXIT       NAME(0x00572694)     /* "ButExit" */
#define BUT_NEXT_RIDER NAME(0x0057269C)     /* "ButNextRider" */
#define BUT_PREV_RIDER NAME(0x005726AC)     /* "ButPrevRider" */
#define BUT_CAMERA     NAME(0x005726BC)     /* "ButCamera" */
#define TOG_PAUSE      NAME(0x005726C8)     /* "TogPause" */
#define TOG_PLAY       NAME(0x005726D4)     /* "TogPlay" */
#define BUT_FRAME      NAME(0x005726DC)     /* "ButFrame" */
#define BUT_END        NAME(0x005726E8)     /* "ButEnd" */
#define BUT_BEGINNING  NAME(0x005726F0)     /* "ButBeginning" */
#define TOG_REVERSE    NAME(0x00572700)     /* "TogReverse" */
#define TOG_FFF        NAME(0x0057270C)     /* "TogFFF" */
#define TOG_FF         NAME(0x00572714)     /* "TogFF" */
#define ALLOW_VCR      NAME(0x0057271C)     /* "AllowVCRControls" */
#define BUT_SAVE       NAME(0x00572730)     /* "ButSave" */
#define SLIDER_TIME    NAME(0x00572738)     /* "SliderTime" */
#define TXT_TIME_END   NAME(0x00572744)     /* "TxtTimeEnd" */
#define TXT_TIME       NAME(0x00572750)     /* "TxtTime" */

enum {
    EVENT_CLICK = 1, EVENT_OPEN = 5, EVENT_CLOSE = 6, EVENT_CHILD_ENDED = 9,
    EVENT_SLIDER = 16,      /* a slider moved; c is the slider */
};

#define KEY_RETURN 0x1C

#define game_net       FIELD(game_main, 0x8, BYTE*)
#define game_events    FIELD(game_main, 0x570, BYTE*)
#define game_language  FIELD(game_main, 0x420, HINSTANCE)
#define race_mode      FIELD(game_main, 0x2D70, int)    /* 4: a ghost race */
#define ghost_answer   FIELD(game_main, 0x2C68, DWORD)  /* the ghost-race prompt's answer */
#define replay_speed   FIELD(game_main, 0x2E0, float)
#define frame_step     FIELD(game_main, 0x1C8, DWORD)
#define cursors_locked (FIELD(game_main, 0x2D5, BYTE) & 2)
#define dialog_gui(d)  FIELD(d, 0x30, gui_manager*)

/* The race's replay state. */
#define RACE_REPLAY_TIME 0x1B8      /* float: where the replay is */
#define RACE_SEEK        0x1C4      /* float: where to go; -1 when not seeking */
#define RACE_REPLAY      0x1A0      /* its length (float) at 0x10C */
#define RACE_VCR_MODE    0x1DC      /* 4 play, 8 FF, 9 FFF, 11 reverse, 12 to the start, 14 */
#define RACE_VCR_STATE   0x1E4      /* -2: nothing to play */
#define RACE_PAUSED      0x3F8      /* BYTE */
#define RACE_STEP        0x3F9      /* BYTE: step one frame */
#define replay_length(race) FIELD(FIELD(race, RACE_REPLAY, BYTE*), 0x10C, float)

#define VCR_LEAVING 0x7F58          /* Exit was clicked: closing keeps the cursor */

static BYTE* find(BYTE* self, const char* name, int type)
{
    return dialog_find_control(self, NULL, name, type);
}

static BOOL clicked(const ui_event* event, const char* name)
{
    return !game_FUN_0053b6e0(name, (const char*) event->b);
}

/* Equal as `test ah, 40h` tests it (C3 set): unordered counts too. */
static BOOL x87_equal(double a, double b)
{
    return !(a < b) && !(a > b);
}

/* ---- The menu over a race ---- */

/*
    Middle shows the replay. Left leaves the race: in a ghost race with a run, after asking (the
    answer, 0x65 or 0x67, comes back as a child's end); in a network game the others are told
    first. Right goes through result_confirm.
*/
PORT(0x00489670)
void __fastcall race_menu_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* race;

    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_MIDDLE)) {
            gui_manager_show_cursors(dialog_gui(self), NULL, FALSE);
            if (FIELD(game_main, 0x3430, int))
                return;
            race = game_FUN_0045e300(game_events);
            if (race && !FIELD(FIELD(race, 0x38, BYTE*), 0x7A4, BYTE)) {
                game_FUN_00421590(race, NULL, 1, 0, 0);
                game_FUN_00526eb0(game_main, NULL, 0, 0x191, 1);
            }
        } else if (clicked(event, BUTTON_LEFT)) {
            race = game_FUN_0045e300(game_events);
            if (race && race_mode == 4 && FIELD(race, 0x1E8, int) > 0) {
                /* A failed allocation still goes on with NULL (the original's). */
                BYTE* prompt = game_new(0x7F58, INGAMEPROCS_CPP, 0x42);

                if (prompt) {
                    dialog_construct(prompt, NULL, 1, MESSAGE_BOX);
                    *(void**) prompt = THREE_WAY_VTABLE;
                }
                gui_manager_open(dialog_gui(self), NULL, prompt, 0, 4, 0, self, 0, 0, 1);
                prompt_setup_ids(prompt, NULL, NULL, 0x1440, NULL, 0, NULL, 0x143E, NULL, 0,
                                 NULL, 0x143D);
                return;
            }
            if (game_net) {
                DWORD player = FIELD(game_net, 0xC, DWORD);
                char name[0x80];

                game_FUN_004b0e10(game_net, NULL, player, name);
                if (name[0]) {
                    /* The first word is left unset by the original; zeroed here. */
                    struct { DWORD unset; char name[16]; } message = { 0 };

                    text_copy_most(message.name, name, 15);
                    game_FUN_004b0f20(game_net, NULL, 0x89, &message, sizeof message, player, 0);
                }
            }
            gui_manager_show_cursors(dialog_gui(self), NULL, FALSE);
            result_back(0x191, event);
        } else if (clicked(event, BUTTON_RIGHT)) {
            gui_manager_show_cursors(dialog_gui(self), NULL, FALSE);
            result_confirm(0x191, event);
            game_FUN_00469760(game_main);
        }
    } else if (event->type == EVENT_OPEN) {
        BYTE* middle;

        control_load_text(find(self, TITLE_TEXT, 12), NULL, game_language, 0x13D9);
        middle = find(self, BUTTON_MIDDLE, 1);
        control_load_text(middle, NULL, game_language, 0x13DA);
        race = game_FUN_0045e300(game_events);
        /* Without a network game the race is read unchecked (the original's). */
        if (game_net || FIELD(FIELD(race, 0x38, BYTE*), 0x7A4, BYTE) || race_mode == 4)
            control_show(middle, NULL, FALSE, TRUE);
        control_load_text(find(self, BUTTON_LEFT, 1), NULL, game_language, 0x13DB);
        control_load_text(find(self, BUTTON_RIGHT, 1), NULL, game_language, 0x13DC);
        gui_manager_show_cursors(dialog_gui(self), NULL, TRUE);
    } else if (event->type == EVENT_CHILD_ENDED) {
        if (event->a == 0x65)
            ghost_answer = 1;
        else if (event->a == 0x67)
            ghost_answer = 0;
        else
            return;
        gui_manager_show_cursors(dialog_gui(self), NULL, FALSE);
        result_back(0x191, event);
    }
}

/* A notice with one button, Middle (taking Return): it goes on through result_go(400). */
PORT(0x00489A60)
void __fastcall race_notice_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* button;

    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_MIDDLE)) {
            gui_manager_show_cursors(dialog_gui(self), NULL, FALSE);
            result_go(400, event);
            game_FUN_00469760(game_main);
        }
    } else if (event->type == EVENT_OPEN) {
        control_load_text(find(self, TITLE_TEXT, 0), NULL, game_language, 0x8FF);
        button = find(self, BUTTON_MIDDLE, 1);
        control_load_text(button, NULL, game_language, 0x916);
        FIELD(button, 0x1D8, DWORD) = KEY_RETURN;
        button = find(self, BUTTON_LEFT, 1);
        control_show(button, NULL, FALSE, TRUE);
        FIELD(button, 0x1D8, DWORD) = 0;
        control_show(find(self, BUTTON_RIGHT, 0), NULL, FALSE, TRUE);
    }
}

/* ---- Times ---- */

/*
    Seconds as "hh:mm:ss.ss"; none (0, FLT_MAX or NaN) as zeros. The minutes are counted from
    0, not from the hour, so an hour or more comes out with negative seconds (the original's).
*/
PORT(0x00489B60)
void __cdecl time_text(char* text, float seconds)
{
    int hours, minutes;

    if (x87_equal(seconds, 0.0f) || x87_equal(seconds, FLT_MAX)) {
        text_copy(text, NO_TIME);
        return;
    }
    hours = (int) (seconds * (double) (1.0f / 3600));      /* 0x0055929C */
    minutes = (int) (seconds * (double) (1.0f / 60));      /* 0x00559298 */
    game_FUN_0053a11f(text, TIME_FORMAT, hours, minutes,
                      (seconds - (double) (hours * 3600)) - (double) (minutes * 60));
}

/* ---- The replay's controls ---- */

__declspec(noinline) void __fastcall vcr_show_mode(BYTE* self);

static BOOL nothing_to_play(BYTE* race)
{
    return FIELD(race, RACE_VCR_STATE, int) == -2;
}

static void unpause(BYTE* race)
{
    if (FIELD(race, RACE_PAUSED, BYTE))
        game_FUN_00420350(race, NULL, 0);
}

static void set_mode(BYTE* race, int mode, float speed)
{
    FIELD(race, RACE_VCR_MODE, int) = mode;
    replay_speed = speed;
}

/* Play at normal speed; also what a fast toggle turned off falls back to. */
static void play(BYTE* race, DWORD pause)
{
    game_FUN_00420350(race, NULL, pause);
    set_mode(race, 4, 1.0f);
}

/* A toggle turned on runs at `speed`; turned off, plays. */
static void fast(BYTE* race, const ui_event* event, int mode, float speed)
{
    if (option_current_index((BYTE*) event->c) == 1) {
        unpause(race);
        set_mode(race, mode, speed);
    } else {
        play(race, 1);
    }
}

static void vcr_click(BYTE* self, BYTE* race, ui_event* event)
{
    if (clicked(event, TOG_PLAY)) {
        if (!nothing_to_play(race)) {
            game_FUN_00420350(race, NULL, 1);
            play(race, 0);
        }
    } else if (clicked(event, TOG_PAUSE)) {
        if (!nothing_to_play(race))
            play(race, 0);
    } else if (clicked(event, TOG_FF)) {
        if (!nothing_to_play(race))
            fast(race, event, 8, 2.0f);
    } else if (clicked(event, TOG_FFF)) {
        if (!nothing_to_play(race))
            fast(race, event, 9, 8.0f);
    } else if (clicked(event, TOG_REVERSE)) {
        /* No check for nothing to play here (the original's). */
        if (option_current_index((BYTE*) event->c) != 1) {
            play(race, 1);
        } else if (FIELD(race, RACE_VCR_MODE, int) != 14) {
            unpause(race);
            set_mode(race, 11, 0.0f);
        }
    } else if (clicked(event, BUT_FRAME)) {
        if (!nothing_to_play(race)) {
            unpause(race);
            FIELD(race, RACE_STEP, BYTE) = 1;
            frame_step = 1;
        }
    } else if (clicked(event, BUT_BEGINNING)) {
        unpause(race);
        FIELD(race, RACE_VCR_MODE, int) = 12;
    } else if (clicked(event, BUT_END)) {
        if (!nothing_to_play(race)) {
            unpause(race);
            set_mode(race, 9, 30.0f);
        }
    } else if (clicked(event, BUT_CAMERA)) {
        BYTE* camera = FIELD(game_FUN_0045e2c0(game_events), 0x3C, BYTE*);

        if (FIELD(camera, 0x244, int) == 5) {
            FIELD(camera, 0x244, int) = FIELD(camera, 0x248, int);
            FIELD(camera, 0x258, DWORD) = FIELD(camera, 0x24C, DWORD);
            VIRTUAL(camera, 0xE8, method_0)(camera);
        } else {
            VIRTUAL(camera, 0x120, method_0)(camera);
        }
    } else if (clicked(event, BUT_PREV_RIDER) || clicked(event, BUT_NEXT_RIDER)) {
        BYTE* riders = FIELD(game_FUN_0045e2c0(game_events), 0x34, BYTE*);

        game_FUN_0041ffd0(riders, NULL, clicked(event, BUT_PREV_RIDER), 1, 0);
    } else if (clicked(event, BUT_EXIT)) {
        /* Back to the menus with the settings saved before the replay. */
        dialog_end((BYTE*) event->sender, NULL, 0);
        event->e = 1;
        __movsd((unsigned long*) (game_main + 0x2D70), (unsigned long*) (game_main + 0x2F5C),
                0x7B);
        __movsd((unsigned long*) (game_main + 0x1550), (unsigned long*) (game_main + 0x15AC),
                0x17);
        __movsd((unsigned long*) (game_main + 0x1EEC), (unsigned long*) (game_main + 0x1FB4),
                0x32);
        game_FUN_0045ddd0(game_events, NULL, 2);
        game_FUN_0045f720(game_events, NULL, 100);
        gui_manager_show_cursors(dialog_gui(self), NULL, TRUE);
        FIELD(self, VCR_LEAVING, DWORD) = 1;
        return;
    }
    vcr_show_mode(self);
}

/* Without AllowVCRControls only play, pause and the camera and rider buttons stay. */
static const char* const vcr_extras[6] = {
    TOG_FF, TOG_FFF, TOG_REVERSE, BUT_BEGINNING, BUT_END, BUT_FRAME,
};

static void control_disable(BYTE* control)
{
    VIRTUAL(control, 0xC4, method_1)(control, NULL, 0);
}

PORT(0x00489C20)
void __fastcall vcr_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* race = game_FUN_0045e300(game_events);
    int i;

    (void) edx_unused;
    switch (event->type) {
    case EVENT_CLICK:
        vcr_click(self, race, event);
        break;
    case EVENT_OPEN:
        FIELD(self, VCR_LEAVING, DWORD) = 0;
        slider_set_range(find(self, SLIDER_TIME, 8), NULL, 1000);
        control_show(find(self, BUT_SAVE, 0), NULL, FALSE, TRUE);
        if (VIRTUAL(game_main, GAME_SETTING_NUMBER, setting_number)(game_main, NULL, ALLOW_VCR,
                                                                    0))
            break;
        for (i = 0; i < 6; i++)
            control_show(find(self, vcr_extras[i], 0), NULL, FALSE, TRUE);
        /* Disabled in the original's order: the first five, the slider, then ButFrame. */
        for (i = 0; i < 5; i++)
            control_disable(find(self, vcr_extras[i], 0));
        control_disable(find(self, SLIDER_TIME, 8));
        control_disable(find(self, BUT_FRAME, 0));
        break;
    case EVENT_CLOSE:
        if (!cursors_locked && !FIELD(self, VCR_LEAVING, DWORD))
            gui_manager_show_cursors(dialog_gui(self), NULL, FALSE);
        break;
    case EVENT_SLIDER:
        if (clicked(event, SLIDER_TIME)) {
            float fraction = (float) slider_get_value((BYTE*) event->c) * 0.001f;

            /* Looked up again (the original's). */
            race = game_FUN_0045e300(game_events);
            FIELD(race, RACE_SEEK, float) = fraction * replay_length(race);
        }
        break;
    }
}

/* Slot 10: the cursor, the times, the slider while not seeking, and the lit toggle. */
PORT(0x0048A350)
int __fastcall vcr_tick(BYTE* self, void* edx_unused, float seconds)
{
    BYTE* slider;
    BYTE* race;
    BYTE* label;
    char text[32];

    (void) edx_unused;
    gui_manager_show_cursors(dialog_gui(self), NULL, TRUE);
    slider = find(self, SLIDER_TIME, 8);
    race = game_FUN_0045e300(game_events);
    label = find(self, TXT_TIME, 12);
    time_text(text, FIELD(race, RACE_REPLAY_TIME, float));
    control_set_text(label, NULL, text);
    label = find(self, TXT_TIME_END, 12);
    time_text(text, replay_length(race));
    control_set_text(label, NULL, text);
    if (FIELD(race, RACE_SEEK, DWORD) == 0xBF800000)       /* -1.0f, compared as bits */
        slider_set_value(slider, NULL, (int) (FIELD(race, RACE_REPLAY_TIME, float) * 1000.0),
                         (unsigned) (int) (replay_length(race) * 1000.0));
    vcr_show_mode(self);
    return dialog_tick(self, NULL, seconds);
}

/* The toggle for what the replay is doing is lit, the others not; another mode leaves them. */
PORT(0x0048A450)
void __fastcall vcr_show_mode(BYTE* self)
{
    BYTE* race = game_FUN_0045e300(game_events);
    BYTE* toggles[5];
    int lit, i;

    toggles[0] = find(self, TOG_PLAY, 2);
    toggles[1] = find(self, TOG_PAUSE, 2);
    toggles[2] = find(self, TOG_REVERSE, 2);
    toggles[3] = find(self, TOG_FF, 2);
    toggles[4] = find(self, TOG_FFF, 2);
    if (FIELD(race, RACE_PAUSED, BYTE)) {
        lit = 1;
    } else {
        switch (FIELD(race, RACE_VCR_MODE, int)) {
        case 4:  lit = 0; break;
        case 8:  lit = 3; break;
        case 9:  lit = 4; break;
        case 11: lit = 2; break;
        case 14: lit = -1; break;
        default: return;
        }
    }
    for (i = 0; i < 5; i++)
        option_select(toggles[i], NULL, i == lit);
}
