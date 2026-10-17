/*
    OpenMCM2 - the multiplayer screen (MPBase.dtm, vtable 0x00559BCC) and the track lists the
    event screens share, ported from the game (SelectGamePicProcs.cpp in the original).

    The race's riders are records of 0xF8 bytes at game_main + 0x215C: the user first, the other
    players next, then the computer riders each player asked for (a "team"). The other players
    sit in a table of 7 slots at 0x0068FE48 that the screen's timer refills from the network.
*/

#include "menus.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif
#include "dirlist.h"

#define SGP_CPP NAME(0x00579D90)        /* "D:\aardvark\VC\krusty2\SelectGamePicProcs.cpp" */

#define game_net       FIELD(game_main, 0x8, BYTE*)
#define game_scenes    FIELD(game_main, 0x574, BYTE*)       /* the tracks' scn files */
#define net_me         FIELD(game_net, 0xC, int)
#define net_hosting    FIELD(game_net, 0x10, DWORD)
#define net_from_lobby FIELD(game_net, 0x14, DWORD)

#define session_showing FIELD(game_session, 0x3C, int)
#define session_bikes   FIELD(game_session, 0x50, BYTE*)
#define session_riders  FIELD(game_session, 0x58, BYTE*)
#define session_tracks  FIELD(game_session, 0x60, BYTE*)
#define TRACK_SIZE          0x54
#define MAKER_SIZE          200
#define MAKER_MODEL         0x40
#define BIKE_MAKER          0x00
#define BIKE_LOOK           0x48
#define BIKE_ENGINE         0x8C
#define BIKE_FOUR_STROKE    0x90
#define RIDER_LOOK          0x40

#define race_computers  FIELD(game_main, 0x2D98, int)       /* computer riders, all teams' */
#define race_humans     FIELD(game_main, 0x2DA5, BYTE)
#define race_track      ((char*) game_main + 0x2DA6)
#define race_skill      FIELD(game_main, 0x2144, int)
#define skill_wanted    FIELD(game_main, 0x2148, int)       /* taken when 1 to 100 */
#define opponent_max    FIELD(game_main, 0x2150, int)
#define riders_count    FIELD(game_main, 0x2158, int)
#define humans_count    FIELD(game_main, 0x18, int)
#define lobby_slot      FIELD(game_main, 0x214C, int)

/* The race's riders. */
#ifdef OPENMCM2_DEV
#define entrant(n)          (entrants() + (n) * 0xF8)
#else
#define entrant(n)          (game_main + 0x215C + (n) * 0xF8)
#endif
#define ENTRANT_MAKER       0x00    /* char[0x40] each */
#define ENTRANT_BIKE        0x40
#define ENTRANT_LOOK        0x80
#define ENTRANT_SKILL       0xC0
#define ENTRANT_C4          0xC4
#define ENTRANT_PING        0xD0    /* float: the lowest seen */
#define ENTRANT_PLAYER      0xD4    /* the player it rides for */
#define ENTRANT_INDEX       0xD8
#define ENTRANT_NAME        0xDC    /* char[16] */
#define ENTRANT_ENGINE      0xEC
#define ENTRANT_FOUR_STROKE 0xF0
#define ENTRANT_F4          0xF4    /* BYTE */

/*
    The other players: 7 slots of {vtable, id, name[16]}, and the screen's flags about them. In
    the dev build a lobby has 30 riders (code/eos/protocol.h, EOS_RIDERS_MOST): the slots and their
    flags are tables of 29 here, the teams one a player, and the screen shows as many as its
    layout has rows for (mp_rows).
*/
#define PLAYER_SIZE       0x18
#ifdef OPENMCM2_DEV
#define PLAYER_SLOTS      29
static BYTE g_mp_players[PLAYER_SLOTS * PLAYER_SIZE];
static int  g_mp_ready[PLAYER_SLOTS + 1];           /* + 1: the original reads one past the slots */
static int  g_mp_announced[PLAYER_SLOTS + 1];
#define mp_players        g_mp_players
#define mp_ready          g_mp_ready
#define mp_announced      g_mp_announced
#else
#define mp_players        ((BYTE*) 0x0068FE48)
#define PLAYER_SLOTS      7
#endif
#define PLAYER_CONSTRUCT  0x004B23B0
#define PLAYER_DESTRUCT   0x005292D0
#define player_id(i)      FIELD(mp_players + (i) * PLAYER_SIZE, 4, int)
#define player_name(i)    ((char*) mp_players + (i) * PLAYER_SIZE + 8)
#ifndef OPENMCM2_DEV
#define mp_ready          ((int*) 0x0068FEFC)
#define mp_announced      ((int*) 0x0068FF18)
#endif
#define mp_user_ready     (*(int*) 0x0068FF34)
#define mp_player_count   (*(int*) 0x0068FF38)
#define mp_track_state    (*(int*) 0x0068FF3C)     /* 0 the host's track, 2 none, 3 another */
#define mp_state_44       (*(int*) 0x0068FF44)
#define settings_send_wait (*(int*) 0x00579D4C)    /* the settings go out every second tick */

/* MPBase's own. */
typedef struct mp_team {
    int player;
    int wanted;
    int given;
} mp_team;

#define MP_RACE_PAGE    0x7F58
#define MP_BIKE_PAGE    0x7F5C
#ifdef OPENMCM2_DEV
#define MP_TEAMS        (PLAYER_SLOTS + 1)
static mp_team g_mp_teams[MP_TEAMS];
#define mp_teams(self)  ((void) (self), g_mp_teams)
#else
#define mp_teams(self)  ((mp_team*) ((self) + 0x7F68))
#define MP_TEAMS        8
#endif
#define MP_RIDERS_MOST  MP_TEAMS                /* a race's riders, the players' and their computers' */
#define TEAMS_DWORDS    (MP_TEAMS * (int) sizeof(mp_team) / 4)

#define COMBO_BUTTON    0x1EC
#define LIST_COUNT      0x1EC

/* Strings. */
#define PATH              NAME(0x0056C7DC)    /* "%s\%s" */
#define UI_FOLDER         NAME(0x0056FE48)    /* "ui" */
#define READ_BINARY       NAME(0x0056C66C)    /* "rb" */
#define SCN               NAME(0x00570134)    /* "scn" */
#define ENV               NAME(0x00570138)    /* "env" */
#define ENV_FILES         NAME(0x00579DE4)    /* "*.env" */
#define NO_NAME           NAME(0x0057012C)    /* "no name" */
#define TWO_WORDS         NAME(0x0056E054)    /* "%s %s" */
#define CHAT_LINE         NAME(0x0056FB7C)    /* "%s: %s" */
#define CHAT_PLAYER       NAME(0x00579E14)    /* "~%1d%s: ~0%s" */
#define CHAT_NOTICE       NAME(0x00579E24)    /* "~5%s: %s" */
#define CHAT_OWN          NAME(0x00579E30)    /* "~1%s: ~0%s" */
#define BUT_RDY_USER      NAME(0x00579DD8)    /* "ButRdyUser" */
#define BUT_RDY_PLAYER    NAME(0x00579E04)    /* "ButRdyPlayer%d" */
#define NAME_PLAYER       NAME(0x00579ED0)    /* "NamePlayer%d" */
#define LIST_CHAT         NAME(0x00579E3C)    /* "ListChat" */
#define EDIT_CHAT         NAME(0x00579E48)    /* "EditChat" */
#define BUT_CHAT_SEND     NAME(0x00579EC4)    /* "ButChatSend" */
#define USER_NAME         NAME(0x00579EE0)    /* "UserName" */
#define LARGEST_OPPONENT  NAME(0x00579DEC)    /* "LargestOpponentDropDown" */
#define OPPONENTS         NAME(0x0056FA5C)    /* "OpponentsListBox" */
#define START             NAME(0x0056F8C4)    /* "Start" */
#define BACK              NAME(0x0056F8CC)    /* "Back" */
#define OPTIONS           NAME(0x0056F8BC)    /* "Options" */
#define JOYSTICK          NAME(0x0056F8B0)    /* "Joystick" */
#define HELP              NAME(0x0056F8A8)    /* "Help" */
#define HELP_TOPIC        NAME(0x0056F560)    /* "MCM2HELP" */
#define TAB_EVENT         NAME(0x0056F8F0)    /* "EventTab" */
#define TAB_BIKE_RIDER    NAME(0x0056F8E0)    /* "BikeRiderTab" */
#define TAB_RACE_INFO     NAME(0x0056F8D4)    /* "RaceInfoTab" */
#define TAB_OPTIONS       NAME(0x00579F2C)    /* "OptionsTab" */
#define OPT_BASE          NAME(0x0056F690)    /* "OptBase.dtm" */
#define MAIN_MENU         NAME(0x0057392C)    /* "mainmenu.dtm" */
#define FULL_PACKETS      NAME(0x00579F0C)    /* "IntervalBetweenFullPacketsMS" */
#define SHORT_PACKETS     NAME(0x00579EEC)    /* "IntervalBetweenShortPacketsMS" */
#define TAB_CONTROL       4

/* Ported elsewhere. */
BYTE*  __fastcall dialog_find_control(BYTE* self, void* edx_unused, const char* name, int type);
BYTE*  __fastcall dialog_construct(BYTE* self, void* edx_unused, DWORD flag, const char* name);
void   __fastcall dialog_end(BYTE* self, void* edx_unused, DWORD code);
void   __fastcall dialog_add_timer(BYTE* self, void* edx_unused, DWORD id, DWORD period,
                                   BYTE* target);
void   __fastcall dialog_remove_timer(BYTE* self, void* edx_unused, DWORD id);
void   __fastcall dialog_notify_controls(BYTE* self, void* edx_unused, DWORD flag);
void   __fastcall dialog_set_controls(BYTE* self, void* edx_unused, DWORD id, DWORD value);
void   __fastcall dialog_enable_controls(BYTE* self, void* edx_unused, DWORD id, DWORD enabled);
int    __fastcall control_set_text(BYTE* self, void* edx_unused, const char* text);
int    __fastcall control_set_colour(BYTE* self, void* edx_unused, COLORREF colour);
int    __fastcall control_set_shadow(BYTE* self, void* edx_unused, BOOL shadow);
int    __fastcall control_set_align(BYTE* self, void* edx_unused, DWORD align);
void   __fastcall control_show(BYTE* self, void* edx_unused, DWORD shown, BOOL animate);
int    __fastcall option_load_text(BYTE* self, void* edx_unused, int index, HINSTANCE module,
                                   UINT id);
BOOL   __fastcall radio_set(BYTE* self, void* edx_unused, int number);
int    __fastcall edit_set_capacity(BYTE* self, void* edx_unused, int length);
int    __fastcall edit_set_caret_colour(BYTE* self, void* edx_unused, COLORREF colour);
int    __fastcall edit_set_text(BYTE* self, void* edx_unused, const char* text);
char*  __fastcall edit_get_text(BYTE* self, void* edx_unused, char* buffer, int size);
void   __fastcall list_set_224(BYTE* self, void* edx_unused, DWORD value);
void   __fastcall list_set_240(BYTE* self, void* edx_unused, DWORD value);
void   __fastcall list_clear(BYTE* self);
BOOL   __fastcall list_add_text(BYTE* self, void* edx_unused, const char* text, DWORD data,
                                DWORD b);
BOOL   __fastcall list_add_file(BYTE* self, void* edx_unused, const char* name, DWORD data,
                                BOOL animated, DWORD b);
void   __fastcall list_sort(BYTE* self, void* edx_unused, BOOL outer);
int    __fastcall list_select_all(BYTE* self, void* edx_unused, int index);
int    __fastcall list_select_text(BYTE* self, void* edx_unused, const char* text);
int    __fastcall list_selected(BYTE* self);
DWORD  __fastcall list_data(BYTE* self, void* edx_unused, int index);
BOOL   __fastcall list_remove(BYTE* self, void* edx_unused, int index);
BOOL   __fastcall list_scroll(BYTE* self, void* edx_unused, int first, BOOL tell);
BYTE*  __fastcall gui_manager_open(gui_manager* self, void* edx_unused, BYTE* dialog, DWORD a,
                                   DWORD flags, DWORD area, BYTE* parent, DWORD arg, DWORD c,
                                   DWORD wait);
void   __fastcall session_go_to(BYTE* self, void* edx_unused, int screen);
void   __fastcall session_quit(BYTE* self);
void   __fastcall session_drop_tracks(BYTE* self);
int    __fastcall session_add_track(BYTE* self, void* edx_unused, DWORD first, DWORD kind,
                                    DWORD id, const char* name, DWORD fourth, DWORD fifth);
void   __fastcall session_pick_names(BYTE* self, void* edx_unused, const char** names, int count);
void   __fastcall session_pick_opponents(BYTE* self, void* edx_unused, int* bikes, int count,
                                         int* riders);
BOOL   __fastcall zone_read_launch(BYTE* self, void* edx_unused, DWORD* found);
BOOL   __fastcall zone_wait_for_slot(BYTE* self);
void   __cdecl    race_start_loading(void);

/* The game's: C library, files, scenes, profile, network. */
int    __cdecl    game_FUN_0053a11f(char* to, const char* format, ...);          /* sprintf */
int    __cdecl    game_FUN_0053b6e0(const char* a, const char* b);               /* _stricmp */
char*  __cdecl    game__strncat(char* to, const char* from, size_t count);
int    __cdecl    game__rand(void);
int    __cdecl    game_FUN_0053aaa0(void (__cdecl* at_exit)(void));              /* atexit */
void   __cdecl    game_FUN_00539a96(void* base, size_t count, size_t size,
                                    int (__cdecl* compare)(const void*, const void*));  /* qsort */
void*  __cdecl    game_FUN_004a45b0(size_t count, size_t size, const char* file, int line);
DWORD  __cdecl    game_FUN_004c41d0(void);                                       /* the time */
void __cdecl net_leave(void);                       /* leave the network game */
void   __stdcall  game_FUN_0053b88e(void* array, size_t size, int count, DWORD construct,
                                    DWORD destruct);
void   __stdcall  game_FUN_0053b7b0(void* array, size_t size, int count, DWORD destruct);
BOOL   __fastcall game_FUN_00526fc0(BYTE* game, void* edx_unused, UINT id, char* text, int size);
#define load_string(id, text) game_FUN_00526fc0(game_main, NULL, id, text, sizeof text)

void*  __fastcall game_FUN_00461c80(void* self, void* edx_unused, void* resources);
void   __fastcall game_FUN_00461cd0(void* self);
#define game_resources (*(void* const*) 0x00578C84)

int __fastcall profile_event(char* self);
char* __fastcall profile_set_event(char* self, void* edx_unused, int kind);
BOOL __fastcall profile_open_help(char* self, void* edx_unused, const char* topic,
                                  const char* parameters);
void __fastcall profile_clear_flags(char* self);
BOOL __fastcall profile_reset_players(char* self);
void   __fastcall game_FUN_004ee910(BYTE* scenes, void* edx_unused, const char* path);
void   __fastcall game_FUN_004eeda0(BYTE* scenes, void* edx_unused, char* title, const char* file,
                                    DWORD variant, const char* kind, DWORD* a, DWORD* b);
BOOL   __fastcall game_FUN_004eea60(BYTE* scenes, void* edx_unused, void* archive,
                                    const char* path, const char* mode, const char* name);
void   __fastcall game_FUN_004eebc0(BYTE* scenes, void* edx_unused, const char* file,
                                    const char* kind, DWORD a);
void   __fastcall game_FUN_004ef120(BYTE* scenes, void* edx_unused, char* title, BYTE* where,
                                    DWORD number);
void __fastcall player_set_names(BYTE* self, void* edx_unused, const char* first,
                                 const char* second, const char* third);
void   __fastcall game_FUN_0045f530(BYTE* events);

void   __fastcall game_FUN_004b0f20(BYTE* net, void* edx_unused, DWORD type, void* data,
                                    DWORD size, DWORD to, DWORD flags);
BOOL   __fastcall game_FUN_004b0e10(BYTE* net, void* edx_unused, DWORD player, char* name);
BYTE*  __fastcall game_FUN_004b0eb0(BYTE* net, void* edx_unused, int* iterator);  /* {id, name} */
void   __fastcall game_FUN_004b09c0(BYTE* net, void* edx_unused, char* address);
int    __fastcall game_FUN_004b0cd0(BYTE* net, void* edx_unused, const char* name);
void   __fastcall game_FUN_004b0c00(BYTE* net, void* edx_unused, DWORD a);
void   __fastcall game_FUN_004b0d20(BYTE* net);
void   __fastcall game_vf_1_4B23D0(BYTE* player);                      /* empty the slot */
int    __fastcall game_vf_24_46A550(void* self, void* edx_unused, DWORD message, DWORD a,
                                    DWORD b, DWORD c, DWORD d);

/* The screen's own, after 0x4F9800. */
void   __fastcall mp_ready_cancel(BYTE* self, void* edx_unused, int warning);
void   __fastcall mp_show_page(BYTE* self, void* edx_unused, int page);
void   __fastcall mp_track_check(BYTE* self);
void   __fastcall mp_event_refresh(BYTE* page);
void   __fastcall mp_event_kind_set(BYTE* page);
void   __fastcall mp_choices_fill(BYTE* page);
void   __fastcall rider_mark_changed(BYTE* page);
void   __fastcall mp_bike_choose(BYTE* page);

typedef BOOL (__fastcall* game_find_file)(void* game, void* edx_unused, const char* path,
                                          char* found);
typedef int  (__fastcall* game_number)(void* game, void* edx_unused, const char* name,
                                       int fallback);
typedef int  (__fastcall* control_press)(void* control, void* edx_unused, DWORD a, DWORD b);

/* The file found for `path` (a mod's, or the game's) into `found`; FALSE when there is none. */
static BOOL file_find(const char* path, char* found)
{
    return VIRTUAL(game_main, 0x48, game_find_file)(game_main, NULL, path, found);
}

static BYTE* dialog_new(size_t size, int line, const char* name, void* vtable)
{
    BYTE* dialog = game_new(size, SGP_CPP, line);

    if (dialog) {
        dialog_construct(dialog, NULL, 1, name);
        *(void**) dialog = vtable;
    }
    return dialog;
}

static void control_enable(BYTE* control, DWORD enabled)
{
    VIRTUAL(control, 0xC4, method_1)(control, NULL, enabled);
}

/* An engine size's class (0 to 4), as the game rounds it; 1 when it falls between. */

/* OpponentsListBox: 0 to opponent_max, the selection kept within it; left alone when it fits. */
static void opponents_offer(BYTE* list)
{
    char text[0x80];
    int selected;
    int i;

    if (!list || FIELD(list, LIST_COUNT, int) - 1 == opponent_max)
        return;
    selected = list_selected(list);
    list_clear(list);
    for (i = 0; i <= opponent_max; i++) {
        game_FUN_0053a11f(text, ONE_NUMBER, i);
        list_add_text(list, NULL, text, i, 0);
    }
    list_select_all(list, NULL, opponent_max < selected ? opponent_max : selected);
}

/* The keyboard back to EditChat. */
static void chat_capture(BYTE* self)
{
    gui_user_queue_capture(FIELD(self, 0x34, gui_user*), NULL,
                           dialog_find_control(self, NULL, EDIT_CHAT, 11), NULL, FALSE);
}

