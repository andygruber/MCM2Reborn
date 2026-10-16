/*
    OpenMCM2 dev build - online play through Epic Online Services, under the game's own netcode.

    The game talks to the network only through IDirectPlay4A (Net.cpp, not ported). In online
    mode the DirectPlay object it makes is this one instead: the sessions it finds are the EOS
    lobbies hosted, opening one hosts or joins a lobby, and its messages go to the lobby's
    members over EOS P2P. The game's packets, its own acks and resends, its screens: all as they
    were. Epic's SDK is 64-bit only, so the EOS side is mcm2-eos.exe (code/eos/helper.c), started
    once and spoken to over a pipe (code/eos/protocol.h).

    Player ids are the helper's peer numbers. The host's group, the one group a session has, is
    GROUP. Only the slots the game calls do anything; the rest answer that they are unsupported.
*/

#include "dev.h"
#include "game.h"
#include "../eos/protocol.h"

#include <intrin.h>
#include <mmsystem.h>
#include <string.h>

#define HELPER          "mcm2-eos.exe"
#define GROUP           0x47525550u     /* the host's group: no peer is numbered this */
#define WAIT_MS         20000           /* for signing in, hosting and joining */
#define FIND_EVERY_MS   2000

#define DP_OK                   0
#define DPERR_GENERIC           ((HRESULT) 0x80004005)
#define DPERR_UNSUPPORTED       ((HRESULT) 0x80004001)
#define DPERR_BUFFERTOOSMALL    ((HRESULT) 0x8877001E)
#define DPERR_INVALIDPLAYER     ((HRESULT) 0x88770096)
#define DPERR_NOMESSAGES        ((HRESULT) 0x887700BE)
#define DPERR_NOSESSIONS        ((HRESULT) 0x887700E1)

#define DPOPEN_JOIN             1
#define DPOPEN_CREATE           2
#define DPENUMSESSIONS_AVAILABLE 1
#define DPESC_TIMEDOUT          1
#define DPSESSION_NEWPLAYERSDISABLED 1
#define DPPLAYERTYPE_GROUP      0
#define DPPLAYERTYPE_PLAYER     1
#define DPSYS_CREATEPLAYERORGROUP 3
#define DPSYS_DESTROYPLAYERORGROUP 5
#define DPSYS_SESSIONLOST       0x31

typedef struct dp_name {
    DWORD size, flags;
    char* short_name;
    char* long_name;
} dp_name;

typedef struct dp_session {         /* DPSESSIONDESC2 */
    DWORD size, flags;
    GUID  instance, application;
    DWORD most, players;
    char* name;
    char* password;
    DWORD reserved[2], user[4];
} dp_session;

typedef struct dp_caps {
    DWORD size, flags, buffer_most, queue_most, players_most, hundred_baud, latency, local_most,
          header, timeout;
} dp_caps;

typedef struct dp_create_player {   /* DPMSG_CREATEPLAYERORGROUP */
    DWORD   type, player_type, id, players;
    void*   data;
    DWORD   data_size;
    dp_name name;
    DWORD   parent, flags;
} dp_msg_create;

typedef struct dp_destroy_player {  /* DPMSG_DESTROYPLAYERORGROUP */
    DWORD   type, player_type, id;
    void*   local_data;
    DWORD   local_size;
    void*   remote_data;
    DWORD   remote_size;
    dp_name name;
    DWORD   parent, flags;
} dp_msg_destroy;

typedef BOOL (WINAPI* players_callback)(DWORD id, DWORD type, const dp_name* name, DWORD flags,
                                        void* context);
typedef BOOL (WINAPI* sessions_callback)(const dp_session* session, DWORD* timeout, DWORD flags,
                                         void* context);

typedef HANDLE (WINAPI* create_pipe_fn)(LPCSTR, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD,
                                        LPSECURITY_ATTRIBUTES);
typedef BOOL   (WINAPI* connect_pipe_fn)(HANDLE, LPOVERLAPPED);
typedef BOOL   (WINAPI* overlapped_result_fn)(HANDLE, LPOVERLAPPED, LPDWORD, BOOL);
typedef BOOL   (WINAPI* create_process_fn)(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES,
                                           LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR,
                                           LPSTARTUPINFOA, LPPROCESS_INFORMATION);
typedef DWORD  (WINAPI* process_id_fn)(void);
typedef BOOL   (WINAPI* duplicate_handle_fn)(HANDLE, HANDLE, HANDLE, LPHANDLE, DWORD, BOOL, DWORD);
typedef HRESULT (WINAPI* co_create_fn)(const GUID* clsid, void* outer, DWORD context, const GUID* iid, void** out);

