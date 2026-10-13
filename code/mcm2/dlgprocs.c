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
DWORD       __fastcall list_data(BYTE* self, void* edx_unused, int index);

#define COMBO_LIST 0x1FC     /* a combo box's list */

/* The game's: atoi, a string from its language module, and the profile it saves to. */
int  __cdecl    game_FUN_0053b6c7(const char* text);
int  __cdecl    game_FUN_0053b6e0(const char* a, const char* b);           /* _stricmp */
void __fastcall game_FUN_00526fc0(BYTE* game, void* edx_unused, UINT id, char* text, int size);
void __fastcall game_FUN_00528820(char* profile);                         /* save it */
int  __cdecl    game_FUN_0053a11f(char* to, const char* format, ...);      /* sprintf */
char* __cdecl   game__strncpy(char* to, const char* from, size_t count);
#define game_profile ((char*) game_main + 0x578)    /* char[16]: the player's profile */
#define game_language FIELD(game_main, 0x420, HINSTANCE)
#define game_session FIELD(game_main, 0x56C, BYTE*)       /* the menus' session */

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

enum { EVENT_CLICK = 1, EVENT_OPEN = 5, EVENT_CLOSE = 6, EVENT_KEY = 11 };

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


/*
    The chosen record and the listed records' paths, freed. The ghost and replay screens share
    this layout; `line` is the first free's line in the leak report, the others follow it.
*/
static void records_free(BYTE* self, int line)
{
    if (FIELD(self, GHOST_CHOSEN, void*))
        game_FUN_004a4450(FIELD(self, GHOST_CHOSEN, void*), DLGPROCS_CPP, line);
    if (FIELD(self, GHOST_PATHS, char**)) {
        int i;

        for (i = 0; i < FIELD(self, GHOST_COUNT, int); i++)
            game_FUN_004a4450(FIELD(self, GHOST_PATHS, char**)[i], DLGPROCS_CPP, line + 4);
        game_FUN_004a4450(FIELD(self, GHOST_PATHS, char**), DLGPROCS_CPP, line + 6);
    }
    FIELD(self, GHOST_CHOSEN, void*) = NULL;
    FIELD(self, GHOST_PATHS, char**) = NULL;
    FIELD(self, GHOST_COUNT, int) = 0;
    FIELD(self, 0x7F5C, DWORD) = 0;
}

PORT(0x0044FDB0)
void __fastcall ghosts_free(BYTE* self)
{
    records_free(self, 0x77A);
}

/*
    A ghost file is the 0xEB0-byte record of a race: the rider (0x8), the time (0x28, float),
    and the track it was run on, by kind (0x60) and name (0x62). The session's tracks are
    0x54 bytes each from its 0x60: the kind at 0x4, the name at 0x14.
*/
#define RECORD_SIZE     0xEB0
#define RECORD_RIDER    0x8
#define RECORD_TIME     0x28
#define RECORD_KIND     0x60
#define RECORD_TRACK    0x62
#define LIST_TEXT_BOX   NAME(0x0056FA10)    /* "DDLTextBox": the track chosen */
#define LIST_GHOSTS     NAME(0x0056F9CC)    /* "DDLGhostRaces" */
#define GHOST_LABEL     NAME(0x0056FB7C)    /* "%s: %s" */

void*  __cdecl game_FUN_0053a35b(const char* path, const char* mode);              /* fopen */
size_t __cdecl game_FUN_0053a5d5(void* to, size_t size, size_t count, void* file); /* fread */
int    __cdecl game_FUN_0053a2ad(void* file);                                      /* fclose */
void*  __cdecl ui_realloc(void* block, size_t size);

/*
    Read the ghost at `path`; one of a lap on the track chosen in DDLTextBox, with a time, is
    kept with its path and listed in DDLGhostRaces. TRUE when it was. (The original first runs
    the record's member constructors, which the read then overwrites.)
*/
/* Out of line, as the original calls it: its 4 KB frame must not join a caller's. */
__declspec(noinline) BOOL __fastcall ghost_add(BYTE* self, void* edx_unused, const char* path);

