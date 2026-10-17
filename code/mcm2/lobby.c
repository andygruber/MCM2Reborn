/*
    OpenMCM2 - the replay tape's header and the Internet Gaming Zone lobby, ported from the game.

    The original's source file names no file of its own (it has no __FILE__ string); it follows
    krustyui.cpp. It holds the header the game writes at the start of a replay ("MCMVCR"), and
    the object at game_main + 0x3410 that reads the event, track and bike a Zone lobby launched
    the game with, and sends the race's results back. Each function here replaces the game's at
    the address in its PORT line, and was checked against the original's instructions.
*/

#include "gui.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

#define NAME(address) ((const char*) (address))

#define race_settings (game_main + 0x2D70)
#define race_kind     FIELD(game_main, 0x2D74, int)
#define race_laps     FIELD(game_main, 0x2D90, int)
#define race_variant  FIELD(game_main, 0x2DA4, BYTE)
#define race_track    ((char*) game_main + 0x2DA6)
#define game_net      FIELD(game_main, 0x8, BYTE*)
#define game_profile  ((char*) game_main + 0x578)

/* ---- Start-up ---- */

/* The file's copies of the header's unit vectors, set up by the runtime before main. */
#define lobby_zero (*(vector3*) 0x00682560)
#define lobby_x    (*(vector3*) 0x00682570)
#define lobby_y    (*(vector3*) 0x006825A0)
#define lobby_z    (*(vector3*) 0x00682540)

/* Out of line, so the entries below stay jumps to them as in the original. */
__declspec(noinline) void __cdecl lobby_init_zero(void);
__declspec(noinline) void __cdecl lobby_init_x(void);
__declspec(noinline) void __cdecl lobby_init_y(void);
__declspec(noinline) void __cdecl lobby_init_z(void);

PORT(0x0049D380)
void __cdecl lobby_init_zero_entry(void) { lobby_init_zero(); }
PORT(0x0049D390)
void __cdecl lobby_init_zero(void) { set_vector(&lobby_zero, 0.0f, 0.0f, 0.0f); }
PORT(0x0049D3D0)
void __cdecl lobby_init_x_entry(void) { lobby_init_x(); }
PORT(0x0049D3E0)
void __cdecl lobby_init_x(void) { set_vector(&lobby_x, 1.0f, 0.0f, 0.0f); }
PORT(0x0049D420)
void __cdecl lobby_init_y_entry(void) { lobby_init_y(); }
PORT(0x0049D430)
void __cdecl lobby_init_y(void) { set_vector(&lobby_y, 0.0f, 1.0f, 0.0f); }
PORT(0x0049D470)
void __cdecl lobby_init_z_entry(void) { lobby_init_z(); }
PORT(0x0049D480)
void __cdecl lobby_init_z(void) { set_vector(&lobby_z, 0.0f, 0.0f, 1.0f); }

/* ---- The replay tape's header ---- */

/*
    A recorder keeps the header it writes (0xEB0 bytes from 0xDC): the tape's mark and version,
    its name and length, the race settings, and a block per rider.
*/
#define VCR_TIME        0xD8    /* float */
#define VCR_HEADER      0xDC
#define VCR_HEADER_SIZE 0xEB0
#define VCR_MARK        0xDC    /* char[8] */
#define VCR_VERSION     0xE4    /* char[8] */
#define VCR_NAME        0xEC    /* char[0x20] */
#define VCR_LENGTH      0x10C   /* float */
#define VCR_SETTINGS    0x110   /* the race settings, 0x7B dwords */
#define VCR_RIDERS      0x300   /* 0x124 bytes each */
#define VCR_RIDER_SIZE  0x124

#define VCR_MARK_TEXT    NAME(0x00573B20)   /* "MCMVCR" */
#define VCR_VERSION_TEXT NAME(0x00573B18)   /* "2.0\x1A" */