/* The multiplayer settings' own copies (0x3148, 0x1608, 0x207C), like the single player's. */
static void mp_settings_save(void)
{
    __movsd((unsigned long*) (game_main + 0x3148), (unsigned long*) race_settings, 0x7B);
    __movsd((unsigned long*) (game_main + 0x1608), (unsigned long*) (game_main + 0x1550), 0x17);
    __movsd((unsigned long*) (game_main + 0x207C), (unsigned long*) (game_main + 0x1EEC), 0x32);
}

static void mp_settings_restore(void)
{
    __movsd((unsigned long*) race_settings, (unsigned long*) (game_main + 0x3148), 0x7B);
    __movsd((unsigned long*) (game_main + 0x1550), (unsigned long*) (game_main + 0x1608), 0x17);
    __movsd((unsigned long*) (game_main + 0x1EEC), (unsigned long*) (game_main + 0x207C), 0x32);
}

/* ---- The other players' table ---- */

static void __cdecl players_destroy(void)               /* 0x004F6680 */
{
    game_FUN_0053b7b0(mp_players, PLAYER_SIZE, PLAYER_SLOTS, PLAYER_DESTRUCT);
}

PORT(0x004F6650)
void __cdecl players_make(void)
{
    game_FUN_0053b88e(mp_players, PLAYER_SIZE, PLAYER_SLOTS, PLAYER_CONSTRUCT, PLAYER_DESTRUCT);
}

PORT(0x004F6670)
int __cdecl players_destroy_at_exit(void)
{
    return game_FUN_0053aaa0(players_destroy);
}

/* The C runtime's initializer for it. */
PORT(0x004F6640)
int __cdecl players_init(void)
{
    players_make();
    return players_destroy_at_exit();
}

/* ---- Track lists ---- */

#define PIC_NUMBERED_SMALL  NAME(0x00579D50)    /* "%s%02ds.tga" */
#define PIC_NUMBERED        NAME(0x00579D5C)    /* "%s%02d.tga" */
#define PIC_FIRST_SMALL     NAME(0x00579D68)    /* "%s01s.tga" */
#define PIC_FIRST           NAME(0x00579D74)    /* "%s01.tga" */
#define PIC_SMALL           NAME(0x00579D80)    /* "%ss.tga" */
#define PIC                 NAME(0x00579D88)    /* "%s.tga" */
#define UNART_SMALL         NAME(0x00579DC0)    /* "unarts.tga" */
#define UNART               NAME(0x00579DCC)    /* "unart.tga" */

/* A track named in the game's six at 0x1914 (32 characters each) is hidden while its flag is 0. */
static BOOL track_hidden(const char* name)
{
    int i;

    for (i = 0; i < 6; i++) {
        if (!game_FUN_0053b6e0(name, (const char*) game_main + 0x1914 + i * 0x20))
            return !FIELD(game_main, 0x18FC + i * 4, int);
    }
    return FALSE;
}

/* The track's entry in the session's list; in a hosted game with the scn file's two values. */
static int track_add(int count, int variant, const char* name, DWORD a, DWORD b)
{
    BOOL shared = game_net && net_hosting;

    return session_add_track(game_session, NULL, count, variant,
                             profile_event(game_profile), name, shared ? a : 0,
                             shared ? b : 0);
}

/*
    The track's picture (from the resource archive, else the file found for it, else unart) and
    its title, "no name" read as string 0x143B.
*/
static void track_show(BYTE* pictures, BYTE* names, void* archive, const char* folder,
                       const char* picture, int entry, char* title, const char* fallback)
{
    char path[0x104];
    char found[0x104];

    game_FUN_0053a11f(path, PATH, folder, picture);
    if (game_FUN_004eea60(game_scenes, NULL, archive, path, READ_BINARY, picture))
        list_add_file(pictures, NULL, picture, entry, TRUE, 0);
    else
        list_add_file(pictures, NULL, file_find(path, found) ? found : fallback, entry, FALSE, 0);
    if (!game_FUN_0053b6e0(NO_NAME, title))
        game_FUN_00526fc0(game_main, NULL, 0x143B, title, 0x80);
    list_add_text(names, NULL, title, entry, 0);
}

/*
    The env files of `pattern` in `folder` (the game's own, then the shared one) listed: their
    pictures in `picture_name` (big or small), their titles in `list_name`'s combo, each added to
    the session's tracks. The track named in `track` is selected (its index into `chosen`), and
    `track` gets the chosen one's file. `keep` adds to what the lists hold. Kept from the
    original: an empty `list_name` leaves the names list NULL and still used, and nothing
    selected reads the track before the first.
*/
PORT(0x004F66A0)
BOOL __cdecl tracks_list_fill(merged_list* files, const char* folder, const char* pattern,
                              const char* kind, BOOL big, const char* picture_name,
                              const char* list_name, char* track, int* chosen, BYTE* dialog,
                              DWORD number, BOOL keep)
{
    char title[0x80];       /* the original's is 0x40, but string 0x143B is read into it as 0x80 */
    char name[0x104];
    char picture[0x104];
    char env_path[0x104];
    char fallback[0x104];
    char wanted[0x104];
    char own[0x104];
    char shared[0x104];
    BYTE* pictures;
    BYTE* names = NULL;
    DWORD second;
    DWORD third;
    void* archive;
    BOOL found;
    int count;
    int index;

    if (!dialog)
        dialog = gui_manager_active_dialog(session_gui);
    pictures = dialog_find_control(dialog, NULL, picture_name, 0);
    list_set_240(pictures, NULL, 1);
    if (!keep)
        list_clear(pictures);
    if (*list_name) {
        names = FIELD(dialog_find_control(dialog, NULL, list_name, 0), COMBO_LIST, BYTE*);
        if (!keep)
            list_clear(names);
    }
    if (!keep)
        session_drop_tracks(game_session);

    text_copy_most(wanted, track, 0x103);
    track[0] = 0;
    if (chosen)
        *chosen = 0;

    game_FUN_0053a11f(own, PATH, (const char*) game_main + 0x2948, folder);
    game_FUN_0053a11f(shared, PATH, (const char*) game_main + 0x2A4C, folder);
    merged_list_set_folders(files, NULL, own, shared);
    dir_list_set_pattern(&files->base, NULL, pattern, 1);
    VIRTUAL(files, 4, method_0)(files);
    if (!dir_list_count(&files->base))
        return TRUE;

    game_FUN_0053a11f(picture, PATH, UI_FOLDER, big ? UNART : UNART_SMALL);
    file_find(picture, fallback);
    found = dir_list_current_title(&files->base, NULL, name);
    archive = game_new(0x134, SGP_CPP, 0xB4);
    if (archive)
        archive = game_FUN_00461c80(archive, NULL, game_resources);

    for (count = 0; found; count++, found = dir_list_next_title(&files->base, NULL, name)) {
        int entry;

        if (track_hidden(name))
            continue;
        profile_find_named_file(game_profile, NULL, folder, name, kind, env_path);
        game_FUN_004ee910(game_scenes, NULL, env_path);
        if (race_kind != 1 && race_kind != 5) {
            DWORD mode;

            game_FUN_004eeda0(game_scenes, NULL, title, name, 0, SCN, &second, &third);
            entry = track_add(count, 0, name, second, third);
            mode = profile_event(game_profile);
            if (mode == 1 || mode == 5)
                game_FUN_0053a11f(picture, big ? PIC_FIRST : PIC_FIRST_SMALL, name);
            else
                game_FUN_0053a11f(picture, big ? PIC : PIC_SMALL, name);
            track_show(pictures, names, archive, folder, picture, entry, title, fallback);
        } else {
            int variants;
            int i;

            game_FUN_004eeda0(game_scenes, NULL, title, name, 1, SCN, NULL, NULL);
            variants = FIELD(game_scenes, 0x390, int);
            for (i = 1; i <= variants; i++) {
                game_FUN_004eeda0(game_scenes, NULL, title, name, i, SCN, &second, &third);
                entry = track_add(count, i, name, second, third);
                game_FUN_0053a11f(picture, big ? PIC_NUMBERED : PIC_NUMBERED_SMALL, name, i);
                track_show(pictures, names, archive, folder, picture, entry, title, fallback);
            }
        }
    }

    list_sort(names, NULL, TRUE);
    if (!keep) {
        list_select_all(names, NULL, 0);
        if (chosen)
            *chosen = 0;
    }
    if (*wanted) {
        game_FUN_004eebc0(game_scenes, NULL, wanted, SCN, 0);
        if (FIELD(game_scenes, 0x38C, DWORD)) {
            game_FUN_004ef120(game_scenes, NULL, title, game_scenes + 0x24C, number);
            list_select_text(names, NULL, title);
            index = list_selected(names);
            if (chosen)
                *chosen = index;
        }
    }
    index = (int) list_data(names, NULL, -1);
    VIRTUAL(files, 4, method_0)(files);
    dir_list_title(&files->base, NULL, FIELD(session_tracks + index * TRACK_SIZE, 0, DWORD), track);
    if (archive) {
        game_FUN_00461cd0(archive);
        game_delete(archive);
    }
    return TRUE;
}

#define event_files       FIELD(game_main, 0x2B5C, merged_list*)
#define CUSTOM_FOLDER     FIELD(game_main, 0xC18, const char*)
#define BAJA_FOLDER       NAME(0x0056CB6C)    /* "Teraform\Baja" */
#define NATIONAL_FOLDER   NAME(0x0056F60C)    /* "Teraform\National" */
#define SX_FOLDER         NAME(0x0056F63C)    /* "Teraform\SX" */
#define ENDURO_FOLDER     NAME(0x0056CB5C)    /* "Teraform\Enduro" */

/* The race kind's tracks; quarries always with big pictures. */
PORT(0x004F7F20)
void __cdecl tracks_fill(BOOL big, const char* picture_name, const char* list_name, int* chosen,
                         BYTE* dialog, BOOL keep)
{
    char track[0x80];

    if (!dialog)
        dialog = gui_manager_active_dialog(session_gui);
    track[0] = 0;
    switch (race_kind) {
    case 0:
        tracks_list_fill(event_files, CUSTOM_FOLDER, ENV_FILES, ENV, TRUE, picture_name,
                         list_name, track, chosen, dialog, 0, keep);
        break;
    case 1:
        tracks_list_fill(event_files, BAJA_FOLDER, ENV_FILES, ENV, big, picture_name, list_name,
                         track, chosen, dialog, FIELD(game_main, 0xF70, DWORD), FALSE);
        break;
    case 2:
        tracks_list_fill(event_files, NATIONAL_FOLDER, ENV_FILES, ENV, big, picture_name,
                         list_name, track, chosen, dialog, 0, FALSE);
        break;
    case 3:
        tracks_list_fill(event_files, SX_FOLDER, ENV_FILES, ENV, big, picture_name, list_name,
                         track, chosen, dialog, 0, FALSE);
        break;
    case 4:
        tracks_list_fill(event_files, CUSTOM_FOLDER, ENV_FILES, ENV, big, picture_name,
                         list_name, track, chosen, dialog, 0, keep);
        break;
    case 5:
        tracks_list_fill(event_files, ENDURO_FOLDER, ENV_FILES, ENV, big, picture_name,
                         list_name, track, chosen, dialog, FIELD(game_main, 0xF80, DWORD), FALSE);
        break;
    }
}

/* ---- Teams ---- */

PORT(0x004F7DA0)
mp_team* __fastcall teams_clear(mp_team* self)
{
    int i;

    for (i = 0; i < MP_TEAMS; i++) {
        self[i].player = 0;
        self[i].wanted = 0;
        self[i].given = 0;
    }
    return self;
}

/* The player's team wants `wanted` riders (-1 leaves it); a player without one takes a free one. */
PORT(0x004F7DC0)
void __fastcall teams_set(mp_team* self, void* edx_unused, int player, int wanted)
{
    int i;

    (void) edx_unused;
    for (i = 0; i < MP_TEAMS; i++) {
        if (self[i].player == player) {
            if (wanted != -1)
                self[i].wanted = wanted;
            return;
        }
    }
    for (i = 0; i < MP_TEAMS; i++) {
        if (!self[i].player) {
            self[i].player = player;
            if (wanted != -1)
                self[i].wanted = wanted;
            return;
        }
    }
}

PORT(0x004F7E20)
void __fastcall teams_remove(mp_team* self, void* edx_unused, int player)
{
    int i;

    (void) edx_unused;
    for (i = 0; i < MP_TEAMS; i++) {
        if (self[i].player == player) {
            self[i].player = 0;
            self[i].wanted = 0;
            self[i].given = 0;
            return;
        }
    }
}

PORT(0x004F7E60)
int __cdecl teams_by_wanted(const mp_team* a, const mp_team* b)
{
    if (a->wanted < b->wanted)
        return -1;
    return a->wanted != b->wanted;
}

/*
    Up to `riders` computer riders handed out one a round to the first `humans` teams (sorted
    by what they want) while they want more; the most there can be (all wanted, within the 8
    riders a race has) is returned.
*/
PORT(0x004F7E80)
int __fastcall teams_share(mp_team* self, void* edx_unused, int humans, int riders)
{
    int most = 0;
    int given = 0;
    BOOL gave;
    int i;

    (void) edx_unused;
    if (humans >= MP_TEAMS)
        return 0;
    game_FUN_00539a96(self, humans, sizeof *self,
                      (int (__cdecl*)(const void*, const void*)) teams_by_wanted);
    for (i = 0; i < MP_TEAMS; i++) {
        self[i].given = 0;
        most += self[i].wanted;
    }
    if (most + humans >= MP_TEAMS)
        most = MP_TEAMS - humans;
    if (!riders)
        return most;
    do {
        gave = FALSE;
        for (i = 0; i < humans; i++) {
            if (self[i].wanted > self[i].given) {
                self[i].given++;
                gave = TRUE;
                if (++given >= riders)
                    return most;
            }
        }
    } while (gave);
    return most;
}

/* ---- Starting grid ---- */

typedef struct grid_slot {      /* the race settings' grid at 0x2EBC */
    int  player;
    BYTE key;
    BYTE index;
    BYTE unset[2];              /* the original stores two bytes of its stack there */
} grid_slot;

typedef struct sort_entry {
    int   player;
    float key;
    BYTE  index;
    BYTE  unset[3];
} sort_entry;

#ifdef OPENMCM2_DEV
#define race_grid ((grid_slot*) start_grid())
#else
#define race_grid ((grid_slot*) (game_main + 0x2EBC))
#endif

PORT(0x004F6F20)
int __cdecl grid_by_key(const grid_slot* a, const grid_slot* b)
{
    if (a->key > b->key)
        return -1;
    return a->key != b->key;
}

PORT(0x004F6F40)
int __cdecl order_by_key(const sort_entry* a, const sort_entry* b)
{
    if (a->key > b->key)
        return -1;
    return a->key < b->key;     /* equal and unordered give 0 */
}

/* ---- The race's riders ---- */

/*
    The riders filled from the players (names from the table, the user's from the profile, its
    looks and skill from the game) and the teams; with the menus up, the race starts.
*/
PORT(0x004F6F70)
void __fastcall mp_race_start(BYTE* self)
{
    int me = net_me;
    mp_team* team = mp_teams(self);
    int next;
    int i;
    int j;

    humans_count = race_humans;
    FIELD(game_main, 0x3424, int) = race_computers;
    riders_count = FIELD(game_main, 0x3424, int) + humans_count;
    for (i = 0; i < humans_count - 1; i++) {
        BYTE* rider = entrant(i + 1);

        FIELD(rider, ENTRANT_PLAYER, int) = player_id(i);
        FIELD(rider, ENTRANT_INDEX, int) = 0;
        text_copy_most((char*) rider + ENTRANT_NAME, player_name(i), 15);
    }
    FIELD(entrant(0), ENTRANT_PLAYER, int) = me;
    text_copy_most((char*) entrant(0) + ENTRANT_NAME, game_profile, 15);
    FIELD(entrant(0), ENTRANT_INDEX, int) = 0;
    player_set_names(entrant(0), NULL, (char*) game_main + 0x1EEC, (char*) game_main + 0x1F2C,
                      (char*) game_main + 0x1F6C);
    FIELD(entrant(0), ENTRANT_C4, DWORD) = FIELD(game_main, 0x1FB0, DWORD);
    if (skill_wanted > 0 && skill_wanted < 101)
        race_skill = skill_wanted;
    FIELD(entrant(0), ENTRANT_SKILL, int) = race_skill;

    next = mp_player_count + 1;
    for (i = 0; i < MP_TEAMS; i++) {
        for (j = 0; j < team[i].given; j++, next++) {
            FIELD(entrant(next), ENTRANT_PLAYER, int) = team[i].player;
            FIELD(entrant(next), ENTRANT_INDEX, int) = next;
        }
        if (team[i].player == me)
            FIELD(race_settings, 0x24, int) = team[i].given;     /* the user's opponents */
    }
    game_FUN_004b0c00(game_net, NULL, 0);
    if (session_gui) {
        if (FIELD(game_main, 0x3334, int)) {
            if (race_kind == 4) {
                FIELD(game_main, 0x3334, int) = 0;
            } else {
                FIELD(game_main, 0x342C, int) = 1;
                FIELD(game_main, 0x3428, int) = 0;
            }
        }
        race_start_loading();
        VIRTUAL(self, 0x68, method_0)(self);
    }
}

#define RAND_SCALE (1.0f / 32768)

