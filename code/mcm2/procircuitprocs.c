/*
    OpenMCM2 - the pro circuit's screens, ported from the game (ProCircuitProcs.cpp in the
    original): the saved circuits, a new one, the circuit's screen with its next race, bike and
    rider and standings pages, the race results and the pay-out, the deals offered when the money
    runs short, and the endings.

    The circuit is the object at game_main + 0x3440 (its file at + 0x3444). Each function here
    replaces the game's at the address in its PORT line, and was checked against the original's
    instructions.
*/

#include "menus.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

BYTE*       __fastcall dialog_find_control(BYTE* self, void* edx_unused, const char* name, int type);
BYTE*       __fastcall dialog_construct(BYTE* self, void* edx_unused, DWORD flag, const char* name);
void        __fastcall dialog_end(BYTE* self, void* edx_unused, DWORD code);
void        __fastcall dialog_notify_controls(BYTE* self, void* edx_unused, DWORD flag);
int         __fastcall control_set_text(BYTE* self, void* edx_unused, const char* text);
void        __fastcall control_show(BYTE* self, void* edx_unused, DWORD shown, BOOL animate);
int         __fastcall control_set_align(BYTE* self, void* edx_unused, DWORD align);
int         __fastcall option_load_text(BYTE* self, void* edx_unused, int index, HINSTANCE module,
                                        UINT id);
int         __fastcall option_select(BYTE* self, void* edx_unused, int index);
int         __fastcall option_current_index(BYTE* self);
BOOL        __fastcall radio_set(BYTE* self, void* edx_unused, int number);
int         __fastcall radio_number(BYTE* self);
int         __fastcall list_selected(BYTE* self);
DWORD       __fastcall list_data(BYTE* self, void* edx_unused, int index);
BOOL        __fastcall list_add_text(BYTE* self, void* edx_unused, const char* text, DWORD data,
                                     DWORD b);
void        __fastcall list_clear(BYTE* self);
int         __fastcall list_select_all(BYTE* self, void* edx_unused, int index);
int         __fastcall list_select_data(BYTE* self, void* edx_unused, DWORD data);
void        __fastcall list_sort(BYTE* self, void* edx_unused, BOOL outer);
typedef int (__cdecl* entry_compare)(const BYTE* a, const BYTE* b);
void        __fastcall list_set_compare(BYTE* self, void* edx_unused, entry_compare compare);
void        __fastcall button_set_sort_list(BYTE* self, void* edx_unused, BYTE* list);
int         __fastcall edit_set_text(BYTE* self, void* edx_unused, const char* text);
char*       __fastcall edit_get_text(BYTE* self, void* edx_unused, char* buffer, int size);
void        __fastcall edit_set_allowed(BYTE* self, void* edx_unused, const char* allowed);
BYTE*       __fastcall gui_manager_open(gui_manager* self, void* edx_unused, BYTE* dialog, DWORD a,
                                        DWORD flags, DWORD area, BYTE* parent, DWORD arg, DWORD c,
                                        DWORD wait);
void        __fastcall prompt_setup_ids(BYTE* self, void* edx_unused, const char* title,
                                        UINT title_id, const char* prompt, UINT prompt_id,
                                        const char* left, UINT left_id, const char* middle,
                                        UINT middle_id, const char* right, UINT right_id);
void        __fastcall session_go_to(BYTE* self, void* edx_unused, int screen);
void        __cdecl    race_start_loading(void);
int         __cdecl    entries_by_number(const BYTE* a, const BYTE* b);
int         __cdecl    entries_by_number_falling(const BYTE* a, const BYTE* b);
void*       __cdecl    ui_realloc(void* block, size_t size);

/* The game's C runtime. */
int    __cdecl game_FUN_0053a11f(char* to, const char* format, ...);      /* sprintf */
int    __cdecl game_FUN_0053b6c7(const char* text);                       /* atoi */
int    __cdecl game_FUN_0053b6e0(const char* a, const char* b);           /* _stricmp */
char*  __cdecl game_FUN_0054aaa5(int value, char* text, int radix);       /* _itoa */
void*  __cdecl game_FUN_0053a35b(const char* path, const char* mode);     /* fopen */
int    __cdecl game_FUN_0053a2ad(void* file);                             /* fclose */
char*  __cdecl game__strrchr(const char* text, int c);
void   __cdecl game_FUN_0053a27e(unsigned seed);                          /* srand */
DWORD  __cdecl game_FUN_004c41d0(void);                                   /* the time */

BOOL __fastcall game_FUN_00526fc0(BYTE* game, void* edx_unused, UINT id, char* text, int size);
BOOL __fastcall profile_open_help(char* self, void* edx_unused, const char* topic, const char* parameters);
char* __fastcall profile_set_event(char* self, void* edx_unused, int kind);
void __fastcall game_FUN_0045f530(BYTE* events);
void __fastcall session_show_scene(BYTE* self, void* edx_unused, BYTE* dialog);  /* models in */
void __fastcall session_hide_scene(BYTE* self);
void __fastcall game_FUN_004eda80(BYTE* screen, void* edx_unused, BYTE* camera);

/* The circuit (the circuit object's file): construct, destruct, start, load and save. */
BYTE* __fastcall game_FUN_004d7bd0(BYTE* self);
void  __fastcall game_FUN_004d8140(BYTE* self);
void  __fastcall game_FUN_004d8250(BYTE* self, void* edx_unused, const char* name, int difficulty,
                                   int size_class, int riders);
BOOL  __fastcall game_FUN_004d8850(BYTE* self, void* edx_unused, const char* path);
BOOL  __fastcall game_FUN_004d88a0(BYTE* self, void* edx_unused, const char* path);

/* This file's second half: the bike and rider page's rider and bike chosen, and the plates. */
void __fastcall pc_rider_choose(BYTE* self);
void __fastcall pc_bike_choose(BYTE* self);
void __fastcall pc_players_set(BYTE* self, void* edx_unused, DWORD value);

#define PROCIRCUIT_CPP NAME(0x00577B20)     /* "...\ProCircuitProcs.cpp" */
#define TWO_STRINGS    NAME(0x0056ED34)     /* "%s%s" */
#define TWO_WORDS      NAME(0x0056E054)     /* "%s %s" */
#define PROFILE_FOLDER NAME(0x0056F548)     /* "ui\profile" */
#define PCTRANS_DTM    NAME(0x00577B14)     /* "PCTrans.dtm" */
#define PCC_DTM        NAME(0x005738EC)     /* "PCC.dtm" */
#define PCSTART_DTM    NAME(0x0056F5F0)     /* "PCStart.dtm" */
#define BACK           NAME(0x0056F8CC)     /* "Back" */
#define TITLE_TEXT     NAME(0x0056F864)     /* "TitleText" */
#define START          NAME(0x0056F8C4)     /* "Start" */
#define HELP           NAME(0x0056F8A8)     /* "Help" */
#define HELP_TOPIC     NAME(0x0056F560)     /* "MCM2HELP" */

/* The race settings (dlgprocs.c), 0x7B dwords at the game's 0x2D70. */

/*
    The circuit being played, 0x12E5 bytes, and the file it is kept in. Its name is at 0; the
    16-byte records from 0x1285 are its event kinds': the races (12 bytes each, the track's file
    first) and the laps (0xC).
*/
#define pro_circuit      FIELD(game_main, 0x3440, BYTE*)
#define pro_circuit_path ((char*) game_main + 0x3444)
#define CIRCUIT_SIZE       0x12E5
#define CIRCUIT_KIND       0x40     /* the race kind its events are */
#define CIRCUIT_RACE       0x44     /* the race it is at, from 1 */
#define CIRCUIT_LEVEL      0x48     /* the highest player kind (0xC4) of riders and makers allowed */
#define CIRCUIT_CLASS      0x4C     /* 1 125cc, 2 250cc, 3 open */
#define CIRCUIT_DIFFICULTY 0x50     /* 1, 2 or 3 */
#define CIRCUIT_RIDER      0x458
#define CIRCUIT_BIKE       0x45C
#define CIRCUIT_RIDERS     0x460
#define CIRCUIT_FLAGS      0x464    /* BYTE */
#define CIRCUIT_OUT        4        /* given up, transferred out: status 0x1442 */
#define CIRCUIT_WON        8        /* the career won: status 0x1443, PCTrans 0x55C294 */
#define CIRCUIT_POINTS     0x47D
#define CIRCUIT_RANK       0x48D
#define CIRCUIT_CASH       0x495
#define CIRCUIT_ENGINE     0x59D    /* the engine size chosen, cc */
#define CIRCUIT_EVENTS     0x1285

static BYTE* event_dialog(const ui_event* event)
{
    return (BYTE*) event->sender;
}

/* A dialog of `size` bytes read from `name`, given its kind's vtable (NULL when out of memory). */
static BYTE* pc_dialog_new(size_t size, int line, const char* name, DWORD vtable)
{
    BYTE* dialog = game_new(size, PROCIRCUIT_CPP, line);

    if (dialog) {
        dialog_construct(dialog, NULL, 1, name);
        *(DWORD*) dialog = vtable;
    }
    return dialog;
}

/* A new circuit object in the game's 0x3440 (NULL when out of memory); the old one is not freed. */
static BYTE* circuit_new(int line)
{
    BYTE* circuit = game_new(CIRCUIT_SIZE, PROCIRCUIT_CPP, line);

    if (circuit)
        circuit = game_FUN_004d7bd0(circuit);
    pro_circuit = circuit;
    return circuit;
}

/* ---- Startup ---- */

#define pc_vector_zero (*(vector3*) 0x0068FC08)
#define pc_vector_x    (*(vector3*) 0x0068FC18)
#define pc_vector_y    (*(vector3*) 0x0068FC28)
#define pc_vector_z    (*(vector3*) 0x0068FBF8)

__declspec(noinline) void __cdecl pc_vector_zero_init(void);
__declspec(noinline) void __cdecl pc_vector_x_init(void);
__declspec(noinline) void __cdecl pc_vector_y_init(void);
__declspec(noinline) void __cdecl pc_vector_z_init(void);

PORT(0x004D9140)
void __cdecl pc_vector_zero_init(void)
{
    set_vector(&pc_vector_zero, 0.0f, 0.0f, 0.0f);
}

PORT(0x004D9190)
void __cdecl pc_vector_x_init(void)
{
    set_vector(&pc_vector_x, 1.0f, 0.0f, 0.0f);
}

PORT(0x004D91E0)
void __cdecl pc_vector_y_init(void)
{
    set_vector(&pc_vector_y, 0.0f, 1.0f, 0.0f);
}

PORT(0x004D9230)
void __cdecl pc_vector_z_init(void)
{
    set_vector(&pc_vector_z, 0.0f, 0.0f, 1.0f);
}

/* The runtime's initializer table calls these, which only jump on. */
PORT(0x004D9130)
void __cdecl pc_vector_zero_init_entry(void)
{
    pc_vector_zero_init();
}

PORT(0x004D9180)
void __cdecl pc_vector_x_init_entry(void)
{
    pc_vector_x_init();
}

PORT(0x004D91D0)
void __cdecl pc_vector_y_init_entry(void)
{
    pc_vector_y_init();
}

PORT(0x004D9220)
void __cdecl pc_vector_z_init_entry(void)
{
    pc_vector_z_init();
}

/* ---- Sorting ---- */

/* List sort orders by cash: entries' texts ("$123") as integers, rising and falling. */
PORT(0x004D9270)
int __cdecl entries_by_cash(const BYTE* a, const BYTE* b)
{
    int first = game_FUN_0053b6c7(FIELD(a, 0x14, const char*) + 1);
    int second = game_FUN_0053b6c7(FIELD(b, 0x14, const char*) + 1);

    if (first < second)
        return -1;
    return first != second;
}

PORT(0x004D92B0)
int __cdecl entries_by_cash_falling(const BYTE* a, const BYTE* b)
{
    int first = game_FUN_0053b6c7(FIELD(a, 0x14, const char*) + 1);
    int second = game_FUN_0053b6c7(FIELD(b, 0x14, const char*) + 1);

    if (second < first)
        return -1;
    return first != second;
}

/* ---- The circuit's screen ---- */

/* The circuit's screen: its ending once over, its intro at the first race, else PCC.dtm. */
PORT(0x004D92F0)
void __cdecl circuit_screen_open(void)
{
    BYTE* screen;

    if (FIELD(pro_circuit, CIRCUIT_FLAGS, BYTE) & CIRCUIT_WON)
        screen = pc_dialog_new(0x7F58, 0x4E, PCTRANS_DTM, 0x0055C294);
    else if (FIELD(pro_circuit, CIRCUIT_RACE, int) == 1)
        screen = pc_dialog_new(0x7F58, 0x50, PCTRANS_DTM, 0x0055C210);
    else
        screen = pc_dialog_new(0x7F64, 0x52, PCC_DTM, 0x00559AC4);
    gui_manager_open(session_gui, NULL, screen, 0, 2, 0, NULL, 0, 0, 1);
}

