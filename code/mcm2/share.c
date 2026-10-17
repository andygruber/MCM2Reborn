/*
    OpenMCM2 dev build - files from the other players in a multiplayer lobby: the race's track
    from the host, a rider's own skins (Riders\*.tga, Bikes\*.tga) from that rider.

    A download asks its owner for a window of parts at a time and asks again for any that do not
    come; the parts go to the asker alone (message types under 0x80 can be sent to one player,
    without the game's guarantee), so the rest of the lobby is not slowed. The file is written
    beside its place as <name>.part and put there once whole. A player gives only what is in
    use: the host the race's track, a rider the skins it rides on.

    The lobby screen (selectgamepicprocs.c) asks before anything is downloaded, hands this the
    messages and drives it from its timer.
*/

#ifdef OPENMCM2_DEV

#include "menus.h"
#include "../dev/dev.h"

#include <intrin.h>
#include <mmsystem.h>

#define game_net        FIELD(game_main, 0x8, BYTE*)
#define net_me          FIELD(game_net, 0xC, int)
#define net_hosting     FIELD(game_net, 0x10, DWORD)
#define profile_root    (game_profile + 0x23D0)
#define own_rider_look  ((const char*) game_main + 0x1F6C)
#define own_bike_look   ((const char*) game_main + 0x1F2C)

#define MSG_FILE_ASK    0x60
#define MSG_FILE_PART   0x61
#define MSG_FILE_NONE   0x62

#define PART_DATA       0x780           /* a part's message stays under the game's 0x8DD bytes */
#define WINDOW          32              /* parts asked for at once */
#define ASK_AGAIN_MS    1500
#define TRIES_MOST      8
#define TRACK_MOST      (96u << 20)
#define SKIN_MOST       (16u << 20)
#define QUEUE_MOST      16

void __fastcall game_FUN_004b0f20(BYTE* net, void* edx_unused, DWORD type, void* data, DWORD size,
                                  DWORD from, DWORD to);
BOOL __fastcall profile_race_scene(char* self, void* edx_unused, char* scene);

typedef struct file_ask {
    DWORD header;
    BYTE  kind, count;
    WORD  unused;
    DWORD first;
    char  name[64];
} file_ask;

typedef struct file_part {
    DWORD header;
    BYTE  kind, unused[3];
    DWORD total, part;
    WORD  size, unused2;
    char  name[64];
    BYTE  data[PART_DATA];
} file_part;

typedef struct download {
    int  kind, from;
    char name[64];
} download;

static download g_queue[QUEUE_MOST];
static int      g_queued;

static struct {
    BOOL     on;
    download what;
    HANDLE   file;
    char     path[MAX_PATH], part_path[MAX_PATH];
    DWORD    total, parts, base, window_got, asked_at;
    int      tries, told;               /* told: the last tenth of it reported */
} g_get;

/* How the last download ended, until the lobby is told. */
enum { ENDED_NOT, ENDED_IN, ENDED_NONE, ENDED_STOPPED, ENDED_UNSAVED };
static int g_ended;

typedef BOOL  (WINAPI* move_fn)(LPCSTR, LPCSTR, DWORD);
typedef DWORD (WINAPI* attributes_fn)(LPCSTR);
typedef DWORD (WINAPI* seek_fn)(HANDLE, LONG, LONG*, DWORD);
typedef DWORD (WINAPI* size_fn)(HANDLE, DWORD*);
typedef BOOL  (WINAPI* remove_fn)(LPCSTR);

static FARPROC kernel(const char* name)
{
    return api("kernel32.dll", name);
}

static BOOL same_text(const char* a, const char* b, int length)
{
    int i;

    for (i = 0; i < length; i++) {
        char x = a[i] >= 'a' && a[i] <= 'z' ? a[i] - 32 : a[i];
        char y = b[i] >= 'a' && b[i] <= 'z' ? b[i] - 32 : b[i];

        if (x != y)
            return FALSE;
        if (!x)
            break;
    }
    return TRUE;
}

/* A file's name alone: no folders, no "..", nothing the file system would read otherwise. */
static BOOL plain_name(const char* name)
{
    int i;

    if (!name[0] || name[0] == '.' || name[0] == ' ')
        return FALSE;
    for (i = 0; name[i]; i++)
        if ((BYTE) name[i] < ' ' || name[i] == '\\' || name[i] == '/' || name[i] == ':' || name[i] == '*'
            || name[i] == '?' || name[i] == '"' || name[i] == '<' || name[i] == '>' || name[i] == '|'
            || (name[i] == '.' && name[i + 1] == '.') || i >= 60)
            return FALSE;
    return TRUE;
}

