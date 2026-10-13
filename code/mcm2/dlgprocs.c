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
BYTE*       __fastcall dialog_construct(BYTE* self, void* edx_unused, DWORD flag, const char* name);
int         __fastcall dialog_tick(BYTE* self, void* edx_unused, float seconds);
int         __fastcall dialog_release(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg);
int         __fastcall list_select_all(BYTE* self, void* edx_unused, int index);
BOOL        __fastcall game_FUN_004a3ef0(void* stream);                 /* start a movie */
BYTE*       __fastcall gui_manager_open(gui_manager* self, void* edx_unused, BYTE* dialog, DWORD a,
                                        DWORD flags, DWORD area, BYTE* parent, DWORD arg, DWORD c,
                                        DWORD wait);
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

#define DLGPROCS_CPP NAME(0x0056F744)     /* "...\dlgprocs.cpp" */

/* Less as the x87 compare tests it (C0 set): unordered counts too. */
static BOOL x87_less(double a, double b)
{
    return !(a >= b);
}

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

/* ---- Screens ---- */

/*
    A screen with a movie (0x7F58) that starts 4 seconds after it is ready (0x7F78), while its
    page at 0x7F60 is hidden. Vtable slot 10: tick.
*/
PORT(0x0044D6F0)
int __fastcall movie_screen_tick(BYTE* self, void* edx_unused, float seconds)
{
    (void) edx_unused;
    if (FIELD(self, 0x7F6C, int)) {
        BYTE* over = dialog_find_control(self, NULL, NAME(0x0056F724), 0);  /* ScreenOverCtl */

        VIRTUAL(over, 0xC8, method_0)(over);
        if (FIELD(self, 0x7F6C, int) && !FIELD(FIELD(self, 0x7F60, BYTE*), 0x68, int)
            && FIELD(self, 0x7F78, int)) {
            if (x87_less(FIELD(self, 0x7F7C, float), 4.0f)) {
                FIELD(self, 0x7F7C, float) = (float) ((double) FIELD(game_main, 0x2F0, float)
                                                      + FIELD(self, 0x7F7C, float));
                return dialog_tick(self, NULL, seconds);
            }
            FIELD(self, 0x7F78, int) = 0;
            game_FUN_004a3ef0(FIELD(FIELD(self, 0x7F58, BYTE*), 0x1EC, void*));
            VIRTUAL(FIELD(FIELD(self, 0x7F58, BYTE*), 0x1EC, void*), 0x40, method_1)(
                FIELD(FIELD(self, 0x7F58, BYTE*), 0x1EC, void*), NULL, FIELD(self, 0x7F78, DWORD));
            control_show(FIELD(self, 0x7F58, BYTE*), NULL, TRUE, TRUE);
        }
    }
    return dialog_tick(self, NULL, seconds);
}

/* The single-player screen's pages: an event's, the bike's, the rider's. */
#define SP_PAGES 0x7F58             /* BYTE*[3], each a dialog open over the screen */

static const struct {
    DWORD       size;
    int         line;
    const char* file;
    void**      vtable;
} sp_pages[3] = {
    { 0x7F68, 0x545, NAME(0x0056F95C), (void**) 0x00556D68 },               /* SPEvent.dtm */
    { 0x7F9C, 0x54B, NAME(0x0056F968), (void**) 0x00556DEC },               /* SPBikeR.dtm */
    { 0x7F58, 0x551, NAME(0x0056F974), (void**) 0x00556E70 },               /* SPRInfo.dtm */
};