extern co_create_fn _imp__CoCreateInstance;         /* the game's own import slot */

#define CLSID_DIRECTPLAY ((const GUID*) 0x0055BA50)
#define player_name      ((const char*) game_main + 0x588)

void __cdecl net_leave(void);
BOOL __cdecl net_make(void);

/* A message for the game's Receive. */
typedef struct message {
    struct message* next;
    DWORD from, size;
    BYTE  data[1];
} message;

static struct {
    CRITICAL_SECTION lock;              /* everything below */
    CRITICAL_SECTION writing;           /* the pipe's writes, a frame at a time */
    overlapped_result_fn overlapped_result;
    duplicate_handle_fn duplicate_handle;
    co_create_fn co_create;
    HANDLE pipe;                        /* NULL until the helper is started */
    HANDLE told;                        /* set on each answer the main thread may wait for */
    BOOL   online;                      /* the next DirectPlay the game makes is ours */
    BOOL   ready, broken, signed_out;  /* signed out: signing in failed */
    DWORD  self;                        /* our peer number */
    DWORD  last_find;

    BOOL   in_lobby, hosting, closed;
    DWORD  owner;
    BYTE   answer;                      /* the last TELL_HOSTING, _JOINED or _FAILED */
    char   lobby[EOS_LOBBY_ID_SIZE];
    char   name[EOS_SESSION_NAME_SIZE];

    tell_session sessions[EOS_SESSIONS_MOST];
    DWORD  session_count;

    struct { DWORD id; char name[EOS_NAME_SIZE]; } peers[16];
    int    peer_count;

    BOOL   player_made;
    char   player[EOS_NAME_SIZE];
    HANDLE receive_event;               /* our copy of the game's, given to CreatePlayer */
    message* first;
    message* last;
    DWORD  queued;
} eos;

/* memcpy: the game's CRT has none of its own (its copies are inline). */
static void copy(void* to, const void* from, size_t size)
{
    __movsb(to, from, size);
}

/* ---- The pipe ---- */

static BOOL pipe_io(BOOL write, void* data, DWORD size)
{
    OVERLAPPED io = { 0 };
    DWORD done = 0;
    BOOL ok;

    io.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok = write ? WriteFile(eos.pipe, data, size, NULL, &io) : ReadFile(eos.pipe, data, size, NULL, &io);
    if (ok || GetLastError() == ERROR_IO_PENDING)
        ok = eos.overlapped_result(eos.pipe, &io, &done, TRUE) && done == size;
    CloseHandle(io.hEvent);
    return ok;
}

static void ask(BYTE kind, const void* fields, DWORD size, const void* more, DWORD more_size)
{
    BYTE frame[5 + sizeof(ask_send) + EOS_DATA_MOST];
    DWORD length = 1 + size + more_size;

    if (!eos.pipe || eos.broken || 4 + length > sizeof frame)
        return;
    copy(frame, &length, 4);
    frame[4] = kind;
    copy(frame + 5, fields, size);
    copy(frame + 5 + size, more, more_size);
    EnterCriticalSection(&eos.writing);
    pipe_io(TRUE, frame, 4 + length);
    LeaveCriticalSection(&eos.writing);
}

/* ---- What the game will Receive (eos.lock held) ---- */

static void queue(DWORD from, const void* data, DWORD size)
{
    message* m = mem_alloc(sizeof *m + size);

    if (!m)
        return;
    m->from = from;
    m->size = size;
    copy(m->data, data, size);
    if (eos.last)
        eos.last->next = m;
    else
        eos.first = m;
    eos.last = m;
    eos.queued++;
    if (eos.receive_event)
        SetEvent(eos.receive_event);
}

static void queue_clear(void)
{
    while (eos.first) {
        message* m = eos.first;

        eos.first = m->next;
        mem_free(m);
    }
    eos.last = NULL;
    eos.queued = 0;
}

static void player_created(DWORD id)
{
    dp_msg_create m = { DPSYS_CREATEPLAYERORGROUP, DPPLAYERTYPE_PLAYER, 0 };

    m.id = id;
    m.players = eos.peer_count + 1;
    m.name.size = sizeof m.name;
    queue(0, &m, sizeof m);
}

static void player_destroyed(DWORD id)
{
    dp_msg_destroy m = { DPSYS_DESTROYPLAYERORGROUP, DPPLAYERTYPE_PLAYER, 0 };

    m.id = id;
    m.name.size = sizeof m.name;
    queue(0, &m, sizeof m);
}