/*
    The computer riders picked for the teams (bike, rider, name and a skill of 100 to 998), and
    the grid ordered: a series by its points, else at random. Kept from the original: the bike
    and rider picks are not freed.
*/
static void computers_pick(BYTE* self, int count)
{
    int* bikes = game_FUN_004a45b0(count, sizeof(int), SGP_CPP, 0x1E9);
    int* riders = game_FUN_004a45b0(count, sizeof(int), SGP_CPP, 0x1EA);
    const char** names = game_FUN_004a4410(count * sizeof *names, SGP_CPP, 0x1EB);
    mp_team* team = mp_teams(self);
    int total;
    int i;
    int j;

    session_pick_opponents(game_session, NULL, bikes, count, riders);
    session_pick_names(game_session, NULL, names, count);
    for (i = 0; i < humans_count - 1; i++) {
        FIELD(entrant(i + 1), ENTRANT_PLAYER, int) = player_id(i);
        FIELD(entrant(i + 1), ENTRANT_INDEX, int) = 0;
    }
    FIELD(entrant(0), ENTRANT_PLAYER, int) = net_me;
    FIELD(entrant(0), ENTRANT_INDEX, int) = 0;

    total = mp_player_count + 1;
    for (i = 0; i < MP_TEAMS; i++) {
        for (j = 0; j < team[i].given; j++, total++) {
            BYTE* rider = entrant(total);
            BYTE* bike = session_bikes + bikes[total] * BIKE_SIZE;
            BYTE* maker = session_players + FIELD(bike, BIKE_MAKER, int) * MAKER_SIZE;

            FIELD(rider, ENTRANT_PLAYER, int) = team[i].player;
            FIELD(rider, ENTRANT_INDEX, int) = total;
            text_copy_most((char*) rider + ENTRANT_NAME, names[total], 15);
            FIELD(rider, ENTRANT_SKILL, int) =
                100 - (int) (game__rand() * (double) RAND_SCALE * -899.0f);
            text_copy_most((char*) rider + ENTRANT_MAKER, (const char*) maker + MAKER_MODEL, 63);
            text_copy_most((char*) rider + ENTRANT_BIKE, (const char*) bike + BIKE_LOOK, 63);
            text_copy_most((char*) rider + ENTRANT_LOOK,
                           (const char*) session_riders
                               + riders[total] * RIDER_SIZE + RIDER_LOOK, 63);
            FIELD(rider, ENTRANT_ENGINE, int) = FIELD(bike, BIKE_ENGINE, int);
            FIELD(rider, ENTRANT_FOUR_STROKE, int) = FIELD(bike, BIKE_FOUR_STROKE, int);
        }
    }

    profile_clear_flags(game_profile);
    if (race_mode == SERIES && FIELD(race_events, 0x48, BYTE)) {
        for (i = 0; i < total; i++) {
            race_grid[i].player = FIELD(entrant(i), ENTRANT_PLAYER, int);
            race_grid[i].key = FIELD(race_events, 0x78 + i * 0x50, BYTE);    /* the points */
            race_grid[i].index = FIELD(entrant(i), ENTRANT_INDEX, BYTE);
        }
        game_FUN_00539a96(race_grid, total, sizeof(grid_slot),
                          (int (__cdecl*)(const void*, const void*)) grid_by_key);
    } else {
#ifdef OPENMCM2_DEV
        sort_entry order[MP_RIDERS_MOST];
#else
        sort_entry order[10];       /* the room the original has */
#endif

        for (i = 0; i < total; i++) {
            order[i].key = (float) (game__rand() * (double) RAND_SCALE * 10.0f);
            order[i].player = FIELD(entrant(i), ENTRANT_PLAYER, int);
            order[i].index = FIELD(entrant(i), ENTRANT_INDEX, BYTE);
        }
        game_FUN_00539a96(order, total, sizeof(sort_entry),
                          (int (__cdecl*)(const void*, const void*)) order_by_key);
        for (i = 0; i < total; i++) {
            race_grid[i].player = order[i].player;
            race_grid[i].key = 0;
            race_grid[i].index = order[i].index;
        }
    }
    game_FUN_004a4450(names, SGP_CPP, 0x233);
}

/*
    The race's set-up (message 0x83): the settings, 0x60C and the teams over the settings' end;
    `full` adds the riders (skills, names, bike looks, engines) and starts the race.
*/
#define MSG_SETUP           0x83
#define SETUP_FULL          0x004   /* WORD */
#define SETUP_SETTINGS      0x008
#define SETUP_TEAMS         0x194
#define SETUP_60C           0x1F4
#define SETUP_SKILLS        0x1F8   /* int[8] */
#define SETUP_NAMES         0x218   /* char[8][16] */
#define SETUP_MAKERS        0x298   /* char[8][0x40] */
#define SETUP_BIKES         0x498
#define SETUP_LOOKS         0x698
#define SETUP_ENGINES       0x898   /* int[8] */
#define SETUP_FOUR_STROKES  0x8B8
#define SETUP_SIZE          0x8D8
#define SETUP_TEAM_DWORDS   0x18    /* the teams it has room for: 8 */

#ifdef OPENMCM2_DEV
/*
    The dev build's set-up, for races of up to 30 riders: the settings, the teams and the start
    grid, and each computer rider by its place in the race. It is too big for one of the game's
    messages (0x8DD bytes at most), so it goes in parts (message 0x93, guaranteed as 0x83 is),
    which the players gather, in whatever order they come, before taking it.
*/
#define MSG_SETUP_PART      0x93
#define SETUP_PART_DATA     0x800

typedef struct setup_rider {
    int  skill;
    char name[16];
    char maker[0x40], bike[0x40], look[0x40];
    int  engine, four_stroke;
} setup_rider;

typedef struct setup_whole {
    DWORD       settings[0x7B];
    DWORD       value_60c;
    mp_team     teams[MP_TEAMS];
    grid_slot   grid[MP_RIDERS_MOST];
    setup_rider riders[MP_RIDERS_MOST];     /* the computers', by their place; the players' unused */
} setup_whole;

typedef struct setup_part {
    DWORD header;
    WORD  full;
    WORD  setup;                            /* the host's count of set-ups, to tell them apart */
    BYTE  part, parts;
    WORD  size;
    BYTE  data[SETUP_PART_DATA];
} setup_part;

static setup_whole g_setup_out, g_setup_in;
static WORD g_setup_sent, g_setup_taking;
static BYTE g_setup_got[sizeof(setup_whole) / SETUP_PART_DATA + 1];
static BOOL g_setup_taken;

static void setup_whole_send(BYTE* self, WORD full, int humans, int count)
{
    setup_whole* whole = &g_setup_out;
    setup_part part;
    int size, parts, i;

    count = min(count, MP_RIDERS_MOST);
    __stosd((unsigned long*) whole, 0, sizeof *whole / 4);
    __movsd(whole->settings, (unsigned long*) race_settings, 0x7B);
    whole->value_60c = FIELD(game_main, 0x60C, DWORD);
    __movsd((unsigned long*) whole->teams, (unsigned long*) mp_teams(self), TEAMS_DWORDS);
    __movsd((unsigned long*) whole->grid, (unsigned long*) race_grid, sizeof whole->grid / 4);
    for (i = humans; full && i < count; i++) {
        setup_rider* rider = &whole->riders[i];
        BYTE* from = entrant(i);

        rider->skill = FIELD(from, ENTRANT_SKILL, int);
        text_copy_most(rider->name, (const char*) from + ENTRANT_NAME, 15);
        text_copy_most(rider->maker, (const char*) from + ENTRANT_MAKER, 63);
        text_copy_most(rider->bike, (const char*) from + ENTRANT_BIKE, 63);
        text_copy_most(rider->look, (const char*) from + ENTRANT_LOOK, 63);
        rider->engine = FIELD(from, ENTRANT_ENGINE, int);
        rider->four_stroke = FIELD(from, ENTRANT_FOUR_STROKE, int);
    }

    size = (int) ((BYTE*) &whole->riders[full ? count : 0] - (BYTE*) whole);
    parts = (size + SETUP_PART_DATA - 1) / SETUP_PART_DATA;
    part.full = full;
    part.setup = ++g_setup_sent;
    part.parts = (BYTE) parts;
    for (i = 0; i < parts; i++) {
        int at = i * SETUP_PART_DATA;

        part.part = (BYTE) i;
        part.size = (WORD) min(size - at, SETUP_PART_DATA);
        __movsb(part.data, (const BYTE*) whole + at, part.size);
        game_FUN_004b0f20(game_net, NULL, MSG_SETUP_PART, &part,
                          (DWORD) (part.data - (BYTE*) &part) + part.size, net_me, 0);
    }
}

static void setup_whole_take(BYTE* self, const setup_whole* whole, WORD full)
{
    int count, i;

    __movsd((unsigned long*) race_settings, whole->settings, 0x7B);
    FIELD(game_main, 0x60C, DWORD) = whole->value_60c;
    __movsd((unsigned long*) race_grid, (const unsigned long*) whole->grid, sizeof whole->grid / 4);
    if (!full) {
        mp_track_check(self);
        return;
    }
    count = min(race_computers + race_humans, MP_RIDERS_MOST);
    for (i = race_humans; i < count; i++) {
        const setup_rider* rider = &whole->riders[i];
        BYTE* to = entrant(i);

        FIELD(to, ENTRANT_SKILL, int) = rider->skill;
        text_copy_most((char*) to + ENTRANT_NAME, rider->name, 15);
        FIELD(to, ENTRANT_INDEX, int) = i;
        text_copy_most((char*) to + ENTRANT_MAKER, rider->maker, 63);
        text_copy_most((char*) to + ENTRANT_BIKE, rider->bike, 63);
        text_copy_most((char*) to + ENTRANT_LOOK, rider->look, 63);
        FIELD(to, ENTRANT_ENGINE, int) = rider->engine;
        FIELD(to, ENTRANT_FOUR_STROKE, int) = rider->four_stroke;
    }
    __movsd((unsigned long*) mp_teams(self), (const unsigned long*) whole->teams, TEAMS_DWORDS);
    mp_race_start(self);
}

/* A part of the host's set-up; the set-up is taken once all its parts are in, and once only. */
static void setup_part_take(BYTE* self, const setup_part* part)
{
    int i;

    if (part->parts == 0 || part->parts > sizeof g_setup_got || part->part >= part->parts
        || part->size > SETUP_PART_DATA || part->part * SETUP_PART_DATA + part->size > sizeof g_setup_in)
        return;
    if (part->setup != g_setup_taking) {
        g_setup_taking = part->setup;
        g_setup_taken = FALSE;
        __stosb(g_setup_got, 0, sizeof g_setup_got);
        __stosd((unsigned long*) &g_setup_in, 0, sizeof g_setup_in / 4);
    }
    if (g_setup_taken)
        return;
    __movsb((BYTE*) &g_setup_in + part->part * SETUP_PART_DATA, part->data, part->size);
    g_setup_got[part->part] = 1;
    for (i = 0; i < part->parts; i++)
        if (!g_setup_got[i])
            return;
    g_setup_taken = TRUE;
    setup_whole_take(self, &g_setup_in, part->full);
}
#endif

/* The game's set-up message, with room for 8 riders. */
static void setup_send(BYTE* self, WORD full, int count)
{
    BYTE setup[SETUP_SIZE];
    int i;

    FIELD(setup, SETUP_FULL, WORD) = full;
    __movsd((unsigned long*) (setup + SETUP_SETTINGS), (unsigned long*) race_settings, 0x7B);
    FIELD(setup, SETUP_60C, DWORD) = FIELD(game_main, 0x60C, DWORD);
    __movsd((unsigned long*) (setup + SETUP_TEAMS), (unsigned long*) mp_teams(self), SETUP_TEAM_DWORDS);
    if (full) {
        int humans = mp_player_count + 1;
        char name[0x80];

        FIELD(setup, SETUP_SKILLS, int) = race_skill;
        game_FUN_004b0e10(game_net, NULL, net_me, name);
        text_copy((char*) setup + SETUP_NAMES, name);
        for (i = 1; i < humans; i++) {
            FIELD(setup, SETUP_SKILLS + i * 4, int) = FIELD(entrant(i), ENTRANT_SKILL, int);
            text_copy((char*) setup + SETUP_NAMES + i * 16,
                      (const char*) entrant(i) + ENTRANT_NAME);
        }
        for (i = humans; i < count; i++) {
            BYTE* rider = entrant(i);

            FIELD(setup, SETUP_SKILLS + i * 4, int) = FIELD(rider, ENTRANT_SKILL, int);
            text_copy((char*) setup + SETUP_NAMES + i * 16, (const char*) rider + ENTRANT_NAME);
            text_copy((char*) setup + SETUP_MAKERS + i * 0x40,
                      (const char*) rider + ENTRANT_MAKER);
            text_copy((char*) setup + SETUP_BIKES + i * 0x40, (const char*) rider + ENTRANT_BIKE);
            text_copy((char*) setup + SETUP_LOOKS + i * 0x40, (const char*) rider + ENTRANT_LOOK);
            FIELD(setup, SETUP_ENGINES + i * 4, int) = FIELD(rider, ENTRANT_ENGINE, int);
            FIELD(setup, SETUP_FOUR_STROKES + i * 4, int) =
                FIELD(rider, ENTRANT_FOUR_STROKE, int);
        }
    }
    game_FUN_004b0f20(game_net, NULL, MSG_SETUP, setup, SETUP_SIZE, net_me, 0);
}

PORT(0x004F71E0)
void __fastcall mp_setup_send(BYTE* self, void* edx_unused, WORD full)
{
    int count;

    (void) edx_unused;
    race_humans = (BYTE) (mp_player_count + 1);
    humans_count = race_humans;
    FIELD(game_main, 0x3424, int) = race_computers;
    riders_count = FIELD(game_main, 0x3424, int) + humans_count;
    count = riders_count;
    opponent_max = teams_share(mp_teams(self), NULL, mp_player_count + 1, race_computers);
    if (opponent_max < race_computers)
        race_computers = opponent_max;
    if (full)
        computers_pick(self, count);
#ifdef OPENMCM2_DEV
    setup_whole_send(self, full, mp_player_count + 1, count);
#else
    setup_send(self, full, count);
#endif
    if (full)
        mp_race_start(self);
}

/*
    Message 0xF: the settings the screen shows, packed into bits, with the track and teams. The
    players and the computer riders have 4 bits each, 15 at most: the dev build's 30 go in full
    in the two bytes the original leaves unset, and its teams are one a player.
*/
#define MSG_SETTINGS 0xF

typedef struct settings_message {
    DWORD   header;
    BYTE    packed[5];
    BYTE    variant;
    BYTE    humans, computers;      /* the dev build's; the original leaves them unset */
    DWORD   settings_138;
    DWORD   settings_13c;
    char    track[0x40];
    mp_team teams[MP_TEAMS];
} settings_message;

#ifndef OPENMCM2_DEV
_Static_assert(sizeof(settings_message) == 0xB4, "the settings message is 0xB4 bytes");
#endif

#define setting_byte(offset) FIELD(race_settings, offset, BYTE)

PORT(0x004F7A30)
void __fastcall mp_settings_send(BYTE* self)
{
    settings_message message;

    opponent_max = teams_share(mp_teams(self), NULL, mp_player_count + 1, race_computers);
    if (opponent_max < race_computers)
        race_computers = opponent_max;
    if (FIELD(self, MP_RACE_PAGE, BYTE*))
        opponents_offer(dialog_find_control(FIELD(self, MP_RACE_PAGE, BYTE*), NULL, OPPONENTS, 0));
    if (++settings_send_wait < 2)
        return;

    race_humans = (BYTE) (mp_player_count + 1);
    humans_count = race_humans;
    FIELD(game_main, 0x3424, int) = race_computers;
    riders_count = FIELD(game_main, 0x3424, int) + humans_count;
    if (FIELD(self, 0x7F58, BYTE*))
        dialog_notify_controls(FIELD(self, 0x7F58, BYTE*), NULL, 1);
    if (FIELD(self, 0x7F64, BYTE*))
        dialog_notify_controls(FIELD(self, 0x7F64, BYTE*), NULL, 1);

    message.packed[0] = (BYTE) (setting_byte(0x04) << 5
                                | ((int) FIELD(race_settings, 0x140, float) & 0x1F));
    message.packed[1] = (BYTE) (setting_byte(0x20) << 3 | (setting_byte(0x00) & 7));
    message.packed[2] = (BYTE) (race_humans << 4 | (setting_byte(0x28) & 0xF));
    message.packed[3] = (BYTE) (setting_byte(0x14) << 7 | (setting_byte(0x10) & 1) << 6
                                | (setting_byte(0x08) & 7) << 3 | (setting_byte(0x0C) & 7));
    message.packed[4] = (BYTE) (FIELD(game_main, 0x60C, BYTE) << 6
                                | (setting_byte(0x148) & 1) << 5 | (setting_byte(0x144) & 1) << 4
                                | (setting_byte(0x1C) & 7) << 1 | (setting_byte(0x18) & 1));
    message.settings_138 = FIELD(race_settings, 0x138, DWORD);
    message.settings_13c = FIELD(race_settings, 0x13C, DWORD);
    message.variant = setting_byte(0x34);
#ifdef OPENMCM2_DEV
    message.humans = race_humans;
    message.computers = setting_byte(0x28);
#endif
    __movsd((unsigned long*) message.teams, (unsigned long*) mp_teams(self), TEAMS_DWORDS);
    message.track[0] = 0;
    game__strncat(message.track, race_track, 0x3F);
    game_FUN_004b0f20(game_net, NULL, MSG_SETTINGS, &message, sizeof message, net_me, 0);
    settings_send_wait = 0;
}

/* ---- Controls ---- */

/* ButRdyUser greyed; whether the network took the profile's name. */
PORT(0x004F7D60)
BOOL __fastcall mp_user_reset(BYTE* self)
{
    control_set_colour(dialog_find_control(self, NULL, BUT_RDY_USER, 1), NULL, 0x808080);
    return game_FUN_004b0cd0(game_net, NULL, game_profile) != 0;
}

#define BUT_WRENCH        NAME(0x0056FB84)    /* "ButWrench" */
#define LIST_BIKES        NAME(0x0056FBCC)    /* "DDLBikes" */
#define BIKE_LEFT         NAME(0x0056FBB4)    /* "BikeLeft" */
#define BIKE_RIGHT        NAME(0x0056FBA8)    /* "BikeRight" */
#define LIST_ENGINE_SIZE  NAME(0x0056FC38)    /* "DDLEngineSize" */

/*
    The host may set the race (0x514 while there is no track yet, 0x3E8); a guest may not, and
    is warned when its bike is above the race's class. A ready user can change nothing.
*/
PORT(0x004F8150)
void __fastcall mp_controls_update(BYTE* self)
{
    BYTE* page;
    DWORD free;

    if (net_hosting) {
        if (FIELD(self, MP_RACE_PAGE, BYTE*))
            mp_event_refresh(FIELD(self, MP_RACE_PAGE, BYTE*));
        if (!race_track[0])
            dialog_set_controls(self, NULL, 0x514, 1);
        dialog_set_controls(self, NULL, 0x3E8, 1);
    } else {
        if (engine_class(FIELD(game_main, 0x1550, int)) > FIELD(race_settings, 0x1C, int)
            && mp_user_ready)
            mp_ready_cancel(self, NULL, 1);
        dialog_set_controls(self, NULL, 0x514, 0);
        dialog_set_controls(self, NULL, 0x3E8, 0);
    }

    free = !mp_user_ready;
    control_enable(dialog_find_control(self, NULL, OPTIONS, 1), free);
    control_enable(dialog_find_control(self, NULL, NAME(0x0056F8B0), 1), free);   /* "Joystick" */
    control_enable(dialog_find_control(self, NULL, HELP, 1), free);
    page = FIELD(self, MP_BIKE_PAGE, BYTE*);
    if (!page)
        return;
    control_enable(dialog_find_control(page, NULL, BUT_WRENCH, 0), free);
    control_enable(dialog_find_control(page, NULL, LIST_BIKES, 0), free);
    control_show(dialog_find_control(page, NULL, BIKE_LEFT, 0), NULL, free, TRUE);
    control_show(dialog_find_control(page, NULL, BIKE_RIGHT, 0), NULL, free, TRUE);
    control_enable(dialog_find_control(page, NULL, LIST_ENGINE_SIZE, 0), free);
    control_show(FIELD(dialog_find_control(page, NULL, LIST_ENGINE_SIZE, 0), COMBO_BUTTON, BYTE*),
                 NULL, free, TRUE);
    if (net_hosting) {
        control_enable(dialog_find_control(page, NULL, LARGEST_OPPONENT, 0), free);
        control_show(FIELD(dialog_find_control(page, NULL, LARGEST_OPPONENT, 0), COMBO_BUTTON,
                           BYTE*), NULL, free, TRUE);
    }
}

/*
    The other players' rows the screen's layout has (NamePlayer2 on): the original's 7, the dev
    build's own layout 29. Each row is the player in that slot.
*/
static int mp_rows(BYTE* self)
{
#ifdef OPENMCM2_DEV
    static BYTE* counted;
    static int rows;
    char name[24];

    if (self != counted) {
        counted = self;
        for (rows = 0; rows < PLAYER_SLOTS; rows++) {
            game_FUN_0053a11f(name, NAME_PLAYER, rows + 2);
            if (!dialog_find_control(self, NULL, name, 0))
                break;
        }
    }
    return rows;
#else
    (void) self;
    return PLAYER_SLOTS;
#endif
}