/* Show page `page` (0 to 2) over the screen, closing the others; one already open stays. */
PORT(0x0044E600)
void __fastcall sp_show_page(BYTE* self, void* edx_unused, int page)
{
    RECT area = { 16, 41, 626, 396 };
    BYTE* dialog;
    int i;

    (void) edx_unused;
    for (i = 0; i < 3; i++) {
        BYTE** open = &FIELD(self, SP_PAGES + 4 * i, BYTE*);

        if (*open && page != i) {
            object_destroy(*open);
            *open = NULL;
        }
    }
    if (page < 0 || page > 2 || FIELD(self, SP_PAGES + 4 * page, BYTE*))
        return;
    dialog = game_new(sp_pages[page].size, DLGPROCS_CPP, sp_pages[page].line);
    if (dialog) {
        dialog_construct(dialog, NULL, 1, sp_pages[page].file);
        *(void***) dialog = sp_pages[page].vtable;
    }
    FIELD(self, SP_PAGES + 4 * page, BYTE*) = dialog;
    gui_manager_open(FIELD(FIELD(game_main, 0x56C, BYTE*), 0x2C, gui_manager*), NULL, dialog, 0, 1,
                     (DWORD) &area, self, 0, 0, 1);
}

/* ---- Loading screens ---- */

/* loading.dtm, named for the event it loads (the name it reads its sections by). */
static BYTE* loading_construct(BYTE* self, DWORD flag, void** vtable, const char* name)
{
    dialog_construct(self, NULL, flag, NAME(0x0056FD04));                   /* loading.dtm */
    *(void***) self = vtable;
    text_copy((char*) self + 0x38, name);
    return self;
}

PORT(0x00451C50)
BYTE* __fastcall loading_supercross_construct(BYTE* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    return loading_construct(self, flag, (void**) 0x00556EF8, NAME(0x0056FCF8));   /* LoadSX.dtm */
}

PORT(0x00451CA0)
BYTE* __fastcall loading_enduro_construct(BYTE* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    return loading_construct(self, flag, (void**) 0x00556F7C, NAME(0x0056FD2C));   /* LoadEnd.dtm */
}

PORT(0x00451CF0)
BYTE* __fastcall loading_quarry_construct(BYTE* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    return loading_construct(self, flag, (void**) 0x00557000, NAME(0x0056FD54));   /* LoadQuar.dtm */
}

PORT(0x00451D40)
BYTE* __fastcall loading_national_construct(BYTE* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    return loading_construct(self, flag, (void**) 0x00557084, NAME(0x0056FD88));   /* LoadNat.dtm */
}

PORT(0x00451D90)
BYTE* __fastcall loading_tag_construct(BYTE* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    return loading_construct(self, flag, (void**) 0x00557108, NAME(0x0056FDB4));   /* LoadTag.dtm */
}

PORT(0x00451DE0)
BYTE* __fastcall loading_baja_construct(BYTE* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    return loading_construct(self, flag, (void**) 0x0055718C, NAME(0x0056FDDC));   /* LoadBaja.dtm */
}

/* ---- Ghost races ---- */

/*
    The ghost race picker keeps the recordings it found: the one chosen (0x7F58), and an array
    of 0x7F64 paths (0x7F60), all on the game's heap.
*/
#define GHOST_CHOSEN 0x7F58
#define GHOST_PATHS  0x7F60
#define GHOST_COUNT  0x7F64

void __fastcall game_FUN_0044fb30(BYTE* self, void* edx_unused, const char* path);  /* add one */

PORT(0x0044FDB0)
void __fastcall ghosts_free(BYTE* self)
{
    if (FIELD(self, GHOST_CHOSEN, void*))
        game_FUN_004a4450(FIELD(self, GHOST_CHOSEN, void*), DLGPROCS_CPP, 0x77A);
    if (FIELD(self, GHOST_PATHS, char**)) {
        int i;

        for (i = 0; i < FIELD(self, GHOST_COUNT, int); i++)
            game_FUN_004a4450(FIELD(self, GHOST_PATHS, char**)[i], DLGPROCS_CPP, 0x77E);
        game_FUN_004a4450(FIELD(self, GHOST_PATHS, char**), DLGPROCS_CPP, 0x780);
    }
    FIELD(self, GHOST_CHOSEN, void*) = NULL;
    FIELD(self, GHOST_PATHS, char**) = NULL;
    FIELD(self, GHOST_COUNT, int) = 0;
    FIELD(self, 0x7F5C, DWORD) = 0;
}