static void lobby_lost(void)
{
    DWORD lost = DPSYS_SESSIONLOST;

    if (!eos.in_lobby)
        return;
    eos.in_lobby = FALSE;
    eos.peer_count = 0;
    queue(0, &lost, sizeof lost);
}

/* ---- The helper's news, read on a thread of its own ---- */

/* The fields each kind of news needs at least. */
static DWORD fields_size(BYTE kind)
{
    switch (kind) {
    case TELL_READY:    return sizeof(tell_ready);
    case TELL_FAILED:   return sizeof(tell_failed);
    case TELL_SESSIONS: return 4;
    case TELL_HOSTING:
    case TELL_JOINED:   return sizeof(tell_lobby);
    case TELL_PEER:     return sizeof(tell_peer);
    case TELL_GONE:     return 4;
    case TELL_DATA:     return sizeof(tell_data);
    default:            return 0;
    }
}

static void heard(BYTE kind, BYTE* fields, DWORD size)
{
    int i;

    if (size < fields_size(kind))
        return;
    EnterCriticalSection(&eos.lock);
    switch (kind) {
    case TELL_READY:
        eos.self = ((tell_ready*) fields)->self;
        eos.ready = TRUE;
        log_line("eos: signed in as %08X", eos.self);
        break;
    case TELL_FAILED:
        log_line("eos: %s", ((tell_failed*) fields)->why);
        eos.signed_out = !eos.ready;
        eos.answer = TELL_FAILED;
        break;
    case TELL_SESSIONS:
        copy(&eos.session_count, fields, 4);
        if (eos.session_count > (size - 4) / sizeof(tell_session))
            eos.session_count = (size - 4) / sizeof(tell_session);
        if (eos.session_count > EOS_SESSIONS_MOST)
            eos.session_count = EOS_SESSIONS_MOST;
        copy(eos.sessions, fields + 4, eos.session_count * sizeof(tell_session));
        break;
    case TELL_HOSTING:
    case TELL_JOINED:
        copy(eos.lobby, ((tell_lobby*) fields)->lobby, sizeof eos.lobby);
        eos.lobby[sizeof eos.lobby - 1] = 0;
        eos.owner = ((tell_lobby*) fields)->owner;
        eos.in_lobby = TRUE;
        eos.answer = kind;
        break;
    case TELL_PEER:
        if (eos.in_lobby && eos.peer_count < 16) {
            const tell_peer* peer = (const tell_peer*) fields;

            eos.peers[eos.peer_count].id = peer->peer;
            copy(eos.peers[eos.peer_count].name, peer->name, EOS_NAME_SIZE);
            eos.peers[eos.peer_count].name[EOS_NAME_SIZE - 1] = 0;
            eos.peer_count++;
            player_created(peer->peer);
        }
        break;
    case TELL_GONE:
        for (i = 0; i < eos.peer_count; i++) {
            if (eos.peers[i].id == *(DWORD*) fields) {
                eos.peers[i] = eos.peers[--eos.peer_count];
                player_destroyed(*(DWORD*) fields);
                break;
            }
        }
        break;
    case TELL_DATA:
        if (eos.in_lobby && ((tell_data*) fields)->size <= size - sizeof(tell_data))
            queue(((tell_data*) fields)->from, fields + sizeof(tell_data), ((tell_data*) fields)->size);
        break;
    case TELL_LOST:
        log_line("eos: the lobby closed");
        lobby_lost();
        break;
    }
    LeaveCriticalSection(&eos.lock);
    SetEvent(eos.told);
}

static DWORD WINAPI helper_reader(void* unused)
{
    static BYTE fields[sizeof(tell_data) + EOS_DATA_MOST];

    (void) unused;
    for (;;) {
        DWORD length;
        BYTE kind;

        if (!pipe_io(FALSE, &length, 4) || !length || length - 1 > sizeof fields
            || !pipe_io(FALSE, &kind, 1) || (length > 1 && !pipe_io(FALSE, fields, length - 1)))
            break;
        heard(kind, fields, length - 1);
    }
    log_line("eos: the helper stopped");
    EnterCriticalSection(&eos.lock);
    eos.broken = TRUE;
    eos.ready = FALSE;
    lobby_lost();
    LeaveCriticalSection(&eos.lock);
    SetEvent(eos.told);
    return 0;
}