/* ---- PCStart.dtm: the circuits saved ---- */

/*
    The player's circuits, in the Lst columns while under way and the LstDone ones when over: the
    files' paths (0x7F58, 0x7F5C of them, on the game's heap), what PCNew.dtm answers (0x7F60 to
    0x806B), and which delete prompt is up (0x806C for LstRank's, 0x8070 for LstDoneRank's).
*/
#define CIRCUIT_PATHS   0x7F58
#define CIRCUIT_COUNT   0x7F5C
#define NEW_NAME        0x7F60      /* char[0x100] */
#define NEW_DIFFICULTY  0x8060      /* RadLODEasy's group */
#define NEW_CLASS       0x8064      /* RadClass125's group */
#define NEW_RIDERS      0x8068      /* opponents, 3 to 10 */
#define DELETE_PROMPT   0x806C
#define DONE_DELETE_PROMPT 0x8070

#define LIST_RANK       NAME(0x00577CC8)    /* "LstRank" */
#define LIST_DONE_RANK  NAME(0x00577C4C)    /* "LstDoneRank" */
#define BUT_CONTINUE    NAME(0x00577B90)    /* "ButContinue" */
#define BUT_DELETE      NAME(0x005700F8)    /* "ButDelete" */
#define BUT_DONE_DELETE NAME(0x00577B80)    /* "ButDoneDelete" */
#define BUT_NEW         NAME(0x00577BA8)    /* "ButNew" */
#define CIRCUIT_FILE    NAME(0x00577B68)    /* "%s\%s\Circuit%03d.pc" */

/* The columns, the buttons sorting them and their orders (NULL: by text). */
static const struct {
    const char*   list;
    const char*   button;
    entry_compare compare;
} columns[13] = {
    { NAME(0x00577CD8), NAME(0x00577CD0), NULL },                       /* LstName, ButName */
    { NAME(0x00577CC8), NAME(0x00577CC0), entries_by_number },          /* LstRank */
    { NAME(0x00577CB4), NAME(0x00577CA8), NULL },                       /* LstClass */
    { NAME(0x00577CA0), NAME(0x00577C98), NULL },                       /* LstDiff */
    { NAME(0x00577C8C), NAME(0x00577C80), entries_by_number_falling },  /* LstPoints */
    { NAME(0x00577C78), NAME(0x00577C70), entries_by_cash_falling },    /* LstCash */
    { NAME(0x00577C64), NAME(0x00577C58), NULL },                       /* LstDoneName */
    { NAME(0x00577C4C), NAME(0x00577C40), entries_by_number },          /* LstDoneRank */
    { NAME(0x00577C30), NAME(0x00577C20), NULL },                       /* LstDoneClass */
    { NAME(0x00577C14), NAME(0x00577C08), NULL },                       /* LstDoneDiff */
    { NAME(0x00577BF8), NAME(0x00577BE8), entries_by_number_falling },  /* LstDonePoints */
    { NAME(0x00577BDC), NAME(0x00577BD0), entries_by_cash_falling },    /* LstDoneCash */
    { NAME(0x00577BC0), NAME(0x00577BB0), NULL },                       /* LstDoneStatus */
};

/* Out of line, as the original calls them; circuits_fill's 5 KB frame must not join a caller's. */
__declspec(noinline) void __fastcall circuit_start_buttons(BYTE* self);
__declspec(noinline) void __fastcall circuits_fill(BYTE* self);
__declspec(noinline) void __fastcall circuits_free(BYTE* self);
__declspec(noinline) void __fastcall circuit_row_add(BYTE* self, void* edx_unused,
                                                     const BYTE* circuit, DWORD index);
__declspec(noinline) void __fastcall circuit_new_bind(BYTE* self, void* edx_unused, char* name,
                                                      int* difficulty, int* size_class,
                                                      DWORD* riders);

/* The row chosen in a column, if it has any. */
static BOOL row_chosen(BYTE* self, const char* name)
{
    BYTE* list = dialog_find_control(self, NULL, name, 3);

    return FIELD(list, 0x1EC, int) && list_selected(list) >= 0;
}

/* Continue and Delete show while a circuit under way is chosen, ButDoneDelete for one over. */
PORT(0x004D9D50)
void __fastcall circuit_start_buttons(BYTE* self)
{
    BOOL chosen = row_chosen(self, LIST_RANK);

    control_show(dialog_find_control(self, NULL, BUT_CONTINUE, 0), NULL, chosen, TRUE);
    control_show(dialog_find_control(self, NULL, BUT_DELETE, 0), NULL, chosen, TRUE);
    chosen = row_chosen(self, LIST_DONE_RANK);
    control_show(dialog_find_control(self, NULL, BUT_DONE_DELETE, 0), NULL, chosen, TRUE);
}

/*
    The columns: each of the profile's *.pc files that loads is kept by path and listed. (The
    paths are added to those kept already: a refill after a delete leaves the old ones behind.)
*/
PORT(0x004D9E10)
void __fastcall circuits_fill(BYTE* self)
{
    BYTE circuit[0x12E8];
    char path[0x80];
    char folder[0x80];
    char pattern[0x80];
    WIN32_FIND_DATAA found;
    HANDLE search;
    int i;

    game_FUN_004d7bd0(circuit);
    for (i = 0; i < 13; i++)
        list_clear(dialog_find_control(self, NULL, columns[i].list, 3));
    game_FUN_0053a11f(folder, NAME(0x00577CE8), PROFILE_FOLDER, game_profile);   /* "%s\%s\" */
    game_FUN_0053a11f(pattern, NAME(0x00577CE0), folder);                         /* "%s*.pc" */
    search = FindFirstFileA(pattern, &found);
    if (search != INVALID_HANDLE_VALUE) {
        do {
            game_FUN_0053a11f(path, TWO_STRINGS, folder, found.cFileName);
            if (game_FUN_004d8850(circuit, NULL, path)) {
                int count = FIELD(self, CIRCUIT_COUNT, int);
                char** paths = ui_realloc(FIELD(self, CIRCUIT_PATHS, char**),
                                          count * sizeof(char*) + sizeof(char*));

                FIELD(self, CIRCUIT_PATHS, char**) = paths;
                paths[count] = game_FUN_004a4410(text_length(path) + 1, PROCIRCUIT_CPP, 0x129);
                text_copy(paths[count], path);
                circuit_row_add(self, NULL, circuit, count);
                FIELD(self, CIRCUIT_COUNT, int)++;
            }
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    game_FUN_004d8140(circuit);
}

static void row_add(BYTE* self, const char* prefix, const char* column, const char* text,
                    DWORD index)
{
    char name[0x40];

    game_FUN_0053a11f(name, TWO_STRINGS, prefix, column);
    list_add_text(dialog_find_control(self, NULL, name, 3), NULL, text, index, 0);
}

/*
    A circuit's row, its file's index the entries' data. A class other than 1 to 3 lists the
    rank's text again, as the original leaves the buffer as it was.
*/
PORT(0x004DA0F0)
void __fastcall circuit_row_add(BYTE* self, void* edx_unused, const BYTE* circuit, DWORD index)
{
    BYTE flags = FIELD(circuit, CIRCUIT_FLAGS, BYTE);
    BOOL over = (flags >> 3) & 1;
    BOOL won = (flags >> 2) & 1;
    const char* prefix = over || won ? NAME(0x00577D34) : NAME(0x00577D3C);  /* LstDone, Lst */
    char text[0x80];
    int difficulty;

    (void) edx_unused;
    row_add(self, prefix, NAME(0x0056ED14), (const char*) circuit, index);    /* Name */

    game_FUN_0053a11f(text, NAME(0x00577D24), FIELD(circuit, CIRCUIT_RANK, int),   /* "%d / %d" */
                      FIELD(circuit, CIRCUIT_RIDERS, int));
    row_add(self, prefix, NAME(0x00577D2C), text, index);                     /* Rank */

    switch (FIELD(circuit, CIRCUIT_CLASS, int)) {
    case 1:
        text_copy(text, NAME(0x00577D14));                                    /* "125" */
        break;
    case 2:
        text_copy(text, NAME(0x00577D18));                                    /* "250" */
        break;
    case 3:
        game_FUN_00526fc0(game_main, NULL, 0x14CF, text, 0x7F);
        break;
    }
    row_add(self, prefix, NAME(0x00577D1C), text, index);                     /* Class */

    difficulty = FIELD(circuit, CIRCUIT_DIFFICULTY, int);
    game_FUN_00526fc0(game_main, NULL,
                      difficulty == 1 ? 0x14CB : difficulty == 3 ? 0x14CD : 0x14CC, text, 0x7F);
    row_add(self, prefix, NAME(0x00577D0C), text, index);                     /* Diff */

    game_FUN_0054aaa5(FIELD(circuit, CIRCUIT_POINTS, int), text, 10);
    row_add(self, prefix, NAME(0x00577D04), text, index);                     /* Points */

    game_FUN_0053a11f(text, NAME(0x00577CF8), FIELD(circuit, CIRCUIT_CASH, int));   /* "$%d" */
    row_add(self, prefix, NAME(0x00577CFC), text, index);                     /* Cash */

    if (over || won) {
        game_FUN_00526fc0(game_main, NULL, won ? 0x1442 : 0x1443, text, 0x80);
        row_add(self, prefix, NAME(0x00577CF0), text, index);                 /* Status */
    }
}

/* The paths kept, freed. */
PORT(0x004DA3F0)
void __fastcall circuits_free(BYTE* self)
{
    if (FIELD(self, CIRCUIT_PATHS, char**)) {
        int i;

        for (i = 0; i < FIELD(self, CIRCUIT_COUNT, int); i++) {
            if (FIELD(self, CIRCUIT_PATHS, char**)[i])
                game_FUN_004a4450(FIELD(self, CIRCUIT_PATHS, char**)[i], PROCIRCUIT_CPP, 0x17B);
        }
        game_FUN_004a4450(FIELD(self, CIRCUIT_PATHS, char**), PROCIRCUIT_CPP, 0x17E);
    }
    FIELD(self, CIRCUIT_COUNT, int) = 0;
    FIELD(self, CIRCUIT_PATHS, char**) = NULL;
}

/* A new circuit from PCNew.dtm's answers, kept in the profile's first free CircuitNNN.pc. */
static void circuit_create(BYTE* self)
{
    int level = FIELD(self, NEW_DIFFICULTY, int);
    char path[0x80];
    void* file;
    int number = 1;

    circuit_new(0xB6);
    game_FUN_004d8250(pro_circuit, NULL, (const char*) self + NEW_NAME,
                      level == 1 ? 1 : level == 2 ? 2 : 3, FIELD(self, NEW_CLASS, int),
                      FIELD(self, NEW_RIDERS, int) + 1);
    game_FUN_0053a11f(path, CIRCUIT_FILE, PROFILE_FOLDER, game_profile, number);
    while ((file = game_FUN_0053a35b(path, NAME(0x0056CA38))) != NULL) {      /* "r" */
        game_FUN_0053a2ad(file);
        game_FUN_0053a11f(path, CIRCUIT_FILE, PROFILE_FOLDER, game_profile, ++number);
    }
    text_copy(pro_circuit_path, path);
    game_FUN_004d88a0(pro_circuit, NULL, pro_circuit_path);
    dialog_end(self, NULL, 0);
    gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL,
                     pc_dialog_new(0x7F58, 0xD6, PCTRANS_DTM, 0x0055C210), 0, 2, 0, NULL,
                     0, 0, 1);
}

/*
    messbox2.dtm asking whether to delete, kept at `field` (the other prompt's is cleared first):
    Yes (0x143E) ends it with 101, No (0x143D) with 103.
*/
static void delete_prompt(BYTE* self, int field, int other, int line)
{
    BYTE* box;

    FIELD(self, other, BYTE*) = NULL;
    box = pc_dialog_new(0x7F58, line, NAME(0x0056F734), 0x005575AC);
    FIELD(self, field, BYTE*) = box;
    gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL, box, 0, 4, 0, self, 0, 0, 1);
    prompt_setup_ids(box, NULL, NULL, 0x143C, NULL, 0, NULL, 0x143E, NULL, 0, NULL, 0x143D);
}