/* The tape itself (VCRfile.cpp): opened for playing or recording, and finished. */
int __fastcall game_FUN_004ec820(BYTE* self, void* edx_unused, void* callback, void* header,
                                 DWORD size, BOOL playing, const char* file, DWORD a, DWORD b);
int __fastcall game_FUN_004ed460(BYTE* self, void* edx_unused, void* header, DWORD size,
                                 DWORD flag);

#define VCR_RIDERS_MOST 11      /* the header's room */

/*
    The header holds 11 riders. In the dev build a race may have more (riders.c): the riders past
    the 11th are not kept, and played back each takes the looks of one of the computers kept.
*/
static BYTE* vcr_rider(BYTE* self, int slot)
{
#ifdef OPENMCM2_DEV
    if (slot >= VCR_RIDERS_MOST)
        slot = 1 + (slot - 1) % (VCR_RIDERS_MOST - 1);
#endif
    return self + VCR_RIDERS + slot * VCR_RIDER_SIZE;
}

/* A tape opened on `file`; one being recorded gets a new header with the race's settings. */
PORT(0x0049D4C0)
int __fastcall vcr_open(BYTE* self, void* edx_unused, void* callback, BOOL playing,
                        const char* file, DWORD arg)
{
    (void) edx_unused;
    FIELD(self, VCR_TIME, DWORD) = 0;
    if (!playing) {
        text_copy_most((char*) self + VCR_MARK, VCR_MARK_TEXT, 7);
        text_copy_most((char*) self + VCR_VERSION, VCR_VERSION_TEXT, 7);
        FIELD(self, VCR_LENGTH, DWORD) = 0;
        __movsd((unsigned long*) (self + VCR_SETTINGS), (unsigned long*) race_settings, 0x7B);
    }
    return game_FUN_004ec820(self, NULL, callback, self + VCR_HEADER, VCR_HEADER_SIZE, playing,
                             file, 0x58, arg);
}

/* The time to play from; the second argument is ignored. Times are passed as their bits. */
PORT(0x0049D5A0)
void __fastcall vcr_set_time(BYTE* self, void* edx_unused, DWORD time, DWORD unused)
{
    (void) edx_unused;
    (void) unused;
    FIELD(self, VCR_TIME, DWORD) = time;
}

PORT(0x0049D5B0)
float __fastcall vcr_length(BYTE* self)
{
    return FIELD(self, VCR_LENGTH, float);
}

/* A recording finished: its name and length go in the header, and the header to the tape. */
PORT(0x0049D5C0)
int __fastcall vcr_finish(BYTE* self, void* edx_unused, DWORD length, const char* name,
                          DWORD flag)
{
    (void) edx_unused;
    text_copy_most((char*) self + VCR_NAME, name, 0x1F);
    FIELD(self, VCR_LENGTH, DWORD) = length;
    return game_FUN_004ed460(self, NULL, self + VCR_HEADER, VCR_HEADER_SIZE, flag);
}

/* A rider's block: an id and kind, a name of 15 characters, four texts of 63 and three values. */
PORT(0x0049D620)
void __fastcall vcr_set_rider(BYTE* self, void* edx_unused, DWORD id, BYTE kind, int slot,
                              const char* name, const char* text1, const char* text2,
                              const char* text3, const char* text4, DWORD a, DWORD b, DWORD c)
{
    BYTE* rider = vcr_rider(self, slot);

    (void) edx_unused;
#ifdef OPENMCM2_DEV
    if (slot >= VCR_RIDERS_MOST)
        return;
#endif
    FIELD(rider, 0x0, DWORD) = id;
    FIELD(rider, 0x4, BYTE) = kind;
    text_copy_most((char*) rider + 0x05, name, 0xF);
    text_copy_most((char*) rider + 0x15, text1, 0x3F);
    text_copy_most((char*) rider + 0x55, text2, 0x3F);
    text_copy_most((char*) rider + 0x95, text3, 0x3F);
    text_copy_most((char*) rider + 0xD5, text4, 0x3F);
    FIELD(rider, 0x118, DWORD) = a;
    FIELD(rider, 0x11C, DWORD) = b;
    FIELD(rider, 0x120, DWORD) = c;
}

