/*
    OpenMCM2 dev build - the lap leaderboards (the LEADERBOARDS screen, dlgprocs.c).

    A lap counts from the second on: the first starts from the gate. In a national or supercross
    a lap with more than two seconds off the course (rider + 0x78C clear, the course arrow's test)
    is not counted, nor is the rest of it. Each rider has one time a track, event and bike class -
    the best - kept when beaten and never when slower: in leaderboard.txt beside the game for this
    computer, and on the server (tools/leaderboard-worker, [leaderboard] url) for everyone's.

    The server (as Open Unleashed's, by its id and key a rider):
      POST /v1/lap     "rider key name kind class track ms version" (tab separated) keeps a better lap
      GET  /v1/board   ?kind=&class=&track=  a line "name<TAB>ms<TAB>date" a rider, best first
      GET  /v1/tracks  ?kind=&class=         a line "track<TAB>riders" a track with laps
    Laps wait in leaderboard-pending.txt until the server has them. The screen's reads are made
    by a thread of their own, the screen shown again when they are in (leaders_version).

    Event kinds are race_kind's: 1 Baja, 2 national, 3 supercross, 5 enduro (quarry and tag are
    timed, not raced in laps). A Baja or enduro route has a board of its own, "TRACK ROUTE n".
*/

#include "dev.h"
#include "game.h"
#include <mmsystem.h>
#include <intrin.h>

#define LEADERS_FILE    "leaderboard.txt"
#define PENDING_FILE    "leaderboard-pending.txt"
#define RECORDS_MOST    4096
#define PENDING_MOST    256
#define OFF_COURSE_MS   2000    /* Open Unleashed's grace (MxaLeaderboard.OffTrackGrace) */
#define SETTINGS_FILE   dev_settings_file()

#define race_kind       FIELD(game_main, 0x2D74, int)
#define race_route      FIELD(game_main, 0x2DA4, BYTE)              /* race settings 0x34 */
#define race_track      ((const char*) game_main + 0x2DA6)
#define bike_cc         FIELD(game_main, 0x1550, int)
#define playing_back    FIELD(game_main, 0x3428, int)
#define profile_name    ((const char*) game_main + 0x578)

#define rider_last(r)   FIELD(r, 0x74C, float)
#define rider_on_course(r) FIELD(r, 0x78C, int)
#define rider_laps(r)   FIELD(r, 0x7A0, unsigned short)

BYTE* __fastcall game_FUN_0045e300(BYTE* events);            /* the race under way */
void  __cdecl    game_FUN_00539a96(void* base, size_t count, size_t size,
                                   int (__cdecl* compare)(const void*, const void*));   /* qsort */

typedef struct record {
    int  kind, cc, ms;
    char track[64], name[16], date[11];
} record;

static record g_records[RECORDS_MOST];
static int    g_count = -1;                 /* -1 before the file is read */
char          g_leaders_url[256] = { 0 };   /* initialised, or it is a common symbol and lands in .data */

/* A whole number from text: its digits, a '-' first allowed. */
static int number(const char* text)
{
    int sign = 1, n = 0;

    if (*text == '-') {
        sign = -1;
        text++;
    }
    while (*text >= '0' && *text <= '9')
        n = n * 10 + *text++ - '0';
    return sign * n;
}

/* Whether `text` starts with `prefix`, whatever their case; `length` its length. */
static BOOL starts_with(const char* text, const char* prefix, int length)
{
    int i;

    for (i = 0; i < length; i++) {
        char a = text[i] >= 'A' && text[i] <= 'Z' ? text[i] + 32 : text[i];
        char b = prefix[i] >= 'A' && prefix[i] <= 'Z' ? prefix[i] + 32 : prefix[i];

        if (a != b)
            return FALSE;
    }
    return TRUE;
}

static BOOL lapped_kind(int kind)
{
    return kind == 1 || kind == 2 || kind == 3 || kind == 5;
}

static BOOL same(const char* a, const char* b)
{
    return lstrcmpiA(a, b) == 0;
}

/* A line's tab-separated fields into `fields` (at most `most`); how many. */
static int split(char* line, char** fields, int most)
{
    int n = 0;

    while (n < most) {
        fields[n++] = line;
        while (*line && *line != '\t')
            line++;
        if (!*line)
            break;
        *line++ = 0;
    }
    return n;
}