/* mcm2-eos.exe started and connected; it signs in on its own. */
static BOOL helper_start(void)
{
    create_pipe_fn create_pipe = (create_pipe_fn) api("kernel32.dll", "CreateNamedPipeA");
    connect_pipe_fn connect_pipe = (connect_pipe_fn) api("kernel32.dll", "ConnectNamedPipe");
    create_process_fn create_process = (create_process_fn) api("kernel32.dll", "CreateProcessA");
    process_id_fn process_id = (process_id_fn) api("kernel32.dll", "GetCurrentProcessId");
    char pipe_name[64], command[160];
    STARTUPINFOA startup = { sizeof startup };
    PROCESS_INFORMATION process;
    OVERLAPPED io = { 0 };
    HANDLE pipe;
    DWORD done;
    BOOL connected;

    if (eos.pipe && !eos.broken)
        return TRUE;
    if (!create_pipe || !connect_pipe || !create_process || !process_id || !eos.overlapped_result)
        return FALSE;
    if (eos.pipe) {                     /* a helper that stopped: start another */
        CloseHandle(eos.pipe);
        eos.pipe = NULL;
    }
    wsprintfA(pipe_name, EOS_PIPE_FORMAT, process_id());
    pipe = create_pipe(pipe_name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT,
                       1, 0x10000, 0x10000, 0, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        log_line("eos: the pipe would not open");
        return FALSE;
    }
    wsprintfA(command, "%s %s \"%s\"", HELPER, pipe_name, player_name[0] ? player_name : "Rider");
    if (!create_process(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &startup,
                        &process)) {
        log_line("eos: " HELPER " would not start (%lu)", GetLastError());
        CloseHandle(pipe);
        return FALSE;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);

    io.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    connected = connect_pipe(pipe, &io) || GetLastError() == ERROR_PIPE_CONNECTED;
    if (!connected && GetLastError() == ERROR_IO_PENDING)
        connected = WaitForSingleObject(io.hEvent, 10000) == WAIT_OBJECT_0
                    && eos.overlapped_result(pipe, &io, &done, FALSE);
    CloseHandle(io.hEvent);
    if (!connected) {
        log_line("eos: " HELPER " did not connect; see mcm2-eos.log");
        CloseHandle(pipe);
        return FALSE;
    }
    eos.pipe = pipe;
    eos.broken = FALSE;
    eos.signed_out = FALSE;
    CloseHandle(CreateThread(NULL, 0, helper_reader, NULL, 0, NULL));
    log_line("eos: " HELPER " started");
    return TRUE;
}

/* Signed in, waiting for that if need be. */
static BOOL wait_until_ready(void)
{
    DWORD until = timeGetTime() + WAIT_MS;

    while (!eos.ready && !eos.broken && !eos.signed_out && (int) (until - timeGetTime()) > 0)
        WaitForSingleObject(eos.told, 250);
    return eos.ready;
}

/* Asked, and answered `wanted` rather than TELL_FAILED. */
static BOOL ask_and_wait(BYTE kind, const void* fields, DWORD size, BYTE wanted)
{
    DWORD until = timeGetTime() + WAIT_MS;

    if (!wait_until_ready())
        return FALSE;
    eos.answer = 0;
    ask(kind, fields, size, NULL, 0);
    while (!eos.answer && !eos.broken && (int) (until - timeGetTime()) > 0)
        WaitForSingleObject(eos.told, 250);
    return eos.answer == wanted;
}

/* ---- Sessions ---- */

static DWORD lobby_hash(const char* lobby)
{
    DWORD hash = 2166136261u;

    while (*lobby)
        hash = (hash ^ (BYTE) *lobby++) * 16777619u;
    return hash;
}

/* A lobby's instance GUID, for the game to hand back when it joins. */
static void lobby_guid(const char* lobby, GUID* guid)
{
    memset(guid, 0, sizeof *guid);
    guid->Data1 = lobby_hash(lobby);
    guid->Data2 = 'E';
    guid->Data3 = 'O';
}

/* ---- The DirectPlay object ---- */

typedef struct dplay {
    const FARPROC* vtable;
    LONG   references;
} dplay;

static HRESULT WINAPI dp_query(dplay* self, REFIID iid, void** out)
{
    (void) iid;
    InterlockedIncrement(&self->references);
    *out = self;
    return DP_OK;
}

static ULONG WINAPI dp_add_ref(dplay* self)
{
    return InterlockedIncrement(&self->references);
}

static HRESULT WINAPI dp_close(dplay* self)
{
    (void) self;
    EnterCriticalSection(&eos.lock);
    if (eos.in_lobby)
        ask(ASK_LEAVE, NULL, 0, NULL, 0);
    eos.in_lobby = FALSE;
    eos.hosting = FALSE;
    eos.closed = FALSE;
    eos.peer_count = 0;
    eos.player_made = FALSE;
    if (eos.receive_event)
        CloseHandle(eos.receive_event);
    eos.receive_event = NULL;
    queue_clear();
    LeaveCriticalSection(&eos.lock);
    return DP_OK;
}