PORT(0x0049D790)
void __fastcall vcr_get_rider(BYTE* self, void* edx_unused, DWORD* id, BYTE* kind, int slot,
                              char* name, char* text1, char* text2, char* text3, char* text4,
                              DWORD* a, DWORD* b, DWORD* c)
{
    BYTE* rider = vcr_rider(self, slot);

    (void) edx_unused;
    *id = FIELD(rider, 0x0, DWORD);
    *kind = FIELD(rider, 0x4, BYTE);
    text_copy(name, (const char*) rider + 0x05);
    text_copy(text1, (const char*) rider + 0x15);
    text_copy(text2, (const char*) rider + 0x55);
    text_copy(text3, (const char*) rider + 0x95);
    text_copy(text4, (const char*) rider + 0xD5);
    *a = FIELD(rider, 0x118, DWORD);
    *b = FIELD(rider, 0x11C, DWORD);
    *c = FIELD(rider, 0x120, DWORD);
}

/* ---- The Zone lobby ---- */

/*
    The lobby's object (game_main + 0x3410). The event is a kind and a place in that kind's
    track list; the bike a maker and a model, which come down to an index in the bike list.
*/
#define ZONE_RIDERS      0x008      /* 0x48 each: name[0x40], position BYTE at 0x40, score 0x44 */
#define ZONE_RIDER_SIZE  0x48
#define ZONE_KIND        0x24C      /* or -1 */
#define ZONE_PLACE       0x250      /* or -1 */
#define ZONE_VARIANT     0x254      /* 1-3 for Baja and enduro tracks, else 0 */
#define ZONE_TRACK       0x258      /* char[0x40] */
#define ZONE_MAKER       0x298
#define ZONE_MODEL       0x29C
#define ZONE_BIKE        0x2A0      /* or -1 */
#define ZONE_RESULTS     0x2A4      /* the 0x88 bytes sent */
#define ZONE_TRACK_FOUND 0x2A5      /* the track's place + 1, or 0 */
#define SENT_SIZE        0x10       /* maker, model, position BYTEs; best lap, time; stunts */

#define SLOT_FORMAT NAME(0x00573C18)       /* "%d,%d,%d,%d" */

/* The lobby's reply to a key it does not hold. */
#define NOT_THERE ((HRESULT) 0x80070005)

typedef struct lobby_settings {
    DWORD unknown_00;
    void* entries;
} lobby_settings;

BOOL    __fastcall game_FUN_004aeb00(BYTE* self, void* edx_unused, char* text, int size);
BOOL    __fastcall game_FUN_004aec80(BYTE* self, void* edx_unused, char* text, int size);
int     __fastcall game_FUN_004aee10(BYTE* self, void* edx_unused, int count, const void* data,
                                     int size);
void*   __fastcall game_FUN_00485e90(void* entries);
void    __fastcall game_FUN_004bce30(void* entries);
void    __fastcall game_FUN_004bcf40(lobby_settings* self, void* edx_unused, const char* text,
                                     int length);
HRESULT __fastcall game_FUN_004bd120(lobby_settings* self, void* edx_unused, const char* key,
                                     int* value);
void    __fastcall game_FUN_004bd0e0(lobby_settings* self, void* edx_unused, const char* key,
                                     char* text, int size);
void    __fastcall game_FUN_004eeda0(BYTE* resources, void* edx_unused, char* text,
                                     const char* file, DWORD a, const char* kind, BYTE* b,
                                     BYTE* c);
int     __cdecl    game_FUN_0053a97b(const char* text, const char* format, ...);  /* sscanf */
void    __cdecl    game_FUN_00525e70(const char* format, ...);                    /* a log line */
int     __cdecl    game_FUN_0053b6e0(const char* a, const char* b);               /* _stricmp */
DWORD   __cdecl    game_FUN_004c41d0(void);                                       /* the time */