/*
    PCStart.dtm (vtable 0x00556AC8): the circuits saved. New asks PCNew.dtm and makes one;
    Continue loads the one chosen and opens PCC.dtm; the deletes ask first. Each new circuit
    replaces the game's without freeing it.
*/
PORT(0x004D9470)
void __fastcall circuit_start_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    switch (event->type) {
    case EVENT_OPEN: {
        int i;

        FIELD(self, CIRCUIT_COUNT, int) = 0;
        FIELD(self, CIRCUIT_PATHS, char**) = NULL;
        circuits_fill(self);
        circuit_start_buttons(self);
        FIELD(self, DELETE_PROMPT, BYTE*) = NULL;
        FIELD(self, DONE_DELETE_PROMPT, BYTE*) = NULL;
        for (i = 0; i < 13; i++) {
            BYTE* list = dialog_find_control(self, NULL, columns[i].list, 3);

            button_set_sort_list(dialog_find_control(self, NULL, columns[i].button, 1), NULL, list);
            if (columns[i].compare)
                list_set_compare(dialog_find_control(self, NULL, columns[i].list, 3), NULL,
                                 columns[i].compare);
        }
        break;
    }
    case EVENT_CLICK:
        if (clicked(event, BUT_NEW)) {
            BYTE* ask = pc_dialog_new(0x7F68, 0x87, NAME(0x00577B9C),           /* PCNew.dtm */
                                      0x0055C318);

            gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL, ask, 0, 4, 0, self, 0, 0, 1);
            circuit_new_bind(ask, NULL, (char*) self + NEW_NAME, &FIELD(self, NEW_DIFFICULTY, int),
                             &FIELD(self, NEW_CLASS, int), &FIELD(self, NEW_RIDERS, DWORD));
        } else if (clicked(event, BUT_CONTINUE)) {
            DWORD index = list_data(dialog_find_control(self, NULL, LIST_RANK, 3), NULL, -1);

            circuit_new(0x8E);
            text_copy(pro_circuit_path, FIELD(self, CIRCUIT_PATHS, char**)[index]);
            if (game_FUN_004d8850(pro_circuit, NULL, pro_circuit_path))
                gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL,
                                 pc_dialog_new(0x7F64, 0x92, PCC_DTM, 0x00559AC4), 0, 2, 0,
                                 NULL, 0, 0, 1);
            dialog_end(self, NULL, 0);
        } else if (clicked(event, BUT_DELETE)) {
            delete_prompt(self, DELETE_PROMPT, DONE_DELETE_PROMPT, 0x98);
        } else if (clicked(event, BUT_DONE_DELETE)) {
            delete_prompt(self, DONE_DELETE_PROMPT, DELETE_PROMPT, 0xA2);
        } else if (clicked(event, BACK)) {
            session_go_to(game_session, NULL, 100);
            dialog_end(self, NULL, 0);
        }
        break;
    case EVENT_SELECT:
        circuit_start_buttons(self);
        break;
    case EVENT_CLOSE:
        circuits_free(self);
        break;
    case EVENT_CHILD_ENDED:
        if (event->a == 0x3E9) {
            circuit_create(self);
        } else if (event->a == 0x65) {
            const char* list = FIELD(self, DELETE_PROMPT, BYTE*) ? LIST_RANK : LIST_DONE_RANK;

            DeleteFileA(FIELD(self, CIRCUIT_PATHS, char**)[
                list_data(dialog_find_control(self, NULL, list, 3), NULL, -1)]);
            circuits_fill(self);
            circuit_start_buttons(self);
        }
        break;
    }
}

/* ---- PCNew.dtm: a new circuit's name, difficulty, class and opponents ---- */

#define EDIT_BOX       NAME(0x0056FE58)     /* "EditBox" */
#define OK_BUTTON      NAME(0x0056FE60)     /* "OkButton" */
#define CANCEL_BUTTON  NAME(0x0056FF44)     /* "CancelButton" */
#define RAD_CLASS_125  NAME(0x00577D40)     /* "RadClass125" */
#define RAD_CLASS_250  NAME(0x00577D5C)     /* "RadClass250" */
#define RAD_LOD_EASY   NAME(0x0056FA40)     /* "RadLODEasy" */
#define RAD_LOD_MEDIUM NAME(0x00577D4C)     /* "RadLODMedium" */
#define LIST_OPPONENTS NAME(0x0056FA5C)     /* "OpponentsListBox" */

/*
    PCNew.dtm (vtable 0x0055C318) answers into the caller's (0x7F58 to 0x7F64): OK ends it with
    1001, Cancel with 0. OK is enabled while the name is not empty (any edit box's change counts).
*/
PORT(0x004DA470)
void __fastcall circuit_new_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, OK_BUTTON)) {
            edit_get_text(dialog_find_control(self, NULL, EDIT_BOX, 11), NULL,
                          FIELD(self, 0x7F58, char*), 0x40);
            *FIELD(self, 0x7F60, int*) = radio_number(dialog_find_control(self, NULL,
                                                                          RAD_CLASS_125, 4));
            *FIELD(self, 0x7F5C, int*) = radio_number(dialog_find_control(self, NULL,
                                                                          RAD_LOD_EASY, 4));
            *FIELD(self, 0x7F64, DWORD*) = list_data(dialog_find_control(self, NULL,
                                                                         LIST_OPPONENTS, 3),
                                                     NULL, -1);
            dialog_end(self, NULL, 0x3E9);
        } else if (clicked(event, CANCEL_BUTTON)) {
            dialog_end(self, NULL, 0);
        }
    } else if (event->type == EVENT_OPEN) {
        char number[0x10];
        BYTE* list;
        int i;

        radio_set(dialog_find_control(self, NULL, RAD_CLASS_250, 4), NULL, 0);
        radio_set(dialog_find_control(self, NULL, RAD_LOD_MEDIUM, 4), NULL, 0);
        edit_set_text(dialog_find_control(self, NULL, EDIT_BOX, 11), NULL, game_profile);
        list = dialog_find_control(self, NULL, LIST_OPPONENTS, 3);
        list_clear(list);
        for (i = 3; i < 11; i++) {
            game_FUN_0053a11f(number, NAME(0x0056EEA8), i);                     /* "%d" */
            list_add_text(list, NULL, number, i, 0);
        }
        list_select_data(list, NULL, 7);
        gui_user_queue_capture(FIELD(self, 0x34, gui_user*), NULL,
                               dialog_find_control(self, NULL, EDIT_BOX, 11), NULL, FALSE);
    } else if (event->type == EVENT_EDIT_CHANGED) {
        char name[0x80];
        BYTE* ok;

        edit_get_text(dialog_find_control(self, NULL, EDIT_BOX, 11), NULL, name, 0x7F);
        ok = dialog_find_control(self, NULL, OK_BUTTON, 0);
        VIRTUAL(ok, 0xC4, method_1)(ok, NULL, name[0] != 0);                    /* enable */
    }
}

/* Where PCNew.dtm puts its answers. */
PORT(0x004DA690)
void __fastcall circuit_new_bind(BYTE* self, void* edx_unused, char* name, int* difficulty,
                                 int* size_class, DWORD* riders)
{
    (void) edx_unused;
    FIELD(self, 0x7F58, char*) = name;
    FIELD(self, 0x7F5C, int*) = difficulty;
    FIELD(self, 0x7F60, int*) = size_class;
    FIELD(self, 0x7F64, DWORD*) = riders;
}

/* ---- PCC.dtm: the circuit under way ---- */

/* Its pages, each a dialog over it: the next race, the bike and rider, the standings. */
#define CENTRAL_PAGES   0x7F58      /* BYTE*[3] */
#define TAB_NEXT        NAME(0x00577D94)    /* "TabNext" */
#define TAB_BIKE_RIDER  NAME(0x00577D84)    /* "TabBikeRider" */
#define TAB_STANDINGS   NAME(0x00577D74)    /* "TabStandings" */
#define TAB_CONTROL     4

static const struct {
    DWORD       size;
    int         line;
    const char* file;
    DWORD       vtable;
} central_pages[3] = {
    { 0x7F58, 0x248, NAME(0x00577E10), 0x0055C39C },               /* PCCNext.dtm */
    { 0x7F9C, 0x24E, NAME(0x00577E1C), 0x0055C420 },               /* PCCBikeR.dtm */
    { 0x7F58, 0x254, NAME(0x00577E2C), 0x0055C4A4 },               /* PCCStand.dtm */
};

__declspec(noinline) void __fastcall central_show_page(BYTE* self, void* edx_unused, int page);

/* Show page `page` (0 to 2) over the screen, closing the others; one already open stays. */
PORT(0x004DACC0)
void __fastcall central_show_page(BYTE* self, void* edx_unused, int page)
{
    RECT area = { 16, 41, 626, 396 };
    BYTE* dialog;
    int i;

    (void) edx_unused;
    for (i = 0; i < 3; i++) {
        BYTE** open = &FIELD(self, CENTRAL_PAGES + 4 * i, BYTE*);

        if (*open && page != i) {
            object_destroy(*open);
            *open = NULL;
        }
    }
    if (page < 0 || page > 2 || FIELD(self, CENTRAL_PAGES + 4 * page, BYTE*))
        return;
    dialog = pc_dialog_new(central_pages[page].size, central_pages[page].line,
                           central_pages[page].file, central_pages[page].vtable);
    FIELD(self, CENTRAL_PAGES + 4 * page, BYTE*) = dialog;
    gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL, dialog, 0, 1, (DWORD) &area, self, 0,
                     0, 1);
}

/* A tab: words for both its states, aligned 0x22. */
static BYTE* central_tab_setup(BYTE* self, const char* name, UINT id)
{
    BYTE* tab = dialog_find_control(self, NULL, name, TAB_CONTROL);

    option_load_text(tab, NULL, 0, game_language, id);
    option_load_text(tab, NULL, 1, game_language, id);
    control_set_align(tab, NULL, 0x22);
    return tab;
}

/* OptBase.dtm over `parent`, on its page `page` (0 the options, 1 the controls, 2 the bike). */
static void central_options_open(BYTE* self, BYTE* parent, int line, DWORD page)
{
    BYTE* options = pc_dialog_new(0x7F70, line, NAME(0x0056F690), 0x00556BD0);

    gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL, options, 0, 2, 0, parent, page, 0, 1);
}

/*
    The circuit's race, or practice on its track: the pages' controls into the settings, the
    race kind and track from the circuit's current race (its file's name without the extension),
    a race with all its riders or practice alone, and the loading screen.
*/
static void central_race_start(BYTE* self, ui_event* event)
{
    BYTE* circuit = pro_circuit;
    BYTE* races;
    char track[0x80];
    char* dot;
    int i;

    session_hide_scene(game_session);
    FIELD(game_main, 0x2B64, BYTE) = 0;                         /* no ghost */
    FIELD(race_settings, SET_28, DWORD) = 0;
    for (i = 0; i < 3; i++) {
        BYTE* page = FIELD(self, CENTRAL_PAGES + 4 * i, BYTE*);

        if (page)
            dialog_notify_controls(page, NULL, 1);
    }
    game_FUN_0045f530(race_events);
    if (clicked(event, START)) {
        race_kind = FIELD(circuit, CIRCUIT_KIND, int);
        race_mode = 1;
        FIELD(race_settings, SET_LAPS, DWORD) =
            FIELD(circuit, CIRCUIT_EVENTS + FIELD(circuit, CIRCUIT_KIND, int) * 16 + 0xC, DWORD);
        FIELD(race_settings, SET_OPPONENTS, int) = FIELD(circuit, CIRCUIT_RIDERS, int) - 1;
    } else {
        race_kind = FIELD(circuit, CIRCUIT_KIND, int);
        race_mode = 0;
        FIELD(race_settings, SET_OPPONENTS, int) = 0;
    }
    profile_set_event(game_profile, NULL, race_kind);
    races = FIELD(circuit, CIRCUIT_EVENTS + FIELD(circuit, CIRCUIT_KIND, int) * 16, BYTE*);
    text_copy(track, FIELD(races, FIELD(circuit, CIRCUIT_RACE, int) * 12 - 12, const char*));
    dot = game__strrchr(track, '.');
    if (dot)
        *dot = 0;
    text_copy((char*) race_settings + SET_TRACK, track);
    i = FIELD(circuit, CIRCUIT_KIND, int);
    FIELD(race_settings, SET_TRACK_KIND, BYTE) = i == 1 || i == 5;
    FIELD(race_settings, SET_TREES, DWORD) = 1;
    FIELD(game_main, 0x60C, DWORD) = FIELD(circuit, CIRCUIT_DIFFICULTY, DWORD);
    race_start_loading();
    VIRTUAL(self, 0x68, method_0)(self);
}