/* Where a shared file is, here: FALSE for a name that is not one of its kind's. */
static BOOL share_path(int kind, const char* name, char* path)
{
    const char* folder = kind == SHARE_RIDER ? "Riders\\" : "Bikes\\";
    int length = lstrlenA(folder), n;

    if (kind == SHARE_TRACK) {
        char scene[0x100];

        if (!plain_name(name))
            return FALSE;
        profile_race_scene(game_profile, NULL, scene);
        wsprintfA(path, "%s\\%s\\%s.env", profile_root, scene, name);
        return TRUE;
    }
    if (kind != SHARE_RIDER && kind != SHARE_BIKE)
        return FALSE;
    n = lstrlenA(name);
    if (!same_text(name, folder, length) || !plain_name(name + length) || n < length + 5
        || !same_text(name + n - 4, ".tga", 4))
        return FALSE;
    wsprintfA(path, "%s\\%s", profile_root, name);
    return TRUE;
}

BOOL share_have(int kind, const char* name)
{
    attributes_fn attributes = (attributes_fn) kernel("GetFileAttributesA");
    char path[MAX_PATH];

    return !share_path(kind, name, path) || !attributes || attributes(path) != INVALID_FILE_ATTRIBUTES;
}

BOOL share_is_skin(const char* look)
{
    return same_text(look, "Riders\\", 7) || same_text(look, "Bikes\\", 6);
}

void share_get(int kind, const char* name, int from)
{
    int i;

    if (g_get.on && g_get.what.kind == kind && !lstrcmpiA(g_get.what.name, name))
        return;
    for (i = 0; i < g_queued; i++)
        if (g_queue[i].kind == kind && !lstrcmpiA(g_queue[i].name, name))
            return;
    if (g_queued == QUEUE_MOST || lstrlenA(name) >= (int) sizeof g_queue[0].name)
        return;
    g_queue[g_queued].kind = kind;
    g_queue[g_queued].from = from;
    lstrcpynA(g_queue[g_queued].name, name, sizeof g_queue[0].name);
    g_queued++;
}

static void ask(void)
{
    file_ask message = { 0 };

    message.kind = (BYTE) g_get.what.kind;
    message.count = WINDOW;
    message.first = g_get.base;
    lstrcpynA(message.name, g_get.what.name, sizeof message.name);
    game_FUN_004b0f20(game_net, NULL, MSG_FILE_ASK, &message, sizeof message, net_me, g_get.what.from);
    g_get.asked_at = timeGetTime();
}

static void get_stop(int ended)
{
    BOOL whole = ended == ENDED_IN;
    remove_fn remove = (remove_fn) kernel("DeleteFileA");
    move_fn move = (move_fn) kernel("MoveFileExA");
    BOOL put = FALSE;

    if (g_get.file != INVALID_HANDLE_VALUE)
        CloseHandle(g_get.file);
    g_get.file = INVALID_HANDLE_VALUE;
    if (whole && move) {
        char kept[MAX_PATH + 8];

        wsprintfA(kept, "%s.bak", g_get.path);
        move(g_get.path, kept, MOVEFILE_REPLACE_EXISTING);     /* fails, harmlessly, when there is none */
        put = move(g_get.part_path, g_get.path, MOVEFILE_REPLACE_EXISTING);
    }
    if (!put && remove)
        remove(g_get.part_path);
    log_line("share: %s %s from player %d", put ? "downloaded" : "gave up on", g_get.what.name, g_get.what.from);
    g_get.on = FALSE;
    g_ended = whole && !put ? ENDED_UNSAVED : ended;
}