__declspec(noinline) void __fastcall zone_resolve(BYTE* self, void* edx_unused, int direction);

/* Settings parsed from a lobby text. */
static void settings_read(lobby_settings* settings, const char* text)
{
    game_FUN_00485e90(&settings->entries);
    game_FUN_004bcf40(settings, NULL, text, lstrlenA(text));
}

PORT(0x0049DBA0)
void __fastcall lobby_settings_destruct(lobby_settings* self)
{
    game_FUN_004bce30(&self->entries);
}

/* A launch setting counted from 1, kept counted from 0 (-1 when missing); `bit` when there. */
static void launch_value(lobby_settings* settings, const char* key, BYTE* self, int field,
                         DWORD* found, DWORD bit)
{
    int value;

    if (game_FUN_004bd120(settings, NULL, key, &value) != NOT_THERE) {
        FIELD(self, field, int) = value - 1;
        *found |= bit;
    } else {
        FIELD(self, field, int) = -1;
    }
}

/*
    The event, track and bike the lobby launched the game with, put in the race settings with
    the lobby's fixed rules. `found` gets a bit for each of the four the lobby gave (1 kind,
    2 place, 4 maker, 8 model), less those that led nowhere.
*/
PORT(0x0049D8A0)
BOOL __fastcall zone_read_launch(BYTE* self, void* edx_unused, DWORD* found)
{
    lobby_settings settings;
    char scene[0x40];
    char text[0x800];

    (void) edx_unused;
    *found = 0;
    if (!game_FUN_004aeb00(self, NULL, text, sizeof text))
        return FALSE;
    settings_read(&settings, text);
    launch_value(&settings, NAME(0x00573C08), self, ZONE_KIND, found, 1);    /* EventTypeIndex */
    launch_value(&settings, NAME(0x00573BF4), self, ZONE_PLACE, found, 2);   /* EventTypeLocation */
    launch_value(&settings, NAME(0x00573BE0), self, ZONE_MAKER, found, 4);   /* BikeManufacturer */
    launch_value(&settings, NAME(0x00573BD4), self, ZONE_MODEL, found, 8);   /* BikeType */
    zone_resolve(self, NULL, 1);
    if (FIELD(self, ZONE_BIKE, int) == -1) {
        *found &= ~0xCu;
    } else {
        FIELD(game_main, 0x1FAC, int) = FIELD(self, ZONE_MAKER, int);
        FIELD(game_main, 0x1FB0, int) = FIELD(self, ZONE_BIKE, int);
    }
    if (FIELD(self, ZONE_KIND, int) == -1)
        *found &= ~3u;
    else
        race_kind = FIELD(self, ZONE_KIND, int);
    race_variant = FIELD(self, ZONE_VARIANT, BYTE);
    if (FIELD(self, ZONE_PLACE, int) != -1)
        text_copy(race_track, (const char*) self + ZONE_TRACK);
    if (FIELD(game_net, 0x10, int))
        game_FUN_004eeda0(FIELD(game_main, 0x574, BYTE*), NULL, scene, race_track, 0,
                          NAME(0x00570134), game_main + 0x2EA8, game_main + 0x2EAC);  /* "scn" */
    FIELD(game_main, 0x2D80, DWORD) = 1;
    FIELD(game_main, 0x2D84, DWORD) = 1;
    FIELD(game_main, 0x2D94, DWORD) = 0;
    FIELD(game_main, 0x2D70, DWORD) = 1;
    FIELD(game_main, 0x2D7C, DWORD) = 0;
    FIELD(game_main, 0x2D88, DWORD) = 0;
    race_laps = 5;
    FIELD(game_main, 0x2EB0, float) = 5.0f;
    FIELD(game_main, 0x2EB8, DWORD) = 0;
    FIELD(game_main, 0x2D9D, BYTE) = 0;
    FIELD(game_main, 0x2EB4, DWORD) = 0;
    FIELD(game_main, 0x610, DWORD) = 3;
    FIELD(game_main, 0x60C, DWORD) = 3;
    lobby_settings_destruct(&settings);
    return TRUE;
}