PORT(0x0044FB30)
BOOL __fastcall ghost_add(BYTE* self, void* edx_unused, const char* path)
{
    char what[0x80];
    char label[0x80];
    BYTE record[RECORD_SIZE];
    BOOL added = FALSE;
    void* file;

    (void) edx_unused;
    file = game_FUN_0053a35b(path, NAME(0x0056C66C));                          /* "rb" */
    if (!file)
        return FALSE;
    if (game_FUN_0053a5d5(record, RECORD_SIZE, 1, file)) {
        BYTE* chosen = FIELD(dialog_find_control(self, NULL, LIST_TEXT_BOX, 6), COMBO_LIST, BYTE*);
        int track = (int) list_data(chosen, NULL, -1) * 0x54;

        if (!game_FUN_0053b6e0((const char*) record + RECORD_TRACK,
                               FIELD(game_session, 0x60, char*) + track + 0x14)
            && FIELD(record, RECORD_TIME, float) > 0.0f
            && FIELD(FIELD(game_session, 0x60, BYTE*) + track, 0x4, DWORD)
               == FIELD(record, RECORD_KIND, BYTE)) {
            int count = FIELD(self, 0x7F5C, int);
            char** paths;

            FIELD(self, 0x7F58, BYTE*) = ui_realloc(FIELD(self, 0x7F58, BYTE*),
                                                    (count + 1) * RECORD_SIZE);
            __movsd((unsigned long*) (FIELD(self, 0x7F58, BYTE*) + count * RECORD_SIZE),
                    (const unsigned long*) record, RECORD_SIZE / 4);
            FIELD(self, 0x7F5C, int)++;

            paths = ui_realloc(FIELD(self, GHOST_PATHS, char**),
                               (FIELD(self, GHOST_COUNT, int) + 1) * sizeof(char*));
            FIELD(self, GHOST_PATHS, char**) = paths;
            paths[FIELD(self, GHOST_COUNT, int)] = game_FUN_004a4410(text_length(path) + 1,
                                                                    DLGPROCS_CPP, 0x763);
            text_copy(paths[FIELD(self, GHOST_COUNT, int)], path);
            FIELD(self, GHOST_COUNT, int)++;

            chosen = FIELD(dialog_find_control(self, NULL, LIST_GHOSTS, 6), COMBO_LIST, BYTE*);
            game_FUN_00526fc0(game_main, NULL, 0x14C8, what, sizeof what);
            game_FUN_0053a11f(label, GHOST_LABEL, what, (const char*) record + RECORD_RIDER);
            list_add_text(chosen, NULL, label, FIELD(self, 0x7F5C, int) - 1, 0);
            added = TRUE;
        }
    }
    game_FUN_0053a2ad(file);
    return added;
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
        ghost_add(self, NULL, path);
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

/* The prompt's title and text, and the buttons' words; a button given none is hidden. */
__declspec(noinline) void __fastcall prompt_setup(BYTE* self, void* edx_unused,
                                                  const char* title, const char* prompt,
                                                  const char* left, const char* middle,
                                                  const char* right);

static void set_or_hide(BYTE* control, const char* text)
{
    if (text && *text)
        control_set_text(control, NULL, text);
    else
        control_show(control, NULL, FALSE, TRUE);
}

PORT(0x00456630)
void __fastcall prompt_setup(BYTE* self, void* edx_unused, const char* title, const char* prompt,
                             const char* left, const char* middle, const char* right)
{
    (void) edx_unused;
    control_set_text(dialog_find_control(self, NULL, TITLE_TEXT, 0), NULL, title);
    control_set_text(dialog_find_control(self, NULL, PROMPT_TEXT, 0), NULL, prompt);
    set_or_hide(dialog_find_control(self, NULL, BUTTON_LEFT, 0), left);
    set_or_hide(dialog_find_control(self, NULL, BUTTON_MIDDLE, 0), middle);
    set_or_hide(dialog_find_control(self, NULL, BUTTON_RIGHT, 0), right);
}

/* The same, each text given by a string id where that is not 0. */
PORT(0x00456700)
void __fastcall prompt_setup_ids(BYTE* self, void* edx_unused, const char* title, UINT title_id,
                                 const char* prompt, UINT prompt_id, const char* left,
                                 UINT left_id, const char* middle, UINT middle_id,
                                 const char* right, UINT right_id)
{
    char texts[5][0x80];

    (void) edx_unused;
    if (title_id)
        game_FUN_00526fc0(game_main, NULL, title_id, texts[0], sizeof texts[0]);
    if (prompt_id)
        game_FUN_00526fc0(game_main, NULL, prompt_id, texts[1], sizeof texts[1]);
    if (left_id)
        game_FUN_00526fc0(game_main, NULL, left_id, texts[2], sizeof texts[2]);
    if (middle_id)
        game_FUN_00526fc0(game_main, NULL, middle_id, texts[3], sizeof texts[3]);
    if (right_id)
        game_FUN_00526fc0(game_main, NULL, right_id, texts[4], sizeof texts[4]);
    prompt_setup(self, NULL, title_id ? texts[0] : title, prompt_id ? texts[1] : prompt,
                 left_id ? texts[2] : left, middle_id ? texts[3] : middle,
                 right_id ? texts[4] : right);
}

/*
    Asks about the thing named in the event's parameter: left ends with 31, right with 0.
    The title reads 'name' and string 0x13CF.
*/
#define QUOTED_NAME NAME(0x0056FF60)    /* "'%s' %s" */

PORT(0x00453180)
void __fastcall named_choice_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        DWORD code;

        if (clicked(event, BUTTON_LEFT))
            code = 0x1F;
        else if (clicked(event, BUTTON_RIGHT))
            code = 0;
        else
            return;
        dialog_end(event_dialog(event), NULL, code);
        event->e = 1;
    } else if (event->type == EVENT_OPEN) {
        BYTE* title = dialog_find_control(self, NULL, TITLE_TEXT, 12);
        BYTE* button = dialog_find_control(self, NULL, BUTTON_LEFT, 1);
        char what[0x80];
        char text[0x100];

        control_load_text(button, NULL, game_language, 0x13EC);
        FIELD(button, 0x1D8, DWORD) = 0;
        button = dialog_find_control(self, NULL, BUTTON_RIGHT, 1);
        control_load_text(button, NULL, game_language, 0x13EB);
        FIELD(button, 0x1D8, DWORD) = 0;
        control_show(dialog_find_control(self, NULL, BUTTON_MIDDLE, 1), NULL, FALSE, TRUE);
        game_FUN_00526fc0(game_main, NULL, 0x13CF, what, sizeof what);
        game_FUN_0053a11f(text, QUOTED_NAME, (const char*) event->param, what);
        control_set_text(title, NULL, text);
        load_prompt(self, 0x1403);
    }
}