/*
    PCC.dtm (vtable 0x00559AC4). Its tabs switch the page shown; Back saves the circuit and
    goes back to PCStart.dtm; Start and Practice start the race.
*/
PORT(0x004DA6C0)
void __fastcall central_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    switch (event->type) {
    case EVENT_OPEN:
        FIELD(self, CENTRAL_PAGES, BYTE*) = NULL;
        FIELD(self, CENTRAL_PAGES + 4, BYTE*) = NULL;
        FIELD(self, CENTRAL_PAGES + 8, BYTE*) = NULL;
        radio_set(central_tab_setup(self, TAB_NEXT, 0x1446), NULL, 0);
        central_tab_setup(self, TAB_BIKE_RIDER, 0x1447);
        central_tab_setup(self, TAB_STANDINGS, 0x1448);
        radio_set(dialog_find_control(self, NULL, TAB_NEXT, TAB_CONTROL), NULL, 0);
        central_show_page(self, NULL, 0);
        break;
    case EVENT_CLICK:
        if (clicked(event, BACK)) {
            game_FUN_004d88a0(pro_circuit, NULL, pro_circuit_path);
            gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL,
                             pc_dialog_new(0x8074, 0x1E8, PCSTART_DTM, 0x00556AC8), 0, 2,
                             0, NULL, 0, 0, 1);
            dialog_end(event_dialog(event), NULL, 0);
            event->e = 1;
        } else if (clicked(event, START) || clicked(event, NAME(0x00577D68))) {  /* "Practice" */
            central_race_start(self, event);
        } else if (clicked(event, TAB_NEXT)) {
            central_show_page(self, NULL, 0);
        } else if (clicked(event, TAB_BIKE_RIDER)) {
            central_show_page(self, NULL, 1);
        } else if (clicked(event, TAB_STANDINGS)) {
            central_show_page(self, NULL, 2);
        } else if (clicked(event, NAME(0x0056F8BC))) {                        /* "Options" */
            central_options_open(self, self, 0x22A, 0);
        } else if (clicked(event, NAME(0x00575700))) {                        /* "Controls" */
            central_options_open(self, self, 0x22D, 1);
        } else if (clicked(event, HELP)) {
            profile_open_help(game_profile, NULL, HELP_TOPIC, 0);
        }
        break;
    case EVENT_CLOSE:
        if (FIELD(self, 0x2C, BYTE*))
            dialog_show(FIELD(self, 0x2C, BYTE*), NULL, 1);
        break;
    }
}

/* ---- PCCBikeR.dtm: the bike and rider page ---- */

/* The layout dlgprocs.c's SPBikeR.dtm page has (BR_* there). */

/* A zero vector the next file's statics set up (0x4DEBF0); this page reads it. */
#define next_vector_zero (*(const vector3*) 0x0068FC48)

float __cdecl   game_FUN_00461ac0(float value);                           /* a fast square root */
void __fastcall game_FUN_0042f8d0(BYTE* camera, void* edx_unused, const vector3* position,
                                  const void* a, const void* b, const void* c, const float* fov);
typedef int (__fastcall* camera_look_at)(void* camera, void* edx_unused, vector3 target);
void __fastcall game_FUN_004300b0(BYTE* camera, void* edx_unused, int x, int y, int width,
                                  int height);

__declspec(noinline) void __fastcall central_choices_fill(BYTE* self);

/* The bikes a class allows: any for open, else those of its engine class (125: 0, 250: 1-2). */
static BOOL bike_in_class(const BYTE* bike, int size_class)
{
    int engine;

    if (size_class == 3 || FIELD(bike, 0x88, DWORD))
        return TRUE;
    engine = engine_class(FIELD(bike, 0x8C, int));
    if (engine == 0)
        return size_class == 1;
    return engine <= 2 && size_class == 2;
}

/*
    DDLBikes: the session's bikes the circuit's class allows whose maker's player kind is at
    most its level, as "maker model", sorted, with the circuit's bike picked. DDLRiders: the
    riders of a kind at most its level or marked at 0xC0, with the circuit's rider picked.
*/
PORT(0x004DB710)
void __fastcall central_choices_fill(BYTE* self)
{
    BYTE* list = combo_list(self, LIST_BIKES);
    int bike_chosen = 0;
    int rider_chosen = 0;
    char label[0x80];
    int i;

    list_clear(list);
    for (i = 0; i < FIELD(game_session, 0x54, int); i++) {
        BYTE* bike = FIELD(game_session, 0x50, BYTE*) + i * BIKE_SIZE;
        BYTE* maker;

        if (!bike_in_class(bike, FIELD(pro_circuit, CIRCUIT_CLASS, int)))
            continue;
        maker = session_players + FIELD(bike, 0, int) * RIDER_SIZE;
        if (FIELD(maker, 0xC4, int) > FIELD(pro_circuit, CIRCUIT_LEVEL, int))
            continue;
        game_FUN_0053a11f(label, TWO_WORDS, (const char*) maker, (const char*) bike + 4);
        list_add_text(list, NULL, label, i, 0);
        if (FIELD(pro_circuit, CIRCUIT_BIKE, int) == i)
            bike_chosen = i;
    }
    if (bike_chosen)
        list_select_data(list, NULL, bike_chosen);
    else
        list_select_all(list, NULL, 0);
    list_sort(list, NULL, TRUE);

    list = combo_list(self, LIST_RIDERS);
    list_clear(list);
    for (i = 0; i < FIELD(game_session, 0x5C, int); i++) {
        BYTE* rider = FIELD(game_session, 0x58, BYTE*) + i * RIDER_SIZE;

        if (FIELD(rider, 0xC4, int) <= FIELD(pro_circuit, CIRCUIT_LEVEL, int)
            || FIELD(rider, 0xC0, DWORD)) {
            list_add_text(list, NULL, (const char*) rider, i, 0);
            if (FIELD(pro_circuit, CIRCUIT_RIDER, int) == i)
                rider_chosen = i;
        }
    }
    if (rider_chosen)
        list_select_data(list, NULL, rider_chosen);
    else
        list_select_all(list, NULL, 0);
}

static void engine_size_add(BYTE* list, const char* text, int cc)
{
    list_add_text(list, NULL, text, engine_class(cc), 0);
}

static void central_bike_rider_open(BYTE* self)
{
    vector3* position = &FIELD(self, BR_POSITION, vector3);
    vector3* target = &FIELD(self, BR_TARGET, vector3);
    RECT* view = &FIELD(self, BR_VIEW, RECT);
    float fov = 50.0f;
    double dx, dy, dz;
    float squared;
    BYTE* list;
    BYTE* camera;
    int size_class;

    FIELD(self, BR_MANUAL, int) = 0;
    option_select(dialog_find_control(self, NULL, CHECK_AUTO_ROTATE, 2), NULL, 1);
    FIELD(self, BR_RIDER_CHANGED, int) = 1;
    FIELD(self, BR_BIKE_CHANGED, int) = 1;
    central_choices_fill(self);

    /* The engine sizes the circuit's class allows. */
    list = combo_list(self, LIST_ENGINE_SIZE);
    list_clear(list);
    size_class = FIELD(pro_circuit, CIRCUIT_CLASS, int);
    if (size_class == 1 || size_class == 3)
        engine_size_add(list, NAME(0x0056FC28), 125);                   /* "125cc 2-stroke" */
    size_class = FIELD(pro_circuit, CIRCUIT_CLASS, int);
    if (size_class == 2 || size_class == 3) {
        engine_size_add(list, NAME(0x0056FC18), 250);                   /* "250cc 2-stroke" */
        engine_size_add(list, NAME(0x0056FC08), 400);                   /* "400cc 4-stroke" */
    }
    if (FIELD(pro_circuit, CIRCUIT_CLASS, int) == 3) {
        engine_size_add(list, NAME(0x0056FBF8), 500);                   /* "500cc 2-stroke" */
        engine_size_add(list, NAME(0x0056FBE8), 600);                   /* "600cc 4-stroke" */
    }
    list_select_data(list, NULL, engine_class(FIELD(pro_circuit, CIRCUIT_ENGINE, int)));
    edit_set_allowed(dialog_find_control(self, NULL, EDIT_PLATE, 11), NULL, DIGITS);
    session_show_scene(game_session, NULL, self);

    view->left = 82;
    view->right = 534;
    view->top = 116;
    view->bottom = 336;
#ifdef OPENMCM2_DEV
    menu_place_view(self, view);         /* where the scaled menus put it */
#endif
    *position = next_vector_zero;
    position->z = 15.0f;
    position->y = 1.0f;
    *target = session_centre;
    target->y = target->y + 3.0f;
    game_FUN_0042f8d0(session_camera, NULL, position, NULL, NULL, NULL, &fov);
    camera = session_camera;
    VIRTUAL(camera, 0x74, camera_look_at)(camera, NULL, *target);
    game_FUN_004300b0(session_camera, NULL, view->left, view->top, view->right - view->left,
                      view->bottom - view->top);

    dx = (double) position->x - session_centre.x;
    dy = (double) position->y - session_centre.y;
    dz = (double) position->z - session_centre.z;
    squared = (float) (dy * dy + dx * dx + dz * dz);
    FIELD(self, BR_DISTANCE, float) = x87_equal(squared, 1.0f) ? 1.0f : game_FUN_00461ac0(squared);

    pc_rider_choose(self);
    pc_bike_choose(self);
    pc_players_set(self, NULL, FIELD(game_main, 0x2144, DWORD));
    game_FUN_0053a27e(game_FUN_004c41d0());
    if (FIELD(self, 0x110, BYTE*))
        FIELD(self, BR_SLOT, int) = background_new_slot(FIELD(self, 0x110, BYTE*), NULL, 0);
}

/*
    PCCBikeR.dtm (vtable 0x0055C420): the single-player bike and rider page's handler, with the
    circuit's choices.
*/
PORT(0x004DAEC0)
void __fastcall central_bike_rider_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    char number[12];
    int plate;

    (void) edx_unused;
    switch (event->type) {
    case EVENT_OPEN:
        central_bike_rider_open(self);
        break;
    case EVENT_SELECT:
        if (clicked(event, LIST_BIKES))
            pc_bike_choose(self);
        else if (clicked(event, LIST_RIDERS))
            pc_rider_choose(self);
        else if (clicked(event, LIST_ENGINE_SIZE))
            pc_bike_choose(self);
        break;
    case EVENT_CLICK:
        if (clicked(event, NAME(0x0056FBB4)))                                  /* "BikeLeft" */
            combo_step(self, LIST_BIKES, TRUE);
        else if (clicked(event, NAME(0x0056FBA8)))                             /* "BikeRight" */
            combo_step(self, LIST_BIKES, FALSE);
        else if (clicked(event, NAME(0x0056FB9C)))                             /* "RiderLeft" */
            combo_step(self, LIST_RIDERS, TRUE);
        else if (clicked(event, NAME(0x0056FB90)))                             /* "RiderRight" */
            combo_step(self, LIST_RIDERS, FALSE);
        else if (clicked(event, CHECK_AUTO_ROTATE))
            FIELD(self, BR_MANUAL, int) =
                !option_current_index(dialog_find_control(self, NULL, CHECK_AUTO_ROTATE, 2));
        else if (clicked(event, NAME(0x0056FB84)))                             /* "ButWrench" */
            central_options_open(self, FIELD(self, 0x2C, BYTE*), 0x2CF, 2);
        break;
    case EVENT_EDIT_DONE:
        /* Plates run 101 to 999; under 100 gains 100 (so 100 itself becomes 101). */
        if (!clicked(event, EDIT_PLATE))
            break;
        edit_get_text((BYTE*) event->c, NULL, number, 9);
        plate = game_FUN_0053b6c7(number);
        if (plate < 100)
            plate += 100;
        if (plate < 101)
            plate = 101;
        else if (plate > 999)
            plate = 999;
        FIELD(game_main, 0x2144, int) = plate;
        game_FUN_0054aaa5(plate, number, 10);
        edit_set_text((BYTE*) event->c, NULL, number);
        pc_players_set(self, NULL, plate);
        break;
    case EVENT_EDIT_CHANGED:
        if (!clicked(event, EDIT_PLATE))
            break;
        edit_get_text((BYTE*) event->c, NULL, number, 9);
        plate = game_FUN_0053b6c7(number);
        if (plate >= 100 && plate <= 999) {
            pc_players_set(self, NULL, plate);
            FIELD(game_main, 0x2144, int) = plate;
        }
        break;
    case EVENT_CLOSE:
        session_hide_scene(game_session);
        if (FIELD(self, 0x110, BYTE*))
            background_free_slot(FIELD(self, 0x110, BYTE*), NULL, FIELD(self, BR_SLOT, int));
        game_FUN_004eda80(object_screen(self), NULL, NULL);
        break;
    }
}

/* ---- Pro Circuit (ProCircuitProcs.cpp), from 0x4DB940 to the end ---- */

/* The series in progress, and its riders (the player is the first). */
#define CIRCUIT_ANNOUNCEMENT   0x54
#define CIRCUIT_FEES           0x454     /* the player's fees so far */
#define CIRCUIT_RIDER_RECORDS         0x465     /* 0x140 bytes each */
#define CIRCUIT_FEE            0x1229    /* int per kind */
#define CIRCUIT_PURSE_SHARE    0x1241    /* floats: the purse, and the caps on repairs, medical, bonus */
#define CIRCUIT_REPAIRS_CAP    0x1245
#define CIRCUIT_MEDICAL_CAP    0x1249
#define CIRCUIT_BONUS_CAP      0x124D
#define CIRCUIT_PLACE_SHARES   0x1251    /* float per place */
#define circuit_series(net, field) FIELD(net, CIRCUIT_EVENTS + FIELD(net, CIRCUIT_KIND, int) * 16 + (field), int)