/* The next line of `*text`, its end cut off (and a '\r'); NULL at the end. */
static char* next_line(char** text)
{
    char* line = *text;
    char* end = line;

    if (!*line)
        return NULL;
    while (*end && *end != '\n')
        end++;
    *text = *end ? end + 1 : end;
    *end = 0;
    if (end > line && end[-1] == '\r')
        end[-1] = 0;
    return line;
}

/* A whole small file into `text` (room `size`), ended; its length, 0 when there is none. */
static int file_read(const char* name, char* text, int size)
{
    HANDLE file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD got = 0;

    text[0] = 0;
    if (file == INVALID_HANDLE_VALUE)
        return 0;
    ReadFile(file, text, size - 1, &got, NULL);
    CloseHandle(file);
    text[got] = 0;
    return (int) got;
}

static void records_read(void)
{
    static char text[RECORDS_MOST * 120];
    char* at = text;
    char* line;

    g_count = 0;
    file_read(LEADERS_FILE, text, sizeof text);
    while ((line = next_line(&at)) && g_count < RECORDS_MOST) {
        char* fields[6];

        if (split(line, fields, 6) == 6) {
            record* r = &g_records[g_count++];

            r->kind = number(fields[0]);
            r->cc = number(fields[1]);
            lstrcpynA(r->track, fields[2], sizeof r->track);
            r->ms = number(fields[3]);
            lstrcpynA(r->name, fields[4], sizeof r->name);
            lstrcpynA(r->date, fields[5], sizeof r->date);
        }
    }
}