/*
    A prompt quoting the name in the event's parameter. Left or Enter answers through
    0x453740, Right or Escape through result_back, both with result 303.
*/
#define QUOTED      NAME(0x0056E540)    /* "\"%s\"" */
#define KEY_ESCAPE  0x1B
#define KEY_ENTER   0x0D

int  __fastcall control_set_align(BYTE* self, void* edx_unused, DWORD align);
void __cdecl    game_FUN_00453740(int code, ui_event* event);
void __cdecl    result_back(int code, ui_event* event);

static void set_string(BYTE* control, UINT id)
{
    char text[0x80];

    game_FUN_00526fc0(game_main, NULL, id, text, sizeof text);
    control_set_text(control, NULL, text);
}

PORT(0x00453300)
void __fastcall quoted_choice_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_RIGHT))
            result_back(0x12F, event);
        else if (clicked(event, BUTTON_LEFT))
            game_FUN_00453740(0x12F, event);
    } else if (event->type == EVENT_OPEN) {
        BYTE* title = dialog_find_control(self, NULL, TITLE_TEXT, 0);
        char text[0x80];

        game_FUN_00526fc0(game_main, NULL, 0xFEF, text, sizeof text);
        control_set_align(title, NULL, 10);
        control_set_text(title, NULL, text);
        game_FUN_0053a11f(text, QUOTED, (const char*) event->param);
        control_set_text(dialog_find_control(self, NULL, PROMPT_TEXT, 12), NULL, text);
        set_string(dialog_find_control(self, NULL, BUTTON_LEFT, 0), 0x13E9);
        set_string(dialog_find_control(self, NULL, BUTTON_RIGHT, 0), 0x13DC);
        control_show(dialog_find_control(self, NULL, BUTTON_MIDDLE, 1), NULL, FALSE, TRUE);
    } else if (event->type == EVENT_KEY) {
        if (event->d == KEY_ESCAPE)
            result_back(0x12F, event);
        else if (event->d == KEY_ENTER)
            game_FUN_00453740(0x12F, event);
    }
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

/* ---- Intro ---- */

#define session_gui     FIELD(game_session, 0x2C, gui_manager*)
#define cursors_locked  (FIELD(game_main, 0x2D5, BYTE) & 2)

void __fastcall gui_manager_show_cursors(gui_manager* self, void* edx_unused, BOOL show);
void __fastcall game_FUN_00469760(BYTE* game);

/* The intro's next screen, opened as this one closes. */
static void open_next(size_t size, int line, const char* name, void* vtable)
{
    BYTE* dialog = game_new(size, DLGPROCS_CPP, line);

    if (dialog) {
        dialog_construct(dialog, NULL, 1, name);
        *(void**) dialog = vtable;
    }
    gui_manager_open(session_gui, NULL, dialog, 0, 2, 0, NULL, 0, 0, 0);
}

/*
    The intro's screens open one another as each closes: intro, intro2, intro3, then the
    intro video, and the menus after it. A key skips them (see wait_skipped).
*/

/* Without the cursor; a key skips it unless the game asks otherwise (0xC44). Then intro2. */
PORT(0x00454240)
void __fastcall intro_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_OPEN) {
        gui_manager_show_cursors(session_gui, NULL, FALSE);
        FIELD(self, 0x7F58, DWORD) = 0;
        wait_skipped = !FIELD(game_main, 0xC44, int);
    } else if (event->type == EVENT_CLOSE && !cursors_locked) {
        open_next(0x7F64, 0x101A, NAME(0x0056FFA8), (void*) 0x00557294);     /* intro2.dtm */
    }
}

/* intro3.dtm: then the intro video. */
PORT(0x00454590)
void __fastcall intro3_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_OPEN) {
        FIELD(self, 0x7F5C, DWORD) = 0;
        FIELD(self, 0x7F58, DWORD) = 0;
        game_FUN_00469760(game_main);
    } else if (event->type == EVENT_CLOSE && !cursors_locked) {
        open_next(0x7F60, 0x106F, NAME(0x0056FFFC), (void*) 0x0055739C);     /* IntroVid.dtm */
    }
}

/* intro2.dtm: then intro3, or the menus when skipped. */
PORT(0x004543B0)
void __fastcall intro2_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_OPEN) {
        FIELD(self, 0x7F60, DWORD) = 0;
        FIELD(self, 0x7F5C, DWORD) = 0;
        FIELD(self, 0x7F58, DWORD) = 0;
        game_FUN_00469760(game_main);
    } else if (event->type == EVENT_CLOSE && !cursors_locked) {
        if (!wait_skipped) {
            open_next(0x7F60, 0x1041, NAME(0x0056FFD0), (void*) 0x00557318);   /* intro3.dtm */
        } else {
            go_to(100);
            gui_manager_show_cursors(session_gui, NULL, TRUE);
        }
    }
}

/* ---- Movies ---- */

BYTE* __fastcall movie_construct(BYTE* self, void* edx_unused, DWORD a, const RECT* area,
                                 BYTE* dialog);
BOOL  __fastcall movie_open(BYTE* self, void* edx_unused, const char* file, BOOL play, DWORD a,
                            DWORD b);
BYTE* __fastcall dialog_add(BYTE* self, void* edx_unused, BYTE* control, DWORD id, DWORD keep);
int   __fastcall game_FUN_0046a0d0(void* self);                 /* unlink from its parent */
#define object_unlink game_FUN_0046a0d0