static ULONG WINAPI dp_release(dplay* self)
{
    LONG left = InterlockedDecrement(&self->references);

    if (!left) {
        dp_close(self);
        mem_free(self);
    }
    return left;
}

static HRESULT WINAPI dp_add_player_to_group(dplay* self, DWORD group, DWORD player)
{
    (void) self;
    (void) player;
    return group == GROUP ? DP_OK : DPERR_INVALIDPLAYER;
}

static HRESULT WINAPI dp_create_group(dplay* self, DWORD* id, const dp_name* name, void* data,
                                      DWORD size, DWORD flags)
{
    (void) self;
    (void) name;
    (void) data;
    (void) size;
    (void) flags;
    *id = GROUP;
    return DP_OK;
}

static HRESULT WINAPI dp_create_player(dplay* self, DWORD* id, const dp_name* name, HANDLE event,
                                       void* data, DWORD size, DWORD flags)
{
    ask_player player = { 0 };

    (void) self;
    (void) data;
    (void) size;
    (void) flags;
    if (!eos.ready)
        return DPERR_GENERIC;
    if (name && name->short_name)
        lstrcpynA(player.name, name->short_name, sizeof player.name);
    EnterCriticalSection(&eos.lock);
    copy(eos.player, player.name, sizeof eos.player);
    eos.player_made = TRUE;
    if (eos.receive_event)
        CloseHandle(eos.receive_event);
    eos.receive_event = NULL;
    /* The game closes its handle before it closes DirectPlay: we signal a copy of our own. */
    if (event && eos.duplicate_handle)
        eos.duplicate_handle(GetCurrentProcess(), event, GetCurrentProcess(), &eos.receive_event, 0,
                             FALSE, DUPLICATE_SAME_ACCESS);
    if (eos.receive_event && eos.first)
        SetEvent(eos.receive_event);
    LeaveCriticalSection(&eos.lock);
    ask(ASK_PLAYER, &player, sizeof player, NULL, 0);
    *id = eos.self;
    return DP_OK;
}

static HRESULT WINAPI dp_destroy_player(dplay* self, DWORD id)
{
    (void) self;
    if (id == eos.self)
        eos.player_made = FALSE;
    return DP_OK;
}

static HRESULT WINAPI dp_enum_group_players(dplay* self, DWORD group, const GUID* instance,
                                            players_callback callback, void* context, DWORD flags)
{
    dp_name name = { sizeof name };

    (void) self;
    (void) instance;
    (void) flags;
    if (group != GROUP || !eos.in_lobby)
        return DPERR_INVALIDPLAYER;
    callback(eos.owner, DPPLAYERTYPE_PLAYER, &name, 0, context);
    return DP_OK;
}

static HRESULT WINAPI dp_enum_groups(dplay* self, const GUID* instance, players_callback callback,
                                     void* context, DWORD flags)
{
    dp_name name = { sizeof name };

    (void) self;
    (void) instance;
    (void) flags;
    if (eos.in_lobby)
        callback(GROUP, DPPLAYERTYPE_GROUP, &name, 0, context);
    return DP_OK;
}

static HRESULT WINAPI dp_enum_players(dplay* self, const GUID* instance, players_callback callback,
                                      void* context, DWORD flags)
{
    struct { DWORD id; char name[EOS_NAME_SIZE]; } players[17];
    int count = 0, i;

    (void) self;
    (void) instance;
    (void) flags;
    EnterCriticalSection(&eos.lock);
    if (eos.player_made) {
        players[count].id = eos.self;
        copy(players[count++].name, eos.player, EOS_NAME_SIZE);
    }
    for (i = 0; i < eos.peer_count; i++) {
        players[count].id = eos.peers[i].id;
        copy(players[count++].name, eos.peers[i].name, EOS_NAME_SIZE);
    }
    LeaveCriticalSection(&eos.lock);
    for (i = 0; i < count; i++) {
        dp_name name = { sizeof name, 0, players[i].name, NULL };

        if (!callback(players[i].id, DPPLAYERTYPE_PLAYER, &name, 0, context))
            break;
    }
    return DP_OK;
}