/* A player's colour in the chat, as "~%1d" takes it: 2 to 8 by its slot. */
static int chat_colour(int slot)
{
    return 2 + slot % 7;
}

/* Start for the host only; the ready buttons' words, the players' (0x925) when hosting. */
PORT(0x004F8510)
void __fastcall mp_buttons_update(BYTE* self)
{
    char text[0x80];
    char name[0x78];
    int i;

    if (net_hosting) {
        dialog_enable_controls(self, NULL, 0x514, 1);
        control_enable(dialog_find_control(self, NULL, START, 0), 1);
    } else {
        dialog_enable_controls(self, NULL, 0x514, 0);
    }
    mp_controls_update(self);
    load_string(0x610, text);
    control_set_text(dialog_find_control(self, NULL, BUT_RDY_USER, 1), NULL, text);
    if (net_hosting)
        load_string(0x925, text);
    for (i = 0; i < mp_rows(self); i++) {
        game_FUN_0053a11f(name, BUT_RDY_PLAYER, i + 2);
        control_set_text(dialog_find_control(self, NULL, name, 1), NULL, text);
    }
}

/* ---- Chat ---- */

#define MSG_CHAT    0x85
#define MSG_NOTICE  0x92
#define MSG_KICK    0x8E
#define CHAT_SIZE   0x4E

typedef struct chat_message {
    DWORD header;
    char  text[0x4A];
} chat_message;

/*
    A line in ListChat: `player`'s (-1 for the game's notices, string 0x14BA), coloured by who
    said it unless the game's 0x538 is set; past 100 lines the first goes. Kept from the
    original: the line has 0x60 bytes, which a long name and text can overflow.
*/
PORT(0x004F8610)
void __fastcall chat_show(BYTE* self, void* edx_unused, int player, const char* text)
{
    BYTE* list = dialog_find_control(self, NULL, LIST_CHAT, 3);
    char line[0x60];
    char name[0x80];
    short shown;

    (void) edx_unused;
    if (!list)
        return;
    if (player != -1) {
        if (!game_FUN_004b0e10(game_net, NULL, player, name))
            return;
#ifdef OPENMCM2_DEV
        eos_chat(name, text);
#endif
        game_FUN_0053a11f(line, CHAT_LINE, name, text);
        if (!FIELD(game_main, 0x538, DWORD)) {
            if (net_me == player) {
                game_FUN_0053a11f(line, CHAT_OWN, name, text);
            } else {
                int i;

                for (i = 0; i < PLAYER_SLOTS; i++) {
                    if (player_id(i) == player)
                        game_FUN_0053a11f(line, CHAT_PLAYER, chat_colour(i), name, text);
                }
            }
        }
    } else {
        char game[0x80];

        load_string(0x14BA, game);
        game_FUN_0053a11f(line, FIELD(game_main, 0x538, DWORD) ? CHAT_LINE : CHAT_NOTICE, game,
                          text);
    }
    shown = (short) FIELD(list, LIST_COUNT, int);
    if (shown >= 100)
        list_remove(list, NULL, 0);
    list_add_text(list, NULL, line, 0, 0);
    list_scroll(list, NULL, shown, TRUE);
}

/* EditChat's text, when there is some, to everyone and to the user's own chat. */
PORT(0x004F8790)
void __fastcall chat_send(BYTE* self)
{
    BYTE* edit = dialog_find_control(self, NULL, EDIT_CHAT, 11);
    chat_message message;

    message.text[0] = 0;
    edit_get_text(edit, NULL, message.text, sizeof message.text);
    if (message.text[0]) {
        int me = net_me;

        game_FUN_004b0f20(game_net, NULL, MSG_CHAT, &message, CHAT_SIZE, me, 0);
        edit_set_text(edit, NULL, NO_TEXT);
        chat_show(self, NULL, me, message.text);
    }
    gui_user_queue_capture(FIELD(self, 0x34, gui_user*), NULL, edit, NULL, FALSE);
}

/* A notice (at most 73 characters) to everyone and to the user's own chat. */
PORT(0x004F8870)
void __fastcall chat_send_notice(BYTE* self, void* edx_unused, const char* text)
{
    chat_message message;

    (void) edx_unused;
    text_copy_most(message.text, text, 0x49);
    game_FUN_004b0f20(game_net, NULL, MSG_NOTICE, &message, CHAT_SIZE, net_me, 0);
    chat_show(self, NULL, -1, message.text);
    chat_capture(self);
}

/* The player in `slot` (not the user) told to leave, its team dropped. */
PORT(0x004F8900)
void __fastcall player_kick(BYTE* self, void* edx_unused, int slot)
{
    struct {
        DWORD header;
        int   player;
    } message;

    (void) edx_unused;
    message.player = player_id(slot);
    if (!message.player || message.player == net_me)
        return;
    game_FUN_004b0f20(game_net, NULL, MSG_KICK, &message, sizeof message, net_me,
                      message.player);
    teams_remove(mp_teams(self), NULL, message.player);
}

/* ---- Network messages ---- */

#define MSG_RIDER   2
#define MSG_LEFT    5

#ifdef OPENMCM2_DEV
/* The lobby's downloads (below, after mp_track_check; code/mcm2/share.c). */
static int  g_lobby_host;               /* the player the race's settings come from */
static void skin_note(int kind, const char* look, int from);
static void downloads_tick(BYTE* self);
static void track_note(void);
static void downloads_forget(void);
#endif

/* Message 2: a player's readiness, looks, bike and the time it was sent. */
typedef struct rider_status {
    BYTE  header;
    BYTE  ready;
    char  bike_look[0x40];      /* the game's 0x1F2C */
    char  maker_model[0x40];    /* 0x1EEC */
    char  rider_look[0x40];     /* 0x1F6C */
    BYTE  c2;                   /* 0x1FB0 */
    BYTE  team;                 /* 0xC54: the riders the player wants */
    int   sent;
    int   engine;
    BYTE  four_stroke;
    BYTE  unset;                /* the sender never sets it, the receiver keeps it */
    BYTE  pad[2];
    int   skill;
} rider_status;

_Static_assert(sizeof(rider_status) == 0xD4, "a rider's status is 0xD4 bytes");

/*
    A player's status: its ready button (announced once with string 0x14D5), its team, its
    rider record, the lowest ping seen. Kept from the original: it looks at 8 slots of a
    table of 7, the eighth's flags being the first announced one and the user's own ready.
*/
static void rider_status_take(BYTE* self, const rider_status* status, int from, DWORD now)
{
    int i;

#ifdef OPENMCM2_DEV
    skin_note(SHARE_RIDER, status->rider_look, from);
    skin_note(SHARE_BIKE, status->bike_look, from);
#endif

#ifdef OPENMCM2_DEV
    for (i = 0; i < PLAYER_SLOTS; i++) {
#else
    for (i = 0; i < PLAYER_SLOTS + 1; i++) {
#endif
        char name[0x80];
        BYTE* button;
        BYTE* rider;
        double ping;

        if (player_id(i) != from)
            continue;
        game_FUN_0053a11f(name, BUT_RDY_PLAYER, i + 2);
        button = dialog_find_control(self, NULL, name, 1);
        if (button && status->ready) {
            control_set_colour(button, NULL, 0xFFFFFF);
            mp_ready[i] = 1;
            teams_set(mp_teams(self), NULL, from, status->team);
            if (!mp_announced[i]) {
                char format[0x80];
                char line[0x80];

                load_string(0x14D5, format);
                game_FUN_0053a11f(line, format, player_name(i), status->engine);
                chat_show(self, NULL, -1, line);
                mp_announced[i] = 1;
            }
        } else if (button) {
            control_set_colour(button, NULL, 0x808080);
            mp_ready[i] = 0;
            mp_announced[i] = 0;
            teams_remove(mp_teams(self), NULL, from);
        }

        rider = entrant(i + 1);
        FIELD(rider, ENTRANT_PLAYER, int) = player_id(i);
        FIELD(rider, ENTRANT_INDEX, int) = 0;
        text_copy_most((char*) rider + ENTRANT_NAME, player_name(i), 15);
        player_set_names(rider, NULL, status->maker_model, status->bike_look, status->rider_look);
        FIELD(rider, ENTRANT_ENGINE, int) = status->engine;
        FIELD(rider, ENTRANT_FOUR_STROKE, int) = status->four_stroke;
        FIELD(rider, ENTRANT_F4, BYTE) = status->unset;
        FIELD(rider, ENTRANT_SKILL, int) = status->skill;
        FIELD(rider, ENTRANT_C4, int) = status->c2;
        ping = (double) now - status->sent;
        if (x87_less(ping, FIELD(rider, ENTRANT_PING, float)))
            FIELD(rider, ENTRANT_PING, float) = (float) ping;
        teams_set(mp_teams(self), NULL, from, status->team);
        if (net_hosting && FIELD(self, MP_RACE_PAGE, BYTE*))
            opponents_offer(dialog_find_control(FIELD(self, MP_RACE_PAGE, BYTE*), NULL,
                                                OPPONENTS, 0));
    }
}

/* The host's set-up: the settings, and for the race the computer riders and teams. */
static void setup_take(BYTE* self, const BYTE* setup)
{
    int count;
    int i;

    __movsd((unsigned long*) race_settings, (unsigned long*) (setup + SETUP_SETTINGS), 0x7B);
    FIELD(game_main, 0x60C, DWORD) = FIELD(setup, SETUP_60C, DWORD);
    count = race_computers + race_humans;
    if (!FIELD(setup, SETUP_FULL, WORD)) {
        mp_track_check(self);
        return;
    }
    for (i = race_humans; i < count; i++) {
        BYTE* rider = entrant(i);

        FIELD(rider, ENTRANT_SKILL, int) = FIELD(setup, SETUP_SKILLS + i * 4, int);
        text_copy_most((char*) rider + ENTRANT_NAME, (const char*) setup + SETUP_NAMES + i * 16,
                       15);
        FIELD(rider, ENTRANT_INDEX, int) = i;
        text_copy_most((char*) rider + ENTRANT_MAKER,
                       (const char*) setup + SETUP_MAKERS + i * 0x40, 63);
        text_copy_most((char*) rider + ENTRANT_BIKE, (const char*) setup + SETUP_BIKES + i * 0x40,
                       63);
        text_copy_most((char*) rider + ENTRANT_LOOK, (const char*) setup + SETUP_LOOKS + i * 0x40,
                       63);
        FIELD(rider, ENTRANT_ENGINE, int) = FIELD(setup, SETUP_ENGINES + i * 4, int);
        FIELD(rider, ENTRANT_FOUR_STROKE, int) = FIELD(setup, SETUP_FOUR_STROKES + i * 4, int);
    }
    __movsd((unsigned long*) mp_teams(self), (unsigned long*) (setup + SETUP_TEAMS), SETUP_TEAM_DWORDS);
    mp_race_start(self);
}

/* The host's settings unpacked, as mp_settings_send packs them. */
static void settings_take(BYTE* self, const settings_message* message)
{
    const BYTE* packed = message->packed;

    FIELD(race_settings, 0x140, float) = (float) (packed[0] & 0x1F);
    FIELD(race_settings, 0x04, int) = packed[0] >> 5;
    FIELD(race_settings, 0x00, int) = packed[1] & 7;
    FIELD(race_settings, 0x08, int) = packed[3] >> 3 & 7;
    FIELD(race_settings, 0x20, int) = packed[1] >> 3;
    FIELD(race_settings, 0x28, int) = packed[2] & 0xF;
    race_humans = packed[2] >> 4;
#ifdef OPENMCM2_DEV
    /* The full counts, when they are a dev build's (the original's bytes there are its stack's). */
    if (message->humans <= MP_RIDERS_MOST && message->computers <= MP_RIDERS_MOST
        && (message->humans & 0xF) == race_humans && (message->computers & 0xF) == (packed[2] & 0xF)) {
        race_humans = message->humans;
        FIELD(race_settings, 0x28, int) = message->computers;
    }
#endif
    FIELD(race_settings, 0x0C, int) = packed[3] & 7;
    FIELD(race_settings, 0x144, int) = packed[4] >> 4 & 1;
    FIELD(race_settings, 0x148, int) = packed[4] >> 5 & 1;
    FIELD(race_settings, 0x10, int) = packed[3] >> 6 & 1;
    FIELD(race_settings, 0x14, int) = packed[3] >> 7;
    FIELD(race_settings, 0x18, int) = packed[4] & 1;
    FIELD(race_settings, 0x1C, int) = packed[4] >> 1 & 7;
    FIELD(race_settings, 0x34, BYTE) = message->variant;
    FIELD(race_settings, 0x138, DWORD) = message->settings_138;
    FIELD(race_settings, 0x13C, DWORD) = message->settings_13c;
    FIELD(game_main, 0x60C, int) = packed[4] >> 6;
    profile_set_event(game_profile, NULL, FIELD(race_settings, 0x08, DWORD));
    race_track[0] = 0;
    game__strncat(race_track, message->track, 0x3F);
    __movsd((unsigned long*) mp_teams(self), (unsigned long*) message->teams, TEAMS_DWORDS);
    mp_track_check(self);
}

/* A player gone (left, or kicked by the host): its team dropped, the opponents offered again. */
static void player_gone(BYTE* self, int player)
{
    BYTE* list;

    teams_remove(mp_teams(self), NULL, player);
    if (!FIELD(self, MP_RACE_PAGE, BYTE*))
        return;
    list = dialog_find_control(FIELD(self, MP_RACE_PAGE, BYTE*), NULL, OPPONENTS, 0);
    if (net_hosting)
        opponents_offer(list);
}

/*
    The screen's network messages. The user kicked goes back to the main menu (or quits, when
    the lobby started the game).
*/
PORT(0x004F8960)
int __fastcall mp_screen_receive(BYTE* self, void* edx_unused, DWORD type, BYTE* data, int from,
                                 DWORD c, DWORD now)
{
    (void) edx_unused;
#ifdef OPENMCM2_DEV
    if (from && share_take(type, data, from))
        return 1;
    if (from && (type == MSG_SETTINGS || type == MSG_SETUP || type == MSG_SETUP_PART) && g_lobby_host != from) {
        g_lobby_host = from;            /* another lobby: what was asked about there is asked again */
        downloads_forget();
    }
#endif
    if (from && type == MSG_RIDER) {
        rider_status_take(self, (const rider_status*) data, from, now);
    } else if (from && type == MSG_SETUP) {
        setup_take(self, data);
#ifdef OPENMCM2_DEV
    } else if (from && type == MSG_SETUP_PART) {
        setup_part_take(self, (const setup_part*) data);
#endif
    } else if (from && type == MSG_SETTINGS) {
        settings_take(self, (const settings_message*) data);
    } else if (from && type == MSG_CHAT) {
        chat_show(self, NULL, from, (const char*) data + 4);
        return 1;
    } else if (from && type == MSG_NOTICE) {
        chat_show(self, NULL, -1, (const char*) data + 4);
        return 1;
    } else if (type == MSG_LEFT) {
        player_gone(self, FIELD(data, 8, int));
    } else if (from && type == MSG_KICK) {
        if (FIELD(data, 4, int) != net_me) {
            player_gone(self, FIELD(data, 4, int));
        } else if (net_from_lobby) {
            session_quit(game_session);
            return 1;
        } else {
            dialog_end(self, NULL, 0);
            gui_manager_open(session_gui, NULL,
                             dialog_new(0x7F80, 0x527, MAIN_MENU, (void*) 0x0055A070), 0x64, 2,
                             0, NULL, 0x63, 0, 1);
            return 1;
        }
    }
    return game_vf_24_46A550(self, NULL, type, (DWORD) data, from, c, now);
}

/* ---- The screen's events ---- */

#define MP_SCREEN_RACE_INFO 0x88E
#define MP_SCREEN_LOBBY     0x868

static const char* const kick_buttons[7] = {
    NAME(0x00579EB4), NAME(0x00579EA4), NAME(0x00579E94), NAME(0x00579E84),  /* "ButRdyPlayer2".. */
    NAME(0x00579E74), NAME(0x00579E64), NAME(0x00579E54),                    /* .."ButRdyPlayer8" */
};

/* Slot `i`'s kick button: the original's own names, past them ButRdyPlayer9 and on. */
static const char* kick_button(int i)
{
    static char name[24];

    if (i < 7)
        return kick_buttons[i];
    game_FUN_0053a11f(name, BUT_RDY_PLAYER, i + 2);
    return name;
}

static void mp_tab_setup(BYTE* self, const char* name, UINT id)
{
    BYTE* tab = dialog_find_control(self, NULL, name, TAB_CONTROL);

#ifdef OPENMCM2_DEV
    if (menu_reborn(self))                      /* its tabs' words are in their pictures */
        return;
#endif
    option_load_text(tab, NULL, 0, game_language, id);
    option_load_text(tab, NULL, 1, game_language, id);
    control_set_align(tab, NULL, 0x22);
}

/* OptBase.dtm over the screen, on its page `page` (0 the options, 1 the joystick). */
static void options_open(BYTE* self, int line, DWORD page)
{
    BYTE* options = dialog_new(0x7F70, line, OPT_BASE, (void*) 0x00556BD0);

    gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL, options, 0, 2, 0, self, page, 0, 1);
}

/* Packet intervals: full ones 2 to 4 seconds, short ones 0.067 to 0.2 (0 or less by link). */
static void packet_intervals_load(void)
{
    float* full = &FIELD(game_main, 0x3414, float);
    float* brief = &FIELD(game_main, 0x3418, float);

    *full = (float) (VIRTUAL(game_main, 0x50, game_number)(game_main, NULL, FULL_PACKETS, -1000)
                     * (double) 0.001f);
    if (x87_less(*full, 0.0f) || x87_less(*full, 2.0))
        *full = 2.0f;
    else if (*full > 4.0f)
        *full = 4.0f;

    *brief = (float) (VIRTUAL(game_main, 0x50, game_number)(game_main, NULL, SHORT_PACKETS, -1000)
                      * (double) 0.001f);
    if (x87_less(*brief, 0.0f)) {
        int link = FIELD(game_net, 0xF4, int);

        *brief = link == 1 || link == 2 || link == 8 ? 0.067f : 0.2f;
    } else if (x87_less(*brief, 0.067f)) {
        *brief = 0.067f;
    } else if (*brief > 0.2f) {
        *brief = 0.2f;
    }
#ifdef OPENMCM2_DEV
    /* [online] packets: each bike's state that many times a second on every link (the game sent it
       5 times a second online, its riders seen jumping), and in full each second. */
    if (dev_settings.online_rate > 0) {
        *brief = 1.0f / dev_settings.online_rate;
        *full = 1.0f;
    }
#endif
}