/* A rider's record. */
#define RIDER_SIZE_PC          0x140
#define RIDER_POINTS           0x00     /* this race */
#define RIDER_WINNINGS         0x04
#define RIDER_REPAIRS          0x08
#define RIDER_MEDICAL          0x0C
#define RIDER_RANK             0x10
#define RIDER_BONUS            0x14
#define RIDER_TOTAL_POINTS     0x18
#define RIDER_TOTAL_WINNINGS   0x1C
#define RIDER_TOTAL_REPAIRS    0x20
#define RIDER_TOTAL_MEDICAL    0x24
#define RIDER_STANDING         0x28
#define RIDER_TOTAL_BONUS      0x2C
#define RIDER_CASH             0x30
#define RIDER_LOAN             0x34
#define RIDER_MAKER_LOOK       0x38
#define RIDER_BIKE_LOOK        0x78
#define RIDER_LOOK             0xB8
#define RIDER_NAME             0xF8
#define RIDER_BIKE_NAME        0x118
#define RIDER_ENGINE           0x138
#define RIDER_FOUR_STROKE      0x13C

static BYTE* circuit_rider(BYTE* net, int i)
{
    return net + CIRCUIT_RIDER_RECORDS + i * RIDER_SIZE_PC;
}

#define circuit_player(net)      circuit_rider(net, 0)
#define series_fee(net)     FIELD(net, CIRCUIT_FEE + FIELD(net, CIRCUIT_KIND, int) * 4, int)

/* The next race's track: its file, name and picture. */
static const char** next_track(BYTE* net)
{
    return (const char**) circuit_series(net, 0) + (FIELD(net, CIRCUIT_RACE, int) - 1) * 3;
}

#define DOLLARS          NAME(0x00577CF8)   /* "$%d" */
#define RACE_OF          NAME(0x0056FA98)   /* "%d/%d" */
#define TWO_LINES        NAME(0x005780CC)   /* "%s\n%s" */
#define BUTTON_ACCEPT    NAME(0x00577FDC)   /* "ButAccept" */
#define BUTTON_DECLINE   NAME(0x00577FD0)   /* "ButDecline" */
#define DESCRIPTION      NAME(0x00578024)   /* "TxtDescription" */
#define PICTURE          NAME(0x00578070)   /* "Pic" */
#define PCDEAL_DTM          NAME(0x00577F94)   /* "PCDeal.dtm" */
#define PCFINISH_DTM        NAME(0x00578054)   /* "PCFinish.dtm" */

#define GREEN 0xFF00
#define RED   0xFF

int   __fastcall game_vf_26_46A1F0(void* self);
int   __fastcall dialog_press(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg);
int   __fastcall control_set_colour(BYTE* self, void* edx_unused, COLORREF colour);
int   __fastcall control_set_picture(BYTE* self, void* edx_unused, int state, DWORD picture);
int   __fastcall control_load_picture(BYTE* self, void* edx_unused, int state, const char* file);
void  __fastcall button_set_sort_list(BYTE* self, void* edx_unused, BYTE* list);
void  __fastcall list_set_compare(BYTE* self, void* edx_unused, entry_compare compare);
int   __fastcall dialog_draw_over(BYTE* self);
void  __fastcall session_hide_scene(BYTE* self);
void  __fastcall game_FUN_004d88f0(BYTE* net);
void __cdecl circuit_screen_open(void);
BOOL  __fastcall game_FUN_00526fc0(BYTE* game, void* edx_unused, UINT id, char* text, int size);
void  __fastcall game_FUN_004eebc0(BYTE* resources, void* edx_unused, const char* file,
                                   const char* kind, DWORD a);      /* load a scn file */
double __cdecl   game_FUN_0053c685(double x);                       /* ceil */
void  __cdecl    game_FUN_00539a96(void* base, size_t count, size_t size,
                                   int (__cdecl* compare)(const void*, const void*));  /* qsort */
char* __cdecl    game__strrchr(const char* text, int c);

static void pc_save(void)
{
    game_FUN_004d88a0(pro_circuit, NULL, pro_circuit_path);
}

/* `next` opened in this screen's place. */
static void pc_open_next(BYTE* self, BYTE* next)
{
    gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL, next, 0, 2, 0, NULL, 0, 0, 1);
    dialog_end(self, NULL, 0);
}

static void pc_text(BYTE* self, const char* name, UINT id)
{
    control_load_text(dialog_find_control(self, NULL, name, 12), NULL, game_language, id);
}

static void pc_description(BYTE* self, UINT id)
{
    BYTE* text = dialog_find_control(self, NULL, DESCRIPTION, 12);

    FIELD(text, 0x1E8, DWORD) = 1;
    control_load_text(text, NULL, game_language, id);
}

static void pc_hide(BYTE* self, const char* button)
{
    control_show(dialog_find_control(self, NULL, button, 1), NULL, FALSE, TRUE);
}

static void pc_picture(BYTE* self, const char* name)
{
    BYTE* picture = dialog_find_control(self, NULL, PICTURE, 5);

    control_set_picture(picture, NULL, 0, (DWORD) dialog_find(self, NULL, name));
}

/* A deal's line: its label, and the sum in `colour`. */
static void deal_item(BYTE* self, const char* label, UINT id, const char* item, int value,
                      COLORREF colour)
{
    char text[0x80];
    BYTE* control;

    pc_text(self, label, id);
    control = dialog_find_control(self, NULL, item, 12);
    game_FUN_0053a11f(text, DOLLARS, value);
    control_set_text(control, NULL, text);
    control_set_colour(control, NULL, colour);
}

#define TEXT_ITEM_1 NAME(0x00578018)    /* "TxtItem1" */
#define ITEM_1      NAME(0x00578010)    /* "Item1" */
#define TEXT_ITEM_2 NAME(0x00578004)    /* "TxtItem2" */
#define ITEM_2      NAME(0x00577FFC)    /* "Item2" */
#define TEXT_ITEM_3 NAME(0x00578090)    /* "TxtItem3" */
#define ITEM_3      NAME(0x00578088)    /* "Item3" */
#define TEXT_ITEM_4 NAME(0x00577FF0)    /* "TxtItem4" */
#define ITEM_4      NAME(0x00577FE8)    /* "Item4" */
#define TEXT_ITEM_5 NAME(0x0057807C)    /* "TxtItem5" */
#define ITEM_5      NAME(0x00578074)    /* "Item5" */

/* ---- The bike and rider page (vtable 0x0055C420) ---- */

/* Fields as SPBikeR's (BR_* above); its own copies of the axis vectors. */
#define shadow_vector_zero (*(vector3*) 0x0068FC48)
#define shadow_vector_x    (*(vector3*) 0x0068FC58)
#define shadow_vector_y    (*(vector3*) 0x0068FC68)
#define shadow_vector_z    (*(vector3*) 0x0068FC38)

PORT(0x004DB940)
void __fastcall pc_players_set(BYTE* self, void* edx_unused, DWORD value)
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

/* Slot 26: the garage goes with the page. */
PORT(0x004DB9F0)
int __fastcall pc_bike_page_slot_26(BYTE* self)
{
    session_hide_scene(game_session);
    return game_vf_26_46A1F0(self);
}

/* Vtable slot 23: the first mouse button pressed well inside the view turns it by hand. */
PORT(0x004DBA10)
int __fastcall hold_press(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg)
{
    (void) edx_unused;
    if (key[1] == 1 && key[0] == 0) {
        const RECT* view = &FIELD(self, BR_VIEW, RECT);
        RECT inner = { view->left + 50, view->top + 30, view->right - 50, view->bottom - 30 };
        gui_input* input = FIELD(self, 0x34, gui_user*)->input;
        POINT at = { input->x, input->y };

        if (PtInRect(&inner, at)) {
            FIELD(self, BR_MANUAL, int) = 1;
            FIELD(self, BR_HELD, int) = 1;
            option_select(dialog_find_control(self, NULL, CHECK_AUTO_ROTATE, 2), NULL, 0);
        }
    }
    return dialog_press(self, NULL, key, arg);
}

static void pc_orbit(BYTE* self, float angle)
{
    vector3* position = &FIELD(self, BR_POSITION, vector3);
    vector3 offset;
    vector3 turned;
    float distance;
    BYTE* camera;

    offset.x = position->x - session_centre.x;
    offset.y = position->y - session_centre.y;
    offset.z = position->z - session_centre.z;
    D3DRMVectorRotate(&turned, &offset, &shadow_vector_y, angle);
    distance = FIELD(self, BR_DISTANCE, float);
    position->x = turned.x * distance + session_centre.x;
    position->y = turned.y * distance + session_centre.y;
    position->z = turned.z * distance + session_centre.z;
    game_FUN_0042f8d0(session_camera, NULL, position, NULL, NULL, NULL, NULL);
    camera = session_camera;
    VIRTUAL(camera, 0x74, camera_look_at)(camera, NULL, FIELD(self, BR_TARGET, vector3));
}

/* Vtable slot 10: as SPBikeR's, and the idling bike plays WaitB with its rider's WaitR. */
PORT(0x004DBAC0)
int __fastcall pc_preview_tick(BYTE* self, void* edx_unused, float seconds)
{
    gui_user* user = FIELD(self, 0x34, gui_user*);
    BYTE* model;

    (void) edx_unused;
    if (FIELD(self, 0x110, BYTE*))
        background_invalidate(FIELD(self, 0x110, BYTE*));
    if (user->input && FIELD(self, BR_MANUAL, int) && FIELD(self, BR_HELD, int)) {
        int across = user->input->x - user->cursor_x;
        int down = user->input->y - user->cursor_y;
        double height = (double) down * 0.1f + FIELD(self, BR_POSITION + 4, float);

        FIELD(self, BR_POSITION + 4, float) = (float) height;
        if (!x87_less(height, 10.0))
            height = 10.0;
        else if (1.0 > height)
            height = 1.0;
        FIELD(self, BR_POSITION + 4, float) = (float) height;
        pc_orbit(self, across * TURN_PER_PIXEL);
        FIELD(self, BR_MANUAL, int) = 1;
        option_select(dialog_find_control(self, NULL, CHECK_AUTO_ROTATE, 2), NULL, 0);
    } else if (!FIELD(self, BR_MANUAL, int)) {
        pc_orbit(self, seconds * TURN_PER_SECOND);
    }
#ifdef OPENMCM2_DEV
    if (preview_zoom(self))
        pc_orbit(self, 0.0f);
#endif

    model = session_rider;
    VIRTUAL(model, 0x1C, model_step)(model, NULL, seconds, 0, 0);
    model = session_bike;
    VIRTUAL(model, 0x1C, model_step)(model, NULL, seconds, 0, 0);

    /* (The original reads DDLBikes' choice before the idle animations, and drops it.) */
    if (FIELD(self, BR_IDLING, int) && game__rand() % 200 == 1) {
        list_data(combo_list(self, LIST_BIKES), NULL, -1);
        switch (game__rand() % 3) {
        case 0:
            game_FUN_004aa170(session_rider, NULL, NAME(0x0056FC60));          /* "LookLeftR" */
            break;
        case 1:
            game_FUN_004aa170(session_rider, NULL, NAME(0x0056FC6C));          /* "LookRightR" */
            break;
        case 2:
            game_FUN_004aa170(session_rider, NULL, NAME(0x0056FC78));          /* "StretchR" */
            break;
        }
        FIELD(self, BR_IDLING, int) = 0;
    }
    if (!FIELD(self, BR_IDLING, int) && FIELD(session_rider, 0xC, int)) {
        BYTE* bike = FIELD(game_session, 0x50, BYTE*)
                     + list_data(combo_list(self, LIST_BIKES), NULL, -1) * BIKE_SIZE;

        game_FUN_004aa170(FIELD(session_players + FIELD(bike, 0, int) * RIDER_SIZE, 0xC0, BYTE*),
                          NULL, NAME(0x00573720));                             /* "WaitB" */
        game_FUN_004aa170(session_rider, NULL, NAME(0x0056FC58));              /* "WaitR" */
        FIELD(self, BR_IDLING, int) = 1;
    }
    if (FIELD(self, 0x110, BYTE*))
        background_slot_redraw(FIELD(self, 0x110, BYTE*), NULL, FIELD(self, BR_SLOT, int),
                               &FIELD(self, BR_VIEW, RECT));
    return dialog_tick(self, NULL, seconds);
}

