/*
    OpenMCM2 dev build - the lap leaderboards (the LEADERBOARDS screen, dlgprocs.c).

    A lap counts from the second on: the first starts from the gate, so the lap timed is each one
    finished after it (the rider crossing into the third lap, and on). Each rider has one time a
    track, event and bike class - the best - kept when beaten and never when slower: in
    leaderboard.txt beside the game for this computer, and, with [leaderboard] url, on that server
    as well (tools/leaderboard_server.py), whose boards the screen shows instead when it answers.

    The server: POST <url>/lap with "kind class track ms name" (tab separated) keeps a better
    lap; GET <url>/board?kind=&class=&track= answers a line "name<TAB>ms<TAB>date" a rider, best
    first. Event kinds are race_kind's: 1 Baja, 2 national, 3 supercross, 5 enduro (quarry and
    tag are timed, not raced in laps).
*/

#include "dev.h"
#include "game.h"

#define LEADERS_FILE    "leaderboard.txt"
#define RECORDS_MOST    4096

#define race_kind       FIELD(game_main, 0x2D74, int)
#define race_track      ((const char*) game_main + 0x2DA6)
#define bike_cc         FIELD(game_main, 0x1550, int)
#define playing_back    FIELD(game_main, 0x3428, int)
#define profile_name    ((const char*) game_main + 0x578)

#define rider_last(r)   FIELD(r, 0x74C, float)
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

static void records_read(void)
{
    HANDLE file;
    static char text[RECORDS_MOST * 120];
    DWORD size = 0;
    char* line;

    g_count = 0;
    file = CreateFileA(LEADERS_FILE, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return;
    ReadFile(file, text, sizeof text - 1, &size, NULL);
    CloseHandle(file);
    text[size] = 0;
    for (line = text; *line && g_count < RECORDS_MOST;) {
        char* end = line;
        char* fields[6];

        while (*end && *end != '\n')
            end++;
        if (*end)
            *end++ = 0;
        if (end > line && end[-1] == '\r')
            end[-1] = 0;
        if (split(line, fields, 6) == 6) {
            record* r = &g_records[g_count++];

            r->kind = number(fields[0]);
            r->cc = number(fields[1]);
            lstrcpynA(r->track, fields[2], sizeof r->track);
            r->ms = number(fields[3]);
            lstrcpynA(r->name, fields[4], sizeof r->name);
            lstrcpynA(r->date, fields[5], sizeof r->date);
        }
        line = end;
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

#define ASK_WAIT_MS     2500    /* each of resolving, connecting, sending and answering */
#define STATUS_NUMBER   (19 | 0x20000000)   /* WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER */

/* `method` `path` on the server, `body` sent; its answer into `answer` (room `size`): FALSE when
   there is no server or it did not answer. http:// or https://host[:port]/base in the url. */
static BOOL server_ask(const char* method, const char* path, const char* body, char* answer, int size)
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
    WCHAR host[128], wide_path[512], wide_method[8];
    const char* at = g_leaders_url;
    BOOL secure = FALSE, ok = FALSE;
    int port, n = 0, got = 0;
    void *session, *link, *call;
    char full[512];

    if (!g_leaders_url[0] || !open || !connect || !request || !send || !receive || !read || !close || !timeouts || !headers)
        return FALSE;
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
    wsprintfA(full, "%s%s", at, path);
    MultiByteToWideChar(CP_UTF8, 0, full, -1, wide_path, 512);
    MultiByteToWideChar(CP_UTF8, 0, method, -1, wide_method, 8);

    session = open(L"MCM2Reborn", 0, NULL, NULL, 0);
    if (session)
        timeouts(session, ASK_WAIT_MS, ASK_WAIT_MS, ASK_WAIT_MS, ASK_WAIT_MS);
    link = session ? connect(session, host, (WORD) port, 0) : NULL;
    call = link ? request(link, wide_method, wide_path, NULL, NULL, NULL, secure ? 0x00800000 : 0) : NULL;
    if (call && send(call, NULL, 0, (void*) body, body ? lstrlenA(body) : 0, body ? lstrlenA(body) : 0, 0)
        && receive(call, NULL)) {
        DWORD read_now = 0, status = 0, room = sizeof status;

        headers(call, STATUS_NUMBER, NULL, &status, &room, NULL);
        while (got < size - 1 && read(call, answer + got, size - 1 - got, &read_now) && read_now)
            got += read_now;
        ok = status == 200;
    }
    if (answer)
        answer[got] = 0;
    if (call)
        close(call);
    if (link)
        close(link);
    if (session)
        close(session);
    return ok;
}

/* The laps on their way to the server, each sent by a thread of its own: room for a few at once. */
static char g_sending[8][160];
static LONG g_send_next;

static DWORD WINAPI lap_send(void* line)
{
    char answer[64];

    server_ask("POST", "/lap", (const char*) line, answer, sizeof answer);
    return 0;
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
    if (g_leaders_url[0]) {
        char* line = g_sending[InterlockedIncrement(&g_send_next) & 7];

        wsprintfA(line, "%d\t%d\t%s\t%d\t%s", kind, cc, track, ms, name);
        CloseHandle(CreateThread(NULL, 0, lap_send, line, 0, NULL));
    }
    return TRUE;
}

void leaders_watch(void)
{
    static BYTE* watched;
    static unsigned laps;
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
        return;
    }
    if (now == laps)
        return;
    laps = now;
    if (now >= 2 && rider_last(me) > 1.0f && profile_name[0]) {
        int ms = (int) (rider_last(me) * 1000 + 0.5f);

        if (lap_keep(race_kind, bike_cc, race_track, profile_name, ms))
            log_line("leaderboard: %s's best lap of %s, %d.%03d s", profile_name, race_track, ms / 1000, ms % 1000);
    }
}

/* ---- The boards ---- */

static int __cdecl by_time(const void* a, const void* b)
{
    return ((const leader*) a)->ms - ((const leader*) b)->ms;
}

int leaders_board(int kind, int cc, const char* track, leader* board, int most, BOOL* online)
{
    static char answer[16384];
    char path[200];
    int n = 0, i;

    *online = FALSE;
    if (g_leaders_url[0]) {
        wsprintfA(path, "/board?kind=%d&class=%d&track=%s", kind, cc, track);
        for (i = 0; path[i]; i++)
            if (path[i] == ' ')
                path[i] = '+';
        if (server_ask("GET", path, NULL, answer, sizeof answer)) {
            char* line = answer;

            *online = TRUE;
            while (*line && n < most) {
                char* end = line;
                char* fields[3];

                while (*end && *end != '\n')
                    end++;
                if (*end)
                    *end++ = 0;
                if (split(line, fields, 3) == 3) {
                    lstrcpynA(board[n].name, fields[0], sizeof board[n].name);
                    board[n].ms = number(fields[1]);
                    lstrcpynA(board[n].date, fields[2], sizeof board[n].date);
                    n++;
                }
                line = end;
            }
            return n;
        }
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

int leaders_tracks(int kind, int cc, char (*tracks)[64], int most)
{
    int n = 0, i, k;

    if (g_count < 0)
        records_read();
    for (i = 0; i < g_count && n < most; i++) {
        if (g_records[i].kind != kind || g_records[i].cc != cc)
            continue;
        for (k = 0; k < n && !same(tracks[k], g_records[i].track); k++)
            ;
        if (k == n)
            lstrcpynA(tracks[n++], g_records[i].track, 64);
    }
    return n;
}