/* DDLGhostRaces: "none" (string 5319), then each Record\*.gho there is. */
PORT(0x0044FA20)
void __fastcall ghosts_fill(BYTE* self)
{
    BYTE* list;
    char none[0x80];
    char path[0x104];
    WIN32_FIND_DATAA found;
    HANDLE search;

    ghosts_free(self);
    list = FIELD(dialog_find_control(self, NULL, NAME(0x0056F9CC), 6), 0x1FC, BYTE*);
    list_clear(list);
    game_FUN_00526fc0(game_main, NULL, 0x14C7, none, sizeof none);
    list_add_text(list, NULL, none, (DWORD) -1, 0);
    list_select_all(list, NULL, 0);
    search = FindFirstFileA(NAME(0x0056FB6C), &found);                      /* Record\*.gho */
    if (search == INVALID_HANDLE_VALUE)
        return;
    do {
        game_FUN_0053a11f(path, NAME(0x0056C7DC), NAME(0x0056FB64), found.cFileName);  /* %s\%s */
        game_FUN_0044fb30(self, NULL, path);
    } while (FindNextFileA(search, &found));
    FindClose(search);
}

/* ---- Odds and ends ---- */

/* Slot 0 of a dialog kind that frees nothing of its own. */
PORT(0x0044E810)
BYTE* __fastcall bare_dialog_delete(BYTE* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    *(void***) self = (void**) 0x00556CD8;
    if (flags & 1)
        game_delete(self);
    return self;
}

/* Vtable slot 22: the first mouse button released ends the hold (0x7F7C). */
PORT(0x004508E0)
int __fastcall hold_release(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg)
{
    (void) edx_unused;
    if (key[1] == 1 && key[0] == 0)
        FIELD(self, 0x7F7C, DWORD) = 0;
    return dialog_release(self, NULL, key, arg);
}

/* ---- Waiting screens ---- */

/* Set when a key ends a wait screen; the race reads it. */
#define wait_skipped (*(DWORD*) 0x005A0F50)

BYTE* __fastcall game_FUN_0045e300(BYTE* events);   /* the race under way, or NULL */

/* Vtable slot 23: any key ends the wait, and closes the screen. */
PORT(0x00454220)
int __fastcall wait_key_close(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg)
{
    (void) edx_unused;
    (void) key;
    (void) arg;
    wait_skipped = 1;
    dialog_end(self, NULL, 0);
    return 1;
}

/* Vtable slot 23: any key ends the wait. */
PORT(0x004543A0)
int __fastcall wait_key(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg)
{
    (void) self;
    (void) edx_unused;
    (void) key;
    (void) arg;
    wait_skipped = 1;
    return 1;
}

/* Vtable slot 23: a key ends the wait once the screen allows it (0x7F60, or 0x7F5C, set). */
PORT(0x00454570)
int __fastcall wait_key_when_ready(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg)
{
    (void) edx_unused;
    (void) key;
    (void) arg;
    if (FIELD(self, 0x7F60, int) == 1)
        wait_skipped = 1;
    return 1;
}

PORT(0x004546D0)
int __fastcall wait_key_when_shown(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg)
{
    (void) edx_unused;
    (void) key;
    (void) arg;
    if (FIELD(self, 0x7F5C, int) == 1)
        wait_skipped = 1;
    return 1;
}

/*
    Vtable slot 10 of the wait screens. A count of ticks (0x7F58) moves the session on once the
    screen has drawn; a timer ends the wait after 3 seconds, or when a key skipped it.
*/
#define WAIT_SECONDS 3.0

void __fastcall game_FUN_00499ca0(BYTE* session, void* edx_unused, int stage);

PORT(0x00454350)
int __fastcall wait_stage_tick(BYTE* self, void* edx_unused, float seconds)
{
    (void) edx_unused;
    if (++FIELD(self, 0x7F58, int) == 3) {
        game_FUN_00499ca0(FIELD(game_main, 0x56C, BYTE*), NULL, 1);
        dialog_end(self, NULL, 0);
    }
    return dialog_tick(self, NULL, seconds);
}