/* Vtable slot 13: as SPBikeR's, drawing over last and keeping the DC. */
PORT(0x004DBFD0)
int __fastcall pc_preview_draw(BYTE* self)
{
    BYTE* session;
    BYTE* look;

    if (FIELD(self, BR_BIKE_CHANGED, int)) {
        BYTE* list = combo_list(self, LIST_BIKES);

        session = game_session;
        look = FIELD(session, 0x50, BYTE*) + list_data(list, NULL, -1) * BIKE_SIZE;
        game_FUN_00445c10(player_model(FIELD(session, 0x48, BYTE*) + FIELD(look, 0, int) * 200),
                          NULL, 0, look + 0x48, game_main + 0x1C);
        FIELD(self, BR_BIKE_CHANGED, int) = 0;
    }
    if (FIELD(self, BR_RIDER_CHANGED, int)) {
        BYTE* list = combo_list(self, LIST_RIDERS);

        session = game_session;
        look = FIELD(session, 0x58, BYTE*) + list_data(list, NULL, -1) * 200;
        game_FUN_00445c10(FIELD(FIELD(session, 0x46C, BYTE*), 0x1A0, BYTE*), NULL, 0, look + 0x40,
                          game_main + 0x1C);
        FIELD(self, BR_RIDER_CHANGED, int) = 0;
    }
    return dialog_draw_over(self);
}

PORT(0x004DC0C0)
void __fastcall pc_rider_choose(BYTE* self)
{
    BYTE* list = combo_list(self, LIST_RIDERS);

    FIELD(self, BR_RIDER_CHANGED, int) = 1;
    FIELD(pro_circuit, CIRCUIT_RIDER, DWORD) = list_data(list, NULL, -1);
}

/*
    The bike picked: only its maker's model shows, its engine becomes the player's (chosen in
    DDLEngineSize where the bike allows), the class's handling is applied - every time, and
    with the list's index as the class for a chosen engine - and the plate is shown again.
*/
PORT(0x004DC100)
void __fastcall pc_bike_choose(BYTE* self)
{
    BYTE* list = combo_list(self, LIST_BIKES);
    BYTE* bike = FIELD(game_session, 0x50, BYTE*) + list_data(list, NULL, -1) * BIKE_SIZE;
    BYTE* maker = session_players + FIELD(bike, 0, int) * RIDER_SIZE;
    BYTE* engine;
    char number[12];
    int size_class;
    int i;

    for (i = 0; i < FIELD(game_session, 0x4C, int); i++)
        model_show(session_players + i * RIDER_SIZE, FALSE);
    model_show(maker, TRUE);
    FIELD(self, BR_BIKE_CHANGED, int) = 1;

    engine = dialog_find_control(self, NULL, LIST_ENGINE_SIZE, 0);
    if (FIELD(bike, 0x88, DWORD)) {
        if (!FIELD(engine, 0x70, DWORD))
            engine_size_show(self, engine, TRUE);
    } else if (FIELD(engine, 0x70, DWORD)) {
        engine_size_show(self, engine, FALSE);
    }

    FIELD(pro_circuit, CIRCUIT_BIKE, DWORD) = list_data(list, NULL, -1);
    if (FIELD(bike, 0x88, DWORD)) {
        int size = (int) list_data(combo_list(self, LIST_ENGINE_SIZE), NULL, -1);

        FIELD(circuit_player(pro_circuit), RIDER_ENGINE, int) = engine_sizes[size];
        FIELD(circuit_player(pro_circuit), RIDER_FOUR_STROKE, DWORD) = size == 2 || size == 4;
        size_class = size;
    } else {
        int index = (int) list_data(list, NULL, -1);

        FIELD(circuit_player(pro_circuit), RIDER_ENGINE, DWORD) =
            FIELD(FIELD(game_session, 0x50, BYTE*) + index * BIKE_SIZE, 0x8C, DWORD);
        FIELD(circuit_player(pro_circuit), RIDER_FOUR_STROKE, DWORD) =
            FIELD(FIELD(game_session, 0x50, BYTE*) + index * BIKE_SIZE, 0x90, DWORD);
        size_class = engine_class(FIELD(circuit_player(pro_circuit), RIDER_ENGINE, int));
    }
    engine_class_apply(size_class);
    FIELD(game_main, 0x1550, DWORD) = FIELD(circuit_player(pro_circuit), RIDER_ENGINE, DWORD);
    FIELD(game_main, 0x1554, DWORD) = FIELD(circuit_player(pro_circuit), RIDER_FOUR_STROKE, DWORD);

    list = dialog_find_control(self, NULL, EDIT_PLATE, 11);
    game_FUN_0054aaa5(FIELD(game_main, 0x2144, int), number, 10);
    edit_set_text(list, NULL, number);
}

/* Vtable slot 31: the choice into the player's record (out only), uncapped. */
PORT(0x004DC570)
void __fastcall pc_bike_rider_sync(BYTE* self, void* edx_unused, DWORD out)
{
    int index;
    BYTE* bike;

    (void) edx_unused;
    if (!out)
        return;
    text_copy((char*) circuit_player(pro_circuit) + RIDER_MAKER_LOOK, NO_TEXT);
    index = (int) list_data(combo_list(self, LIST_BIKES), NULL, -1);
    bike = FIELD(game_session, 0x50, BYTE*) + index * BIKE_SIZE;
    text_copy((char*) circuit_player(pro_circuit) + RIDER_MAKER_LOOK,
              (const char*) session_players + FIELD(bike, 0, int) * RIDER_SIZE + 0x40);
    text_copy((char*) circuit_player(pro_circuit) + RIDER_BIKE_LOOK,
              (const char*) FIELD(game_session, 0x50, BYTE*) + index * BIKE_SIZE + 0x48);
    text_copy((char*) circuit_player(pro_circuit) + RIDER_BIKE_NAME,
              (const char*) FIELD(game_session, 0x50, BYTE*) + index * BIKE_SIZE + 4);
    text_copy((char*) circuit_player(pro_circuit) + RIDER_LOOK, NO_TEXT);
    index = (int) list_data(combo_list(self, LIST_RIDERS), NULL, -1);
    text_copy((char*) circuit_player(pro_circuit) + RIDER_LOOK,
              FIELD(game_session, 0x58, char*) + index * 200 + 0x40);
}

/* ---- The next race (vtable 0x0055C39C) ---- */

/*
    Opening shows the fee, the purse (the fee times the series' share), the track, the kind of
    race, the race count, the player's cash, fees, repairs and medical bills, the announcement,
    and the track's picture; the track's env and scn files are loaded.
*/
PORT(0x004DC740)
void __fastcall pc_next_race_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* net = pro_circuit;
    char text[0x80];
    char picture_file[0x80];
    char path[0x104];
    const char* folder;
    const char* extension = NULL;
    BYTE* picture;
    char* dot;
    int fee;
    int races;

    (void) edx_unused;
    if (event->type != EVENT_OPEN)
        return;
    fee = series_fee(net);
    folder = (const char*) fee;     /* kind 4 is left with this as its folder (the original's) */

    game_FUN_0053a11f(text, DOLLARS, fee);
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577ECC), 12), NULL, text); /* "TxtNextFee" */
    game_FUN_0053a11f(text, DOLLARS, (int) ((double) fee * FIELD(net, CIRCUIT_PURSE_SHARE, float)));
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577EBC), 12), NULL, text); /* "TxtNextPurse" */
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577EAC), 12), NULL,       /* "TxtNextTrack" */
                     next_track(net)[1]);

    /* Kind 4 has no words: the purse stays in `text` (the original's). */
    switch (FIELD(net, CIRCUIT_KIND, int)) {
    case 1: game_FUN_00526fc0(game_main, NULL, 0x13ED, text, sizeof text); break;
    case 2: game_FUN_00526fc0(game_main, NULL, 0x13EF, text, sizeof text); break;
    case 3: game_FUN_00526fc0(game_main, NULL, 0x13F2, text, sizeof text); break;
    case 5: game_FUN_00526fc0(game_main, NULL, 0x13F0, text, sizeof text); break;
    }
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577EA0), 12), NULL, text); /* "TxtNextType" */

    races = circuit_series(net, 8) ? circuit_series(net, 8) - 1 : circuit_series(net, 4);
    game_FUN_0053a11f(text, RACE_OF, FIELD(net, CIRCUIT_RACE, int), races);
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577E94), 12), NULL, text); /* "TxtNextRace" */
    game_FUN_0053a11f(text, DOLLARS, FIELD(circuit_player(net), RIDER_CASH, int));
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577E8C), 12), NULL, text); /* "TxtCash" */
    game_FUN_0053a11f(text, DOLLARS, FIELD(net, CIRCUIT_FEES, int));
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577E84), 12), NULL, text); /* "TxtFees" */
    game_FUN_0053a11f(text, DOLLARS, FIELD(circuit_player(net), RIDER_TOTAL_REPAIRS, int));
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577E78), 12), NULL, text); /* "TxtRepairs" */
    game_FUN_0053a11f(text, DOLLARS, FIELD(circuit_player(net), RIDER_TOTAL_MEDICAL, int));
    control_set_text(dialog_find_control(self, NULL, NAME(0x00577E6C), 12), NULL, text); /* "TxtMedical" */

    picture = dialog_find_control(self, NULL, NAME(0x00577E5C), 12);                   /* "TxtAnnouncement" */
    FIELD(picture, 0x1E8, DWORD) = 1;
    control_set_text(picture, NULL, (const char*) net + CIRCUIT_ANNOUNCEMENT);

    picture = dialog_find_control(self, NULL, NAME(0x00577E4C), 5);                    /* "NextTrackPic" */
    text_copy(picture_file, next_track(net)[2]);
    dot = game__strrchr(picture_file, '.');
    if (dot)
        *dot = 0;
    switch (FIELD(net, CIRCUIT_KIND, int)) {
    case 1: folder = NAME(0x0056CB6C); extension = NAME(0x00577E44); break; /* "Teraform\Baja", "01s.tga" */
    case 2: folder = NAME(0x0056F60C); extension = NAME(0x00577E3C); break; /* "Teraform\National", "s.tga" */
    case 3: folder = NAME(0x0056F63C); extension = NAME(0x00577E3C); break; /* "Teraform\SX" */
    case 5: folder = NAME(0x0056CB5C); extension = NAME(0x00577E44); break; /* "Teraform\Enduro" */
    }
    if (extension)
        text_append(picture_file, extension);
    profile_find_named_file(game_profile, NULL, folder, next_track(net)[0], NAME(0x00570138),
                      path);                                                            /* "env" */
    game_FUN_004ee910(FIELD(game_main, 0x574, BYTE*), NULL, path);
    game_FUN_004eebc0(FIELD(game_main, 0x574, BYTE*), NULL, next_track(net)[0], NAME(0x00570134),
                      0);                                                               /* "scn" */
    control_load_picture(picture, NULL, 0, picture_file);
}

/* ---- Standings ---- */

#define LIST_NAME      NAME(0x00577CD8)     /* "LstName" */
#define LIST_POINTS    NAME(0x00577C8C)     /* "LstPoints" */
#define LIST_BIKE      NAME(0x00577F10)     /* "LstBike" */
#define LIST_CASH      NAME(0x00577C78)     /* "LstCash" */
#define LIST_WINNINGS  NAME(0x00577F04)     /* "LstWinnings" */
#define LIST_EXPENSES  NAME(0x00577EF8)     /* "LstExpenses" */
#define LIST_REPAIRS   NAME(0x00577FC4)     /* "LstRepairs" */
#define LIST_MEDICAL   NAME(0x00577FB8)     /* "LstMedical" */
#define BUT_RANK       NAME(0x00577CC0)     /* "ButRank" */
#define BUT_NAME       NAME(0x00577CD0)     /* "ButName" */
#define BUT_POINTS     NAME(0x00577C80)     /* "ButPoints" */
#define BUT_BIKE       NAME(0x00577EF0)     /* "ButBike" */
#define BUT_CASH       NAME(0x00577C70)     /* "ButCash" */
#define BUT_WINNINGS   NAME(0x00577EE4)     /* "ButWinnings" */
#define BUT_EXPENSES   NAME(0x00577ED8)     /* "ButExpenses" */
#define BUT_REPAIRS    NAME(0x00577FAC)     /* "ButRepairs" */
#define BUT_MEDICAL    NAME(0x00577FA0)     /* "ButMedical" */

static BYTE* pc_list(BYTE* self, const char* name)
{
    return dialog_find_control(self, NULL, name, 3);
}

static void add_number(BYTE* list, int value, int row)
{
    char text[0x80];

    game_FUN_0054aaa5(value, text, 10);
    list_add_text(list, NULL, text, row, 0);
}

static void add_dollars(BYTE* list, int value, int row)
{
    char text[0x80];

    game_FUN_0053a11f(text, DOLLARS, value);
    list_add_text(list, NULL, text, row, 0);
}

/* A column's heading sorts it; with `compare`, by that. */
static void pc_column(BYTE* self, const char* button, BYTE* list, entry_compare compare)
{
    button_set_sort_list(dialog_find_control(self, NULL, button, 1), NULL, list);
    if (compare)
        list_set_compare(list, NULL, compare);
}

