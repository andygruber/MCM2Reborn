/*
    OpenMCM2 - the menus' screens, ported from the game (dlgprocs.cpp in the original).

    Each function here replaces the game's at the address in its PORT line, and was checked against
    the original's instructions. Most take the event a control sent (its dialog is the event's
    sender) and look the screen's controls up by the names the dialog files give them.
*/

#include "gui.h"

/* The controls and lists these use, ported in ui.c. */
BYTE*       __fastcall dialog_find_control(BYTE* self, void* edx_unused, const char* name, int type);
int         __fastcall control_set_text(BYTE* self, void* edx_unused, const char* text);
int         __fastcall control_load_text(BYTE* self, void* edx_unused, HINSTANCE module, UINT id);
int         __fastcall list_selected(BYTE* self);
const char* __fastcall list_text(BYTE* self, void* edx_unused, int index);
BOOL        __fastcall list_add_text(BYTE* self, void* edx_unused, const char* text, DWORD data,
                                     DWORD b);
void        __fastcall list_clear(BYTE* self);
void        __fastcall control_show(BYTE* self, void* edx_unused, DWORD shown, BOOL animate);
void        __fastcall dialog_end(BYTE* self, void* edx_unused, DWORD code);
void        __fastcall list_sort(BYTE* self, void* edx_unused, BOOL outer);
int         __fastcall list_select_text(BYTE* self, void* edx_unused, const char* text);
int         __fastcall list_set_208(BYTE* self, void* edx_unused, DWORD value);
int         __fastcall list_set_text_colour(BYTE* self, void* edx_unused, COLORREF colour, int index);
int         __fastcall list_set_colour_2(BYTE* self, void* edx_unused, COLORREF colour);

/* The game's: atoi, a string from its language module, and the profile it saves to. */
int  __cdecl    game_FUN_0053b6c7(const char* text);
int  __cdecl    game_FUN_0053b6e0(const char* a, const char* b);           /* _stricmp */
void __fastcall game_FUN_00526fc0(BYTE* game, void* edx_unused, UINT id, char* text, int size);
void __fastcall game_FUN_00528820(char* profile);                         /* save it */
int  __cdecl    game_FUN_0053a11f(char* to, const char* format, ...);      /* sprintf */
char* __cdecl   game__strncpy(char* to, const char* from, size_t count);
#define game_profile ((char*) game_main + 0x578)    /* char[16]: the player's profile */
#define game_language FIELD(game_main, 0x420, HINSTANCE)

/* The directory lister (dirlist.cpp) at game_main + 0x3338, and its calls. */
#define game_files FIELD(game_main, 0x3338, BYTE*)
void __fastcall game_FUN_0044b160(BYTE* files, void* edx_unused, const char* folder);
void __fastcall game_FUN_0044b1b0(BYTE* files, void* edx_unused, const char* pattern, DWORD a);
BOOL __fastcall game_FUN_0044b4f0(BYTE* files, void* edx_unused, char* name);   /* the first */
BOOL __fastcall game_FUN_0044b460(BYTE* files, void* edx_unused, char* name);   /* the next */

#define NAME(address) ((const char*) (address))
#define LIST_PROFILES   NAME(0x0056F3E8)    /* "LstProfiles" */
#define PROFILE_FOLDER  NAME(0x0056F548)    /* "ui\profile" */
#define PROFILE_FILES   NAME(0x0056F334)    /* "*" */
#define TEXT_PROFILE    NAME(0x0056F7C0)    /* "TxtCurrentProfile" */
#define TWO_WORDS       NAME(0x0056E054)    /* "%s %s" */

#define PROFILE_COLOUR  0xFEB97A
#define PROFILE_LENGTH  15

static BYTE* event_dialog(const ui_event* event)
{
    return (BYTE*) event->sender;
}

/* ---- Sorting ---- */

/* List sort orders by number: entries' texts as integers, rising and falling. */
PORT(0x0044C040)
int __cdecl entries_by_number(const BYTE* a, const BYTE* b)
{
    int first = game_FUN_0053b6c7(FIELD(a, 0x14, const char*));
    int second = game_FUN_0053b6c7(FIELD(b, 0x14, const char*));

    if (first < second)
        return -1;
    return first != second;
}

PORT(0x0044C080)
int __cdecl entries_by_number_falling(const BYTE* a, const BYTE* b)
{
    int first = game_FUN_0053b6c7(FIELD(a, 0x14, const char*));
    int second = game_FUN_0053b6c7(FIELD(b, 0x14, const char*));

    if (second < first)
        return -1;
    return first != second;
}