static void mp_screen_open(BYTE* self)
{
    char name[0x10];    /* the original's room for the user's name */
    char text[0x80];
    BYTE* control;
    int link;
    int i;

    FIELD(self, 0x7F58, BYTE*) = NULL;
    FIELD(self, 0x7F5C, BYTE*) = NULL;
    FIELD(self, 0x7F60, BYTE*) = NULL;
    FIELD(self, 0x7F64, BYTE*) = NULL;
    FIELD(self, 0x80D0, BYTE) = 0;
    mp_tab_setup(self, TAB_EVENT, 0x13E3);
    mp_tab_setup(self, TAB_BIKE_RIDER, 0x13E4);
    mp_tab_setup(self, TAB_RACE_INFO, 0x13E5);
    mp_tab_setup(self, TAB_OPTIONS, 0x1468);
    mp_settings_restore();
    FIELD(self, 0x7FC8, char) = 0;
    FIELD(self, 0x80CC, DWORD) = 0;
    packet_intervals_load();
    settings_send_wait = 2;
    profile_reset_players(game_profile);
    if (FIELD(self, 0xC4, int) != MP_SCREEN_LOBBY)
        game_FUN_0045f530(race_events);
    FIELD(game_main, 0x2D9C, BYTE) = 0;
    mp_state_44 = 2;
    mp_track_state = !net_hosting;
    if (race_kind == 4) {
        mp_state_44 = 0;
        race_mode = 0;
    } else {
        mp_state_44 = 2;
    }

    control = dialog_find_control(self, NULL, LIST_CHAT, 0);
    list_set_224(control, NULL, 0);
    control_set_align(control, NULL, 0x11);
    control = dialog_find_control(self, NULL, EDIT_CHAT, 0);
    edit_set_capacity(control, NULL, 0x3C);
    control_set_align(control, NULL, 0x11);
    edit_set_caret_colour(control, NULL, 0xFFFFFF);
    if (session_showing == 0x866 && !net_from_lobby)
        mp_user_reset(self);
    mp_user_ready = 0;

    control = dialog_find_control(self, NULL, USER_NAME, 12);
    game_FUN_004b0e10(game_net, NULL, net_me, name);
    control_set_text(control, NULL, name);
    control_set_shadow(control, NULL, FALSE);
    control_set_align(control, NULL, 0x12);
    control = dialog_find_control(self, NULL, BUT_RDY_USER, 0);
    control_set_colour(control, NULL, 0x808080);
    control_set_align(control, NULL, 0x12);
    for (i = 0; i < mp_rows(self); i++) {
        game_FUN_0053a11f(text, BUT_RDY_PLAYER, i + 2);
        control = dialog_find_control(self, NULL, text, 0);
        control_set_colour(control, NULL, 0x808080);
        control_set_align(control, NULL, 0x12);
    }
    for (i = 0; i < PLAYER_SLOTS; i++) {
        if (i < mp_rows(self)) {
            game_FUN_0053a11f(text, NAME_PLAYER, i + 2);
            control = dialog_find_control(self, NULL, text, 0);
            control_set_shadow(control, NULL, FALSE);
            control_set_align(control, NULL, 0x12);
        }
        mp_ready[i] = 0;
        mp_announced[i] = 0;
    }
    mp_buttons_update(self);
    list_set_224(dialog_find_control(self, NULL, LIST_CHAT, 0), NULL, 0);
    dialog_set_controls(self, NULL, 0x258, 0);
    mp_controls_update(self);

    if (net_from_lobby) {
        if (lobby_slot == -1) {
            zone_read_launch(FIELD(game_main, 0x3410, BYTE*), NULL, (DWORD*) &lobby_slot);
            zone_wait_for_slot(FIELD(game_main, 0x3410, BYTE*));
            if (FIELD(self, MP_RACE_PAGE, BYTE*)) {
                dialog_notify_controls(FIELD(self, MP_RACE_PAGE, BYTE*), NULL, 0);
                mp_event_kind_set(FIELD(self, MP_RACE_PAGE, BYTE*));
                dialog_notify_controls(FIELD(self, MP_RACE_PAGE, BYTE*), NULL, 0);
            }
        }
    } else {
        lobby_slot = 0;
    }
    if (FIELD(self, 0xC4, int) != MP_SCREEN_RACE_INFO
        && FIELD(self, 0xC4, int) != MP_SCREEN_LOBBY) {
        radio_set(dialog_find_control(self, NULL, TAB_EVENT, TAB_CONTROL), NULL, 0);
        mp_show_page(self, NULL, 0);
    } else {
        radio_set(dialog_find_control(self, NULL, TAB_RACE_INFO, TAB_CONTROL), NULL, 0);
        mp_show_page(self, NULL, 2);
    }
    FIELD(self, 0x118, COLORREF) = 0xFEB97A;

    link = FIELD(game_net, 0xF4, int);
    if ((link == 4 || link == 2) && net_hosting) {
        char address[0x80];
        char line[0x80];

        load_string(0x14B9, text);
        game_FUN_004b09c0(game_net, NULL, address);
        game_FUN_0053a11f(line, TWO_WORDS, text, address);
        chat_show(self, NULL, -1, line);
    }
    dialog_add_timer(self, NULL, 0, 1000, NULL);
    chat_capture(self);
}

/* ButRdyUser: ready and not ready in turn, unless the bike is above the class (or a warning). */
static void user_ready_toggle(BYTE* self)
{
    BOOL over = engine_class(FIELD(game_main, 0x1550, int)) > FIELD(race_settings, 0x1C, int);
    BYTE* button;

    if (lobby_slot)
        over = engine_class(FIELD(game_main, 0x1550, int)) > 2;
    if (over)
        mp_ready_cancel(self, NULL, 1);
    if (!over && !mp_track_state)
        mp_user_ready = !mp_user_ready;
    else if (mp_track_state == 2 || mp_track_state == 3)
        mp_ready_cancel(self, NULL, mp_track_state);

    button = dialog_find_control(self, NULL, BUT_RDY_USER, 0);
    if (mp_user_ready) {
        char format[0x80];
        char line[0x80];

        mp_controls_update(self);
        control_set_colour(button, NULL, 0xFFFFFF);
        load_string(0x14D3, format);
        game_FUN_0053a11f(line, format, game_profile);
        chat_send_notice(self, NULL, line);
    } else {
        control_set_colour(button, NULL, 0x808080);
    }
}

/* Every second: the players' table from the network, their names, Start, the user's status. */
static void mp_screen_tick(BYTE* self)
{
    rider_status status;
    char text[0x80];
    int iterator = 0;
    int me;
    BYTE* player;
    int i;

#ifdef OPENMCM2_DEV
    downloads_tick(self);
#endif

    for (i = 0; i < PLAYER_SLOTS; i++)
        game_vf_1_4B23D0(mp_players + i * PLAYER_SIZE);
    mp_player_count = 0;
    me = net_me;
    for (player = game_FUN_004b0eb0(game_net, NULL, &iterator); player;
         player = game_FUN_004b0eb0(game_net, NULL, &iterator)) {
        if (FIELD(player, 0, int) == me)
            continue;
        player_id(mp_player_count) = FIELD(player, 0, int);
        text_copy_most(player_name(mp_player_count), (const char*) player + 4, 15);
        teams_set(mp_teams(self), NULL, FIELD(player, 0, int), -1);
        mp_player_count++;      /* the original never checks it against the 7 slots */
#ifdef OPENMCM2_DEV
        if (mp_player_count == PLAYER_SLOTS)
            break;
#endif
    }
    for (i = 0; i < mp_rows(self); i++) {
        game_FUN_0053a11f(text, NAME_PLAYER, i + 2);
        control_set_text(dialog_find_control(self, NULL, text, 0), NULL, player_name(i));
    }
    for (i = 0; i < mp_rows(self); i++) {
        BYTE* button;

        game_FUN_0053a11f(text, BUT_RDY_PLAYER, i + 2);
        button = dialog_find_control(self, NULL, text, 0);
        if (!player_name(i)[0])
            control_set_colour(button, NULL, 0x808080);
    }
    if (FIELD(self, MP_BIKE_PAGE, BYTE*))
        dialog_notify_controls(FIELD(self, MP_BIKE_PAGE, BYTE*), NULL, 1);
    mp_buttons_update(self);

    if (net_hosting) {
        BYTE* start = dialog_find_control(self, NULL, START, 0);
        BOOL all_ready = TRUE;

        for (i = 0; i < mp_player_count; i++) {
            if (!mp_ready[i])
                all_ready = FALSE;
        }
        control_enable(start, mp_user_ready && all_ready && mp_player_count >= 1);
        teams_set(mp_teams(self), NULL, me, FIELD(game_main, 0xC54, int));
        mp_settings_send(self);
    }

    status.ready = (BYTE) mp_user_ready;
    text_copy_most(status.maker_model, (const char*) game_main + 0x1EEC, 0x3F);
    text_copy_most(status.bike_look, (const char*) game_main + 0x1F2C, 0x3F);
    text_copy_most(status.rider_look, (const char*) game_main + 0x1F6C, 0x3F);
    status.c2 = FIELD(game_main, 0x1FB0, BYTE);
    status.team = FIELD(game_main, 0xC54, BYTE);
    status.engine = FIELD(game_main, 0x1550, int);
    status.four_stroke = FIELD(game_main, 0x1554, BYTE);
    if (skill_wanted > 0 && skill_wanted < 101)
        race_skill = skill_wanted;
    status.skill = race_skill;
    status.sent = (int) game_FUN_004c41d0();
    game_FUN_004b0f20(game_net, NULL, MSG_RIDER, &status, sizeof status, net_me, 0);
}

static void mp_screen_click(BYTE* self, ui_event* event)
{
    int i;

    if (clicked(event, TAB_EVENT)) {
        mp_show_page(self, NULL, 0);
    } else if (clicked(event, TAB_BIKE_RIDER)) {
        mp_show_page(self, NULL, 1);
    } else if (clicked(event, TAB_RACE_INFO)) {
        mp_show_page(self, NULL, 2);
    } else if (clicked(event, TAB_OPTIONS)) {
        mp_show_page(self, NULL, 3);
    } else if (clicked(event, BACK)) {
        if (FIELD(self, 0x7F58, BYTE*))
            dialog_notify_controls(FIELD(self, 0x7F58, BYTE*), NULL, 1);
        if (FIELD(self, 0x7F5C, BYTE*))
            dialog_notify_controls(FIELD(self, 0x7F5C, BYTE*), NULL, 1);
        dialog_notify_controls(self, NULL, 1);
        mp_settings_save();
        if (net_from_lobby) {
            session_quit(game_session);
            return;
        }
        game_FUN_004b0d20(game_net);
        net_leave();
        dialog_end((BYTE*) event->sender, NULL, 0);
        event->e = 1;
        session_go_to(game_session, NULL, 100);
    } else if (clicked(event, BUT_RDY_USER)) {
        user_ready_toggle(self);
    } else if (clicked(event, START)) {
        if (mp_user_ready) {
            dialog_notify_controls(self, NULL, 1);
            if (FIELD(self, 0x7F58, BYTE*))
                dialog_notify_controls(FIELD(self, 0x7F58, BYTE*), NULL, 1);
            if (FIELD(self, 0x7F5C, BYTE*))
                dialog_notify_controls(FIELD(self, 0x7F5C, BYTE*), NULL, 1);
            if (FIELD(self, 0x7F64, BYTE*))
                dialog_notify_controls(FIELD(self, 0x7F64, BYTE*), NULL, 1);
            mp_settings_save();
            mp_setup_send(self, NULL, 1);
        }
    } else if (clicked(event, BUT_CHAT_SEND)) {
        chat_send(self);
        chat_capture(self);
    } else if (clicked(event, OPTIONS)) {
        options_open(self, 0x6AC, 0);
    } else if (clicked(event, JOYSTICK)) {
        options_open(self, 0x6AF, 1);
    } else if (clicked(event, HELP)) {
        profile_open_help(game_profile, NULL, HELP_TOPIC, 0);
    } else {
        for (i = 0; i < mp_rows(self); i++) {
            if (clicked(event, kick_button(i))) {
                player_kick(self, NULL, i);
                break;
            }
        }
    }
    if (!clicked(event, BACK) && !clicked(event, START))
        chat_capture(self);
}

/*
    The multiplayer screen (vtable 0x00559BCC). It shows the players and their readiness, keeps
    the chat, and the host sends the settings every other second and the race's set-up on Start.
    A page asks (event 9, code 0x51) for the bike page to be refilled.
*/
PORT(0x004F9400)
void __fastcall mp_screen_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    switch (event->type) {
    case EVENT_OPEN:
        mp_screen_open(self);
        break;
    case EVENT_CLICK:
        mp_screen_click(self, event);
        break;
    case EVENT_KEY:
        if (event->d == 0x2F
            && FIELD(FIELD(self, 0x34, gui_user*), 0x1D4, BYTE*)
                   != dialog_find_control(self, NULL, EDIT_CHAT, 0))
            chat_capture(self);
        if (event->d == 0xD) {
            BYTE* send = dialog_find_control(self, NULL, BUT_CHAT_SEND, 0);

            VIRTUAL(send, 0xE8, control_press)(send, NULL, 2, 0);
            chat_send(self);
        }
        break;
    case EVENT_TIMER:
        mp_screen_tick(self);
        break;
    case EVENT_CHILD_ENDED:
        if (event->a == 0x51 && FIELD(self, MP_BIKE_PAGE, BYTE*)) {
            BYTE* page = FIELD(self, MP_BIKE_PAGE, BYTE*);

            dialog_notify_controls(page, NULL, 0);
            mp_choices_fill(page);
            rider_mark_changed(page);
            mp_bike_choose(page);
        }
        break;
    case EVENT_CLOSE:
        dialog_remove_timer(self, NULL, 0);
        break;
    }
}

#define mp_rules          FIELD(game_main, 0x214C, DWORD) /* host's: 1 event, 2 track, 4/8 bikes */
#define mp_largest        FIELD(game_main, 0x2D8C, int)  /* the largest engine class allowed */
#define mp_max_ai         FIELD(game_main, 0xC54, int)
#define SET_TRACK_ID      0x08
#define SET_TRACK_138     0x138
#define SET_TRACK_CHECK   0x13C   /* compared with the track file's */
#define SET_TAG_BALL      0x144
#define SET_STUNT         0x148

#define MP_PAGES          0x7F58  /* MPBase: BYTE*[4] */
#define MP_TRACK          0x7FC8  /* MPBase: char[0x104], the track last checked */
#define MP_TRACK_KIND     0x80CC
#define MP_TRACK_NAME     0x80D0  /* MPBase: its title */
#define EVENT_FILLED      0x7F58  /* the event page: set once filled */

#define BUTTON_READY      NAME(0x00579DD8)    /* "ButRdyUser" */
#define CHECK_TAG_BALL    NAME(0x00579FF4)    /* "ChkTagBall" */
#define CHECK_STUNT       NAME(0x00579FE4)    /* "ChkStuntMode" */
#define LIST_LARGEST      NAME(0x00579DEC)    /* "LargestOpponentDropDown" */
#define BUTTON_WRENCH     NAME(0x0056FB84)    /* "ButWrench" */
#define CHECK_FAST        NAME(0x0057A024)    /* "FastFinishesCheckBox" */
#define CHECK_RIDERS      NAME(0x0057A010)    /* "ChkRiderCollision" */
#define EDIT_MAX_AI       NAME(0x0057A03C)    /* "EditMaxAIBikes" */
#define UICONTROL_CLASS   NAME(0x005717A0)    /* "UIControl" */

#define mp_vector_z    (*(vector3*) 0x0068FE18)
#define mp_vector_zero (*(vector3*) 0x0068FE28)
#define mp_vector_x    (*(vector3*) 0x0068FE38)
#define mp_vector_y    (*(vector3*) 0x0068FEF0)

int   __fastcall dialog_press(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg);
void  __fastcall session_show_scene(BYTE* self, void* edx_unused, BYTE* dialog);
void  __fastcall session_hide_scene(BYTE* self);
char* __cdecl    number_text(int value, char* text);
BOOL  __fastcall list_add_file(BYTE* self, void* edx_unused, const char* name, DWORD data,
                               BOOL animated, DWORD b);
int   __fastcall control_set_colour(BYTE* self, void* edx_unused, COLORREF colour);
void  __fastcall game_FUN_0046a880(void* walk, void* edx_unused, void* root, DWORD deep,
                                   const char* class_name);
BYTE* __fastcall game_FUN_0046a980(void* walk);
void  __fastcall game_FUN_0046a970(void* walk);

BOOL __fastcall profile_race_scene(char* self, void* edx_unused, char* scene);
void  __cdecl    game_FUN_0051dcc0(char* text, float seconds);    /* a time, as text */

/* OpponentsListBox: 0 to the host's most, the old count kept where it still fits. */
#define LIST_TEXT_BOX NAME(0x0056FA10)    /* "DDLTextBox": the track chosen */

const char* __fastcall list_text(BYTE* self, void* edx_unused, int index);
int         __fastcall list_select_data(BYTE* self, void* edx_unused, DWORD data);
int         __fastcall option_select(BYTE* self, void* edx_unused, int index);
int         __cdecl    game__strcmp(const char* a, const char* b);
void        __fastcall edit_set_allowed(BYTE* self, void* edx_unused, const char* allowed);
int         __cdecl    game_FUN_0053b6c7(const char* text);                    /* atoi */
char*       __cdecl    game_FUN_0054aaa5(int value, char* text, int radix);    /* _itoa */

/* This half's own, ahead of their callers. */
void __fastcall mp_ready_cancel(BYTE* self, void* edx_unused, int reason);
void __fastcall mp_show_page(BYTE* self, void* edx_unused, int page);
void __fastcall mp_track_check(BYTE* self);
int __fastcall mp_event_message(BYTE* self, void* edx_unused, DWORD message, DWORD a, DWORD b,
                                DWORD c, DWORD d);
void __fastcall mp_event_show(BYTE* self);
void __fastcall mp_event_handle(BYTE* self, void* edx_unused, ui_event* event);
void __fastcall mp_event_refresh(BYTE* self);
void __fastcall mp_event_kind_set(BYTE* self);
void __fastcall mp_event_sync(BYTE* self, void* edx_unused, DWORD out);
void __fastcall mp_track_picture(BYTE* self);
void __fastcall track_picture_add(BYTE* self, void* edx_unused, BYTE* list, const char* folder,
                                  const char* track);
void __fastcall rider_mark_changed(BYTE* self);
void __fastcall mp_bike_rider_handle(BYTE* self, void* edx_unused, ui_event* event);
void __fastcall mp_choices_fill(BYTE* self);
void __fastcall mp_players_set(BYTE* self, void* edx_unused, DWORD value);
int __fastcall mp_bike_rider_message(BYTE* self, void* edx_unused, DWORD message, DWORD a,
                                     DWORD b, DWORD c, DWORD d);
void __fastcall mp_largest_check(BYTE* self);
void __fastcall mp_largest_enable(BYTE* self);
int __fastcall mp_preview_press(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg);
int __fastcall mp_preview_tick(BYTE* self, void* edx_unused, float seconds);
void __fastcall mp_bike_choose(BYTE* self);
void __fastcall mp_bike_rider_sync(BYTE* self, void* edx_unused, DWORD out);
void __fastcall mp_options_handle(BYTE* self, void* edx_unused, ui_event* event);
int __fastcall mp_options_message(BYTE* self, void* edx_unused, DWORD message, DWORD a, DWORD b,
                                  DWORD c, DWORD d);