static void records_write(void)
{
    HANDLE file = CreateFileA(LEADERS_FILE, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    char line[160];
    DWORD done;
    int i;

    if (file == INVALID_HANDLE_VALUE)
        return;
    for (i = 0; i < g_count; i++) {
        const record* r = &g_records[i];
        int n = wsprintfA(line, "%d\t%d\t%s\t%d\t%s\t%s\r\n", r->kind, r->cc, r->track, r->ms, r->name, r->date);

        WriteFile(file, line, n, &done, NULL);
    }
    CloseHandle(file);
}

/* ---- The server ---- */

typedef void*  (WINAPI* open_fn)(LPCWSTR, DWORD, LPCWSTR, LPCWSTR, DWORD);
typedef void*  (WINAPI* connect_fn)(void*, LPCWSTR, WORD, DWORD);
typedef void*  (WINAPI* request_fn)(void*, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR, LPCWSTR*, DWORD);
typedef BOOL   (WINAPI* send_fn)(void*, LPCWSTR, DWORD, void*, DWORD, DWORD, DWORD_PTR);
typedef BOOL   (WINAPI* receive_fn)(void*, void*);
typedef BOOL   (WINAPI* read_fn)(void*, void*, DWORD, DWORD*);
typedef BOOL   (WINAPI* close_fn)(void*);
typedef BOOL   (WINAPI* timeouts_fn)(void*, int, int, int, int);
typedef BOOL   (WINAPI* headers_fn)(void*, DWORD, LPCWSTR, void*, DWORD*, DWORD*);

#define ASK_WAIT_MS     5000    /* each of resolving, connecting, sending and answering */
#define STATUS_NUMBER   (19 | 0x20000000)   /* WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER */

/* `method` `path` on the server, `body` sent; its answer into `answer` (room `size`). The
   answer's HTTP status, 0 when there is no server or it did not answer. http:// or
   https://host[:port]/base in the url. Not for the game's thread: it waits on the network. */
static DWORD server_ask(const char* method, const char* path, const char* body, char* answer, int size)
{
    open_fn open = (open_fn) api("winhttp.dll", "WinHttpOpen");
    connect_fn connect = (connect_fn) api("winhttp.dll", "WinHttpConnect");
    request_fn request = (request_fn) api("winhttp.dll", "WinHttpOpenRequest");
    send_fn send = (send_fn) api("winhttp.dll", "WinHttpSendRequest");
    receive_fn receive = (receive_fn) api("winhttp.dll", "WinHttpReceiveResponse");
    read_fn read = (read_fn) api("winhttp.dll", "WinHttpReadData");
    close_fn close = (close_fn) api("winhttp.dll", "WinHttpCloseHandle");
    timeouts_fn timeouts = (timeouts_fn) api("winhttp.dll", "WinHttpSetTimeouts");
    headers_fn headers = (headers_fn) api("winhttp.dll", "WinHttpQueryHeaders");
    WCHAR host[128], wide_path[1024], wide_method[8];
    const char* at = g_leaders_url;
    BOOL secure = FALSE;
    int port, n = 0, got = 0;
    DWORD status = 0;
    void *session, *link, *call;
    char full[1024];

    answer[0] = 0;
    if (!g_leaders_url[0] || !open || !connect || !request || !send || !receive || !read || !close || !timeouts || !headers)
        return 0;
    if (starts_with(at, "https://", 8)) {
        secure = TRUE;
        at += 8;
    } else if (starts_with(at, "http://", 7)) {
        at += 7;
    }
    while (*at && *at != '/' && *at != ':' && n < 127)
        host[n++] = (WCHAR) *at++;
    host[n] = 0;
    port = secure ? 443 : 80;
    if (*at == ':') {
        port = number(at + 1);
        while (*at && *at != '/')
            at++;
    }
    lstrcpynA(full, at, sizeof full);
    if (lstrlenA(full) && full[lstrlenA(full) - 1] == '/')
        full[lstrlenA(full) - 1] = 0;
    if (lstrlenA(full) + lstrlenA(path) >= (int) sizeof full)
        return 0;
    lstrcpynA(full + lstrlenA(full), path, sizeof full - lstrlenA(full));
    MultiByteToWideChar(CP_UTF8, 0, full, -1, wide_path, 1024);
    MultiByteToWideChar(CP_UTF8, 0, method, -1, wide_method, 8);

    session = open(L"MCM2Reborn", 0, NULL, NULL, 0);
    if (session)
        timeouts(session, ASK_WAIT_MS, ASK_WAIT_MS, ASK_WAIT_MS, ASK_WAIT_MS);
    link = session ? connect(session, host, (WORD) port, 0) : NULL;
    call = link ? request(link, wide_method, wide_path, NULL, NULL, NULL, secure ? 0x00800000 : 0) : NULL;
    if (call && send(call, NULL, 0, (void*) body, body ? lstrlenA(body) : 0, body ? lstrlenA(body) : 0, 0)
        && receive(call, NULL)) {
        DWORD read_now = 0, room = sizeof status;

        headers(call, STATUS_NUMBER, NULL, &status, &room, NULL);
        while (got < size - 1 && read(call, answer + got, size - 1 - got, &read_now) && read_now)
            got += read_now;
    }
    answer[got] = 0;
    if (call)
        close(call);
    if (link)
        close(link);
    if (session)
        close(session);
    return status;
}

/* `text` for a url's query: letters and digits as they are, anything else %XX. */
static void url_word(char* out, const char* text, int size)
{
    static const char hex[] = "0123456789ABCDEF";
    int n = 0;

    for (; *text && n < size - 4; text++) {
        unsigned char c = (unsigned char) *text;

        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '-' || c == '_') {
            out[n++] = (char) c;
        } else {
            out[n++] = '%';
            out[n++] = hex[c >> 4];
            out[n++] = hex[c & 15];
        }
    }
    out[n] = 0;
}

/* This computer's rider on the server: an id, and the key that proves it, made once and kept in
   the settings ([leaderboard] rider, key) as Open Unleashed keeps its own. */
static char g_rider[33], g_key[33];