/*
    Up to 20 seconds for the lobby to give this profile a slot (to game_main + 0x2148), asking
    once a second. FALSE when it gave none, or -2. The slot starts as -1, where the original
    leaves it unset: a reply without a number then waits on instead of reading stack garbage.
*/
PORT(0x0049DBB0)
BOOL __fastcall zone_wait_for_slot(BYTE* self)
{
    lobby_settings settings;
    char text[0x800];
    char reply[0x800];
    int slot = -1;
    int second, third, fourth;
    DWORD start = game_FUN_004c41d0();

    while (game_FUN_004c41d0() - start < 20000u) {
        if (!game_FUN_004aec80(self, NULL, text, sizeof text))
            return FALSE;
        settings_read(&settings, text);
        game_FUN_004bd0e0(&settings, NULL, game_profile, reply, sizeof reply);
        game_FUN_0053a97b(reply, SLOT_FORMAT, &slot, &second, &third, &fourth);
        if (slot != -1) {
            FIELD(game_main, 0x2148, int) = slot;
            lobby_settings_destruct(&settings);
            break;
        }
        Sleep(1000);
        lobby_settings_destruct(&settings);
    }
    return slot != -1 && slot != -2;
}

/* Seconds as hundredths, cut as __ftol does. */
static int hundredths(float seconds)
{
    return (int) (seconds * (double) 100.0f);
}

#define race_entry(i) (FIELD(game_main, 0x570, BYTE*) + 0x50 + (i) * 0x50)

/* The race's results to the lobby: the event, and each rider's bike, place, times and score. */
PORT(0x0049DD20)
int __fastcall zone_send_results(BYTE* self)
{
    int longest = 0;
    int i;

    if (!FIELD(game_net, 0x14, int))
        return 1;
    FIELD(self, ZONE_RESULTS, BYTE) = (BYTE) (FIELD(game_main, 0x2D74, BYTE) + 1);
    FIELD(self, ZONE_KIND, int) = race_kind;
    text_copy((char*) self + ZONE_TRACK, race_track);
    FIELD(self, ZONE_VARIANT, DWORD) = race_variant;
    FIELD(self, ZONE_RESULTS + 2, BYTE) = (BYTE) race_laps;
    for (i = 0; i < FIELD(game_main, 0x18, int); i++) {
        BYTE* entry = race_entry(i);
        BYTE* sent = self + ZONE_RESULTS + 8 + i * SENT_SIZE;
        BYTE* rider = self + ZONE_RIDERS + i * ZONE_RIDER_SIZE;
        int j;

        FIELD(sent, 2, BYTE) = FIELD(entry, 0x04, BYTE);
        FIELD(sent, 4, int) = hundredths(FIELD(entry, 0x08, float));
        FIELD(sent, 8, int) = hundredths(FIELD(entry, 0x14, float));
        if (longest < FIELD(sent, 8, int))
            longest = FIELD(sent, 8, int);
        FIELD(sent, 0xC, int) = FIELD(entry, 0x0C, int);
        FIELD(rider, 0x40, BYTE) = FIELD(sent, 2, BYTE);
        text_copy((char*) rider, (const char*) entry + 0x40);
        if (FIELD(entry, 0x20, DWORD)) {
            if (race_kind != 0 && race_kind != 4)
                FIELD(rider, 0x44, int) = hundredths(FIELD(entry, 0x08, float));
            else
                FIELD(rider, 0x44, int) = (int) FIELD(entry, 0x2C, float);
        }
        for (j = 0; j < FIELD(game_main, 0x2158, int); j++) {
#ifdef OPENMCM2_DEV
            BYTE* entrant = entrants() + j * 0xF8;
#else
            BYTE* entrant = game_main + 0x215C + j * 0xF8;
#endif

            if (FIELD(entry, 0x00, int) == FIELD(entrant, 0xD4, int)) {
                FIELD(self, ZONE_BIKE, int) = FIELD(entrant, 0xC4, int);
                break;
            }
        }
        zone_resolve(self, NULL, 2);
        FIELD(sent, 0, BYTE) = FIELD(self, ZONE_MAKER, BYTE);
        FIELD(sent, 1, BYTE) = FIELD(self, ZONE_MODEL, BYTE);
        game_FUN_00525e70(NAME(0x00573CA0), i + 1);                         /* "\nPlayer %d\n" */
        game_FUN_00525e70(NAME(0x00573C8C), FIELD(sent, 0, BYTE), FIELD(sent, 1, BYTE),
                          FIELD(self, ZONE_BIKE, int)); /* "Bike %d-%d (%d)" */
        game_FUN_00525e70(NAME(0x00573C7C), FIELD(sent, 2, BYTE));          /* "Position %d\n" */
        game_FUN_00525e70(NAME(0x00573C64), FIELD(sent, 4, int));           /* "FastestLap %d..." */
        game_FUN_00525e70(NAME(0x00573C44), FIELD(sent, 8, int));           /* "TotalrunningTime" */
        game_FUN_00525e70(NAME(0x00573C24), FIELD(sent, 0xC, int));         /* "LargestSingle..." */
    }
    FIELD(self, ZONE_RESULTS + 4, int) = longest;
    return game_FUN_004aee10(self, NULL, FIELD(game_main, 0x18, int), self + ZONE_RESULTS, 0x88);
}