#define MOVIE_ENDED 0x0044C1C0      /* what the movie calls when it ends */

/* A movie filling the 640x480 screen, added to it. */
static BYTE* movie_add(BYTE* self, int line)
{
    RECT area = { 0, 0, 640, 480 };
    BYTE* movie = game_new(0x200, DLGPROCS_CPP, line);

    if (movie)
        movie = movie_construct(movie, NULL, 0, &area, self);
    return movie;
}

/* IntroVid.dtm: ui\Intro.avi, unless skipped; the menus once it closes. */
PORT(0x004540D0)
void __fastcall intro_video_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_OPEN) {
        gui_manager_show_cursors(session_gui, NULL, FALSE);
        if (!wait_skipped) {
            FIELD(self, 0x7F5C, BYTE*) = movie_add(self, 0xFEF);
            dialog_add(self, NULL, FIELD(self, 0x7F5C, BYTE*), 0, 1);
            if (movie_open(FIELD(self, 0x7F5C, BYTE*), NULL, NAME(0x0056FF80), TRUE, MOVIE_ENDED,
                           (DWORD) self))
                return;
        }
        dialog_end(self, NULL, 0);
    } else if (event->type == EVENT_CLOSE && !cursors_locked) {
        go_to(100);
        gui_manager_show_cursors(session_gui, NULL, TRUE);
    }
}

/* The credits: ui\Credits.avi behind the Back button; the menus once it closes. */
PORT(0x00453F80)
void __fastcall credits_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, NAME(0x0056F8CC)))                                 /* "Back" */
            dialog_end(self, NULL, 0);
    } else if (event->type == EVENT_OPEN) {
        FIELD(self, 0x7F58, BYTE*) = movie_add(self, 0xFAD);
        dialog_add(self, NULL, FIELD(self, 0x7F58, BYTE*), 0, 1);
        object_unlink(FIELD(self, 0x7F58, BYTE*));
        object_move_before(FIELD(self, 0x7F58, BYTE*), NULL,
                           dialog_find_control(self, NULL, NAME(0x0056F8CC), 0), -1);
        if (!movie_open(FIELD(self, 0x7F58, BYTE*), NULL, NAME(0x0056FF70), TRUE, MOVIE_ENDED,
                        (DWORD) self))
            dialog_end(self, NULL, 0);
    } else if (event->type == EVENT_CLOSE && !cursors_locked) {
        go_to(100);
    }
}

/* ---- Previews ---- */

int   __fastcall dialog_draw_over(BYTE* self);
void  __fastcall game_FUN_00445c10(BYTE* model, void* edx_unused, DWORD a, BYTE* look,
                                   BYTE* display);

#define session_players FIELD(game_session, 0x48, BYTE*)    /* 200 bytes each */
#define player_model(player) FIELD(FIELD(player, 0xC0, BYTE*), 0x1A0, BYTE*)

/*
    Vtable slot 13: draw. A bike or rider picked from DDLBikes or DDLRiders since the last draw
    (0x7F84, 0x7F88) dresses the model showing it.
*/
PORT(0x00450DF0)
int __fastcall preview_draw(BYTE* self)
{
    int drawn = dialog_draw_over(self);
    BYTE* combo;
    BYTE* session;
    BYTE* look;

    if (FIELD(self, 0x110, BYTE*))
        background_release_dc(FIELD(self, 0x110, BYTE*));
    if (FIELD(self, 0x7F84, int)) {
        combo = dialog_find_control(self, NULL, NAME(0x0056FBCC), 6);       /* DDLBikes */
        session = game_session;
        look = FIELD(session, 0x50, BYTE*)
               + list_data(FIELD(combo, COMBO_LIST, BYTE*), NULL, -1) * 0x94;
        game_FUN_00445c10(player_model(FIELD(session, 0x48, BYTE*) + FIELD(look, 0, int) * 200),
                          NULL, 0, look + 0x48, game_main + 0x1C);
        FIELD(self, 0x7F84, int) = 0;
    }
    if (FIELD(self, 0x7F88, int)) {
        combo = dialog_find_control(self, NULL, NAME(0x0056FBC0), 6);       /* DDLRiders */
        session = game_session;
        look = FIELD(session, 0x58, BYTE*)
               + list_data(FIELD(combo, COMBO_LIST, BYTE*), NULL, -1) * 200;
        game_FUN_00445c10(FIELD(FIELD(session, 0x46C, BYTE*), 0x1A0, BYTE*), NULL, 0, look + 0x40,
                          game_main + 0x1C);
        FIELD(self, 0x7F88, int) = 0;
    }
    return drawn;
}

void __fastcall game_FUN_00418300(DWORD* scope, void* edx_unused, BYTE* display);
void __fastcall game_FUN_00418470(DWORD* scope, void* edx_unused, BYTE* model, DWORD value);
void __fastcall game_FUN_00418370(DWORD* scope);

/* Each player's model is given `value`, under one scope on the display. */
PORT(0x00450830)
void __fastcall players_set(BYTE* self, void* edx_unused, DWORD value)
{
    DWORD scope[11];
    int i;

    (void) self;
    (void) edx_unused;
    game_FUN_00418300(scope, NULL, FIELD(game_main, 0x1C, BYTE*));
    for (i = 0; i < FIELD(game_session, 0x4C, int); i++)
        game_FUN_00418470(scope, NULL, player_model(session_players + i * 200), value);
    game_FUN_00418370(scope);
}