void __fastcall mp_options_load(BYTE* self);
void __fastcall mp_options_refresh(BYTE* self);
void __cdecl init_mp_vector_zero(void);
void __cdecl init_mp_vector_x(void);
void __cdecl init_mp_vector_y(void);
void __cdecl init_mp_vector_z(void);
void __cdecl init_mp_vector_zero_entry(void);
void __cdecl init_mp_vector_x_entry(void);
void __cdecl init_mp_vector_y_entry(void);
void __cdecl init_mp_vector_z_entry(void);

static void opponents_fill(BYTE* list, int chosen)
{
    char number[0x80];
    int i;

    list_clear(list);
    for (i = 0; i <= opponent_max; i++) {
        game_FUN_0053a11f(number, ONE_NUMBER, i);
        list_add_text(list, NULL, number, i, 0);
    }
    list_select_all(list, NULL, opponent_max < chosen ? opponent_max : chosen);
}

static BOOL opponents_stale(BYTE* list)
{
    return race_kind != 4 && FIELD(list, 0x1EC, int) - 1 != opponent_max;
}

/* DDLTextBox on a client: only the host's track, by its title. */
static void track_show_hosts(BYTE* self)
{
    BYTE* list = combo_list(self, LIST_TEXT_BOX);

    list_clear(list);
    list_add_text(list, NULL, (char*) FIELD(self, 0x2C, BYTE*) + MP_TRACK_NAME, 0, 0);
    list_select_all(list, NULL, 0);
}

static void tracks_list(BYTE* self, DWORD append)
{
    tracks_fill(0, SP_PICTURE, LIST_TEXT_BOX, 0, self, append);
}

/*
    The player is no longer ready, for `reason`: 1 a bike over the largest engine allowed,
    2 no such track here, 3 another track by that name. The chat is told, the others too for
    2 and 3, and ButRdyUser greys.
*/
PORT(0x004FA580)
void __fastcall mp_ready_cancel(BYTE* self, void* edx_unused, int reason)
{
    char format[0x80];
    char text[0x80];
    char message[0x80];         /* left unset for other reasons, as in the original */

    (void) edx_unused;
    if (reason == 1) {
        game_FUN_00526fc0(game_main, NULL, 0x146A, format, sizeof format);
        game_FUN_0053a11f(text, format, engine_sizes[mp_largest]);
        if (mp_rules)
            game_FUN_0053a11f(text, format, engine_sizes[2]);
        game_FUN_00526fc0(game_main, NULL, 0x14D1, format, sizeof format);
        game_FUN_0053a11f(message, format, text);
    } else if (reason == 2 || reason == 3) {
        game_FUN_00526fc0(game_main, NULL, reason == 2 ? 0x14D2 : 0x14DA, format, sizeof format);
        text_copy_most(message, format, 0x7F);
    }
    chat_show(self, NULL, -1, message);
    mp_user_ready = 0;
    if (reason == 2 || reason == 3) {
        game_FUN_00526fc0(game_main, NULL, reason == 2 ? 0x14D4 : 0x14DB, format, sizeof format);
        game_FUN_0053a11f(text, format, game_profile);
        chat_send_notice(self, NULL, text);
    }
    control_set_colour(dialog_find_control(self, NULL, BUTTON_READY, 1), NULL, 0x808080);
}

static const struct {
    DWORD       size;
    int         line;
    const char* file;
    void**      vtable;
} mp_pages[4] = {
    { 0x7F5C, 0x779, NAME(0x00579FD8), (void**) 0x0055CD08 },              /* MPEvent.dtm */
    { 0x7F9C, 0x77F, NAME(0x00579FCC), (void**) 0x0055CC84 },              /* MPBikeR.dtm */
    { 0x7F58, 0x785, NAME(0x00579FC0), (void**) 0x0055CC00 },              /* MPRInfo.dtm */
    { 0x7F58, 0x78B, NAME(0x00579FB4), (void**) 0x0055CB7C },              /* MPOpt.dtm */
};

/* Show page `page` (0 to 3) over the screen, closing the others; one already open stays. */
PORT(0x004FA7C0)
void __fastcall mp_show_page(BYTE* self, void* edx_unused, int page)
{
    RECT area = { 17, 35, 426, 273 };
    BYTE* dialog;
    int i;

    (void) edx_unused;
#ifdef OPENMCM2_DEV
    if ((unsigned) page <= 3)
        page_title_show(self, page);
#endif
    for (i = 0; i < 4; i++) {
        BYTE** open = &FIELD(self, MP_PAGES + 4 * i, BYTE*);

        if (*open && page != i) {
            object_destroy(*open);
            *open = NULL;
        }
    }
    if ((unsigned) page > 3 || FIELD(self, MP_PAGES + 4 * page, BYTE*))
        return;
    dialog = game_new(mp_pages[page].size, SGP_CPP, mp_pages[page].line);
    if (dialog) {
        dialog_construct(dialog, NULL, 1, mp_pages[page].file);
        *(void***) dialog = mp_pages[page].vtable;
    }
    FIELD(self, MP_PAGES + 4 * page, BYTE*) = dialog;
    gui_manager_open(FIELD(game_session, 0x2C, gui_manager*), NULL, dialog, 0, 1, (DWORD) &area,
                     self, 0, 0, 1);
}

/*
    The race's track, once it changes: is it here (its scn file names it), and the same as the
    host's (0x13C)? A missing or different one cancels Ready. The title is kept for the lists.
*/
PORT(0x004FAA60)
void __fastcall mp_track_check(BYTE* self)
{
    const char* track = (const char*) race_settings + SET_TRACK;
    char* title = (char*) self + MP_TRACK_NAME;
    char folder[0x104];
    char path[0x104];
    DWORD found = 0;
    DWORD check = 0;
    DWORD variant;

    mp_controls_update(self);
    if (!game__strcmp((char*) self + MP_TRACK, track)
        && FIELD(self, MP_TRACK_KIND, DWORD) == FIELD(race_settings, SET_TRACK_KIND, BYTE))
        return;
    profile_race_scene(game_profile, NULL, folder);
    profile_find_named_file(game_profile, NULL, folder, track, ENV, path);
    game_FUN_004ee910(FIELD(game_main, 0x574, BYTE*), NULL, path);
    if (profile_event(game_profile) == 1 || profile_event(game_profile) == 5)
        variant = FIELD(race_settings, SET_TRACK_KIND, BYTE);
    else
        variant = 0;
    game_FUN_004eeda0(FIELD(game_main, 0x574, BYTE*), NULL, title, track, variant, SCN, &found,
                      &check);
    if (!game_FUN_0053b6e0(NO_NAME, title)) {
        mp_track_state = 2;
        if (mp_user_ready)
            mp_ready_cancel(self, NULL, 2);
        game_FUN_00526fc0(game_main, NULL, 0x143B, title, 0x80);   /* 0x40 left (the original's) */
    } else if (check == FIELD(race_settings, SET_TRACK_CHECK, DWORD)) {
        mp_track_state = 0;
    } else {
        mp_track_state = 3;
        if (mp_user_ready)
            mp_ready_cancel(self, NULL, 3);
    }
    if (FIELD(self, MP_PAGES, BYTE*))
        mp_track_picture(FIELD(self, MP_PAGES, BYTE*));
    text_copy_most((char*) self + MP_TRACK, track, 0x103);
    FIELD(self, MP_TRACK_KIND, DWORD) = FIELD(race_settings, SET_TRACK_KIND, BYTE);
    text_copy_most((char*) game_main + race_kind * 0x80 + 0xC6C, title, 0x7F);
#ifdef OPENMCM2_DEV
    track_note();
#endif
}

#ifdef OPENMCM2_DEV
/*
    The lobby's downloads (share.c): the host's track, when this player has none by its name or
    another one, and the other riders' own skins - each asked about once, in messbox2.dtm on a
    copy of its vtable with these events, and downloaded one at a time from the lobby's ticks.
    A track come in is checked again, so the player can ready up.
*/
#define PROMPT_VTABLE   0x00557210      /* messbox2.dtm's (named_choice_handle): slot 29 below */
#define PROMPT_SLOTS    32
#define SKINS_MOST      64

typedef struct skin_seen {
    int  kind, from;
    BOOL asked;
    char look[64];
} skin_seen;

static void*     g_prompt_vtable[PROMPT_SLOTS + 1];
static BOOL      g_prompt_open, g_track_new;
static int       g_prompt_kind;         /* SHARE_TRACK, or SHARE_RIDER for the skins */
static char      g_track_asked[64];
static skin_seen g_skins[SKINS_MOST];
static int       g_skin_count;

static void downloads_forget(void)
{
    g_track_asked[0] = 0;
    g_skin_count = 0;
}

static void track_note(void)
{
    const char* track = (const char*) race_settings + SET_TRACK;

    if (net_hosting || (mp_track_state != 2 && mp_track_state != 3) || !lstrcmpiA(g_track_asked, track))
        return;
    lstrcpynA(g_track_asked, track, sizeof g_track_asked);
    g_track_new = TRUE;
}

static void skin_note(int kind, const char* status_look, int from)
{
    char look[64];
    int i;

    lstrcpynA(look, status_look, sizeof look);
    if (!share_is_skin(look) || share_have(kind, look) || g_skin_count == SKINS_MOST)
        return;
    for (i = 0; i < g_skin_count; i++)
        if (!lstrcmpiA(g_skins[i].look, look))
            return;
    g_skins[g_skin_count].kind = kind;
    g_skins[g_skin_count].from = from;
    g_skins[g_skin_count].asked = FALSE;
    lstrcpynA(g_skins[g_skin_count].look, look, sizeof look);
    g_skin_count++;
}

static int skins_unasked(void)
{
    int i, n = 0;

    for (i = 0; i < g_skin_count; i++)
        n += !g_skins[i].asked;
    return n;
}

static void prompt_write(BYTE* self, const char* name, const char* words)
{
    BYTE* control = dialog_find_control(self, NULL, name, 0);

    if (control)
        control_set_text(control, NULL, words);
}

static void __fastcall download_prompt_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    char words[0x180];
    int i;

    (void) edx_unused;
    if (event->type == EVENT_OPEN) {
        BYTE* prompt = dialog_find_control(self, NULL, "TxtPrompt", 12);

        if (g_prompt_kind == SHARE_TRACK)
            wsprintfA(words, mp_track_state == 3
                      ? "YOUR TRACK %s IS NOT THE HOST'S. DOWNLOAD THE HOST'S? YOURS IS KEPT AS %s.ENV.BAK."
                      : "THE HOST'S TRACK, %s, IS NOT INSTALLED. DOWNLOAD IT FROM THE HOST? YOU CAN READY UP "
                        "ONCE IT IS IN.", g_track_asked, g_track_asked);
        else
            wsprintfA(words, "%d OF THE OTHER RIDERS' OWN SKINS ARE NOT INSTALLED. DOWNLOAD THEM FROM THEIR "
                      "RIDERS?", skins_unasked());
        prompt_write(self, "TitleText", g_prompt_kind == SHARE_TRACK ? "DOWNLOAD TRACK" : "DOWNLOAD SKINS");
        if (prompt)
            FIELD(prompt, 0x1E8, DWORD) = 1;        /* as load_prompt: the words wrap */
        prompt_write(self, "TxtPrompt", words);
        prompt_write(self, "ButLeft", "NO");
        prompt_write(self, "ButRight", "DOWNLOAD");
        control_show(dialog_find_control(self, NULL, "ButMiddle", 1), NULL, FALSE, TRUE);
        return;
    }
    if (event->type != EVENT_CLICK || (!clicked(event, "ButRight") && !clicked(event, "ButLeft")))
        return;
    if (clicked(event, "ButRight") && g_prompt_kind == SHARE_TRACK)
        share_get(SHARE_TRACK, g_track_asked, g_lobby_host);
    for (i = 0; g_prompt_kind != SHARE_TRACK && i < g_skin_count; i++) {
        if (!g_skins[i].asked && clicked(event, "ButRight"))
            share_get(g_skins[i].kind, g_skins[i].look, g_skins[i].from);
        g_skins[i].asked = TRUE;
    }
    g_prompt_open = FALSE;
    dialog_end(self, NULL, 0);
    event->e = 1;
}

static void prompt_open(BYTE* self, int kind)
{
    if (!g_prompt_vtable[1]) {
        __movsd((unsigned long*) g_prompt_vtable, (const unsigned long*) (PROMPT_VTABLE - 4), PROMPT_SLOTS + 1);
        g_prompt_vtable[1 + 29] = (void*) download_prompt_handle;
    }
    g_prompt_kind = kind;
    g_prompt_open = TRUE;
    gui_manager_open(session_gui, NULL, dialog_new(0x7F58, __LINE__, NAME(0x0056F734), &g_prompt_vtable[1]),
                     0, 4, 0, self, 0, 0, 1);                                 /* messbox2.dtm */
}

static void downloads_tick(BYTE* self)
{
    char news[0x80];
    int got = share_tick(news);

    if (got)
        chat_show(self, NULL, -1, news);
    if (got == SHARE_TRACK_IN) {
        ((char*) self)[MP_TRACK] = 0;           /* checked again, not taken as seen */
        mp_track_check(self);
    }
    if (g_prompt_open)
        return;
    if (g_track_new) {
        g_track_new = FALSE;
        prompt_open(self, SHARE_TRACK);
    } else if (skins_unasked()) {
        prompt_open(self, SHARE_RIDER);
    }
}
#endif

__declspec(noinline) void __fastcall mp_event_kind_set(BYTE* self);
__declspec(noinline) void __fastcall mp_event_refresh(BYTE* self);
__declspec(noinline) void __fastcall mp_event_show(BYTE* self);
__declspec(noinline) void __fastcall mp_track_picture(BYTE* self);

/* Slot 24: 0x101 refills the page from the settings; 0xF (with b) shows the host's. */
PORT(0x004FACF0)
int __fastcall mp_event_message(BYTE* self, void* edx_unused, DWORD message, DWORD a, DWORD b,
                                DWORD c, DWORD d)
{
    (void) edx_unused;
    (void) a;
    (void) c;
    (void) d;
    if (message == 0x101) {
        int opponents = race_computers;
        BYTE* list;

        FIELD(self, EVENT_FILLED, int) = 0;
        dialog_notify_controls(self, NULL, 0);
        mp_event_kind_set(self);
        FIELD(self, EVENT_FILLED, int) = 1;
        dialog_notify_controls(self, NULL, 0);
        list = dialog_find_control(self, NULL, SP_OPPONENTS, 3);
        if (opponents_stale(list))
            opponents_fill(list, opponents);
    } else if (b && message == 0xF) {
        mp_event_show(self);
        mp_event_refresh(self);
    }
    return 0;
}

static void tag_stunt_show(BYTE* self, BOOL shown)
{
    control_show(dialog_find_control(self, NULL, CHECK_TAG_BALL, 2), NULL, shown, TRUE);
    control_show(dialog_find_control(self, NULL, CHECK_STUNT, 2), NULL, shown, TRUE);
}

/* A client's view of the host's settings: its track, kind, mode, opponents and laps. */
PORT(0x004FAE30)
void __fastcall mp_event_show(BYTE* self)
{
    BYTE* list;
    char text[0x80];

    track_show_hosts(self);
    list_select_data(combo_list(self, SP_EVENT_TYPE), NULL, race_kind);
    list_select_data(combo_list(self, SP_RACE_MODE), NULL, race_mode);
    if (race_kind == 4) {
        BYTE* check;

        control_show(dialog_find_control(self, NULL, SP_OPPONENTS_BOX, 5), NULL, FALSE, TRUE);
        check = dialog_find_control(self, NULL, CHECK_TAG_BALL, 2);
        control_show(check, NULL, TRUE, TRUE);
        option_select(check, NULL, FIELD(race_settings, SET_TAG_BALL, int));
        check = dialog_find_control(self, NULL, CHECK_STUNT, 2);
        control_show(check, NULL, TRUE, TRUE);
        option_select(check, NULL, FIELD(race_settings, SET_STUNT, int));
    } else {
        control_show(dialog_find_control(self, NULL, SP_OPPONENTS_BOX, 5), NULL, TRUE, TRUE);
        list = dialog_find_control(self, NULL, SP_OPPONENTS, 3);
        if (list) {
            list_clear(list);
            number_text(race_computers, text);
            list_add_text(list, NULL, text, race_computers, 0);
        }
        tag_stunt_show(self, FALSE);
    }
    if (race_mode != SERIES)
        series_started = 0;

    if (event_timed(race_kind)) {
        control_load_text(dialog_find_control(self, NULL, SP_LAPS_LABEL, 12), NULL, game_language,
                          0x92F);
        list = dialog_find_control(self, NULL, SP_LAPS, 3);
        list_clear(list);
        game_FUN_0051dcc0(text, FIELD(race_settings, SET_TIME, float) * 60.0f);
        list_add_text(list, NULL, text, (int) FIELD(race_settings, SET_TIME, float), 0);
    } else {
        control_load_text(dialog_find_control(self, NULL, SP_LAPS_LABEL, 12), NULL, game_language,
                          0x929);
        list = dialog_find_control(self, NULL, SP_LAPS, 3);
        list_clear(list);
        number_text(FIELD(race_settings, SET_LAPS, int), text);
        list_add_text(list, NULL, text, FIELD(race_settings, SET_LAPS, DWORD), 0);
    }
    if (FIELD(game_main, 0x2148, int) > 0 && FIELD(game_main, 0x2148, int) < 101)
        FIELD(game_main, 0x2144, int) = FIELD(game_main, 0x2148, int);
}

/* The multiplayer event page. */
PORT(0x004FB130)
void __fastcall mp_event_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    switch (event->type) {
    case EVENT_CLICK:
        if (clicked(event, SP_TRACK_LEFT))
            combo_step(self, LIST_TEXT_BOX, TRUE);
        else if (clicked(event, SP_TRACK_RIGHT))
            combo_step(self, LIST_TEXT_BOX, FALSE);
        else if (clicked(event, SP_RACES_LEFT) || clicked(event, SP_RACES_RIGHT))
            race_round = list_data(dialog_find_control(self, NULL, SP_RACES, 3), NULL, -1);
        break;
    case EVENT_SELECT: {
        BYTE* list = (BYTE*) event->c;

        if (clicked(event, SP_RACE_MODE)) {
            race_mode = list_data(combo_list(self, SP_RACE_MODE), NULL, -1);
            mp_event_refresh(self);
            if (list_data(list, NULL, -1) != SERIES) {
                series_started = 0;
                game_FUN_004b0c00(game_net, NULL, 1);
            }
        } else if (clicked(event, SP_EVENT_TYPE)) {
            dialog_notify_controls(self, NULL, 1);
            mp_event_kind_set(self);
            mp_event_refresh(self);
        } else if (clicked(event, LIST_TEXT_BOX)) {
            BYTE* track = FIELD(game_session, 0x60, BYTE*)
                          + list_data(list, NULL, -1) * TRACK_ENTRY;

            profile_set_event(game_profile, NULL, FIELD(track, 0x8, DWORD));
            text_copy_most((char*) race_settings + SET_TRACK, (const char*) track + 0x14, 0xFF);
            FIELD(race_settings, SET_TRACK_KIND, BYTE) = FIELD(track, 0x4, BYTE);
            FIELD(race_settings, SET_TRACK_138, DWORD) = FIELD(track, 0xC, DWORD);
            FIELD(race_settings, SET_TRACK_CHECK, DWORD) = FIELD(track, 0x10, DWORD);
            text_copy((char*) game_main + race_kind * 0x80 + 0xC6C, list_text(list, NULL, -1));
        }
        break;
    }
    case EVENT_OPEN: {
        BYTE* list = combo_list(self, SP_EVENT_TYPE);
        int opponents;

        FIELD(self, EVENT_FILLED, int) = 0;
        list_add_string(list, 0x13ED, 1);
        list_add_string(list, 0x13EE, 0);
        list_add_string(list, 0x13F0, 5);
        list_add_string(list, 0x13EF, 2);
        list_add_string(list, 0x13F2, 3);
        list_add_string(list, 0x13F1, 4);
        list_select_data(list, NULL, FIELD(race_settings, SET_EVENT, DWORD));
        list = combo_list(self, SP_RACE_MODE);
        list_add_string(list, 0x13F3, 0);
        list_add_string(list, 0x13F4, 1);
        list_add_string(list, 0x13F5, 2);
        list_select_all(list, NULL, FIELD(race_settings, SET_MODE, int));
        control_bind(dialog_find_control(self, NULL, CHECK_TAG_BALL, 0),
                     race_settings + SET_TAG_BALL);
        control_bind(dialog_find_control(self, NULL, CHECK_STUNT, 0), race_settings + SET_STUNT);
        opponents = race_computers;
        dialog_notify_controls(self, NULL, 0);
        mp_event_kind_set(self);
        mp_event_refresh(self);
        list = dialog_find_control(self, NULL, SP_OPPONENTS, 3);
        if (net_hosting && opponents_stale(list))
            opponents_fill(list, opponents);
        if (!net_hosting)
            mp_track_picture(self);
        break;
    }
    case EVENT_CLOSE:
        race_computers = race_kind != 4
            ? (int) list_data(dialog_find_control(self, NULL, SP_OPPONENTS, 3), NULL, -1) : 0;
        break;
    }
}