/* The series so far: standing, name, points, bike, cash, winnings (with bonuses), expenses. */
PORT(0x004DCB80)
void __fastcall pc_standings_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* net;
    BYTE* rank, * name, * points, * bike, * cash, * winnings, * expenses;
    int i;

    (void) edx_unused;
    if (event->type != EVENT_OPEN)
        return;
    net = pro_circuit;
    rank = pc_list(self, LIST_RANK);
    name = pc_list(self, LIST_NAME);
    points = pc_list(self, LIST_POINTS);
    bike = pc_list(self, LIST_BIKE);
    cash = pc_list(self, LIST_CASH);
    winnings = pc_list(self, LIST_WINNINGS);
    expenses = pc_list(self, LIST_EXPENSES);
    for (i = 0; i < FIELD(net, CIRCUIT_RIDERS, int); i++) {
        BYTE* rider = circuit_rider(net, i);

        add_number(rank, FIELD(rider, RIDER_STANDING, int), i);
        list_add_text(name, NULL, (const char*) rider + RIDER_NAME, i, 0);
        add_number(points, FIELD(rider, RIDER_TOTAL_POINTS, int), i);
        list_add_text(bike, NULL, (const char*) rider + RIDER_BIKE_NAME, i, 0);
        add_dollars(cash, FIELD(rider, RIDER_CASH, int), i);
        add_dollars(winnings, FIELD(rider, RIDER_TOTAL_WINNINGS, int)
                              + FIELD(rider, RIDER_TOTAL_BONUS, int), i);
        /* The player's fees go into every rider's expenses (the original's). */
        add_dollars(expenses, FIELD(rider, RIDER_TOTAL_REPAIRS, int)
                              + FIELD(rider, RIDER_TOTAL_MEDICAL, int) + FIELD(net, CIRCUIT_FEES, int), i);
    }
    pc_column(self, BUT_RANK, rank, entries_by_number);
    pc_column(self, BUT_NAME, name, NULL);
    pc_column(self, BUT_POINTS, points, entries_by_number_falling);
    pc_column(self, BUT_BIKE, bike, NULL);
    pc_column(self, BUT_CASH, cash, entries_by_cash_falling);
    pc_column(self, BUT_WINNINGS, winnings, entries_by_cash_falling);
    pc_column(self, BUT_EXPENSES, expenses, entries_by_cash);
    list_sort(rank, NULL, TRUE);
}

__declspec(noinline) void __fastcall pc_race_pay(BYTE* self);

/*
    PCLRace.dtm (vtable 0x00559B48). Opening pays out the race and lists it; Back goes on: after
    the last race the series winner's deal, else one of the deals for running short (then the
    transfer), or the next race.
*/
PORT(0x004DCE60)
void __fastcall pc_race_results_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* net = pro_circuit;

    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (!clicked(event, BACK))
            return;
        if (FIELD(net, CIRCUIT_RACE, int) == circuit_series(net, 8)
            && FIELD(circuit_player(net), RIDER_STANDING, int) == 1) {
            gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL,
                             pc_dialog_new(0x7F58, 0x558, PCDEAL_DTM, 0x0055C6B4), 0, 2, 0, NULL, 0, 0, 1);
        } else if (FIELD(circuit_player(net), RIDER_CASH, int) < series_fee(pro_circuit)) {
            BYTE warnings = FIELD(net, CIRCUIT_FLAGS, BYTE);

            if (!(warnings & 1)) {
                gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL,
                                 pc_dialog_new(0x7F64, 0x55E, PCDEAL_DTM, 0x0055C630), 0, 2, 0, NULL, 0, 0, 1);
                FIELD(net, CIRCUIT_FLAGS, BYTE) |= 1;
            } else if (!(warnings & 2)) {
                gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL,
                                 pc_dialog_new(0x7F64, 0x562, PCDEAL_DTM, 0x0055C5AC), 0, 2, 0, NULL, 0, 0, 1);
                FIELD(net, CIRCUIT_FLAGS, BYTE) |= 2;
            } else {
                FIELD(net, CIRCUIT_FLAGS, BYTE) = warnings | 4;
                gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL,
                                 pc_dialog_new(0x7F58, 0x567, PCTRANS_DTM, 0x0055C528), 0, 2, 0, NULL, 0, 0, 1);
            }
        } else {
            circuit_screen_open();
        }
        dialog_end(self, NULL, 0);
    } else if (event->type == EVENT_OPEN) {
        BYTE* rank = pc_list(self, LIST_RANK);
        BYTE* name = pc_list(self, LIST_NAME);
        BYTE* points = pc_list(self, LIST_POINTS);
        BYTE* bike = pc_list(self, LIST_BIKE);
        BYTE* winnings = pc_list(self, LIST_WINNINGS);
        BYTE* repairs = pc_list(self, LIST_REPAIRS);
        BYTE* medical = pc_list(self, LIST_MEDICAL);
        int i;

        pc_race_pay(self);
        for (i = 0; i < FIELD(pro_circuit, CIRCUIT_RIDERS, int); i++) {
            add_number(rank, FIELD(circuit_rider(pro_circuit, i), RIDER_RANK, int), i);
            list_add_text(name, NULL, (const char*) circuit_rider(pro_circuit, i) + RIDER_NAME, i, 0);
            add_number(points, FIELD(circuit_rider(pro_circuit, i), RIDER_POINTS, int), i);
            list_add_text(bike, NULL, (const char*) circuit_rider(pro_circuit, i) + RIDER_BIKE_NAME, i, 0);
            add_dollars(winnings, FIELD(circuit_rider(pro_circuit, i), RIDER_WINNINGS, int), i);
            add_dollars(repairs, FIELD(circuit_rider(pro_circuit, i), RIDER_REPAIRS, int), i);
            add_dollars(medical, FIELD(circuit_rider(pro_circuit, i), RIDER_MEDICAL, int), i);
        }
        pc_column(self, BUT_RANK, rank, entries_by_number);
        pc_column(self, BUT_NAME, name, NULL);
        pc_column(self, BUT_POINTS, points, entries_by_number_falling);
        pc_column(self, BUT_BIKE, bike, NULL);
        pc_column(self, BUT_WINNINGS, winnings, entries_by_cash_falling);
        pc_column(self, BUT_REPAIRS, repairs, entries_by_cash);
        pc_column(self, BUT_MEDICAL, medical, entries_by_cash);
        list_sort(rank, NULL, TRUE);
        game_FUN_004d88f0(pro_circuit);
        pc_save();
    }
}

/* The final standings, PCFinish.dtm (vtable 0x0055C73C); Back goes to PCStart.dtm. */
PORT(0x004DDCE0)
void __fastcall pc_final_standings_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* net = pro_circuit;

    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BACK))
            pc_open_next(self, pc_dialog_new(0x8074, 0x67B, PCSTART_DTM, 0x00556AC8));
    } else if (event->type == EVENT_OPEN) {
        BYTE* rank = pc_list(self, LIST_RANK);
        BYTE* name = pc_list(self, LIST_NAME);
        BYTE* points = pc_list(self, LIST_POINTS);
        BYTE* bike = pc_list(self, LIST_BIKE);
        BYTE* cash = pc_list(self, LIST_CASH);
        BYTE* repairs = pc_list(self, LIST_REPAIRS);
        BYTE* medical = pc_list(self, LIST_MEDICAL);
        int i;

        for (i = 0; i < FIELD(net, CIRCUIT_RIDERS, int); i++) {
            BYTE* rider = circuit_rider(net, i);

            add_number(rank, FIELD(rider, RIDER_STANDING, int), i);
            list_add_text(name, NULL, (const char*) rider + RIDER_NAME, i, 0);
            add_number(points, FIELD(rider, RIDER_TOTAL_POINTS, int), i);
            list_add_text(bike, NULL, (const char*) rider + RIDER_BIKE_NAME, i, 0);
            add_dollars(cash, FIELD(rider, RIDER_CASH, int), i);
            add_dollars(repairs, FIELD(rider, RIDER_TOTAL_REPAIRS, int), i);
            add_dollars(medical, FIELD(rider, RIDER_TOTAL_MEDICAL, int), i);
        }
        pc_column(self, BUT_RANK, rank, entries_by_number);
        pc_column(self, BUT_NAME, name, NULL);
        pc_column(self, BUT_POINTS, points, entries_by_number_falling);
        pc_column(self, BUT_BIKE, bike, NULL);
        pc_column(self, BUT_CASH, cash, entries_by_cash_falling);
        pc_column(self, BUT_REPAIRS, repairs, entries_by_cash);
        pc_column(self, BUT_MEDICAL, medical, entries_by_cash);
        list_sort(rank, NULL, TRUE);
    }
}

/* ---- Paying out a race ---- */

/* qsort order for (key, rider) pairs: highest key first. */
PORT(0x004DD420)
int __cdecl pairs_by_key_falling(const void* a, const void* b)
{
    int first = *(const int*) a;
    int second = *(const int*) b;

    if (first > second)
        return -1;
    return first != second;
}

/* The lower of a cost's cap and its amount, as the original's x87 compare picks it. */
static int capped(double cap, float amount)
{
    return (int) (x87_less(cap, amount) ? cap : amount);
}

/*
    Every finisher pays the fee and their damage (capped by shares of the purse), earns a bonus
    and their place's share of the purse (rounded up); the rest of the purse goes to the race's
    winner. Then the race and the series are ranked by points.
*/
PORT(0x004DD440)
void __fastcall pc_race_pay(BYTE* self)
{
    BYTE* net = pro_circuit;
    int fee = series_fee(net);
    int purse = (int) ((double) fee * FIELD(net, CIRCUIT_PURSE_SHARE, float));
    float* shares = game_FUN_004a4410(FIELD(net, CIRCUIT_RIDERS, int) * 4, PROCIRCUIT_CPP, 0x58C);
    int count = FIELD(net, CIRCUIT_RIDERS, int);
    double total = 0.0;
    float purse_part = (float) purse;
    int paid = 0;
    int ranks[11][2];       /* the original's room: more riders overrun its stack */
    BYTE* winner;
    int i;

    (void) self;
    /* `shares` is never freed (the original's). */
    for (i = 0; i < count; i++)
        total += FIELD(net, CIRCUIT_PLACE_SHARES + i * 4, float);
    for (i = 0; i < FIELD(net, CIRCUIT_RIDERS, int); i++)
        shares[i] = (float) (FIELD(net, CIRCUIT_PLACE_SHARES + i * 4, float) / total);
    FIELD(net, CIRCUIT_FEES, int) += fee;

    for (i = 0; i < FIELD(race_events, 0x4C, int); i++) {
        BYTE* entry = race_events + 0x50 + i * 0x50;
        BYTE* rider = circuit_rider(net, FIELD(entry, 0, int));
        int winnings;

        FIELD(rider, RIDER_MEDICAL, int) = capped((double) purse_part * FIELD(net, CIRCUIT_MEDICAL_CAP, float),
                                                  FIELD(entry, 0x38, float) * 250.0f);
        FIELD(rider, RIDER_REPAIRS, int) = capped((double) purse_part * FIELD(net, CIRCUIT_REPAIRS_CAP, float),
                                                  FIELD(entry, 0x34, float) * 500.0f);
        FIELD(rider, RIDER_BONUS, int) = capped((double) purse_part * FIELD(net, CIRCUIT_BONUS_CAP, float),
                                                FIELD(entry, 0x3C, float) * 400.0f);
        winnings = (int) game_FUN_0053c685((double) purse_part * shares[FIELD(entry, 4, int) - 1]);
        FIELD(rider, RIDER_WINNINGS, int) = winnings;
        FIELD(rider, RIDER_POINTS, int) = FIELD(entry, 0x28, int);
        paid += winnings;

        FIELD(rider, RIDER_CASH, int) += FIELD(rider, RIDER_BONUS, int) - FIELD(rider, RIDER_MEDICAL, int)
                                         - FIELD(rider, RIDER_REPAIRS, int) - fee + winnings;
        FIELD(rider, RIDER_TOTAL_MEDICAL, int) += FIELD(rider, RIDER_MEDICAL, int);
        FIELD(rider, RIDER_TOTAL_REPAIRS, int) += FIELD(rider, RIDER_REPAIRS, int);
        FIELD(rider, RIDER_TOTAL_BONUS, int) += FIELD(rider, RIDER_BONUS, int);
        FIELD(rider, RIDER_TOTAL_WINNINGS, int) += winnings;
        FIELD(rider, RIDER_TOTAL_POINTS, int) += FIELD(rider, RIDER_POINTS, int);
    }

    for (i = 0; i < FIELD(net, CIRCUIT_RIDERS, int); i++) {
        ranks[i][0] = FIELD(circuit_rider(net, i), RIDER_POINTS, int);
        ranks[i][1] = i;
    }
    game_FUN_00539a96(ranks, FIELD(net, CIRCUIT_RIDERS, int), sizeof ranks[0], pairs_by_key_falling);
    for (i = 0; i < FIELD(net, CIRCUIT_RIDERS, int); i++)
        FIELD(circuit_rider(net, ranks[i][1]), RIDER_RANK, int) = i + 1;

    /* With no riders, ranks[0] is unset here (the original's). */
    winner = circuit_rider(net, ranks[0][1]);
    FIELD(winner, RIDER_WINNINGS, int) += purse - paid;
    FIELD(winner, RIDER_TOTAL_WINNINGS, int) += purse - paid;
    FIELD(winner, RIDER_CASH, int) += purse - paid;

    for (i = 0; i < FIELD(net, CIRCUIT_RIDERS, int); i++) {
        ranks[i][0] = FIELD(circuit_rider(net, i), RIDER_TOTAL_POINTS, int);
        ranks[i][1] = i;
    }
    game_FUN_00539a96(ranks, FIELD(net, CIRCUIT_RIDERS, int), sizeof ranks[0], pairs_by_key_falling);
    for (i = 0; i < FIELD(net, CIRCUIT_RIDERS, int); i++)
        FIELD(circuit_rider(net, ranks[i][1]), RIDER_STANDING, int) = i + 1;
}