/* ---- Starting ---- */

#define race_starting (*(DWORD*) 0x005A0FD4)

int  __fastcall game_FUN_0045d930(BYTE* events);
void __fastcall game_FUN_004b0f20(BYTE* net, void* edx_unused, DWORD type, void* data, DWORD size,
                                  DWORD to, DWORD flags);
void __cdecl    game_FUN_004b3610(void);                 /* leave the network game */
void __fastcall game_FUN_0049aa80(BYTE* session);
void __fastcall game_FUN_00527680(BYTE* player);
void __fastcall dialog_add_timer(BYTE* self, void* edx_unused, DWORD id, DWORD period,
                                 BYTE* target);

/*
    Vtable slot 10: at the third tick the race is started. If it cannot be, the screen closes,
    a network game is told (message 204) and left, and the menus go back to screen 100.
    Otherwise a network game's players are readied and a timer set.
*/
PORT(0x00452080)
int __fastcall race_start_tick(BYTE* self, void* edx_unused, float seconds)
{
    (void) edx_unused;
    if (FIELD(self, 0x7F58, int) == 2 && !race_starting) {
        BYTE* net;
        int i;

        if (!game_FUN_0045d930(FIELD(game_main, 0x570, BYTE*))) {
            dialog_end(self, NULL, 0);
            net = FIELD(game_main, 0x8, BYTE*);
            if (net) {
                /* The first word is left unset by the original; zeroed here. */
                DWORD message[2] = { 0, FIELD(net, 0xC, DWORD) };

                game_FUN_004b0f20(net, NULL, 0xCC, message, sizeof message, message[1], 0);
                game_FUN_004b3610();
            }
            game_FUN_0049aa80(game_session);
            go_to(100);
            return 1;
        }
        if (FIELD(game_main, 0x8, BYTE*))
            for (i = 0; i < FIELD(game_main, 0x2158, int); i++)
                game_FUN_00527680(game_main + 0x215C + i * 0xF8);
        race_starting = 1;
        dialog_add_timer(self, NULL, 0, 100, NULL);
    }
    FIELD(self, 0x7F58, int)++;
    return dialog_tick(self, NULL, seconds);
}

/*
    The loading screen. Opening puts a 20-step ProgressBar (drawn with the BlueBar animation) in
    place of StaBarRect, and starts the race loading; its timer (event 7) ends the screen and
    starts the race on the tenth tick (0x5A0F4C).
*/
#define loading_ticks   (*(int*) 0x005A0F4C)
#define loading_d8      (*(DWORD*) 0x005A0FD8)   /* cleared as the screen opens */
#define EVENT_TIMER     7

BYTE* __fastcall progress_construct(BYTE* self, void* edx_unused, DWORD a, const RECT* area,
                                    BYTE* dialog);
void  __fastcall progress_set_picture(BYTE* self, void* edx_unused, BYTE* picture, BOOL owned);
void  __fastcall control_set_name(BYTE* self, void* edx_unused, const char* name);
int   __fastcall dialog_find(void* dialog, void* edx_unused, const char* name);
BYTE* __fastcall anim_picture(ui_anim* self);
void  __fastcall dialog_remove_timer(BYTE* self, void* edx_unused, DWORD id);
void  __fastcall game_FUN_0049aa90(BYTE* session);
void  __fastcall game_FUN_00528bb0(char* profile);
void  __fastcall game_FUN_004b0fc0(BYTE* net, void* edx_unused, DWORD a, DWORD b);
void  __fastcall game_FUN_0045e280(BYTE* events);       /* start the race */
void  __fastcall game_FUN_00527090(BYTE* game);
void  __fastcall game_FUN_00421f90(BYTE* race);
void  __fastcall game_FUN_004e6c70(BYTE* object);

static void loading_open(BYTE* self)
{
    BYTE* place = dialog_find_control(self, NULL, NAME(0x0056FE00), 5);         /* StaBarRect */
    BYTE* bar = game_new(0x204, DLGPROCS_CPP, 0xADF);

    if (bar)
        bar = progress_construct(bar, NULL, 0, (const RECT*) (place + 0x3C), self);
    dialog_add(self, NULL, bar, 0, 0);
    control_set_name(bar, NULL, NAME(0x0056FDF4));                              /* ProgressBar */
    progress_set_picture(bar, NULL,
                         anim_picture((ui_anim*) dialog_find(self, NULL, NAME(0x0056FDEC))),
                         FALSE);                                                /* BlueBar */
    FIELD(bar, 0x1F0, int) = 20;                    /* steps */
    FIELD(bar, 0x1FC, DWORD) = 0;                   /* no percentage */
    control_show(place, NULL, FALSE, TRUE);

    FIELD(game_session, 0x490, BYTE*) = self;
    game_FUN_0049aa90(game_session);
    race_starting = 0;
    game_FUN_00528bb0(game_profile);
    if (FIELD(game_main, 0x8, BYTE*))
        game_FUN_004b0fc0(FIELD(game_main, 0x8, BYTE*), NULL,
                          FIELD(FIELD(game_main, 0x570, BYTE*), 0x2C, DWORD),
                          FIELD(FIELD(game_main, 0x570, BYTE*), 0x30, DWORD));
    loading_ticks = 0;
    loading_d8 = 0;
    FIELD(self, 0x7F58, DWORD) = 0;
}