/*
    What can be changed: everything for a host without rule 1, else the event and mode lists
    hide (the track stays the host's to pick without rule 2); the boxes for the race mode; the
    races list, fixed at the race under way during a series.
*/
PORT(0x004FB7A0)
void __fastcall mp_event_refresh(BYTE* self)
{
    BOOL open = net_hosting && !(FIELD(game_main, 0x214C, BYTE) & 1);
    BYTE walk[0x94];
    BYTE* control;
    BYTE* laps_box;
    BYTE* races_box;
    BYTE* races_left;
    BYTE* races_right;
    BYTE* opponents_left;
    BYTE* opponents_right;
    BYTE* races_label;
    BYTE* races;
    BYTE* opponents;
    BOOL locked;
    DWORD round;
    char text[0x20];
    int i;

    game_FUN_0046a880(walk, NULL, FIELD(self, 0x7F3C, void*), 1, UICONTROL_CLASS);
    for (control = game_FUN_0046a980(walk); control; control = game_FUN_0046a980(walk))
        control_enable(control, open);
    dialog_enable_controls(self, NULL, 0x141, open);
    control_show(FIELD(dialog_find_control(self, NULL, SP_EVENT_TYPE, 6), COMBO_BUTTON, BYTE*),
                 NULL, open, TRUE);
    control_show(FIELD(dialog_find_control(self, NULL, SP_RACE_MODE, 6), COMBO_BUTTON, BYTE*),
                 NULL, open, TRUE);
    if (!open && net_hosting && !(FIELD(game_main, 0x214C, BYTE) & 2)) {
        control_enable(dialog_find_control(self, NULL, LIST_TEXT_BOX, 6), TRUE);
        control = dialog_find_control(self, NULL, SP_TRACK_LEFT, 0);
        control_enable(control, TRUE);
        control_show(control, NULL, TRUE, TRUE);
        control = dialog_find_control(self, NULL, SP_TRACK_RIGHT, 0);
        control_enable(control, TRUE);
        control_show(control, NULL, TRUE, TRUE);
    }

    i = (int) list_data(combo_list(self, SP_RACE_MODE), NULL, -1);
    laps_box = dialog_find_control(self, NULL, SP_LAPS_BOX, 5);
    races_box = dialog_find_control(self, NULL, SP_RACES_BOX, 5);
    if (i >= 0 && i <= 2) {
        control_show(laps_box, NULL, i != 0, TRUE);
        control_show(races_box, NULL, i == 2, TRUE);
    }

    races_left = dialog_find_control(self, NULL, SP_RACES_LEFT, 9);
    races_right = dialog_find_control(self, NULL, SP_RACES_RIGHT, 10);
    opponents_left = dialog_find_control(self, NULL, SP_OPPONENTS_LEFT, 0);
    opponents_right = dialog_find_control(self, NULL, SP_OPPONENTS_RIGHT, 0);
    races_label = dialog_find_control(self, NULL, SP_RACES_LABEL, 12);
    races = dialog_find_control(self, NULL, SP_RACES, 3);
    opponents = dialog_find_control(self, NULL, SP_OPPONENTS, 3);
    round = net_hosting && FIELD(races, 0x1EC, int) ? list_data(races, NULL, -1)
                                                      : FIELD(game_main, 0x2D7C, DWORD);
    list_clear(races);
    for (i = 3; i <= 7; i += 2) {
        game_FUN_0053a11f(text, ONE_NUMBER, i);
        list_add_text(races, NULL, text, i - 1, 0);    /* "3" holds 2, as the original has it */
    }
    list_select_data(races, NULL, round);

    locked = race_mode == SERIES && series_started;
    if (net_hosting) {
        control_show(opponents_left, NULL, !locked, TRUE);
        control_show(opponents_right, NULL, !locked, TRUE);
        control_enable(opponents, !locked);
        control_show(races_left, NULL, !locked, TRUE);
        control_show(races_right, NULL, !locked, TRUE);
    }
    control_load_text(races_label, NULL, game_language, locked ? 0x92B : 0x92A);
    if (locked) {
        game_FUN_0053a11f(text, RACE_OF, FIELD(race_events, 0x48, BYTE) + 1, race_round + 1);
        list_clear(races);
        list_add_text(races, NULL, text, 0, 0);
    }
    game_FUN_0046a970(walk);
}

/*
    The kind picked in EventTypeDropDown becomes the race's: tag (4) shows its two checks for
    the opponents, the laps list holds laps (2 or 1 to 20) or times (2 to 30 minutes), and the
    host lists the kind's tracks (0 adds 5's; 4 adds 1, 5 and 0), the one last raced picked.
*/
PORT(0x004FBC30)
void __fastcall mp_event_kind_set(BYTE* self)
{
    BYTE* laps;
    BYTE* label;
    char text[0x100];
    int kind;
    int i;

    if (net_hosting)
        mp_track_state = 0;
    kind = (int) list_data(combo_list(self, SP_EVENT_TYPE), NULL, -1);
    race_kind = kind;
    dialog_find_control(self, NULL, SP_OPPONENTS, 3);          /* looked up, and not used */
    laps = dialog_find_control(self, NULL, SP_LAPS, 3);
    dialog_find_control(self, NULL, SP_RACES, 3);              /* the same */
    label = dialog_find_control(self, NULL, SP_LAPS_LABEL, 12);
    control_load_text(label, NULL, game_language, 0x929);
    control_show(dialog_find_control(self, NULL, SP_OPPONENTS_BOX, 5), NULL, kind != 4, TRUE);
    tag_stunt_show(self, kind == 4);
    if (event_timed(kind)) {
        control_load_text(label, NULL, game_language, 0x92F);
        list_clear(laps);
        for (i = 2; i <= 30; i++) {
            game_FUN_0051dcc0(text, (float) i * 60.0f);
            list_add_text(laps, NULL, text, i, 0);
        }
        list_select_data(laps, NULL, (DWORD) (int) FIELD(race_settings, SET_TIME, float));
    } else {
        numbers_fill(laps, ONE_NUMBER, kind == 2 || kind == 3 ? 2 : 1, 20);
        list_select_data(laps, NULL, FIELD(race_settings, SET_LAPS, DWORD));
    }

    if (!net_hosting) {
        track_show_hosts(self);
    } else {
        if (kind == 0) {
            profile_set_event(game_profile, NULL, 0);
            tracks_list(self, 0);
            profile_set_event(game_profile, NULL, 5);
            tracks_list(self, 1);
        } else if (kind == 4) {
            profile_set_event(game_profile, NULL, 4);
            tracks_list(self, 0);
            profile_set_event(game_profile, NULL, 1);
            tracks_list(self, 1);
            profile_set_event(game_profile, NULL, 5);
            tracks_list(self, 1);
            profile_set_event(game_profile, NULL, 0);
            tracks_list(self, 1);
        } else {
            profile_set_event(game_profile, NULL, kind);
            tracks_list(self, 0);
        }
        list_select_text(combo_list(self, LIST_TEXT_BOX), NULL,
                         (char*) game_main + kind * 0x80 + 0xC6C);
    }
    profile_set_event(game_profile, NULL, FIELD(race_settings, SET_TRACK_ID, DWORD));
}

/* Slot 31, host only: the page into the settings (`out`), or back from them. */
PORT(0x004FBFE0)
void __fastcall mp_event_sync(BYTE* self, void* edx_unused, DWORD out)
{
    BYTE* settings = race_settings;
    BYTE* list;

    (void) edx_unused;
    if (!game_net || !net_hosting)
        return;
    if (out) {
        BYTE* track;

        list = dialog_find_control(self, NULL, SP_LAPS, 3);
        if (!event_timed(FIELD(settings, SET_EVENT, int)))
            FIELD(settings, SET_LAPS, DWORD) = list_data(list, NULL, -1);
        else
            FIELD(settings, SET_TIME, float) = (float) (int) list_data(list, NULL, -1);
        if (!series_started)
            FIELD(settings, SET_RACES, DWORD) =
                list_data(dialog_find_control(self, NULL, SP_RACES, 3), NULL, -1);

        text_copy((char*) settings + SET_TRACK, NO_TEXT);
        list = combo_list(self, LIST_TEXT_BOX);
        track = FIELD(game_session, 0x60, BYTE*) + list_data(list, NULL, -1) * TRACK_ENTRY;
        text_copy_most((char*) settings + SET_TRACK, (const char*) track + 0x14, 0xFF);
        profile_set_event(game_profile, NULL, FIELD(track, 0x8, DWORD));
        FIELD(settings, SET_TRACK_KIND, BYTE) = FIELD(track, 0x4, BYTE);
        FIELD(settings, SET_TRACK_138, DWORD) = FIELD(track, 0xC, DWORD);
        FIELD(settings, SET_TRACK_CHECK, DWORD) = FIELD(track, 0x10, DWORD);
        text_copy((char*) game_main + race_kind * 0x80 + 0xC6C, list_text(list, NULL, -1));
        FIELD(game_main, 0xF6C + race_kind * 4, DWORD) = FIELD(settings, SET_TRACK_KIND, BYTE);

        FIELD(settings, SET_EVENT, DWORD) = list_data(combo_list(self, SP_EVENT_TYPE), NULL, -1);
        FIELD(settings, SET_MODE, DWORD) = list_data(combo_list(self, SP_RACE_MODE), NULL, -1);
        list = dialog_find_control(self, NULL, SP_OPPONENTS, 3);
        race_computers = FIELD(settings, SET_EVENT, int) != 4 ? (int) list_data(list, NULL, -1) : 0;
    } else {
        list_select_data(dialog_find_control(self, NULL, SP_RACES, 3), NULL,
                         FIELD(settings, SET_RACES, DWORD));
        list = dialog_find_control(self, NULL, SP_LAPS, 3);
        if (!event_timed(FIELD(settings, SET_EVENT, int)))
            list_select_data(list, NULL, FIELD(settings, SET_LAPS, DWORD));
        else
            list_select_data(list, NULL, (DWORD) (int) FIELD(settings, SET_TIME, float));
        list_select_data(combo_list(self, SP_EVENT_TYPE), NULL, race_kind);
        list_select_data(combo_list(self, SP_RACE_MODE), NULL, race_mode);
        if (FIELD(game_net, 0x14, int) && FIELD(self, EVENT_FILLED, int)) {
            int i;

            list = combo_list(self, LIST_TEXT_BOX);
            for (i = 0; i < FIELD(list, 0x1EC, int); i++) {
                int track = (int) list_data(list, NULL, i);

                if (!game_FUN_0053b6e0(FIELD(game_session, 0x60, char*)
                                       + track * TRACK_ENTRY + 0x14,
                                       (const char*) settings + SET_TRACK)) {
                    list_select_data(list, NULL, track);
                    break;
                }
            }
        }
    }
}

/* PictureBox: the race track's picture from its folder. */
PORT(0x004FC3A0)
void __fastcall mp_track_picture(BYTE* self)
{
    BYTE* list = dialog_find_control(self, NULL, SP_PICTURE, 3);
    char folder[0x104];

    list_clear(list);
    profile_find_named_file(game_profile, NULL, FIELD(game_main, 0xC18, const char*),
                      (const char*) race_settings + SET_TRACK, ENV, folder);
    track_picture_add(self, NULL, list, folder, (const char*) race_settings + SET_TRACK);
}

/* "ui\unarts.tga", wherever it is found: the picture of a track that has none. */
static void picture_add_unknown(BYTE* list)
{
    char path[0x104];
    char found[0x104];

    game_FUN_0053a11f(path, NAME(0x0057A000), NAME(0x0056FE48));        /* "%s\unarts.tga", "ui" */
    VIRTUAL(game_main, 0x48, game_find_file)(game_main, NULL, path, found);
    list_add_file(list, NULL, found, 0, TRUE, 0);
}

/*
    A track's picture: "<track>NNs.tga" with the track's variant for kinds 1 and 5,
    "<track>01s.tga" for profile kinds 1 and 5, else "<track>s.tga"; from the resources, else
    found on disk, else the unknown art.
*/
PORT(0x004FC410)
void __fastcall track_picture_add(BYTE* self, void* edx_unused, BYTE* list, const char* folder,
                                  const char* track)
{
    char picture[0x104];
    char path[0x104];
    char found[0x104];
    void* file;

    (void) self;
    (void) edx_unused;
    if (!list)
        return;
    if (!*folder) {
        picture_add_unknown(list);
        return;
    }
    file = game_new(0x134, SGP_CPP, 0xA2E);
    if (file)
        file = game_FUN_00461c80(file, NULL, game_resources);
    if (race_kind == 1 || race_kind == 5)
        game_FUN_0053a11f(picture, NAME(0x00579D50), track,                 /* "%s%02ds.tga" */
                          FIELD(race_settings, SET_TRACK_KIND, BYTE));
    else if (profile_event(game_profile) == 1 || profile_event(game_profile) == 5)
        game_FUN_0053a11f(picture, NAME(0x00579D68), track);                /* "%s01s.tga" */
    else
        game_FUN_0053a11f(picture, NAME(0x00579D80), track);                /* "%ss.tga" */
    game_FUN_0053a11f(path, NAME(0x0056C7DC), folder, picture);             /* "%s\%s" */
    if (game_FUN_004eea60(FIELD(game_main, 0x574, BYTE*), NULL, file, path, NAME(0x0056C66C),
                          picture))                                         /* "rb" */
        list_add_file(list, NULL, picture, 0, TRUE, 0);
    else if (VIRTUAL(game_main, 0x48, game_find_file)(game_main, NULL, path, found))
        list_add_file(list, NULL, found, 0, TRUE, 0);
    else
        picture_add_unknown(list);
    if (file) {
        game_FUN_00461cd0(file);
        game_delete(file);
    }
}

#define LARGEST_FORMAT 0x146A       /* "%dcc"-like: a size's entry in LargestOpponentDropDown */

__declspec(noinline) void __fastcall mp_bike_choose(BYTE* self);
__declspec(noinline) void __fastcall mp_choices_fill(BYTE* self);
__declspec(noinline) void __fastcall mp_largest_enable(BYTE* self);

/* The bike page's rider was picked again: the next draw dresses the model. */
PORT(0x004FDAF0)
void __fastcall rider_mark_changed(BYTE* self)
{
    FIELD(self, BR_RIDER_CHANGED, int) = 1;
}

static void mp_options_open(BYTE* self)
{
    BYTE* options = game_new(0x7F70, SGP_CPP, 0xAD5);

    if (options) {
        dialog_construct(options, NULL, 1, OPT_BASE);
        *(void**) options = (void*) 0x00556BD0;
    }
    gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL, options, 0, 2, 0,
                     FIELD(self, 0x2C, BYTE*), 2, 0, 1);
}

static void mp_bike_rider_open(BYTE* self)
{
    vector3* position = &FIELD(self, BR_POSITION, vector3);
    vector3* target = &FIELD(self, BR_TARGET, vector3);
    RECT* view = &FIELD(self, BR_VIEW, RECT);
    float fov = 50.0f;
    double dx, dy, dz;
    float squared;
    char format[0x80];
    char text[0x80];
    BYTE* list;
    BYTE* camera;
    int i;

    FIELD(self, BR_MANUAL, int) = 0;
    FIELD(game_main, 0x614, DWORD) = 0;
    FIELD(self, BR_RIDER_CHANGED, int) = 1;
    FIELD(self, BR_BIKE_CHANGED, int) = 1;
    mp_choices_fill(self);

    list = combo_list(self, LIST_ENGINE_SIZE);
    list_clear(list);
    list_add_text(list, NULL, NAME(0x0056FC28), 0, 0);                     /* "125cc 2-stroke" */
    list_add_text(list, NULL, NAME(0x0056FC18), engine_class(engine_sizes[1]), 0);  /* 250cc */
    list_add_text(list, NULL, NAME(0x0056FC08), engine_class(engine_sizes[2]), 0);  /* 400cc */
    list_add_text(list, NULL, NAME(0x0056FBF8), engine_class(engine_sizes[3]), 0);  /* 500cc */
    list_add_text(list, NULL, NAME(0x0056FBE8), engine_class(engine_sizes[4]), 0);  /* 600cc */
    list_select_data(list, NULL, engine_class(FIELD(game_main, 0x1550, int)));

    game_FUN_00526fc0(game_main, NULL, LARGEST_FORMAT, format, sizeof format);
    list = combo_list(self, LIST_LARGEST);
    list_clear(list);
    for (i = 0; i < 5; i++) {
        game_FUN_0053a11f(text, format, engine_sizes[i]);
        list_add_text(list, NULL, text, i, 0);
    }
    list_select_all(list, NULL, mp_largest);
    edit_set_allowed(dialog_find_control(self, NULL, EDIT_PLATE, 11), NULL, DIGITS);
    session_show_scene(game_session, NULL, self);

    view->left = 27;
    view->right = 312;
    view->top = 105;
    view->bottom = 246;
#ifdef OPENMCM2_DEV
    menu_place_view(self, view);         /* where the scaled menus put it */
#endif
    *position = mp_vector_zero;
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
    squared = (float) (dx * dx + dy * dy + dz * dz);
    FIELD(self, BR_DISTANCE, float) = x87_equal(squared, 1.0f) ? 1.0f : game_FUN_00461ac0(squared);

    rider_mark_changed(self);
    mp_bike_choose(self);
    mp_players_set(self, NULL, FIELD(game_main, 0x2144, DWORD));
    game_FUN_0053a27e(game_FUN_004c41d0());
    if (FIELD(self, 0x110, BYTE*))
        FIELD(self, BR_SLOT, int) = background_new_slot(FIELD(self, 0x110, BYTE*), NULL, 1);

    /* The host's rule 4, or being ready, fixes the bike. */
    if ((FIELD(game_main, 0x214C, BYTE) & 4) || mp_user_ready) {
        control_enable(dialog_find_control(self, NULL, BUTTON_WRENCH, 0), FALSE);
        control_enable(dialog_find_control(self, NULL, LIST_BIKES, 6), FALSE);
        control_show(dialog_find_control(self, NULL, NAME(0x0056FBB4), 0), NULL, FALSE, TRUE);
        control_show(dialog_find_control(self, NULL, NAME(0x0056FBA8), 0), NULL, FALSE, TRUE);
        control_enable(dialog_find_control(self, NULL, LIST_ENGINE_SIZE, 6), FALSE);
    }
    mp_largest_enable(self);
}