/* The lobbies last found; another search asked for, at most every FIND_EVERY_MS. */
static HRESULT WINAPI dp_enum_sessions(dplay* self, const dp_session* wanted, DWORD timeout,
                                       sessions_callback callback, void* context, DWORD flags)
{
    tell_session sessions[EOS_SESSIONS_MOST];
    DWORD count, i;

    (void) self;
    (void) wanted;
    if (eos.ready && timeGetTime() - eos.last_find >= FIND_EVERY_MS) {
        eos.last_find = timeGetTime();
        ask(ASK_FIND, NULL, 0, NULL, 0);
    }
    EnterCriticalSection(&eos.lock);
    count = eos.session_count;
    copy(sessions, eos.sessions, count * sizeof *sessions);
    LeaveCriticalSection(&eos.lock);

    for (i = 0; i < count; i++) {
        const tell_session* found = &sessions[i];
        dp_session session = { sizeof session };
        char name[EOS_SESSION_NAME_SIZE + 16];

        if ((flags & DPENUMSESSIONS_AVAILABLE) && (found->closed || found->players >= found->most))
            continue;
        wsprintfA(name, "%.63s  (%lu/%lu)", found->name[0] ? found->name : "Race", found->players, found->most);
        lobby_guid(found->lobby, &session.instance);
        session.flags = found->closed ? DPSESSION_NEWPLAYERSDISABLED : 0;
        session.most = found->most;
        session.players = found->players;
        session.name = name;
        if (!callback(&session, &timeout, 0, context))
            return DP_OK;
    }
    callback(NULL, &timeout, DPESC_TIMEDOUT, context);
    return DP_OK;
}

static HRESULT WINAPI dp_get_caps(dplay* self, dp_caps* caps, DWORD flags)
{
    (void) self;
    (void) flags;
    memset(caps, 0, sizeof *caps);
    caps->size = sizeof *caps;
    caps->buffer_most = EOS_DATA_MOST;
    caps->players_most = 8;
    caps->latency = 50;
    caps->local_most = 1;
    caps->timeout = 5000;
    return DP_OK;
}

static HRESULT WINAPI dp_get_message_count(dplay* self, DWORD player, DWORD* count)
{
    (void) self;
    (void) player;
    *count = eos.queued;
    return DP_OK;
}

static HRESULT WINAPI dp_get_player_name(dplay* self, DWORD id, void* data, DWORD* size)
{
    char name[EOS_NAME_SIZE] = "";
    DWORD needed;
    int i;

    (void) self;
    EnterCriticalSection(&eos.lock);
    if (id == eos.self)
        copy(name, eos.player, sizeof name);
    for (i = 0; i < eos.peer_count; i++)
        if (eos.peers[i].id == id)
            copy(name, eos.peers[i].name, sizeof name);
    LeaveCriticalSection(&eos.lock);
    if (!name[0] && id != eos.self)
        return DPERR_INVALIDPLAYER;
    needed = sizeof(dp_name) + lstrlenA(name) + 1;
    if (!data || *size < needed) {
        *size = needed;
        return DPERR_BUFFERTOOSMALL;
    }
    {
        dp_name* out = data;

        out->size = sizeof *out;
        out->flags = 0;
        out->short_name = (char*) (out + 1);
        out->long_name = NULL;
        lstrcpyA(out->short_name, name);
    }
    return DP_OK;
}

static HRESULT WINAPI dp_get_session_desc(dplay* self, void* data, DWORD* size)
{
    dp_session* session = data;

    (void) self;
    if (!data || *size < sizeof *session) {
        *size = sizeof *session;
        return DPERR_BUFFERTOOSMALL;
    }
    memset(session, 0, sizeof *session);
    session->size = sizeof *session;
    session->flags = eos.closed ? DPSESSION_NEWPLAYERSDISABLED : 0;
    lobby_guid(eos.lobby, &session->instance);
    session->most = 8;
    session->players = eos.peer_count + 1;
    session->name = eos.name;
    return DP_OK;
}

static HRESULT WINAPI dp_open(dplay* self, const dp_session* session, DWORD flags)
{
    (void) self;
    if (flags & DPOPEN_CREATE) {
        ask_host host = { 0 };

        host.most = session->most ? session->most : 8;
        if (session->name)
            lstrcpynA(host.name, session->name, sizeof host.name);
        lstrcpynA(eos.name, host.name, sizeof eos.name);
        if (!ask_and_wait(ASK_HOST, &host, sizeof host, TELL_HOSTING))
            return DPERR_GENERIC;
        eos.hosting = TRUE;
        return DP_OK;
    }
    if (flags & DPOPEN_JOIN) {
        ask_join join = { 0 };
        DWORD i;

        EnterCriticalSection(&eos.lock);
        for (i = 0; i < eos.session_count; i++)
            if (lobby_hash(eos.sessions[i].lobby) == session->instance.Data1) {
                copy(join.lobby, eos.sessions[i].lobby, sizeof join.lobby);
                copy(eos.name, eos.sessions[i].name, sizeof eos.name);
            }
        LeaveCriticalSection(&eos.lock);
        if (!join.lobby[0])
            return DPERR_NOSESSIONS;
        if (!ask_and_wait(ASK_JOIN, &join, sizeof join, TELL_JOINED))
            return DPERR_GENERIC;
        eos.hosting = FALSE;
        return DP_OK;
    }
    return DPERR_UNSUPPORTED;
}