/* The time waited (0x7F58); keys count once it has ticked (0x7F5C). */
PORT(0x00454680)
int __fastcall wait_timed_tick(BYTE* self, void* edx_unused, float seconds)
{
    double waited = (double) seconds + FIELD(self, 0x7F58, float);

    (void) edx_unused;
    FIELD(self, 0x7F5C, int) = 1;
    FIELD(self, 0x7F58, float) = (float) waited;
    if (wait_skipped || waited > WAIT_SECONDS)
        dialog_end(self, NULL, 0);
    return dialog_tick(self, NULL, seconds);
}

/* Both: the session moves on at the third tick, and the wait (0x7F5C) ends no sooner. */
PORT(0x004544F0)
int __fastcall wait_stage_timed_tick(BYTE* self, void* edx_unused, float seconds)
{
    double waited;

    (void) edx_unused;
    if (++FIELD(self, 0x7F58, int) == 3)
        game_FUN_00499ca0(FIELD(game_main, 0x56C, BYTE*), NULL, 2);
    waited = (double) seconds + FIELD(self, 0x7F5C, float);
    FIELD(self, 0x7F60, int) = 1;
    FIELD(self, 0x7F5C, float) = (float) waited;
    if ((wait_skipped || waited > WAIT_SECONDS) && FIELD(self, 0x7F58, int) >= 3)
        dialog_end(self, NULL, 0);
    return dialog_tick(self, NULL, seconds);
}

/* Vtable slot 23: a key closes the screen once the race under way has finished (0x18A). */
PORT(0x00456AE0)
int __fastcall race_over_key(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg)
{
    BYTE* race = game_FUN_0045e300(FIELD(game_main, 0x570, BYTE*));

    (void) edx_unused;
    (void) key;
    (void) arg;
    if (race && FIELD(race, 0x18A, BYTE))
        dialog_end(self, NULL, 0);
    return 1;
}

/* Vtable slot 10: the session's request to leave the menus (0x44) closes the screen. */
PORT(0x00456B20)
int __fastcall leave_request_tick(BYTE* self, void* edx_unused, float seconds)
{
    BYTE* session = FIELD(game_main, 0x56C, BYTE*);

    (void) edx_unused;
    if (FIELD(session, 0x44, int)) {
        FIELD(session, 0x44, int) = 0;
        dialog_end(self, NULL, 0);
        return 0;
    }
    return dialog_tick(self, NULL, seconds);
}

/* ---- Asking ---- */

/* Three choices ending the dialog itself: left with 101, middle with 102, right with 103. */
PORT(0x004565B0)
void __fastcall three_way_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type != EVENT_CLICK)
        return;
    if (clicked(event, BUTTON_LEFT))
        dialog_end(self, NULL, 0x65);
    else if (clicked(event, BUTTON_MIDDLE))
        dialog_end(self, NULL, 0x66);
    else if (clicked(event, BUTTON_RIGHT))
        dialog_end(self, NULL, 0x67);
}

/*
    A line of text to type: the caller's buffer (0x7F58) and its size (0x7F5C) take the edit
    box's text when OK ends the dialog with 201; Cancel ends it with 202.
*/
#define EDIT_BOX      NAME(0x0056FE58)      /* "EditBox" */
#define OK_BUTTON     NAME(0x0056FE60)      /* "OkButton" */
#define CANCEL_BUTTON NAME(0x0056FF44)      /* "CancelButton" */

int   __fastcall edit_set_capacity(BYTE* self, void* edx_unused, int length);
int   __fastcall edit_set_text(BYTE* self, void* edx_unused, const char* text);
char* __fastcall edit_get_text(BYTE* self, void* edx_unused, char* buffer, int size);

/* Out of line: the ids variant calls it, as in the original. */
__declspec(noinline) void __fastcall text_entry_setup(BYTE* self, void* edx_unused,
                                                      const char* title, const char* prompt,
                                                      char* buffer, int size);