static BOOL rider_known(void)
{
    typedef DWORD (WINAPI* read_text_fn)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
    typedef BOOL (WINAPI* write_text_fn)(LPCSTR, LPCSTR, LPCSTR, LPCSTR);
    typedef BOOLEAN (WINAPI* random_fn)(void*, ULONG);
    read_text_fn read_text = (read_text_fn) api("kernel32.dll", "GetPrivateProfileStringA");
    write_text_fn write_text = (write_text_fn) api("kernel32.dll", "WritePrivateProfileStringA");
    random_fn random = (random_fn) api("advapi32.dll", "SystemFunction036");     /* RtlGenRandom */
    static const char hex[] = "0123456789abcdef";
    char* made[2] = { g_rider, g_key };
    BYTE bytes[16];
    int k, i;

    if (g_rider[0])
        return TRUE;
    if (!read_text || !write_text)
        return FALSE;
    read_text("leaderboard", "rider", "", g_rider, sizeof g_rider, SETTINGS_FILE);
    read_text("leaderboard", "key", "", g_key, sizeof g_key, SETTINGS_FILE);
    if (lstrlenA(g_rider) == 32 && lstrlenA(g_key) == 32)
        return TRUE;
    if (!random)
        return g_rider[0] = 0, FALSE;
    for (k = 0; k < 2; k++) {
        if (!random(bytes, sizeof bytes))
            return g_rider[0] = 0, FALSE;
        for (i = 0; i < 16; i++) {
            made[k][2 * i] = hex[bytes[i] >> 4];
            made[k][2 * i + 1] = hex[bytes[i] & 15];
        }
        made[k][32] = 0;
    }
    write_text("leaderboard", "rider", g_rider, SETTINGS_FILE);
    write_text("leaderboard", "key", g_key, SETTINGS_FILE);
    return TRUE;
}

/* ---- Laps on their way to the server: kept in PENDING_FILE, sent by one thread at a time ---- */

static CRITICAL_SECTION g_lock;
static char g_pending[PENDING_MOST][160];   /* "kind class track ms name" */
static int  g_pending_count = -1;           /* -1 before the file is read */
static volatile LONG g_sending;

static void lock_ready(void)
{
    static volatile LONG made;

    if (!InterlockedExchange(&made, 1))
        InitializeCriticalSection(&g_lock);
}

/* Under g_lock. */
static void pending_read(void)
{
    static char text[PENDING_MOST * 160];
    char* at = text;
    char* line;

    g_pending_count = 0;
    file_read(PENDING_FILE, text, sizeof text);
    while ((line = next_line(&at)) && g_pending_count < PENDING_MOST)
        if (*line)
            lstrcpynA(g_pending[g_pending_count++], line, sizeof g_pending[0]);
}