PORT(0x00451E30)
void __fastcall loading_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_OPEN) {
        loading_open(self);
    } else if (event->type == EVENT_CLOSE) {
        dialog_remove_timer(self, NULL, 0);
        gui_manager_show_cursors(FIELD(self, 0x30, gui_manager*), NULL, FALSE);
        FIELD(game_session, 0x490, BYTE*) = NULL;
    } else if (event->type == EVENT_TIMER) {
        if (loading_ticks >= 10) {
            if (FIELD(game_main, 0x8, BYTE*)) {
                FIELD(game_main, 0xC1C, DWORD) = 0;
                dialog_end(event_dialog(event), NULL, 0);
                event->e = 1;
            } else {
                dialog_end(event_dialog(event), NULL, 0);
                event->e = 1;
                game_FUN_00527090(game_main);
                game_FUN_00469760(game_main);
                game_FUN_00421f90(game_FUN_0045e300(FIELD(game_main, 0x570, BYTE*)));
                if (FIELD(game_main, 0x55C, BYTE*))
                    game_FUN_004e6c70(FIELD(game_main, 0x55C, BYTE*));
                if (FIELD(game_main, 0x568, BYTE*))
                    game_FUN_004e6c70(FIELD(game_main, 0x568, BYTE*));
            }
            game_FUN_0045e280(FIELD(game_main, 0x570, BYTE*));
        }
        loading_ticks++;
    }
}

/* ---- Tracks ---- */

#define LIST_TRACK  NAME(0x005700EC)    /* "LstTrack" */
#define TRACK_SIZE  0xEB0
#define TRACK_NAME_LENGTH 0x103

void __fastcall game_FUN_00529730(char* profile, void* edx_unused, DWORD track);
void __cdecl    game_FUN_004546f0(void);

/* The track picked in the list (of those at 0x7F58): its settings become the race's. */
static BYTE* track_take(BYTE* self, BYTE* list)
{
    BYTE* track = FIELD(self, 0x7F58, BYTE*) + list_data(list, NULL, -1) * TRACK_SIZE;

    __movsd((unsigned long*) (game_main + 0x2D70), (unsigned long*) (track + 0x34), 0x7B);
    return track;
}

/* Its name, from the screen's names (0x7F60), at most 259 characters. */
static void track_name_copy(BYTE* self, BYTE* list, char* to)
{
    char** names = FIELD(self, 0x7F60, char**);
    size_t length = text_length(names[list_data(list, NULL, -1)]);

    if ((int) length > TRACK_NAME_LENGTH)
        length = TRACK_NAME_LENGTH;
    game__strncpy(to, names[list_data(list, NULL, -1)], length);
    to[length] = 0;
}

static void track_chosen(BYTE* self)
{
    BYTE* owner = FIELD(self, 0x2C, BYTE*);

    VIRTUAL(owner, 0x68, method_0)(owner);
}

PORT(0x00455970)
void __fastcall track_choose(BYTE* self)
{
    BYTE* list = dialog_find_control(self, NULL, LIST_TRACK, 3);
    BYTE* track = track_take(self, list);

    track_name_copy(self, list, (char*) game_main + 0x2B64);
    track_chosen(self);
    game_FUN_00529730(game_profile, NULL, FIELD(track, 0x3C, DWORD));
    game_FUN_004546f0();
}

/* The same for a track whose settings at 0x33C come with it. */
PORT(0x00456490)
void __fastcall track_choose_set(BYTE* self)
{
    BYTE* list = dialog_find_control(self, NULL, LIST_TRACK, 3);
    BYTE* track = track_take(self, list);

    game_FUN_00529730(game_profile, NULL, FIELD(track, 0x3C, DWORD));
    FIELD(game_main, 0x3428, DWORD) = 1;
    FIELD(game_main, 0x342C, DWORD) = 1;
    FIELD(game_main, 0x1550, DWORD) = FIELD(track, 0x33C, DWORD);
    FIELD(game_main, 0x1554, DWORD) = FIELD(track, 0x340, DWORD);
    track_name_copy(self, list, (char*) game_main + 0x2C6C);
    track_chosen(self);
    game_FUN_004546f0();
}

/*
    The record screens' lists, refilled from the Record folder's files of a kind; the owner's
    Start button is shown only when some were listed.
*/
typedef BOOL (__fastcall* record_add)(BYTE* self, void* edx_unused, const char* path);

BOOL __fastcall game_FUN_00455640(BYTE* self, void* edx_unused, const char* path);  /* a ghost */
BOOL __fastcall game_FUN_00456040(BYTE* self, void* edx_unused, const char* path);  /* a replay */

#define LIST_RIDER  NAME(0x00570120)    /* "LstRider" */
#define LIST_LAPS   NAME(0x0057010C)    /* "LstLaps" */
#define LIST_LENGTH NAME(0x00570114)    /* "LstLength" */
#define LIST_DESC   NAME(0x005700CC)    /* "LstDesc" */
#define START       NAME(0x0056F8C4)    /* "Start" */