static BOOL get_start(void)
{
    download next = g_queue[0];

    g_queued--;
    __movsb((BYTE*) g_queue, (const BYTE*) (g_queue + 1), g_queued * sizeof *g_queue);
    __stosb((BYTE*) &g_get, 0, sizeof g_get);
    g_get.what = next;
    g_get.file = INVALID_HANDLE_VALUE;
    if (share_path(next.kind, next.name, g_get.path)) {
        wsprintfA(g_get.part_path, "%s.part", g_get.path);
        g_get.file = CreateFileA(g_get.part_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    }
    if (g_get.file == INVALID_HANDLE_VALUE) {
        g_ended = ENDED_UNSAVED;
        return FALSE;
    }
    g_get.on = TRUE;
    g_get.told = -1;
    ask();
    return TRUE;
}

/* A part come in: written at its place; the window asked on from once it is all in. */
static void part_take(const file_part* part, int from)
{
    seek_fn seek = (seek_fn) kernel("SetFilePointer");
    DWORD most = g_get.what.kind == SHARE_TRACK ? TRACK_MOST : SKIN_MOST, done, want, i;
    char name[64];

    lstrcpynA(name, part->name, sizeof name);
    if (!g_get.on || from != g_get.what.from || part->kind != g_get.what.kind
        || lstrcmpiA(name, g_get.what.name) || !part->total || part->total > most || !seek)
        return;
    if (!g_get.total) {
        g_get.total = part->total;
        g_get.parts = (part->total + PART_DATA - 1) / PART_DATA;
    }
    if (part->total != g_get.total || part->part < g_get.base || part->part >= g_get.base + WINDOW
        || part->part >= g_get.parts
        || part->size != min(PART_DATA, g_get.total - part->part * PART_DATA))
        return;
    if (g_get.window_got & (1u << (part->part - g_get.base)))
        return;
    seek(g_get.file, (LONG) (part->part * PART_DATA), NULL, FILE_BEGIN);
    if (!WriteFile(g_get.file, part->data, part->size, &done, NULL) || done != part->size) {
        get_stop(ENDED_UNSAVED);
        return;
    }
    g_get.window_got |= 1u << (part->part - g_get.base);
    g_get.tries = 0;
    g_get.asked_at = timeGetTime();

    want = 0;
    for (i = g_get.base; i < g_get.base + WINDOW && i < g_get.parts; i++)
        want |= 1u << (i - g_get.base);
    if (g_get.window_got != want)
        return;
    g_get.base += WINDOW;
    g_get.window_got = 0;
    if (g_get.base >= g_get.parts)
        get_stop(ENDED_IN);
    else
        ask();
}

/* Another player's ask: the parts sent to it, when the file is one this player gives. */
static void ask_take(const file_ask* message, int from)
{
    seek_fn seek = (seek_fn) kernel("SetFilePointer");
    size_fn size_of = (size_fn) kernel("GetFileSize");
    char path[MAX_PATH], name[64];
    file_part part;
    HANDLE file;
    DWORD total, i;
    BOOL gives;

    lstrcpynA(name, message->name, sizeof name);
    if (message->kind == SHARE_TRACK)
        gives = net_hosting && !lstrcmpiA(name, (const char*) race_settings + SET_TRACK);
    else
        gives = !lstrcmpiA(name, message->kind == SHARE_RIDER ? own_rider_look : own_bike_look);
    file = gives && seek && size_of && share_path(message->kind, name, path)
           ? CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)
           : INVALID_HANDLE_VALUE;
    if (file == INVALID_HANDLE_VALUE) {
        __stosb((BYTE*) &part, 0, sizeof part);
        part.kind = message->kind;
        lstrcpynA(part.name, name, sizeof part.name);
        game_FUN_004b0f20(game_net, NULL, MSG_FILE_NONE, &part, (DWORD) (part.data - (BYTE*) &part),
                          net_me, from);
        return;
    }
    total = size_of(file, NULL);
    if (message->first >= (total + PART_DATA - 1) / PART_DATA) {
        CloseHandle(file);
        return;
    }
    __stosb((BYTE*) &part, 0, sizeof part);
    part.kind = message->kind;
    part.total = total;
    lstrcpynA(part.name, name, sizeof part.name);
    for (i = message->first; i < message->first + min(message->count, WINDOW) && i * PART_DATA < total; i++) {
        DWORD got = 0;

        part.part = i;
        part.size = (WORD) min(PART_DATA, total - i * PART_DATA);
        seek(file, (LONG) (i * PART_DATA), NULL, FILE_BEGIN);
        if (!ReadFile(file, part.data, part.size, &got, NULL) || got != part.size)
            break;
        game_FUN_004b0f20(game_net, NULL, MSG_FILE_PART, &part,
                          (DWORD) (part.data - (BYTE*) &part) + part.size, net_me, from);
    }
    CloseHandle(file);
}

BOOL share_take(DWORD type, const BYTE* data, int from)
{
    if (type == MSG_FILE_ASK) {
        ask_take((const file_ask*) data, from);
    } else if (type == MSG_FILE_PART) {
        part_take((const file_part*) data, from);
    } else if (type == MSG_FILE_NONE) {
        const file_part* none = (const file_part*) data;
        char name[64];

        lstrcpynA(name, none->name, sizeof name);
        if (g_get.on && from == g_get.what.from && none->kind == g_get.what.kind
            && !lstrcmpiA(name, g_get.what.name))
            get_stop(ENDED_NONE);
    } else {
        return FALSE;
    }
    return TRUE;
}

/* Each tick of the lobby: the next download started, a silent owner asked again. What there is to
   tell the player goes into `news` (SHARE_NEWS); SHARE_TRACK_IN when a track has just come in. */
int share_tick(char* news)
{
    const char* what = g_get.what.kind == SHARE_TRACK ? "TRACK" : "SKIN";
    int tenth;

    news[0] = 0;
    if (g_ended) {
        static const char* const how[] = { "", "DOWNLOADED %s %s", "THE %s %s IS NOT ON OFFER",
                                           "THE %s DOWNLOAD STOPPED: %s", "COULD NOT SAVE THE %s %s" };
        int ended = g_ended;

        g_ended = ENDED_NOT;
        wsprintfA(news, how[ended], what, g_get.what.name);
        return ended == ENDED_IN && g_get.what.kind == SHARE_TRACK ? SHARE_TRACK_IN : SHARE_NEWS;
    }
    if (!g_get.on) {
        if (g_queued)
            get_start();
        return 0;
    }
    if (timeGetTime() - g_get.asked_at > ASK_AGAIN_MS) {
        if (++g_get.tries > TRIES_MOST) {
            get_stop(ENDED_STOPPED);
            return 0;
        }
        ask();
    }
    tenth = g_get.parts ? (int) (g_get.base * 10 / g_get.parts) : 0;
    if (tenth == g_get.told)
        return 0;
    g_get.told = tenth;
    wsprintfA(news, "DOWNLOADING %s %s: %d%%", what, g_get.what.name, tenth * 10);
    return SHARE_NEWS;
}

#endif