/* ---- Deals ---- */

#define DEAL_FEE   0x7F58
#define DEAL_CASH  0x7F5C
#define DEAL_NEED  0x7F60

static COLORREF cash_colour(BYTE* net)
{
    return FIELD(circuit_player(net), RIDER_CASH, int) >= 0 ? GREEN : RED;
}

/* PCDeal.dtm, short for the fee (vtable 0x0055C630): accept a loan of the difference, or quit. */
PORT(0x004DD7C0)
void __fastcall pc_deal_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* net = pro_circuit;
    BYTE* next;

    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_ACCEPT)) {
            FIELD(circuit_player(net), RIDER_CASH, int) += FIELD(self, DEAL_NEED, int);
            FIELD(circuit_player(net), RIDER_LOAN, int) += FIELD(self, DEAL_NEED, int);
            pc_save();
            next = pc_dialog_new(0x7F64, 0x60F, PCC_DTM, 0x00559AC4);
        } else if (clicked(event, BUTTON_DECLINE)) {
            FIELD(net, CIRCUIT_FLAGS, BYTE) |= 4;
            next = pc_dialog_new(0x7F58, 0x614, PCTRANS_DTM, 0x0055C528);
        } else {
            return;
        }
        pc_open_next(self, next);
    } else if (event->type == EVENT_OPEN) {
        FIELD(self, DEAL_FEE, int) = series_fee(net);
        FIELD(self, DEAL_CASH, int) = FIELD(circuit_player(net), RIDER_CASH, int);
        FIELD(self, DEAL_NEED, int) = FIELD(self, DEAL_FEE, int) - FIELD(self, DEAL_CASH, int);
        pc_text(self, TITLE_TEXT, 0x144A);
        pc_description(self, 0x1449);
        deal_item(self, TEXT_ITEM_1, 0x144B, ITEM_1, FIELD(self, DEAL_CASH, int), cash_colour(net));
        deal_item(self, TEXT_ITEM_2, 0x144D, ITEM_2, FIELD(self, DEAL_FEE, int), RED);
        deal_item(self, TEXT_ITEM_4, 0x144C, ITEM_4, FIELD(self, DEAL_NEED, int), GREEN);
        pc_hide(self, BACK);
    }
}

/* PCDeal.dtm, short again (vtable 0x0055C5AC): the loan is paid off into a new one. */
PORT(0x004DE060)
void __fastcall pc_loan_deal_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* net = pro_circuit;
    BYTE* next;

    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_ACCEPT)) {
            FIELD(circuit_player(net), RIDER_CASH, int) +=
                FIELD(self, DEAL_NEED, int) - FIELD(circuit_player(net), RIDER_LOAN, int);
            FIELD(circuit_player(net), RIDER_LOAN, int) = 0;
            pc_save();
            next = pc_dialog_new(0x7F64, 0x6D0, PCC_DTM, 0x00559AC4);
        } else if (clicked(event, BUTTON_DECLINE)) {
            FIELD(net, CIRCUIT_FLAGS, BYTE) |= 4;
            next = pc_dialog_new(0x7F58, 0x6D5, PCTRANS_DTM, 0x0055C528);
        } else {
            return;
        }
        pc_open_next(self, next);
    } else if (event->type == EVENT_OPEN) {
        int loan = FIELD(circuit_player(net), RIDER_LOAN, int);

        FIELD(self, DEAL_FEE, int) = series_fee(net);
        FIELD(self, DEAL_CASH, int) = FIELD(circuit_player(net), RIDER_CASH, int);
        FIELD(self, DEAL_NEED, int) = FIELD(self, DEAL_FEE, int) - FIELD(self, DEAL_CASH, int) + loan;
        pc_text(self, TITLE_TEXT, 0x1452);
        pc_description(self, 0x1451);
        deal_item(self, TEXT_ITEM_1, 0x144B, ITEM_1, FIELD(self, DEAL_CASH, int), cash_colour(net));
        deal_item(self, TEXT_ITEM_2, 0x144D, ITEM_2, FIELD(self, DEAL_FEE, int), RED);
        deal_item(self, TEXT_ITEM_3, 0x1453, ITEM_3, loan, RED);
        deal_item(self, TEXT_ITEM_4, 0x144F, ITEM_4, FIELD(self, DEAL_NEED, int), GREEN);
        deal_item(self, TEXT_ITEM_5, 0x1450, ITEM_5, 1000, GREEN);
        pc_hide(self, BACK);
    }
}

/*
    PCDeal.dtm for the series winner (vtable 0x0055C6B4): the next class opens and $3000 is
    paid; accepting starts the first race of the next series, declining ends the career.
*/
PORT(0x004DE4D0)
void __fastcall pc_champion_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* net = pro_circuit;

    (void) edx_unused;
    if (event->type == EVENT_OPEN) {
        /* Other kinds read an unset stack slot for both ids in the original; 0 here. */
        UINT title = 0;
        UINT text = 0;

        FIELD(game_main, 0x18FC + FIELD(net, CIRCUIT_KIND, int) * 4, DWORD) = 1;
        if (FIELD(net, CIRCUIT_KIND, int) == 2) {
            title = 0x1457;
            text = 0x1459;
        } else if (FIELD(net, CIRCUIT_KIND, int) == 3) {
            title = 0x1458;
            text = 0x145A;
        }
        pc_text(self, TITLE_TEXT, title);
        pc_description(self, text);
        pc_text(self, TEXT_ITEM_1, 0x145B);
        {
            char sum[0x80];
            BYTE* item = dialog_find_control(self, NULL, ITEM_1, 12);

            game_FUN_0053a11f(sum, DOLLARS, 3000);
            control_set_text(item, NULL, sum);
            control_set_colour(item, NULL, GREEN);
        }
        pc_hide(self, BACK);
        FIELD(circuit_player(net), RIDER_CASH, int) += 3000;
    } else if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_ACCEPT)) {
            char track[0x80];
            char* dot;

            game_FUN_0045f530(race_events);
            FIELD(game_main, 0x2D74, int) = FIELD(net, CIRCUIT_KIND, int);
            FIELD(game_main, 0x2D70, int) = 1;
            FIELD(game_main, 0x2D90, int) = circuit_series(net, 0xC);
            FIELD(game_main, 0x2D94, int) = FIELD(net, CIRCUIT_RIDERS, int) - 1;
            FIELD(game_main, 0x2D80, int) = 1;
            profile_set_event(game_profile, NULL, FIELD(net, CIRCUIT_KIND, int));
            text_copy(track, next_track(net)[0]);
            dot = game__strrchr(track, '.');
            if (dot)
                *dot = 0;
            text_copy((char*) game_main + 0x2DA6, track);
            FIELD(game_main, 0x2DA4, BYTE) = FIELD(net, CIRCUIT_KIND, int) == 1 || FIELD(net, CIRCUIT_KIND, int) == 5;
            FIELD(game_main, 0x60C, DWORD) = FIELD(net, CIRCUIT_DIFFICULTY, DWORD);
            race_start_loading();
            VIRTUAL(self, 0x68, method_0)(self);
        } else if (clicked(event, BUTTON_DECLINE)) {
            game_FUN_004d88f0(net);
            circuit_screen_open();
            dialog_end(self, NULL, 0);
        }
    }
}

/* ---- Endings ---- */

#define BUTTON_ACCEPT_TEXT 0x144E

/* PCTrans.dtm, transferred out (vtable 0x0055C528); any button shows the final standings. */
PORT(0x004DDB40)
void __fastcall pc_transfer_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        pc_open_next(self, pc_dialog_new(0x7F58, 0x639, PCFINISH_DTM, 0x0055C73C));
    } else if (event->type == EVENT_OPEN) {
        pc_save();
        pc_description(self, 0x1464);
        pc_text(self, TITLE_TEXT, 0x1467);
        pc_hide(self, BUTTON_DECLINE);
        control_load_text(dialog_find_control(self, NULL, BUTTON_ACCEPT, 1), NULL, game_language,
                          BUTTON_ACCEPT_TEXT);
        pc_picture(self, NAME(0x00578064));                                    /* "FailScreen" */
    }
}

/* A series finished (vtable 0x0055C210): its picture and words; any button goes to PCC.dtm. */
PORT(0x004DE7D0)
void __fastcall pc_finished_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        pc_open_next(self, pc_dialog_new(0x7F64, 0x768, PCC_DTM, 0x00559AC4));
    } else if (event->type == EVENT_OPEN) {
        /* Other kinds use the event's address for both (the original's). */
        UINT text = (UINT) event;
        const char* picture = (const char*) event;

        switch (FIELD(pro_circuit, CIRCUIT_KIND, int)) {
        case 1: text = 0x1454; picture = NAME(0x005780B4); break;  /* "BajaScreen" */
        case 2: text = 0x1455; picture = NAME(0x005780A8); break;  /* "NatScreen" */
        case 3: text = 0x1456; picture = NAME(0x0057809C); break;  /* "SuperScreen" */
        case 5: text = 0x1465; picture = NAME(0x005780C0); break;  /* "WinScreen" */
        }
        pc_description(self, text);
        pc_text(self, TITLE_TEXT, 0x1467);
        pc_hide(self, BUTTON_DECLINE);
        control_load_text(dialog_find_control(self, NULL, BUTTON_ACCEPT, 1), NULL, game_language,
                          0x916);
        pc_picture(self, picture);
    }
}

/* The career won (vtable 0x0055C294), more said to the series' winner; then the standings. */
PORT(0x004DE9B0)
void __fastcall pc_win_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* net = pro_circuit;

    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        pc_open_next(self, pc_dialog_new(0x7F58, 0x795, PCFINISH_DTM, 0x0055C73C));
    } else if (event->type == EVENT_OPEN) {
        char first[0x400];
        char second[0x400];
        char message[0x800];
        BYTE* description;

        pc_save();
        pc_text(self, TITLE_TEXT, 0x145D);
        description = dialog_find_control(self, NULL, DESCRIPTION, 12);
        game_FUN_00526fc0(game_main, NULL, 0x145E, first, 0x3FF);
        text_copy(second, NO_TEXT);
        if (FIELD(circuit_player(net), RIDER_STANDING, int) == 1)
            game_FUN_00526fc0(game_main, NULL, 0x145F, second, 0x3FF);
        game_FUN_0053a11f(message, TWO_LINES, first, second);
        FIELD(description, 0x1E8, DWORD) = 1;
        control_set_text(description, NULL, message);
        pc_hide(self, BUTTON_DECLINE);
        control_load_text(dialog_find_control(self, NULL, BUTTON_ACCEPT, 1), NULL, game_language,
                          BUTTON_ACCEPT_TEXT);
        pc_picture(self, NAME(0x005780C0));                                    /* "WinScreen" */
    }
}

/*
    ---- The vectors set up at 0x4DEBE0-0x4DED1F ----

    The runtime runs these before main. They sit at the end of this file's code, where the
    next object file (ProjectedShadow.cpp) begins, so they may be its; the bike page above
    reads their y axis.
*/

PORT(0x004DEBF0)
void __cdecl shadow_init_vector_zero(void) { set_vector(&shadow_vector_zero, 0.0f, 0.0f, 0.0f); }

PORT(0x004DEC40)
void __cdecl shadow_init_vector_x(void) { set_vector(&shadow_vector_x, 1.0f, 0.0f, 0.0f); }

PORT(0x004DEC90)
void __cdecl shadow_init_vector_y(void) { set_vector(&shadow_vector_y, 0.0f, 1.0f, 0.0f); }

PORT(0x004DECE0)
void __cdecl shadow_init_vector_z(void) { set_vector(&shadow_vector_z, 0.0f, 0.0f, 1.0f); }

/* The runtime's initializer table calls these, which only jump on. */
PORT(0x004DEBE0)
void __cdecl shadow_init_vector_zero_entry(void) { shadow_init_vector_zero(); }

PORT(0x004DEC30)
void __cdecl shadow_init_vector_x_entry(void) { shadow_init_vector_x(); }

PORT(0x004DEC80)
void __cdecl shadow_init_vector_y_entry(void) { shadow_init_vector_y(); }

PORT(0x004DECD0)
void __cdecl shadow_init_vector_z_entry(void) { shadow_init_vector_z(); }