/* Each maker's number of bikes in the lobby's list: 3, 3, 5, 4, 5, 5, 5. */
#define maker_models ((const int*) 0x00573BB4)
#define ZONE_MAKERS  7

typedef struct zone_track {
    char name[16];
    int  variant;
} zone_track;

/*
    The tracks of each kind, in the order the game lays them out on its stack. Its search reads
    16 whatever the kind's count, so a shorter kind runs on into the next; one array keeps that.
*/
static const zone_track zone_tracks[70] = {
    { "Quarry05", 0 }, { "Quarry01", 0 }, { "Quarry04", 0 }, { "Quarry03", 0 },
    { "Quarry02", 0 },
    { "baja01", 1 }, { "baja01", 2 }, { "baja01", 3 }, { "baja03", 1 }, { "baja03", 2 },
    { "baja03", 3 }, { "baja02", 1 }, { "baja02", 2 }, { "baja02", 3 }, { "baja04", 1 },
    { "baja04", 2 }, { "baja04", 3 }, { "baja05", 1 }, { "baja05", 2 }, { "baja05", 3 },
    { "SX01", 0 }, { "SX02", 0 }, { "SX03", 0 }, { "SX04", 0 }, { "SX05", 0 }, { "SX06", 0 },
    { "SX07", 0 }, { "SX08", 0 }, { "SX09", 0 }, { "SX10", 0 }, { "SX11", 0 }, { "SX12", 0 },
    { "SX13", 0 }, { "SX14", 0 }, { "SX15", 0 }, { "SX16", 0 },
    { "Tag01", 0 }, { "Tag02", 0 }, { "Tag03", 0 },
    { "SkiLodge", 1 }, { "SkiLodge", 2 }, { "SkiLodge", 3 }, { "OpenPit", 1 }, { "OpenPit", 2 },
    { "OpenPit", 3 }, { "Farm", 1 }, { "Farm", 2 }, { "Farm", 3 }, { "Trailer", 1 },
    { "Trailer", 2 }, { "Trailer", 3 }, { "Airfield", 1 }, { "Airfield", 2 }, { "Airfield", 3 },
    { "Nat12", 0 }, { "Nat01", 0 }, { "Nat05", 0 }, { "Nat08", 0 }, { "Nat10", 0 },
    { "Nat03", 0 },
    { "Nat09", 0 }, { "iffendic", 0 }, { "hillside", 0 }, { "Nat11", 0 }, { "Nat07", 0 },
    { "Nat06", 0 }, { "SkyLab2", 0 }, { "Nat02", 0 }, { "VV", 0 }, { "Nat04", 0 },
};