/* The next message: from 0 for the system's. */
static HRESULT WINAPI dp_receive(dplay* self, DWORD* from, DWORD* to, DWORD flags, void* data,
                                 DWORD* size)
{
    message* m;

    (void) self;
    (void) flags;
    EnterCriticalSection(&eos.lock);
    m = eos.first;
    if (!m) {
        LeaveCriticalSection(&eos.lock);
        return DPERR_NOMESSAGES;
    }
    if (!data || *size < m->size) {
        *size = m->size;
        LeaveCriticalSection(&eos.lock);
        return DPERR_BUFFERTOOSMALL;
    }
    eos.first = m->next;
    if (!eos.first)
        eos.last = NULL;
    eos.queued--;
    LeaveCriticalSection(&eos.lock);
    *from = m->from;
    *to = eos.self;
    *size = m->size;
    copy(data, m->data, m->size);
    mem_free(m);
    return DP_OK;
}

static HRESULT WINAPI dp_set_session_desc(dplay* self, const dp_session* session, DWORD flags)
{
    BOOL closed = (session->flags & DPSESSION_NEWPLAYERSDISABLED) != 0;

    (void) self;
    (void) flags;
    if (eos.hosting && closed != eos.closed) {
        eos.closed = closed;
        ask(closed ? ASK_CLOSED : ASK_OPENED, NULL, 0, NULL, 0);
    }
    return DP_OK;
}

static HRESULT WINAPI dp_enum_connections(dplay* self, const GUID* application, void* callback,
                                          void* context, DWORD flags)
{
    (void) self;
    (void) application;
    (void) callback;
    (void) context;
    (void) flags;
    return DP_OK;
}

static HRESULT WINAPI dp_initialize_connection(dplay* self, void* connection, DWORD flags)
{
    (void) self;
    (void) connection;
    (void) flags;
    return DP_OK;
}

/* To a player, or to every other one (0, or the host's group). */
static HRESULT WINAPI dp_send_ex(dplay* self, DWORD from, DWORD to, DWORD flags, const void* data,
                                 DWORD size, DWORD priority, DWORD timeout, void* context,
                                 DWORD* id)
{
    ask_send send;

    (void) self;
    (void) from;
    (void) flags;
    (void) priority;
    (void) timeout;
    (void) context;
    if (id)
        *id = 0;
    if (!eos.in_lobby || size > EOS_DATA_MOST)
        return DPERR_GENERIC;
    send.to = to == GROUP ? 0 : to;
    send.size = size;
    ask(ASK_SEND, &send, sizeof send, data, size);
    return DP_OK;
}

/* The slots the game never calls, by how many arguments each takes after `this`. */
static HRESULT WINAPI dp_no_1(void* s, DWORD a) { (void) s; (void) a; return DPERR_UNSUPPORTED; }
static HRESULT WINAPI dp_no_2(void* s, DWORD a, DWORD b)
{ (void) s; (void) a; (void) b; return DPERR_UNSUPPORTED; }
static HRESULT WINAPI dp_no_3(void* s, DWORD a, DWORD b, DWORD c)
{ (void) s; (void) a; (void) b; (void) c; return DPERR_UNSUPPORTED; }
static HRESULT WINAPI dp_no_4(void* s, DWORD a, DWORD b, DWORD c, DWORD d)
{ (void) s; (void) a; (void) b; (void) c; (void) d; return DPERR_UNSUPPORTED; }
static HRESULT WINAPI dp_no_5(void* s, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e)
{ (void) s; (void) a; (void) b; (void) c; (void) d; (void) e; return DPERR_UNSUPPORTED; }
static HRESULT WINAPI dp_no_6(void* s, DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f)
{ (void) s; (void) a; (void) b; (void) c; (void) d; (void) e; (void) f; return DPERR_UNSUPPORTED; }