static void records_list(BYTE* self, const char* pattern, record_add add)
{
    char path[0x104];
    WIN32_FIND_DATAA found;
    HANDLE search = FindFirstFileA(pattern, &found);
    int listed = 0;

    if (search != INVALID_HANDLE_VALUE) {
        do {
            game_FUN_0053a11f(path, NAME(0x0056C7DC), NAME(0x0056FB64), found.cFileName);
            if (add(self, NULL, path))
                listed++;
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    control_show(dialog_find_control(FIELD(self, 0x2C, BYTE*), NULL, START, 0), NULL, listed != 0,
                 TRUE);
}

static void list_clear_named(BYTE* self, const char* name)
{
    list_clear(dialog_find_control(self, NULL, name, 3));
}

PORT(0x00455470)
void __fastcall ghost_records_fill(BYTE* self)
{
    records_free(self, 0x118B);
    list_clear_named(self, LIST_TRACK);
    list_clear_named(self, LIST_RIDER);
    list_clear_named(self, LIST_LENGTH);
    list_clear_named(self, LIST_DESC);
    records_list(self, NAME(0x0056FB6C), game_FUN_00455640);                 /* Record\*.gho */
}

PORT(0x00455E60)
void __fastcall replay_records_fill(BYTE* self)
{
    records_free(self, 0x125E);
    list_clear_named(self, LIST_TRACK);
    list_clear_named(self, LIST_RIDER);
    list_clear_named(self, LIST_LAPS);
    list_clear_named(self, LIST_LENGTH);
    list_clear_named(self, LIST_DESC);
    records_list(self, NAME(0x0056F848), game_FUN_00456040);                 /* Record\*.vcr */
}

/*
    The record screens' filter panel over (20, 35)-(620, 395): the ghost filter (0x7F58) for
    kind 0, the replay filter (0x7F5C) for kind 1. The other kind's panel is closed first.
*/
enum { FILTER_GHOSTS, FILTER_REPLAYS };

static BYTE* filter_open(int line, const char* name, void* vtable)
{
    BYTE* panel = game_new(0x7F88, DLGPROCS_CPP, line);

    if (panel) {
        dialog_construct(panel, NULL, 1, name);
        *(void**) panel = vtable;
    }
    return panel;
}

static void filter_close(BYTE* self, int offset)
{
    BYTE* panel = FIELD(self, offset, BYTE*);

    VIRTUAL(panel, 8, method_0)(panel);
    FIELD(self, offset, BYTE*) = NULL;
}

PORT(0x00454E90)
void __fastcall records_filter_show(BYTE* self, void* edx_unused, int kind)
{
    RECT area = { 20, 35, 620, 395 };
    BYTE* panel;

    (void) edx_unused;
    if (FIELD(self, 0x7F58, BYTE*) && kind != FILTER_GHOSTS)
        filter_close(self, 0x7F58);
    if (FIELD(self, 0x7F5C, BYTE*) && kind != FILTER_REPLAYS)
        filter_close(self, 0x7F5C);

    if (kind == FILTER_GHOSTS) {
        if (FIELD(self, 0x7F58, BYTE*))
            return;
        panel = filter_open(0x1126, NAME(0x00570060), (void*) 0x00557420);     /* GhostFil.dtm */
        FIELD(self, 0x7F58, BYTE*) = panel;
    } else if (kind == FILTER_REPLAYS) {
        if (FIELD(self, 0x7F5C, BYTE*))
            return;
        panel = filter_open(0x112C, NAME(0x00570070), (void*) 0x005574A4);     /* ReplyFil.dtm */
        FIELD(self, 0x7F5C, BYTE*) = panel;
    } else {
        return;
    }
    gui_manager_open(session_gui, NULL, panel, 0, 1, (DWORD) &area, self, 0, 0, 1);
}

/*
    The record screen: TabLeft shows the ghost filter, TabRight the replay filter (shown at
    first). Start takes the open panel's track; Back closes the panels and goes to screen 100.
*/
#define TAB_LEFT    NAME(0x00570018)    /* "TabLeft" */
#define TAB_RIGHT   NAME(0x0057000C)    /* "TabRight" */
#define HELP        NAME(0x0056F8A8)    /* "Help" */
#define HELP_TOPIC  NAME(0x0056F560)    /* "MCM2HELP" */
#define TAB_CONTROL 4

/* Out of line: the original calls these. */
__declspec(noinline) void __fastcall records_filter_show(BYTE* self, void* edx_unused, int kind);
__declspec(noinline) void __fastcall track_choose(BYTE* self);
__declspec(noinline) void __fastcall track_choose_set(BYTE* self);

int  __fastcall option_load_text(BYTE* self, void* edx_unused, int index, HINSTANCE module,
                                 UINT id);
BOOL __fastcall radio_set(BYTE* self, void* edx_unused, int number);
void __fastcall game_FUN_00529380(char* profile, void* edx_unused, const char* topic, DWORD a);

static void tab_setup(BYTE* self, const char* name, UINT id)
{
    BYTE* tab = dialog_find_control(self, NULL, name, TAB_CONTROL);

    option_load_text(tab, NULL, 0, game_language, id);
    option_load_text(tab, NULL, 1, game_language, id);
    control_set_align(tab, NULL, 0x22);
}

PORT(0x00454C10)
void __fastcall records_screen_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, NAME(0x0056F8CC))) {                                 /* "Back" */
            if (FIELD(self, 0x7F58, BYTE*))
                filter_close(self, 0x7F58);
            if (FIELD(self, 0x7F5C, BYTE*))
                filter_close(self, 0x7F5C);
            dialog_end(event_dialog(event), NULL, 0);
            event->e = 1;
            go_to(100);
        } else if (clicked(event, TAB_LEFT)) {
            records_filter_show(self, NULL, FILTER_GHOSTS);
        } else if (clicked(event, TAB_RIGHT)) {
            records_filter_show(self, NULL, FILTER_REPLAYS);
        } else if (clicked(event, HELP)) {
            game_FUN_00529380(game_profile, NULL, HELP_TOPIC, 0);
        } else if (clicked(event, START)) {
            if (FIELD(self, 0x7F58, BYTE*)) {
                track_choose(FIELD(self, 0x7F58, BYTE*));
                dialog_end(self, NULL, 0);
                return;
            }
            if (FIELD(self, 0x7F5C, BYTE*))
                track_choose_set(FIELD(self, 0x7F5C, BYTE*));
            dialog_end(self, NULL, 0);
        }
    } else if (event->type == EVENT_OPEN) {
        FIELD(self, 0x7F58, BYTE*) = NULL;
        FIELD(self, 0x7F5C, BYTE*) = NULL;
        tab_setup(self, TAB_LEFT, 0x143A);
        tab_setup(self, TAB_RIGHT, 0x1439);
        records_filter_show(self, NULL, FILTER_REPLAYS);
        radio_set(dialog_find_control(self, NULL, TAB_RIGHT, TAB_CONTROL), NULL, 0);
    }
}