PORT(0x00456840)
void __fastcall text_entry_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, OK_BUTTON)) {
            edit_get_text(dialog_find_control(self, NULL, EDIT_BOX, 11), NULL,
                          FIELD(self, 0x7F58, char*), FIELD(self, 0x7F5C, int));
            dialog_end(self, NULL, 0xC9);
        } else if (clicked(event, CANCEL_BUTTON)) {
            dialog_end(self, NULL, 0xCA);
        }
    } else if (event->type == EVENT_OPEN) {
        FIELD(self, 0x7F58, char*) = NULL;
        FIELD(self, 0x7F5C, int) = 0;
        gui_user_queue_capture(FIELD(self, 0x34, gui_user*), NULL,
                               dialog_find_control(self, NULL, EDIT_BOX, 0), NULL, FALSE);
    }
}

PORT(0x004568F0)
void __fastcall text_entry_setup(BYTE* self, void* edx_unused, const char* title,
                                 const char* prompt, char* buffer, int size)
{
    BYTE* edit;

    (void) edx_unused;
    FIELD(self, 0x7F58, char*) = buffer;
    FIELD(self, 0x7F5C, int) = size;
    control_set_text(dialog_find_control(self, NULL, TITLE_TEXT, 0), NULL, title);
    control_set_text(dialog_find_control(self, NULL, PROMPT_TEXT, 0), NULL, prompt);
    edit = dialog_find_control(self, NULL, EDIT_BOX, 11);
    edit_set_capacity(edit, NULL, size - 1);
    edit_set_text(edit, NULL, buffer);
}

/* The same, the title and prompt given by string ids where those are not 0. */
PORT(0x00456970)
void __fastcall text_entry_setup_ids(BYTE* self, void* edx_unused, const char* title,
                                     UINT title_id, const char* prompt, UINT prompt_id,
                                     char* buffer, int size)
{
    char title_text[0x80];
    char prompt_text[0x80];

    (void) edx_unused;
    if (title_id)
        game_FUN_00526fc0(game_main, NULL, title_id, title_text, sizeof title_text);
    if (prompt_id) {
        game_FUN_00526fc0(game_main, NULL, prompt_id, prompt_text, sizeof prompt_text);
        prompt = prompt_text;
    }
    text_entry_setup(self, NULL, title_id ? title_text : title, prompt, buffer, size);
}

/* ---- Leaving ---- */

void __fastcall game_FUN_0045f530(BYTE* events);
void __fastcall game_FUN_0049aaa0(BYTE* session, void* edx_unused, int screen);
void __fastcall game_FUN_00526eb0(BYTE* game, void* edx_unused, DWORD a, DWORD b, DWORD c);

/* The session's next screen. */
static void go_to(int screen)
{
    game_FUN_0049aaa0(FIELD(game_main, 0x56C, BYTE*), NULL, screen);
}

/*
    Where a screen's result code takes the session. Those it knows end the screen that sent the
    event (with 14 for 252, else 0) and mark the event handled; 400 asks the game instead.
*/
PORT(0x00453EA0)
void __cdecl result_go(int code, ui_event* event)
{
    DWORD end = 0;
    BYTE* session;
    int screen;

    switch (code) {
    case 0x10E:
        game_FUN_0045f530(FIELD(game_main, 0x570, BYTE*));
        go_to(FIELD(game_main, 0x8, int) ? 0x867 : 0x65);
        break;
    case 0x65:
        go_to(100);
        break;
    case 0x66:
        go_to(FIELD(FIELD(game_main, 0x56C, BYTE*), 0x30, int));
        break;
    case 0xFC:
        end = 14;
        break;
    case 0x190:
        game_FUN_00526eb0(game_main, NULL, 0, 400, 1);
        return;
    case 0x910:
    case 0x911:
        session = FIELD(game_main, 0x56C, BYTE*);
        screen = FIELD(session, 0x3C, int);
        if (screen == 0x910)
            go_to(0x65);
        else if (screen == 0x911)
            go_to(FIELD(session, 0x38, int));
        break;
    default:
        return;
    }
    dialog_end(event_dialog(event), NULL, end);
    event->e = 1;
}