/* ---- Profiles ---- */

/* The profile picked in LstProfiles becomes the player's (at most 15 characters), and is saved. */
PORT(0x0044C0C0)
BOOL __cdecl profile_choose(ui_event* event)
{
    BYTE* list = dialog_find_control(event_dialog(event), NULL, LIST_PROFILES, 0);
    short index = (short) list_selected(list);
    char name[PROFILE_LENGTH + 1];
    size_t length;

    if (index == -1)
        return FALSE;
    length = text_length(list_text(list, NULL, index));
    if (length > PROFILE_LENGTH)
        length = PROFILE_LENGTH;
    game__strncpy(name, list_text(list, NULL, index), length);
    name[length] = 0;
    length = text_length(name);
    if (length > PROFILE_LENGTH)
        length = PROFILE_LENGTH;
    game__strncpy(game_profile, name, length);
    game_profile[length] = 0;
    game_FUN_00528820(game_profile);
    return TRUE;
}

/* LstProfiles: the profile files there are, sorted, the player's selected. */
PORT(0x0044D7D0)
void __fastcall profiles_fill(BYTE* self)
{
    BYTE* list = dialog_find_control(self, NULL, LIST_PROFILES, 0);
    char name[0x104];

    list_clear(list);
    game_FUN_0044b160(game_files, NULL, PROFILE_FOLDER);
    game_FUN_0044b1b0(game_files, NULL, PROFILE_FILES, 0);
    VIRTUAL(game_files, 4, method_0)(game_files);
    game_FUN_0044b4f0(game_files, NULL, name);
    if (name[0])
        list_add_text(list, NULL, name, 0, 0);
    while (game_FUN_0044b460(game_files, NULL, name)) {
        if (name[0])
            list_add_text(list, NULL, name, 0, 0);
    }
    list_sort(list, NULL, TRUE);
    list_set_text_colour(list, NULL, PROFILE_COLOUR, -1);
    list_select_text(list, NULL, game_profile);
    list_set_208(list, NULL, 0xFFFFFF);
    list_set_colour_2(list, NULL, PROFILE_COLOUR);
}

/* TxtCurrentProfile: string 5222 and the player's profile. */
PORT(0x0044D8F0)
void __fastcall profile_show(BYTE* self)
{
    BYTE* label = dialog_find_control(self, NULL, TEXT_PROFILE, 12);
    char caption[0x80];
    char text[0x80];

    game_FUN_00526fc0(game_main, NULL, 0x1466, caption, sizeof caption);
    game_FUN_0053a11f(text, TWO_WORDS, caption, game_profile);
    control_set_text(label, NULL, text);
}

/* ---- Statistics ---- */

static const char* const stats_buttons[4] = {
    NAME(0x0056F7F8), NAME(0x0056F7EC), NAME(0x0056F7E0), NAME(0x0056F7D4),   /* "ButStats1".. */
};

static const char* const stats_lists[4] = {
    NAME(0x0056F828), NAME(0x0056F81C), NAME(0x0056F810), NAME(0x0056F804),   /* "LstStats1".. */
};

/* The four columns' headings, from the language module's strings; 0 leaves one as it is. */
PORT(0x0044D960)
void __cdecl stats_load_headings(ui_event* event, UINT first, UINT second, UINT third, UINT fourth)
{
    const UINT ids[4] = { first, second, third, fourth };
    int i;

    for (i = 0; i < 4; i++) {
        if (ids[i])
            control_load_text(dialog_find_control(event_dialog(event), NULL, stats_buttons[i], 0),
                              NULL, game_language, ids[i]);
    }
}

/* The four columns' headings as texts; NULL leaves one as it is. */
PORT(0x0044DA20)
void __cdecl stats_set_headings(ui_event* event, const char* first, const char* second,
                                const char* third, const char* fourth)
{
    const char* texts[4] = { first, second, third, fourth };
    int i;

    for (i = 0; i < 4; i++) {
        if (texts[i])
            control_set_text(dialog_find_control(event_dialog(event), NULL, stats_buttons[i], 0),
                             NULL, texts[i]);
    }
}

/* Empty the four columns. */
PORT(0x0044DAB0)
void __cdecl stats_clear(ui_event* event)
{
    int i;

    for (i = 0; i < 4; i++)
        list_clear(dialog_find_control(event_dialog(event), NULL, stats_lists[i], 0));
}