/* IDirectPlay4A, in its slots' order. */
static const FARPROC dplay_vtable[] = {
    (FARPROC) dp_query, (FARPROC) dp_add_ref, (FARPROC) dp_release,
    (FARPROC) dp_add_player_to_group,     /* 3 */
    (FARPROC) dp_close,
    (FARPROC) dp_create_group,
    (FARPROC) dp_create_player,
    (FARPROC) dp_no_2,                    /* DeletePlayerFromGroup */
    (FARPROC) dp_no_1,                    /* DestroyGroup */
    (FARPROC) dp_destroy_player,
    (FARPROC) dp_enum_group_players,      /* 10 */
    (FARPROC) dp_enum_groups,
    (FARPROC) dp_enum_players,
    (FARPROC) dp_enum_sessions,
    (FARPROC) dp_get_caps,
    (FARPROC) dp_no_4,                    /* GetGroupData */
    (FARPROC) dp_no_3,                    /* GetGroupName */
    (FARPROC) dp_get_message_count,
    (FARPROC) dp_no_3,                    /* GetPlayerAddress */
    (FARPROC) dp_no_3,                    /* GetPlayerCaps */
    (FARPROC) dp_no_4,                    /* 20: GetPlayerData */
    (FARPROC) dp_get_player_name,
    (FARPROC) dp_get_session_desc,
    (FARPROC) dp_no_1,                    /* Initialize */
    (FARPROC) dp_open,
    (FARPROC) dp_receive,
    (FARPROC) dp_no_5,                    /* Send */
    (FARPROC) dp_no_4,                    /* SetGroupData */
    (FARPROC) dp_no_3,                    /* SetGroupName */
    (FARPROC) dp_no_4,                    /* SetPlayerData */
    (FARPROC) dp_no_3,                    /* 30: SetPlayerName */
    (FARPROC) dp_set_session_desc,
    (FARPROC) dp_no_2,                    /* AddGroupToGroup */
    (FARPROC) dp_no_6,                    /* CreateGroupInGroup */
    (FARPROC) dp_no_2,                    /* DeleteGroupFromGroup */
    (FARPROC) dp_enum_connections,
    (FARPROC) dp_no_5,                    /* EnumGroupsInGroup */
    (FARPROC) dp_no_4,                    /* GetGroupConnectionSettings */
    (FARPROC) dp_initialize_connection,
    (FARPROC) dp_no_4,                    /* SecureOpen */
    (FARPROC) dp_no_4,                    /* 40: SendChatMessage */
    (FARPROC) dp_no_3,                    /* SetGroupConnectionSettings */
    (FARPROC) dp_no_2,                    /* StartSession */
    (FARPROC) dp_no_2,                    /* GetGroupFlags */
    (FARPROC) dp_no_2,                    /* GetGroupParent */
    (FARPROC) dp_no_4,                    /* GetPlayerAccount */
    (FARPROC) dp_no_2,                    /* GetPlayerFlags */
    (FARPROC) dp_no_2,                    /* GetGroupOwner */
    (FARPROC) dp_no_2,                    /* SetGroupOwner */
    (FARPROC) dp_send_ex,
    (FARPROC) dp_no_5,                    /* 50: GetMessageQueue */
    (FARPROC) dp_no_2,                    /* CancelMessage */
    (FARPROC) dp_no_3,                    /* CancelPriority */
};
_Static_assert(sizeof dplay_vtable / sizeof dplay_vtable[0] == 53, "IDirectPlay4A has 53 slots");

static HRESULT WINAPI co_create(const GUID* clsid, void* outer, DWORD context, const GUID* iid, void** out)
{
    dplay* made;

    if (!eos.online || memcmp(clsid, CLSID_DIRECTPLAY, sizeof(GUID)))
        return eos.co_create(clsid, outer, context, iid, out);
    made = mem_alloc(sizeof *made);
    if (!made)
        return E_OUTOFMEMORY;
    made->vtable = dplay_vtable;
    made->references = 1;
    *out = made;
    return S_OK;
}

/* ---- Switching ---- */

void eos_install(void)
{
    co_create_fn mine = co_create;

    InitializeCriticalSection(&eos.lock);
    InitializeCriticalSection(&eos.writing);
    eos.told = CreateEventA(NULL, FALSE, FALSE, NULL);
    eos.overlapped_result = (overlapped_result_fn) api("kernel32.dll", "GetOverlappedResult");
    eos.duplicate_handle = (duplicate_handle_fn) api("kernel32.dll", "DuplicateHandle");
    eos.co_create = _imp__CoCreateInstance;
    if (!write_protected(&_imp__CoCreateInstance, &mine, sizeof mine))
        log_line("eos: could not take over CoCreateInstance; online play is off");
}

BOOL eos_online(BOOL on)
{
    if (on == eos.online)
        return TRUE;
    if (on && !helper_start())
        return FALSE;
    eos.online = on;
    net_leave();
    return net_make();
}