/* ---- Records ---- */

/* A record of three 128-character names and the numbers after them, emptied; scales 1, 1, 0.5. */
PORT(0x00456BA0)
BYTE* __fastcall record_init(BYTE* self)
{
    int offset;

    self[0] = 0;
    self[0x80] = 0;
    self[0x100] = 0;
    for (offset = 0x180; offset <= 0x1B8; offset += 4)
        FIELD(self, offset, DWORD) = 0;
    FIELD(self, 0x1BC, float) = 1.0f;
    FIELD(self, 0x1C0, float) = 1.0f;
    FIELD(self, 0x1C4, DWORD) = 0;
    FIELD(self, 0x1C8, float) = 0.5f;
    FIELD(self, 0x204, DWORD) = 0;
    FIELD(self, 0x20C, DWORD) = 0;
    FIELD(self, 0x208, DWORD) = 0;
    return self;
}

/* ---- Numbers ---- */

char* __cdecl game_FUN_0054aaa5(int value, char* text, int radix);   /* _itoa */

PORT(0x00452190)
char* __cdecl number_text(int value, char* text)
{
    return game_FUN_0054aaa5(value, text, 10);
}

/* ---- Startup ---- */

/* The file's statics, set up by the runtime before main: unit vectors and two timers. */
typedef struct vector3 {
    float x, y, z;
} vector3;

#define vector_zero   (*(vector3*) 0x005A0F10)
#define vector_x      (*(vector3*) 0x005A0F20)
#define vector_y      (*(vector3*) 0x005A0F40)
#define vector_z      (*(vector3*) 0x005A0F00)

void __fastcall game_FUN_004cfdc0(void* timer, void* edx_unused, DWORD period);  /* start it */

/* Out of line, so the entries below stay jumps to them as in the original. */
__declspec(noinline) void __cdecl init_vector_zero(void);
__declspec(noinline) void __cdecl init_vector_x(void);
__declspec(noinline) void __cdecl init_vector_y(void);
__declspec(noinline) void __cdecl init_vector_z(void);
__declspec(noinline) void __cdecl init_timer_a(void);
__declspec(noinline) void __cdecl init_timer_b(void);

static void set_vector(vector3* v, float x, float y, float z)
{
    v->x = x;
    v->y = y;
    v->z = z;
}

PORT(0x00454B30)
void __cdecl init_vector_zero(void)
{
    set_vector(&vector_zero, 0.0f, 0.0f, 0.0f);
}

PORT(0x00454B80)
void __cdecl init_vector_x(void)
{
    set_vector(&vector_x, 1.0f, 0.0f, 0.0f);
}

PORT(0x00454BD0)
void __cdecl init_vector_y(void)
{
    set_vector(&vector_y, 0.0f, 1.0f, 0.0f);
}

PORT(0x00454E50)
void __cdecl init_vector_z(void)
{
    set_vector(&vector_z, 0.0f, 0.0f, 1.0f);
}

PORT(0x00456B70)
void __cdecl init_timer_a(void)
{
    game_FUN_004cfdc0((void*) 0x005A1040, NULL, 5000);
}

PORT(0x00456B90)
void __cdecl init_timer_b(void)
{
    game_FUN_004cfdc0((void*) 0x005A1018, NULL, 5000);
}

/*
    The runtime's table calls these, which only jump on. The entries for 0x00454B20 and
    0x00454B70 read as text to the exporter, so the table keeps the originals; they jump to the
    ports above all the same.
*/
PORT(0x00454BC0)
void __cdecl init_vector_y_entry(void)
{
    init_vector_y();
}

PORT(0x00454E40)
void __cdecl init_vector_z_entry(void)
{
    init_vector_z();
}

PORT(0x00456B60)
void __cdecl init_timer_a_entry(void)
{
    init_timer_a();
}

PORT(0x00456B80)
void __cdecl init_timer_b_entry(void)
{
    init_timer_b();
}