/* A row: a text for each column; NULL adds nothing to one. */
PORT(0x0044DB10)
void __cdecl stats_add_row(ui_event* event, const char* first, const char* second,
                           const char* third, const char* fourth)
{
    const char* texts[4] = { first, second, third, fourth };
    int i;

    for (i = 0; i < 4; i++) {
        if (texts[i])
            list_add_text(dialog_find_control(event_dialog(event), NULL, stats_lists[i], 0), NULL,
                          texts[i], 0, 0);
    }
}

/* ---- Prompts ---- */

/*
    Small dialogs of a prompt and buttons. Their handlers (vtable slot 29) word them when they
    open (event 5) and end the dialog with a code for the screen behind when a button is
    clicked (event 1, naming the button).
*/
#define BUTTON_LEFT   NAME(0x0056F888)      /* "ButLeft" */
#define BUTTON_MIDDLE NAME(0x0056F87C)      /* "ButMiddle" */
#define BUTTON_RIGHT  NAME(0x0056F870)      /* "ButRight" */
#define TITLE_TEXT    NAME(0x0056F864)      /* "TitleText" */
#define PROMPT_TEXT   NAME(0x0056F858)      /* "TxtPrompt" */
#define KEY_RETURN    0x1C

enum { EVENT_CLICK = 1, EVENT_OPEN = 5 };

static void load_text(BYTE* self, const char* name, int type, UINT id)
{
    control_load_text(dialog_find_control(self, NULL, name, type), NULL, game_language, id);
}

/* The prompt's text, wrapped (0x1E8). */
static void load_prompt(BYTE* self, UINT id)
{
    BYTE* prompt = dialog_find_control(self, NULL, PROMPT_TEXT, 12);

    FIELD(prompt, 0x1E8, DWORD) = 1;
    control_load_text(prompt, NULL, game_language, id);
}

/* One button, the middle, taking Return; the others hidden. */
static void one_button(BYTE* self, UINT id)
{
    BYTE* button = dialog_find_control(self, NULL, BUTTON_LEFT, 1);

    control_show(button, NULL, FALSE, TRUE);
    FIELD(button, 0x1D8, DWORD) = 0;
    control_show(dialog_find_control(self, NULL, BUTTON_RIGHT, 1), NULL, FALSE, TRUE);
    button = dialog_find_control(self, NULL, BUTTON_MIDDLE, 1);
    control_load_text(button, NULL, game_language, id);
    FIELD(button, 0x1D8, DWORD) = KEY_RETURN;
}

static BOOL clicked(const ui_event* event, const char* button)
{
    return !game_FUN_0053b6e0(button, (const char*) event->b);
}

/* Three choices: left ends with 60, middle with 61, right with 0. */
PORT(0x0044DBB0)
void __fastcall choice_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_LEFT)) {
            dialog_end(event_dialog(event), NULL, 60);
            event->e = 1;
        } else if (clicked(event, BUTTON_MIDDLE)) {
            dialog_end(event_dialog(event), NULL, 61);
            event->e = 1;
        } else if (clicked(event, BUTTON_RIGHT)) {
            dialog_end(event_dialog(event), NULL, 0);
            event->e = 1;
        }
    } else if (event->type == EVENT_OPEN) {
        load_text(self, BUTTON_LEFT, 1, 0x13E7);
        load_text(self, BUTTON_MIDDLE, 1, 0x13E6);
        load_text(self, BUTTON_RIGHT, 1, 0x13DC);
        load_text(self, TITLE_TEXT, 12, 0x13E8);
        load_prompt(self, 0x13FE);
    }
}

/* A notice whose one button ends it with 51. */
PORT(0x0044DD20)
void __fastcall notice_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_MIDDLE)) {
            dialog_end(event_dialog(event), NULL, 51);
            event->e = 1;
        }
    } else if (event->type == EVENT_OPEN) {
        one_button(self, 0x13E9);
        load_text(self, TITLE_TEXT, 12, 0x13EA);
        load_prompt(self, 0x1400);
    }
}

/* A notice whose one button ends it with 0, leaving the event to others. */
PORT(0x0044DE40)
void __fastcall notice_close_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_MIDDLE))
            dialog_end(event_dialog(event), NULL, 0);
    } else if (event->type == EVENT_OPEN) {
        one_button(self, 0x13E9);
        load_text(self, TITLE_TEXT, 12, 0x146D);
        load_prompt(self, 0x146E);
    }
}