PORT(0x004FC670)
void __fastcall mp_bike_rider_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    char number[12];
    int plate;

    (void) edx_unused;
    switch (event->type) {
    case EVENT_OPEN:
        mp_bike_rider_open(self);
        break;
    case EVENT_SELECT:
        if (clicked(event, LIST_BIKES)) {
            mp_bike_choose(self);
            dialog_notify_controls(self, NULL, 1);
        } else if (clicked(event, LIST_RIDERS)) {
            rider_mark_changed(self);
            dialog_notify_controls(self, NULL, 1);
        } else if (clicked(event, LIST_ENGINE_SIZE)) {
            mp_bike_choose(self);
        } else if (clicked(event, LIST_LARGEST) && net_hosting) {
            mp_largest = (int) list_data((BYTE*) event->c, NULL, -1);
        }
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
        else if (clicked(event, BUTTON_WRENCH))
            mp_options_open(self);
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
        mp_players_set(self, NULL, plate);
        break;
    case EVENT_EDIT_CHANGED:
        if (!clicked(event, EDIT_PLATE))
            break;
        edit_get_text((BYTE*) event->c, NULL, number, 9);
        plate = game_FUN_0053b6c7(number);
        if (plate >= 100 && plate <= 999) {
            mp_players_set(self, NULL, plate);
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

/*
    A bike the host's rules allow (the player's own never are): with 4, of the maker required
    (0x3410's 0x298; 0 means maker 7) and at most class 2, and with 8 too only the bike chosen;
    8 alone allows none; else at most class 2.
*/
static BOOL bike_allowed(const BYTE* bike, int index, DWORD rules)
{
    if (rules & 4) {
        int maker = FIELD(FIELD(game_main, 0x3410, BYTE*), 0x298, int);

        if (maker != FIELD(bike, 0, int) && (maker || FIELD(bike, 0, int) != 7))
            return FALSE;
        if (engine_class(FIELD(bike, 0x8C, int)) > 2)
            return FALSE;
        return !(rules & 8) || index == FIELD(game_main, 0x1FB0, int);
    }
    if (rules & 8)
        return FALSE;
    return engine_class(FIELD(bike, 0x8C, int)) <= 2;
}

/* DDLBikes and DDLRiders, as the single player's but within the host's rules. */
PORT(0x004FCFF0)
void __fastcall mp_choices_fill(BYTE* self)
{
    BYTE* list = combo_list(self, LIST_BIKES);
    int bike_chosen = 0;
    int rider_chosen = 0;
    char label[0x80];
    int i;

    list_clear(list);
    for (i = 0; i < FIELD(game_session, 0x54, int); i++) {
        BYTE* bike = FIELD(game_session, 0x50, BYTE*) + i * BIKE_SIZE;
        BYTE* maker = session_players + FIELD(bike, 0, int) * RIDER_SIZE;
        DWORD rules;

        if (!FIELD(bike, 0x88, DWORD) && !maker_rideable(FIELD(maker, 0xC4, int)))
            continue;
        rules = FIELD(game_main, 0x214C, DWORD);
        if (rules && (FIELD(bike, 0x88, DWORD) || !bike_allowed(bike, i, rules)))
            continue;
        game_FUN_0053a11f(label, TWO_WORDS, (const char*) maker, (const char*) bike + 4);
        list_add_text(list, NULL, label, i, 0);
        if (FIELD(game_main, 0x1FB0, int) == i)
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

        if (rider_offered(rider)) {
            list_add_text(list, NULL, (const char*) rider, i, 0);
            rider = FIELD(game_session, 0x58, BYTE*) + i * RIDER_SIZE;
            if (!game__strcmp((const char*) rider + 0x40, (const char*) game_main + 0x1F6C))
                rider_chosen = i;
        }
    }
    list_select_data(list, NULL, rider_chosen);
}

/* Each player's model is given `value`, under one scope on the display. */
PORT(0x004FD340)
void __fastcall mp_players_set(BYTE* self, void* edx_unused, DWORD value)
{
    DWORD scope[11];
    int i;

    (void) self;
    (void) edx_unused;
    game_FUN_00418300(scope, NULL, FIELD(game_main, 0x1C, BYTE*));
    for (i = 0; i < FIELD(game_session, 0x4C, int); i++)
        game_FUN_00418470(scope, NULL, player_model(session_players + i * RIDER_SIZE), value);
    game_FUN_00418370(scope);
}

/* Slot 24: 0xF (with b) checks the largest engine allowed; both it and 0x101 re-enable its list. */
PORT(0x004FD3F0)
int __fastcall mp_bike_rider_message(BYTE* self, void* edx_unused, DWORD message, DWORD a,
                                     DWORD b, DWORD c, DWORD d)
{
    (void) edx_unused;
    (void) a;
    (void) c;
    (void) d;
    if (message != 0x101) {
        if (!b || message != 0xF)
            return 0;
        mp_largest_check(self);
    }
    mp_largest_enable(self);
    return 0;
}

/* A client: the host's largest engine shows, and a bigger bike cancels Ready. */
PORT(0x004FD420)
void __fastcall mp_largest_check(BYTE* self)
{
    if (net_hosting)
        return;
    list_select_data(combo_list(self, LIST_LARGEST), NULL, mp_largest);
    if (engine_class(FIELD(game_main, 0x1550, int)) > mp_largest && mp_user_ready)
        mp_ready_cancel(FIELD(self, 0x2C, BYTE*), NULL, 1);
}

/* LargestOpponentDropDown is the host's to set. */
PORT(0x004FD4D0)
void __fastcall mp_largest_enable(BYTE* self)
{
    BOOL host = net_hosting != 0;

    control_enable(dialog_find_control(self, NULL, LIST_LARGEST, 0), host);
    control_show(FIELD(dialog_find_control(self, NULL, LIST_LARGEST, 6), COMBO_BUTTON, BYTE*),
                 NULL, host, TRUE);
}

/* Slot 23: the first mouse button pressed inside the view (inset 50 by 30) turns it by hand. */
PORT(0x004FD550)
int __fastcall mp_preview_press(BYTE* self, void* edx_unused, const DWORD* key, DWORD arg)
{
    (void) edx_unused;
    if (key[1] == 1 && key[0] == 0) {
        const RECT* view = &FIELD(self, BR_VIEW, RECT);
        gui_input* input = FIELD(self, 0x34, gui_user*)->input;    /* not checked, as original */
        RECT area;
        POINT at;

        area.left = view->left + 50;
        area.top = view->top + 30;
        area.right = view->right - 50;
        area.bottom = view->bottom - 30;
        at.x = input->x;
        at.y = input->y;
        if (PtInRect(&area, at)) {
            FIELD(self, BR_MANUAL, int) = 1;
            FIELD(self, BR_HELD, int) = 1;
        }
    }
    return dialog_press(self, NULL, key, arg);
}

/* Turn the camera about the session's point by `angle` (this file's own y axis). */
static void mp_preview_orbit(BYTE* self, float angle)
{
    vector3* position = &FIELD(self, BR_POSITION, vector3);
    float distance;
    vector3 offset;
    vector3 turned;
    BYTE* camera;

    offset.x = position->x - session_centre.x;
    offset.y = position->y - session_centre.y;
    offset.z = position->z - session_centre.z;
    D3DRMVectorRotate(&turned, &offset, &mp_vector_y, angle);
    distance = FIELD(self, BR_DISTANCE, float);
    turned.x = turned.x * distance;
    turned.y = turned.y * distance;
    turned.z = turned.z * distance;
    position->x = turned.x + session_centre.x;
    position->y = turned.y + session_centre.y;
    position->z = turned.z + session_centre.z;
    game_FUN_0042f8d0(session_camera, NULL, position, NULL, NULL, NULL, NULL);
    camera = session_camera;
    VIRTUAL(camera, 0x74, camera_look_at)(camera, NULL, FIELD(self, BR_TARGET, vector3));
}

/*
    Slot 10, as the single player's preview tick, but dragging leaves no checkbox to clear,
    and the idling bike plays WaitB with the rider's WaitR.
*/
PORT(0x004FD5F0)
int __fastcall mp_preview_tick(BYTE* self, void* edx_unused, float seconds)
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
        mp_preview_orbit(self, across * TURN_PER_PIXEL);
        FIELD(self, BR_MANUAL, int) = 1;
    } else if (!FIELD(self, BR_MANUAL, int)) {
        mp_preview_orbit(self, seconds * TURN_PER_SECOND);
    }
#ifdef OPENMCM2_DEV
    if (preview_zoom(self))
        mp_preview_orbit(self, 0.0f);
#endif

    model = session_rider;
    VIRTUAL(model, 0x1C, model_step)(model, NULL, seconds, 0, 0);
    model = session_bike;
    VIRTUAL(model, 0x1C, model_step)(model, NULL, seconds, 0, 0);

    if (FIELD(self, BR_IDLING, int) && game__rand() % 200 == 1) {
        list_data(combo_list(self, LIST_BIKES), NULL, -1);     /* read and dropped, as original */
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
        BYTE* maker = session_players + FIELD(bike, 0, int) * RIDER_SIZE;

        game_FUN_004aa170(FIELD(maker, 0xC0, BYTE*), NULL, NAME(0x00573720));  /* "WaitB" */
        game_FUN_004aa170(session_rider, NULL, NAME(0x0056FC58));              /* "WaitR" */
        FIELD(self, BR_IDLING, int) = 1;
    }
    if (FIELD(self, 0x110, BYTE*))
        background_slot_redraw(FIELD(self, 0x110, BYTE*), NULL, FIELD(self, BR_SLOT, int),
                               &FIELD(self, BR_VIEW, RECT));
    return dialog_tick(self, NULL, seconds);
}

/*
    The bike picked in DDLBikes, as the single player's, but the class's handling is always
    applied, an own bike's size index is taken as its class and written back to the bike, and
    being ready locks DDLEngineSize when it appears; a plate the host fixed (1 to 100) wins.
*/
PORT(0x004FDB00)
void __fastcall mp_bike_choose(BYTE* self)
{
    BYTE* list = combo_list(self, LIST_BIKES);
    BYTE* bike = FIELD(game_session, 0x50, BYTE*) + list_data(list, NULL, -1) * BIKE_SIZE;
    BYTE* maker = session_players + FIELD(bike, 0, int) * RIDER_SIZE;
    BYTE* engine;
    BYTE* edit;
    char number[12];
    int i;

    for (i = 0; i < FIELD(game_session, 0x4C, int); i++)
        model_show(session_players + i * RIDER_SIZE, FALSE);
    model_show(maker, TRUE);
    FIELD(self, BR_BIKE_CHANGED, int) = 1;

    engine = dialog_find_control(self, NULL, LIST_ENGINE_SIZE, 0);
    if (FIELD(bike, 0x88, DWORD)) {
        if (!FIELD(engine, 0x70, DWORD)) {
            engine_size_show(self, engine, TRUE);
            if (mp_user_ready)
                control_enable(engine, FALSE);
        }
    } else if (FIELD(engine, 0x70, DWORD)) {
        engine_size_show(self, engine, FALSE);
    }

    FIELD(game_main, 0x1FB0, DWORD) = list_data(list, NULL, -1);
    if (FIELD(bike, 0x88, DWORD)) {
        int size = (int) list_data(combo_list(self, LIST_ENGINE_SIZE), NULL, -1);

        FIELD(game_main, 0x1550, int) = engine_sizes[size];
        FIELD(game_main, 0x1554, DWORD) = size == 2 || size == 4;       /* the 4-strokes */
        engine_class_apply(size);
        bike = FIELD(game_session, 0x50, BYTE*) + FIELD(game_main, 0x1FB0, int) * BIKE_SIZE;
        FIELD(bike, 0x8C, DWORD) = FIELD(game_main, 0x1550, DWORD);
        bike = FIELD(game_session, 0x50, BYTE*) + FIELD(game_main, 0x1FB0, int) * BIKE_SIZE;
        FIELD(bike, 0x90, DWORD) = FIELD(game_main, 0x1554, DWORD);
    } else {
        bike = FIELD(game_session, 0x50, BYTE*) + list_data(list, NULL, -1) * BIKE_SIZE;
        FIELD(game_main, 0x1550, DWORD) = FIELD(bike, 0x8C, DWORD);
        FIELD(game_main, 0x1554, DWORD) = FIELD(bike, 0x90, DWORD);
        engine_class_apply(engine_class(FIELD(game_main, 0x1550, int)));
    }

    edit = dialog_find_control(self, NULL, EDIT_PLATE, 11);
    if (FIELD(game_main, 0x2148, int) > 0 && FIELD(game_main, 0x2148, int) < 101) {
        FIELD(game_main, 0x2144, int) = FIELD(game_main, 0x2148, int);
        game_FUN_0053a11f(number, ONE_NUMBER, FIELD(game_main, 0x2144, int));
    } else {
        game_FUN_0054aaa5(FIELD(game_main, 0x2144, int), number, 10);
    }
    edit_set_text(edit, NULL, number);
}

/* Slot 31, out only: the bike's maker and model and the rider, each at most 63 characters. */
PORT(0x004FDFC0)
void __fastcall mp_bike_rider_sync(BYTE* self, void* edx_unused, DWORD out)
{
    BYTE* bike;

    (void) edx_unused;
    if (!out)
        return;
    text_copy_most((char*) game_main + 0x1EEC, NO_TEXT, RIDER_TEXT_LENGTH);
    bike = FIELD(game_session, 0x50, BYTE*)
           + list_data(combo_list(self, LIST_BIKES), NULL, -1) * BIKE_SIZE;
    text_copy_most((char*) game_main + 0x1EEC,
                   (const char*) session_players + FIELD(bike, 0, int) * RIDER_SIZE + 0x40,
                   RIDER_TEXT_LENGTH);
    text_copy_most((char*) game_main + 0x1F2C, (const char*) bike + 0x48, RIDER_TEXT_LENGTH);
    text_copy_most((char*) game_main + 0x1F6C, NO_TEXT, RIDER_TEXT_LENGTH);
    text_copy_most((char*) game_main + 0x1F6C,
                   FIELD(game_session, 0x58, char*)
                   + list_data(combo_list(self, LIST_RIDERS), NULL, -1) * RIDER_SIZE + 0x40,
                   RIDER_TEXT_LENGTH);
}

__declspec(noinline) void __fastcall mp_options_refresh(BYTE* self);

PORT(0x004FE1A0)
void __fastcall mp_options_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    char text[0x80];
    BYTE* edit;

    (void) edx_unused;
    switch (event->type) {
    case EVENT_CLICK:
        /* Slot 59: ChkRecordRace's state goes out to its value at once. */
        if (clicked(event, SP_RECORD))
            VIRTUAL((BYTE*) event->c, 0xEC, method_1)((BYTE*) event->c, NULL, TRUE);
        break;
    case EVENT_OPEN:
        control_bind(dialog_find_control(self, NULL, CHECK_FAST, 0), game_main + 0x2D88);
        control_bind(dialog_find_control(self, NULL, SP_TREES, 0), game_main + 0x2D80);
        control_bind(dialog_find_control(self, NULL, CHECK_RIDERS, 0), game_main + 0x2D84);
        control_bind(dialog_find_control(self, NULL, SP_LOD_EASY, 0), game_main + 0x60C);
        control_bind(dialog_find_control(self, NULL, SP_RECORD, 0), game_main + 0x3334);
        edit_set_allowed(dialog_find_control(self, NULL, EDIT_MAX_AI, 11), NULL, DIGITS);
        edit = dialog_find_control(self, NULL, EDIT_MAX_AI, 11);
        edit_set_text(edit, NULL, game_FUN_0054aaa5(mp_max_ai, text, 10));
        dialog_notify_controls(self, NULL, 0);
        mp_options_refresh(self);
        break;
    case EVENT_CLOSE:
        edit = dialog_find_control(self, NULL, EDIT_MAX_AI, 11);
        mp_max_ai = game_FUN_0053b6c7(edit_get_text(edit, NULL, text, sizeof text));
        break;
    }
}

/* Slot 24: 0xF (with b) reloads the options from the settings. */
PORT(0x004FE360)
int __fastcall mp_options_message(BYTE* self, void* edx_unused, DWORD message, DWORD a, DWORD b,
                                  DWORD c, DWORD d)
{
    (void) edx_unused;
    (void) a;
    (void) c;
    (void) d;
    if (b && message == 0xF) {
        mp_options_load(self);
        mp_options_refresh(self);
    }
    return 0;
}

PORT(0x004FE390)
void __fastcall mp_options_load(BYTE* self)
{
    dialog_notify_controls(self, NULL, 0);
}

/* The options (id 0x3F3) are a host's without rules; tag races are never recorded. */
PORT(0x004FE3A0)
void __fastcall mp_options_refresh(BYTE* self)
{
    dialog_set_controls(self, NULL, 0x3F3, net_hosting && !mp_rules);
    control_enable(dialog_find_control(self, NULL, SP_RECORD, 0), race_kind != 4);
}

/* The file's own unit vectors, set up by the runtime before main. */
__declspec(noinline) void __cdecl init_mp_vector_zero(void);
__declspec(noinline) void __cdecl init_mp_vector_x(void);
__declspec(noinline) void __cdecl init_mp_vector_y(void);
__declspec(noinline) void __cdecl init_mp_vector_z(void);

PORT(0x004FE420)
void __cdecl init_mp_vector_zero(void)
{
    set_vector(&mp_vector_zero, 0.0f, 0.0f, 0.0f);
}

PORT(0x004FE470)
void __cdecl init_mp_vector_x(void)
{
    set_vector(&mp_vector_x, 1.0f, 0.0f, 0.0f);
}

PORT(0x004FE4C0)
void __cdecl init_mp_vector_y(void)
{
    set_vector(&mp_vector_y, 0.0f, 1.0f, 0.0f);
}

PORT(0x004FE510)
void __cdecl init_mp_vector_z(void)
{
    set_vector(&mp_vector_z, 0.0f, 0.0f, 1.0f);
}

/* The runtime's initializer table calls these, which only jump on. */
PORT(0x004FE410)
void __cdecl init_mp_vector_zero_entry(void)
{
    init_mp_vector_zero();
}

PORT(0x004FE460)
void __cdecl init_mp_vector_x_entry(void)
{
    init_mp_vector_x();
}

PORT(0x004FE4B0)
void __cdecl init_mp_vector_y_entry(void)
{
    init_mp_vector_y();
}

PORT(0x004FE500)
void __cdecl init_mp_vector_z_entry(void)
{
    init_mp_vector_z();
}