#define TRACKS_SEARCHED 16

/* By race kind: quarries, Baja, nationals, supercross, tag, enduro. */
static const zone_track* const kind_tracks[6] = {
    zone_tracks + 0, zone_tracks + 5, zone_tracks + 54, zone_tracks + 20, zone_tracks + 36,
    zone_tracks + 39,
};

#define RESOLVE_FROM_LOBBY 1    /* maker and model to bike, kind and place to track */
#define RESOLVE_TO_LOBBY   2    /* bike to maker and model, track to its place */

/*
    The lobby's numbers and the game's, either way. From the lobby (1): maker and model, from 0,
    give the bike; kind and place give the track and its variant. To the lobby (2): the bike
    gives maker and model, from 1, and the track's place + 1 is looked up (0 when not found).
    Maker 4 is the list's sixth, and maker 0's models from 3 on are the last bikes (27 on).
*/
PORT(0x0049E010)
void __fastcall zone_resolve(BYTE* self, void* edx_unused, int direction)
{
    const zone_track* track;
    int start = 0;
    int i;

    (void) edx_unused;
    if (direction == RESOLVE_FROM_LOBBY) {
        int maker = FIELD(self, ZONE_MAKER, int);

        if (maker < 0 || maker > 4) {
            FIELD(self, ZONE_BIKE, int) = -1;
        } else {
            if (maker == 4)
                FIELD(self, ZONE_MAKER, int) = 6;
            if (FIELD(self, ZONE_MAKER, int) == 0 && FIELD(self, ZONE_MODEL, int) >= 3) {
                FIELD(self, ZONE_BIKE, int) = FIELD(self, ZONE_MODEL, int) + 27;
            } else {
                for (i = 0; i < FIELD(self, ZONE_MAKER, int); i++)
                    start += maker_models[i];
                FIELD(self, ZONE_BIKE, int) =
                    i < ZONE_MAKERS ? FIELD(self, ZONE_MODEL, int) + start : -1;
            }
        }
        if (FIELD(self, ZONE_KIND, int) < 0 || FIELD(self, ZONE_KIND, int) > 5) {
            FIELD(self, ZONE_KIND, int) = -1;
            return;
        }
        track = kind_tracks[FIELD(self, ZONE_KIND, int)] + FIELD(self, ZONE_PLACE, int);
        FIELD(self, ZONE_VARIANT, int) = track->variant;
        text_copy((char*) self + ZONE_TRACK, track->name);
    } else if (direction == RESOLVE_TO_LOBBY) {
        int bike = FIELD(self, ZONE_BIKE, int);

        if (bike >= 30) {
            FIELD(self, ZONE_MAKER, int) = 1;
            bike -= 26;
        } else {
            for (i = 0; i < ZONE_MAKERS; i++) {
                int end = maker_models[i] + start;

                if (bike < end)
                    break;
                start = end;
            }
            FIELD(self, ZONE_MAKER, int) = i + 1 == ZONE_MAKERS ? 5 : i + 1;
            bike = bike - start + 1;
        }
        FIELD(self, ZONE_MODEL, int) = bike;
        FIELD(self, ZONE_TRACK_FOUND, BYTE) = 0;
        /*
            The kind is not checked here. The original's -1 reads the zero just before its
            table, so it compares with no track at all and crashes; NULL keeps that.
        */
        track = FIELD(self, ZONE_KIND, int) >= 0 && FIELD(self, ZONE_KIND, int) <= 5
                    ? kind_tracks[FIELD(self, ZONE_KIND, int)] : NULL;
        for (i = 0; i < TRACKS_SEARCHED; i++, track++) {
            if (!game_FUN_0053b6e0(track->name, (const char*) self + ZONE_TRACK)
                && FIELD(self, ZONE_VARIANT, int) == track->variant) {
                FIELD(self, ZONE_TRACK_FOUND, BYTE) = (BYTE) (i + 1);
                return;
            }
        }
    }
}