/* ---- Replays ---- */

void __fastcall game_FUN_0045ddd0(BYTE* events, void* edx_unused, DWORD a);
void __fastcall game_FUN_0045f720(BYTE* events, void* edx_unused, DWORD a);

/* Back ends it; closing puts back the race settings saved before it and shows the cursor. */
PORT(0x00456A10)
void __fastcall back_restore_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, NAME(0x0056F8CC)))                                 /* "Back" */
            dialog_end(self, NULL, 0);
    } else if (event->type == EVENT_CLOSE && !cursors_locked) {
        game_FUN_0045ddd0(FIELD(game_main, 0x570, BYTE*), NULL, 2);
        __movsd((unsigned long*) (game_main + 0x2D70), (unsigned long*) (game_main + 0x2F5C), 0x7B);
        __movsd((unsigned long*) (game_main + 0x1550), (unsigned long*) (game_main + 0x15AC), 0x17);
        __movsd((unsigned long*) (game_main + 0x1EEC), (unsigned long*) (game_main + 0x1FB4), 0x32);
        game_FUN_0045f720(FIELD(game_main, 0x570, BYTE*), NULL, 100);
        gui_manager_show_cursors(FIELD(self, 0x30, gui_manager*), NULL, TRUE);
    }
}

void __fastcall game_FUN_004ade20(BYTE* replay);
void __fastcall game_FUN_004b0c00(BYTE* net, void* edx_unused, DWORD a);

/*
    Leaving the race (401): the replay being recorded of the watched rider is closed, the race
    stopped, and the menus go back to the session's screen - out of a network game for a
    one-race session (0x2D70 = 2).
*/
static void race_leave(void)
{
    BYTE* replay;
    BYTE* net;

    game_FUN_00526eb0(game_main, NULL, 0, 0x191, 0);
    replay = FIELD(game_main, 0x568, BYTE*);
    net = FIELD(game_main, 0x8, BYTE*);
    if (replay && net && FIELD(net, 0x10, DWORD) && FIELD(replay, 0xA8, BYTE*)
        && FIELD(replay, 0x34, BYTE*)
        && FIELD(replay, 0xA8, BYTE*) == FIELD(FIELD(replay, 0x34, BYTE*), 0x38, BYTE*)) {
        BYTE* rider = FIELD(FIELD(replay, 0x34, BYTE*), 0x38, BYTE*);

        if (rider)
            FIELD(rider, 0x4A0, DWORD) = 1;
        game_FUN_004ade20(FIELD(game_main, 0x568, BYTE*));
    }
    game_FUN_0045ddd0(FIELD(game_main, 0x570, BYTE*), NULL, 2);
    if (FIELD(game_main, 0x8, BYTE*)) {
        game_FUN_004b0c00(FIELD(game_main, 0x8, BYTE*), NULL, 1);
        if (FIELD(game_main, 0x2D70, int) == 2) {
            game_FUN_004b3610();
            game_FUN_0045f720(FIELD(game_main, 0x570, BYTE*), NULL, 100);
        } else {
            game_FUN_0045f720(FIELD(game_main, 0x570, BYTE*), NULL, 0x867);
        }
    } else {
        game_FUN_0045f720(FIELD(game_main, 0x570, BYTE*), NULL,
                          FIELD(game_main, 0x3440, int) ? 0x88F : 0x65);
    }
}

/*
    The other answer to a screen's result code (see result_go). Those it knows end the screen
    that sent the event, most with 0, and mark the event handled; 401 leaves the race instead.
*/
PORT(0x004534C0)
void __cdecl result_back(int code, ui_event* event)
{
    DWORD end = 0;

    switch (code) {
    case 0xC9:
    case 0xCA:
    case 0xCB:
        go_to(0x66);
        break;
    case 0xCD:
        go_to(FIELD(game_session, 0x30, int));
        break;
    case 0xD0:
    case 0x12F:
    case 0x1F9:
    case 0xBB9:
    case 0xBBA:
    case 0xBBB:
        break;
    case 0xD8:
    case 0xD9:
        end = 14;
        break;
    case 0x104:
    case 0x105:
        go_to(0x65);
        break;
    case 0x12C:
        end = 0x15;
        break;
    case 0x191:
        race_leave();
        return;
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