/* Under g_lock. */
static void pending_write(void)
{
    HANDLE file;
    DWORD done;
    int i;

    if (!g_pending_count) {
        DeleteFileA(PENDING_FILE);
        return;
    }
    file = CreateFileA(PENDING_FILE, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;
    for (i = 0; i < g_pending_count; i++) {
        WriteFile(file, g_pending[i], lstrlenA(g_pending[i]), &done, NULL);
        WriteFile(file, "\r\n", 2, &done, NULL);
    }
    CloseHandle(file);
}

/* Each waiting lap sent, first to last, until the server has them all or does not answer (the
   rest wait for the next lap or a look at the boards). One it turns away is dropped. */
static DWORD WINAPI pending_send(void* unused)
{
    char line[160], body[256], answer[128];
    char* fields[5];
    DWORD status;

    (void) unused;
    for (;;) {
        EnterCriticalSection(&g_lock);
        if (g_pending_count <= 0) {
            LeaveCriticalSection(&g_lock);
            break;
        }
        lstrcpynA(line, g_pending[0], sizeof line);
        LeaveCriticalSection(&g_lock);

        {
            char copy[160];

            lstrcpynA(copy, line, sizeof copy);
            if (split(copy, fields, 5) == 5) {
                wsprintfA(body, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s", g_rider, g_key, fields[4], fields[0], fields[1],
                          fields[2], fields[3], GAME_VERSION);
                status = server_ask("POST", "/v1/lap", body, answer, sizeof answer);
            } else {
                status = 400;
            }
        }
        if (!status || status == 429 || status >= 500)
            break;
        if (status != 200)
            log_line("leaderboard: the server turned a lap away (%lu %s): %s", status, answer, line);

        EnterCriticalSection(&g_lock);
        if (g_pending_count > 0 && !lstrcmpA(g_pending[0], line)) {
            __movsb((unsigned char*) g_pending[0], (const unsigned char*) g_pending[1], (g_pending_count - 1) * sizeof g_pending[0]);
            g_pending_count--;
            pending_write();
        }
        LeaveCriticalSection(&g_lock);
    }
    InterlockedExchange(&g_sending, 0);
    return 0;
}

/* Under g_lock: the laps this computer kept before it had the server, sent once ([leaderboard]
   sent_old). Baja and enduro ones are left: they were kept without their route. */
static void pending_old_laps(void)
{
    typedef DWORD (WINAPI* read_text_fn)(LPCSTR, LPCSTR, LPCSTR, LPSTR, DWORD, LPCSTR);
    typedef BOOL (WINAPI* write_text_fn)(LPCSTR, LPCSTR, LPCSTR, LPCSTR);
    read_text_fn read_text = (read_text_fn) api("kernel32.dll", "GetPrivateProfileStringA");
    write_text_fn write_text = (write_text_fn) api("kernel32.dll", "WritePrivateProfileStringA");
    char sent[4];
    int i;

    if (!read_text || !write_text)
        return;
    read_text("leaderboard", "sent_old", "", sent, sizeof sent, SETTINGS_FILE);
    if (sent[0] == '1')
        return;
    if (g_count < 0)
        records_read();
    for (i = 0; i < g_count && g_pending_count < PENDING_MOST; i++) {
        const record* r = &g_records[i];

        if (r->kind == 2 || r->kind == 3)
            wsprintfA(g_pending[g_pending_count++], "%d\t%d\t%s\t%d\t%s", r->kind, r->cc, r->track, r->ms, r->name);
    }
    pending_write();
    write_text("leaderboard", "sent_old", "1", SETTINGS_FILE);
}

static void pending_kick(void)
{
    HANDLE thread;

    if (!g_leaders_url[0] || !rider_known())
        return;
    lock_ready();
    EnterCriticalSection(&g_lock);
    if (g_pending_count < 0) {
        pending_read();
        pending_old_laps();
    }
    LeaveCriticalSection(&g_lock);
    if (InterlockedCompareExchange(&g_sending, 1, 0))
        return;
    thread = CreateThread(NULL, 0, pending_send, NULL, 0, NULL);
    if (thread)
        CloseHandle(thread);
    else
        InterlockedExchange(&g_sending, 0);
}

static void pending_add(int kind, int cc, const char* track, int ms, const char* name)
{
    if (!g_leaders_url[0])
        return;
    lock_ready();
    EnterCriticalSection(&g_lock);
    if (g_pending_count < 0)
        pending_read();
    if (g_pending_count == PENDING_MOST) {              /* the oldest go first */
        __movsb((unsigned char*) g_pending[0], (const unsigned char*) g_pending[1], (PENDING_MOST - 1) * sizeof g_pending[0]);
        g_pending_count--;
    }
    wsprintfA(g_pending[g_pending_count++], "%d\t%d\t%s\t%d\t%s", kind, cc, track, ms, name);
    pending_write();
    LeaveCriticalSection(&g_lock);
    pending_kick();
}

/* ---- Laps ---- */

static void today(char* date)
{
    typedef void (WINAPI* local_time_fn)(SYSTEMTIME*);
    local_time_fn local_time = (local_time_fn) api("kernel32.dll", "GetLocalTime");
    SYSTEMTIME now = { 0 };

    if (local_time)
        local_time(&now);
    wsprintfA(date, "%04d-%02d-%02d", now.wYear, now.wMonth, now.wDay);
}

/* The race's board: its track, and for a Baja or enduro route that route. */
static void race_board(char* track)
{
    char name[48];

    lstrcpynA(name, race_track, sizeof name);
    if ((race_kind == 1 || race_kind == 5) && race_route)
        wsprintfA(track, "%s ROUTE %d", name, race_route);
    else
        lstrcpynA(track, name, 64);
}

/* A lap kept if it is the rider's best for its track, event and class: TRUE when it was. */
static BOOL lap_keep(int kind, int cc, const char* track, const char* name, int ms)
{
    record* r = NULL;
    int i;

    if (g_count < 0)
        records_read();
    for (i = 0; i < g_count; i++)
        if (g_records[i].kind == kind && g_records[i].cc == cc && same(g_records[i].track, track)
            && same(g_records[i].name, name)) {
            r = &g_records[i];
            break;
        }
    if (r && r->ms <= ms)
        return FALSE;
    if (!r) {
        if (g_count == RECORDS_MOST)
            return FALSE;
        r = &g_records[g_count++];
        r->kind = kind;
        r->cc = cc;
        lstrcpynA(r->track, track, sizeof r->track);
        lstrcpynA(r->name, name, sizeof r->name);
    }
    r->ms = ms;
    today(r->date);
    records_write();
    pending_add(kind, cc, track, ms, name);
    return TRUE;
}

void leaders_watch(void)
{
    static BYTE* watched;
    static unsigned laps;
    static DWORD off_since;         /* when the rider left the course, 0 on it */
    static BOOL void_lap;           /* this lap does not count */
    BYTE* race = game_main ? game_FUN_0045e300(FIELD(game_main, 0x570, BYTE*)) : NULL;
    BYTE* me = race ? FIELD(race, 0x38, BYTE*) : NULL;
    unsigned now;

    if (!me || playing_back || !lapped_kind(race_kind)) {
        watched = NULL;
        return;
    }
    now = rider_laps(me);
    if (me != watched || now < laps) {
        watched = me;
        laps = now;
        off_since = 0;
        void_lap = FALSE;
        pending_kick();             /* laps left from before go now, off the game's thread */
        return;
    }
    if ((race_kind == 2 || race_kind == 3) && !rider_on_course(me)) {
        if (!off_since)
            off_since = timeGetTime() | 1;
        else if (!void_lap && timeGetTime() - off_since > OFF_COURSE_MS) {
            void_lap = TRUE;
            log_line("leaderboard: off the course, lap %u not counted", now + 1);
        }
    } else {
        off_since = 0;
    }
    if (now == laps)
        return;
    laps = now;
    if (now >= 2 && !void_lap && rider_last(me) > 1.0f && profile_name[0]) {
        int ms = (int) (rider_last(me) * 1000 + 0.5f);
        char track[64];

        race_board(track);
        if (lap_keep(race_kind, bike_cc, track, profile_name, ms))
            log_line("leaderboard: %s's best lap of %s, %d.%03d s", profile_name, track, ms / 1000, ms % 1000);
    }
    void_lap = FALSE;
    off_since = rider_on_course(me) || (race_kind != 2 && race_kind != 3) ? 0 : timeGetTime() | 1;
}

/* ---- The boards ---- */

/* The server's answer for one event, class and track, read by a thread of its own. */
static struct {
    int    kind, cc;
    char   track[64];
    int    state;                           /* LEADERS_* */
    DWORD  asked;                           /* when */
    leader board[BOARD_MOST];
    int    count;
    char   tracks[TRACKS_FROM_SERVER][64];
    int    track_count;
} g_online;
static volatile LONG g_online_version;
static volatile LONG g_fetching;

#define ONLINE_FRESH_MS 30000               /* asked again after this */

static int __cdecl by_time(const void* a, const void* b)
{
    return ((const leader*) a)->ms - ((const leader*) b)->ms;
}

static DWORD WINAPI online_fetch(void* unused)
{
    static char answer[32768];
    static leader board[BOARD_MOST];
    static char tracks[TRACKS_FROM_SERVER][64];
    char path[400], word[200], *at, *line, *fields[3];
    int kind, cc, n = 0, t = 0;
    char track[64];
    BOOL ok;

    (void) unused;
    EnterCriticalSection(&g_lock);
    kind = g_online.kind;
    cc = g_online.cc;
    lstrcpynA(track, g_online.track, sizeof track);
    LeaveCriticalSection(&g_lock);

    wsprintfA(path, "/v1/tracks?kind=%d&class=%d", kind, cc);
    ok = server_ask("GET", path, NULL, answer, sizeof answer) == 200;
    for (at = answer; ok && (line = next_line(&at)) && t < TRACKS_FROM_SERVER;)
        if (split(line, fields, 2) >= 1 && fields[0][0])
            lstrcpynA(tracks[t++], fields[0], 64);
    if (ok && track[0]) {
        url_word(word, track, sizeof word);
        wsprintfA(path, "/v1/board?kind=%d&class=%d&track=%s", kind, cc, word);
        ok = server_ask("GET", path, NULL, answer, sizeof answer) == 200;
        for (at = answer; ok && (line = next_line(&at)) && n < BOARD_MOST;)
            if (split(line, fields, 3) == 3) {
                lstrcpynA(board[n].name, fields[0], sizeof board[n].name);
                board[n].ms = number(fields[1]);
                lstrcpynA(board[n].date, fields[2], sizeof board[n].date);
                n++;
            }
    }

    EnterCriticalSection(&g_lock);
    if (g_online.kind == kind && g_online.cc == cc && same(g_online.track, track)) {
        g_online.state = ok ? LEADERS_ONLINE : LEADERS_OFFLINE;
        __movsb((unsigned char*) g_online.board, (const unsigned char*) board, n * sizeof *board);
        g_online.count = n;
        __movsb((unsigned char*) g_online.tracks, (const unsigned char*) tracks, t * sizeof *tracks);
        g_online.track_count = t;
    }
    LeaveCriticalSection(&g_lock);
    InterlockedIncrement(&g_online_version);
    InterlockedExchange(&g_fetching, 0);
    return 0;
}

/* The server asked for the board, unless its answer for it is fresh or on its way. */
static void online_ask(int kind, int cc, const char* track)
{
    HANDLE thread;
    BOOL wanted;

    EnterCriticalSection(&g_lock);
    wanted = g_online.kind != kind || g_online.cc != cc || !same(g_online.track, track)
             || g_online.state == LEADERS_OFFLINE || (g_online.state == LEADERS_LOADING && !g_fetching)
             || timeGetTime() - g_online.asked > ONLINE_FRESH_MS;
    if (wanted) {
        if (g_online.kind != kind || g_online.cc != cc || !same(g_online.track, track)) {
            g_online.count = 0;
            if (g_online.kind != kind || g_online.cc != cc)
                g_online.track_count = 0;
        }
        g_online.kind = kind;
        g_online.cc = cc;
        lstrcpynA(g_online.track, track, sizeof g_online.track);
        g_online.state = LEADERS_LOADING;
        g_online.asked = timeGetTime();
    }
    LeaveCriticalSection(&g_lock);
    if (!wanted || InterlockedCompareExchange(&g_fetching, 1, 0))
        return;
    thread = CreateThread(NULL, 0, online_fetch, NULL, 0, NULL);
    if (thread)
        CloseHandle(thread);
    else
        InterlockedExchange(&g_fetching, 0);
}

LONG leaders_version(void)
{
    return g_online_version;
}

int leaders_board(int kind, int cc, const char* track, leader* board, int most, int* state)
{
    int n = 0, i;

    *state = LEADERS_LOCAL;
    if (g_leaders_url[0]) {
        lock_ready();
        pending_kick();
        online_ask(kind, cc, track);
        EnterCriticalSection(&g_lock);
        *state = g_online.state;
        if (g_online.state == LEADERS_ONLINE || g_online.count) {   /* or its last answer, asked again */
            *state = LEADERS_ONLINE;
            n = g_online.count < most ? g_online.count : most;
            __movsb((unsigned char*) board, (const unsigned char*) g_online.board, n * sizeof *board);
        }
        LeaveCriticalSection(&g_lock);
        if (*state == LEADERS_ONLINE)
            return n;
    }
    if (g_count < 0)
        records_read();
    for (i = 0; i < g_count && n < most; i++) {
        const record* r = &g_records[i];

        if (r->kind != kind || r->cc != cc || !same(r->track, track))
            continue;
        lstrcpynA(board[n].name, r->name, sizeof board[n].name);
        board[n].ms = r->ms;
        lstrcpynA(board[n].date, r->date, sizeof board[n].date);
        n++;
    }
    game_FUN_00539a96(board, n, sizeof *board, by_time);
    return n;
}

/* Into `tracks` (n of them already, at most `most`) a track not yet among them; the count. */
static int track_add_new(char (*tracks)[64], int n, int most, const char* track)
{
    int k;

    for (k = 0; k < n && !same(tracks[k], track); k++)
        ;
    if (k == n && n < most)
        lstrcpynA(tracks[n++], track, 64);
    return n;
}

int leaders_tracks(int kind, int cc, char (*tracks)[64], int n, int most)
{
    int i;

    if (g_count < 0)
        records_read();
    for (i = 0; i < g_count; i++)
        if (g_records[i].kind == kind && g_records[i].cc == cc)
            n = track_add_new(tracks, n, most, g_records[i].track);
    if (g_leaders_url[0]) {
        lock_ready();
        EnterCriticalSection(&g_lock);
        if (g_online.kind == kind && g_online.cc == cc)
            for (i = 0; i < g_online.track_count; i++)
                n = track_add_new(tracks, n, most, g_online.tracks[i]);
        LeaveCriticalSection(&g_lock);
    }
    return n;
}
