/*
    mcm2-lan.exe - MCM2:Reborn's LAN helper: mcm2-lan <pipe name> <rider's name>

    LAN play the way Internet play works: the same pipe (protocol.h) and the same DirectPlay of the
    game's (code/dev/eosplay.c), with the local network where mcm2-eos.exe has Epic Online
    Services. There is no signing in and no eos.ini, and nothing leaves the local network.

    A host listens for searches on LAN_PORT. A search is a broadcast there, and each host answers
    it with its lobby as TELL_SESSIONS shows one: its race, its riders and its chat, as Internet's
    lobbies have them. Joining is asked of the host, which answers with the lobby's other members,
    and from then on every member sends to every other directly, as over EOS P2P. The game's
    packets go whole, a datagram each: the game resends what is lost, and a LAN carries datagrams
    of EOS_DATA_MOST. Members tell each other they are there every PING_EVERY_MS, with their
    rider's name once the game has made its player; one not heard from for GONE_AFTER_MS has gone.
    It quits when the pipe closes.
*/

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "protocol.h"

#define LAN_PORT        28420               /* a host's, for searches */
#define LAN_MAGIC       0x4C324D4Du         /* "MM2L" */
#define LAN_VERSION     2                   /* one version of the game's netcode, as Internet's BUCKET */
#define GROUP           0x47525550u         /* eosplay.c's host group: no peer is numbered this */
#define PING_EVERY_MS   500
#define GONE_AFTER_MS   8000
#define FIND_WAIT_MS    500                 /* for the hosts' answers to a search */
#define FOUND_FOR_MS    10000               /* a host found is joinable by its lobby for this long */
#define JOIN_EVERY_MS   500
#define JOIN_WAIT_MS    5000
#define FOUND_MOST      16
#define PEERS_MOST      (EOS_RIDERS_MOST - 1)  /* the riders but ours */
#define BUFFER_SIZE     (1 << 20)           /* the sockets': 30 riders' packets, 20 a second each */

#pragma pack(push, 1)

/* What helpers send each other over UDP: a lan_head, then the kind's fields. */
enum {
    LAN_FIND = 1,       /* no fields: to LAN_PORT, broadcast */
    LAN_SESSION,        /* tell_session: a host's answer to a search */
    LAN_JOIN,           /* no fields: to the host, for the lobby in the head */
    LAN_WELCOME,        /* lan_welcome: the host's answer, the lobby's other members */
    LAN_REFUSE,         /* tell_failed: the host's answer when it cannot be joined */
    LAN_PING,           /* lan_ping: still here */
    LAN_DATA,           /* the game's packet */
    LAN_LEAVE,          /* no fields */
};

typedef struct lan_head {
    uint32_t magic;
    uint8_t  version, kind;
    uint16_t unused;
    uint32_t from;                          /* the sender's peer number */
    uint32_t lobby;                         /* its lobby's key (key_of), 0 when it is in none */
} lan_head;

typedef struct lan_member {
    uint32_t number;
    uint32_t address;                       /* as sockaddr_in has them */
    uint16_t port;
} lan_member;

typedef struct lan_welcome {
    char       lobby[EOS_LOBBY_ID_SIZE];
    uint32_t   count;
    lan_member members[PEERS_MOST];
} lan_welcome;

typedef struct lan_ping {
    char name[EOS_NAME_SIZE];               /* empty until the game has made its player */
} lan_ping;

#pragma pack(pop)

_Static_assert(sizeof(tell_session) <= EOS_DATA_MOST && sizeof(lan_welcome) <= EOS_DATA_MOST,
               "a LAN message is bigger than a packet");

static FILE* log_file;
static HANDLE pipe = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION asks_lock;
static BOOL quitting;

static SOCKET game_socket = INVALID_SOCKET;     /* everything but searches */
static SOCKET find_socket = INVALID_SOCKET;     /* hosting: searches, on LAN_PORT */
static uint32_t self;
static char self_name[EOS_NAME_SIZE];
static char player_name[EOS_NAME_SIZE];         /* the game's player, once it has made it */

/* The lobby we are in: hosting it, or joined. */
static char lobby_id[EOS_LOBBY_ID_SIZE];
static uint32_t lobby_key;                      /* also the one being joined */
static uint32_t owner;
static BOOL hosting;
static ask_host hosted;
static BOOL closed;
static ask_settings race;
static ask_chat chat;

/* Its other members. */
static struct peer {
    uint32_t number;
    struct sockaddr_in address;
    BOOL     named;                             /* its player told us its name */
    char     name[EOS_NAME_SIZE];
    DWORD    heard_at;
} peers[PEERS_MOST];
static int peer_count;

/* The hosts that answered searches, and how to reach each. */
static struct found {
    tell_session session;
    struct sockaddr_in address;
    DWORD heard_at;
} found[FOUND_MOST];
static int found_count;
static DWORD find_sent_at;
static BOOL finding;

static struct {
    BOOL  on;
    char  lobby[EOS_LOBBY_ID_SIZE];
    struct sockaddr_in host;
    DWORD started, sent_at;
} joining;

static DWORD pinged_at;

static void say(const char* fmt, ...)
{
    va_list args;

    if (!log_file)
        return;
    va_start(args, fmt);
    vfprintf(log_file, fmt, args);
    va_end(args);
    fputc('\n', log_file);
    fflush(log_file);
}

/* ---- The pipe, as mcm2-eos.exe has it ---- */

static BOOL pipe_io(BOOL write, void* data, DWORD size, DWORD* done)
{
    OVERLAPPED io = { 0 };
    BOOL ok;

    io.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok = write ? WriteFile(pipe, data, size, NULL, &io) : ReadFile(pipe, data, size, NULL, &io);
    if (ok || GetLastError() == ERROR_IO_PENDING)
        ok = GetOverlappedResult(pipe, &io, done, TRUE);
    CloseHandle(io.hEvent);
    return ok;
}

static void tell(uint8_t kind, const void* fields, uint32_t size, const void* more, uint32_t more_size)
{
    uint8_t message[5 + sizeof(tell_data) + EOS_DATA_MOST];
    uint32_t length = 1 + size + more_size;
    DWORD done;

    if (4 + length > sizeof message)
        return;
    memcpy(message, &length, 4);
    message[4] = kind;
    memcpy(message + 5, fields, size);
    memcpy(message + 5 + size, more, more_size);
    pipe_io(TRUE, message, 4 + length, &done);
}

static void tell_failure(const char* why)
{
    tell_failed failed = { 0 };

    strncpy(failed.why, why, sizeof failed.why - 1);
    say("failed: %s", why);
    tell(TELL_FAILED, &failed, sizeof failed, NULL, 0);
}

static struct ask {
    struct ask* next;
    uint8_t kind;
    uint32_t size;
    uint8_t fields[1];
} *asks_first, *asks_last;

static BOOL read_all(void* to, DWORD size)
{
    DWORD got;

    while (size) {
        if (!pipe_io(FALSE, to, size, &got) || !got)
            return FALSE;
        to = (char*) to + got;
        size -= got;
    }
    return TRUE;
}

static DWORD WINAPI pipe_reader(void* unused)
{
    (void) unused;
    for (;;) {
        uint32_t length;
        struct ask* ask;

        if (!read_all(&length, 4) || !length || length > sizeof(ask_send) + EOS_DATA_MOST + 1)
            break;
        ask = malloc(sizeof *ask + length);
        if (!ask || !read_all(&ask->kind, 1) || !read_all(ask->fields, length - 1))
            break;
        ask->size = length - 1;
        ask->next = NULL;
        EnterCriticalSection(&asks_lock);
        if (asks_last)
            asks_last->next = ask;
        else
            asks_first = ask;
        asks_last = ask;
        LeaveCriticalSection(&asks_lock);
    }
    quitting = TRUE;
    return 0;
}

static struct ask* next_ask(void)
{
    struct ask* ask;

    EnterCriticalSection(&asks_lock);
    ask = asks_first;
    if (ask) {
        asks_first = ask->next;
        if (!asks_first)
            asks_last = NULL;
    }
    LeaveCriticalSection(&asks_lock);
    return ask;
}

/* ---- Sending ---- */

/* A lobby's id as the heads carry it (eosplay.c's lobby_hash, never 0). */
static uint32_t key_of(const char* lobby)
{
    uint32_t hash = 2166136261u;

    while (*lobby)
        hash = (hash ^ (uint8_t) *lobby++) * 16777619u;
    return hash ? hash : 1;
}

static void send_packet(const struct sockaddr_in* to, uint8_t kind, const void* fields, uint32_t size)
{
    static uint8_t packet[sizeof(lan_head) + EOS_DATA_MOST];
    lan_head head = { LAN_MAGIC, LAN_VERSION, 0, 0, 0, 0 };

    if (size > EOS_DATA_MOST)
        return;
    head.kind = kind;
    head.from = self;
    head.lobby = lobby_key;
    memcpy(packet, &head, sizeof head);
    memcpy(packet + sizeof head, fields, size);
    sendto(game_socket, (const char*) packet, (int) (sizeof head + size), 0, (const struct sockaddr*) to,
           sizeof *to);
}

static void ping(struct peer* peer)
{
    lan_ping told = { 0 };

    memcpy(told.name, player_name, sizeof told.name);
    send_packet(&peer->address, LAN_PING, &told, sizeof told);
}

/*
    A search: to LAN_PORT on every network this computer is on, by each one's own broadcast
    address (Windows sends 255.255.255.255 out of one alone), and to this computer itself.
*/
static void find_send(void)
{
    INTERFACE_INFO interfaces[32];
    struct sockaddr_in to = { 0 };
    DWORD size = 0;
    DWORD i;

    to.sin_family = AF_INET;
    to.sin_port = htons(LAN_PORT);
    to.sin_addr.s_addr = htonl(INADDR_BROADCAST);
    send_packet(&to, LAN_FIND, NULL, 0);
    to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    send_packet(&to, LAN_FIND, NULL, 0);
    if (WSAIoctl(game_socket, SIO_GET_INTERFACE_LIST, NULL, 0, interfaces, sizeof interfaces, &size, NULL, NULL))
        return;
    for (i = 0; i < size / sizeof *interfaces; i++) {
        const INTERFACE_INFO* face = &interfaces[i];

        if (!(face->iiFlags & IFF_UP) || !(face->iiFlags & IFF_BROADCAST) || (face->iiFlags & IFF_LOOPBACK)
            || face->iiAddress.AddressIn.sin_family != AF_INET)
            continue;
        to.sin_addr.s_addr = face->iiAddress.AddressIn.sin_addr.s_addr | ~face->iiNetmask.AddressIn.sin_addr.s_addr;
        send_packet(&to, LAN_FIND, NULL, 0);
    }
}

/* An address as a log shows it, in one of two buffers. */
static const char* address_text(const struct sockaddr_in* address)
{
    static char texts[2][24];
    static int next;
    const uint8_t* ip = (const uint8_t*) &address->sin_addr;
    char* text = texts[next++ & 1];

    snprintf(text, sizeof texts[0], "%u.%u.%u.%u:%u", ip[0], ip[1], ip[2], ip[3], ntohs(address->sin_port));
    return text;
}

/* ---- Peers ---- */

static struct peer* peer_numbered(uint32_t number)
{
    int i;

    for (i = 0; i < peer_count; i++)
        if (peers[i].number == number)
            return &peers[i];
    return NULL;
}

static struct peer* peer_add(uint32_t number, const struct sockaddr_in* address)
{
    struct peer* peer = peer_numbered(number);

    if (peer || !number || number == self || number == GROUP || peer_count == PEERS_MOST)
        return peer;
    peer = &peers[peer_count++];
    memset(peer, 0, sizeof *peer);
    peer->number = number;
    peer->address = *address;
    peer->heard_at = GetTickCount();
    say("peer %08X joined the lobby, at %s", number, address_text(address));
    ping(peer);
    return peer;
}

static void lobby_clear(void)
{
    if (find_socket != INVALID_SOCKET)
        closesocket(find_socket);
    find_socket = INVALID_SOCKET;
    lobby_id[0] = 0;
    lobby_key = 0;
    owner = 0;
    hosting = FALSE;
    joining.on = FALSE;
    peer_count = 0;
}

/* The host gone: the lobby is over, as when Epic closes one. */
static void lobby_lost(void)
{
    say("the lobby closed");
    lobby_clear();
    tell(TELL_LOST, NULL, 0, NULL, 0);
}

static void peer_remove(struct peer* peer, const char* why)
{
    uint32_t number = peer->number;
    BOOL named = peer->named;

    say("peer %08X %s", number, why);
    *peer = peers[--peer_count];
    if (!hosting && number == owner) {
        lobby_lost();
        return;
    }
    if (named)
        tell(TELL_GONE, &number, 4, NULL, 0);
}

/* ---- Lobbies ---- */

/* Our lobby as a search shows it. */
static void session_of_ours(tell_session* session)
{
    int i, riders = 0;

    memset(session, 0, sizeof *session);
    memcpy(session->lobby, lobby_id, sizeof session->lobby);
    memcpy(session->name, hosted.name, sizeof session->name);
    session->players = 1 + peer_count;
    session->most = hosted.most;
    session->closed = closed;
    strncpy(session->host, player_name[0] ? player_name : self_name, EOS_NAME_SIZE - 1);
    memcpy(session->riders[riders++], session->host, EOS_NAME_SIZE);
    for (i = 0; i < peer_count; i++)
        if (peers[i].named)
            memcpy(session->riders[riders++], peers[i].name, EOS_NAME_SIZE);
    session->race = race;
    session->chat = chat;
}

/* Every text of a session heard, ended within its field. */
static void session_end_texts(tell_session* session)
{
    int i;

    session->lobby[sizeof session->lobby - 1] = 0;
    session->name[sizeof session->name - 1] = 0;
    session->host[sizeof session->host - 1] = 0;
    for (i = 0; i < EOS_RIDERS_MOST; i++)
        session->riders[i][EOS_NAME_SIZE - 1] = 0;
    session->race.track[sizeof session->race.track - 1] = 0;
    for (i = 0; i < EOS_CHAT_LINES; i++) {
        session->chat.lines[i].name[sizeof session->chat.lines[i].name - 1] = 0;
        session->chat.lines[i].text[sizeof session->chat.lines[i].text - 1] = 0;
    }
}

static void host(const ask_host* ask)
{
    struct sockaddr_in at = { 0 };
    u_long nonblocking = 1;
    BOOL yes = TRUE;

    lobby_clear();
    find_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    at.sin_family = AF_INET;
    at.sin_port = htons(LAN_PORT);
    at.sin_addr.s_addr = htonl(INADDR_ANY);
    if (find_socket == INVALID_SOCKET
        || setsockopt(find_socket, SOL_SOCKET, SO_REUSEADDR, (const char*) &yes, sizeof yes)
        || bind(find_socket, (const struct sockaddr*) &at, sizeof at)
        || ioctlsocket(find_socket, FIONBIO, &nonblocking)) {
        char why[96];

        snprintf(why, sizeof why, "could not listen for searches on UDP port %d (%d)", LAN_PORT, WSAGetLastError());
        lobby_clear();
        tell_failure(why);
        return;
    }
    hosted = *ask;
    hosted.name[sizeof hosted.name - 1] = 0;
    if (!hosted.most || hosted.most > EOS_RIDERS_MOST)
        hosted.most = EOS_RIDERS_MOST;
    snprintf(lobby_id, sizeof lobby_id, "LAN-%08X-%08lX", self, GetTickCount());
    lobby_key = key_of(lobby_id);
    owner = self;
    hosting = TRUE;
    closed = FALSE;
    memset(&race, 0, sizeof race);
    race.kind = -1;
    memset(&chat, 0, sizeof chat);
    say("hosting lobby %s, %u at most", lobby_id, hosted.most);
    {
        tell_lobby lobby = { 0 };

        memcpy(lobby.lobby, lobby_id, sizeof lobby.lobby);
        lobby.owner = self;
        tell(TELL_HOSTING, &lobby, sizeof lobby, NULL, 0);
    }
}

static void find(void)
{
    int i;

    for (i = 0; i < found_count; i++)           /* hosts gone quiet */
        if (GetTickCount() - found[i].heard_at > FOUND_FOR_MS)
            found[i--] = found[--found_count];
    find_sent_at = GetTickCount();
    finding = TRUE;
    find_send();
}

/* The answers to the last search, once they have had time to come. */
static void find_answer(void)
{
    tell_session sessions[EOS_SESSIONS_MOST];
    uint32_t count = 0;
    int i;

    finding = FALSE;
    for (i = 0; i < found_count && count < EOS_SESSIONS_MOST; i++)
        if ((int) (found[i].heard_at - find_sent_at) >= 0)
            sessions[count++] = found[i].session;
    say("found %u games", count);
    {
        uint8_t message[4 + sizeof sessions];

        memcpy(message, &count, 4);
        memcpy(message + 4, sessions, count * sizeof *sessions);
        tell(TELL_SESSIONS, message, 4 + count * (uint32_t) sizeof *sessions, NULL, 0);
    }
}

static void join(const ask_join* ask)
{
    char lobby[EOS_LOBBY_ID_SIZE];
    int i;

    memcpy(lobby, ask->lobby, sizeof lobby);
    lobby[sizeof lobby - 1] = 0;
    for (i = 0; i < found_count && strcmp(found[i].session.lobby, lobby); i++)
        ;
    if (i == found_count) {
        tell_failure("that game is no longer on the network");
        return;
    }
    lobby_clear();
    joining.on = TRUE;
    memcpy(joining.lobby, lobby, sizeof joining.lobby);
    joining.host = found[i].address;
    joining.started = GetTickCount();
    joining.sent_at = joining.started;
    lobby_key = key_of(lobby);
    say("joining lobby %s at %s", lobby, address_text(&joining.host));
    send_packet(&joining.host, LAN_JOIN, NULL, 0);
}

static void leave(void)
{
    int i, times;

    if (!lobby_id[0] && !joining.on)
        return;
    for (times = 0; times < 2; times++)        /* twice: a LEAVE lost leaves it to GONE_AFTER_MS */
        for (i = 0; i < peer_count; i++)
            send_packet(&peers[i].address, LAN_LEAVE, NULL, 0);
    say("left the lobby");
    lobby_clear();
}

/* ---- What other helpers send ---- */

static void joined(const lan_head* head, const lan_welcome* welcome, uint32_t size,
                   const struct sockaddr_in* from)
{
    uint32_t i;
    tell_lobby lobby = { 0 };

    if (size < offsetof(lan_welcome, members) || welcome->count > PEERS_MOST
        || size < offsetof(lan_welcome, members) + welcome->count * sizeof(lan_member))
        return;
    joining.on = FALSE;
    memcpy(lobby_id, joining.lobby, sizeof lobby_id);
    owner = head->from;
    memcpy(lobby.lobby, lobby_id, sizeof lobby.lobby);
    lobby.owner = owner;
    say("joined lobby %s, owned by %08X", lobby_id, owner);
    tell(TELL_JOINED, &lobby, sizeof lobby, NULL, 0);
    peer_add(owner, from);
    for (i = 0; i < welcome->count; i++) {
        struct sockaddr_in address = { 0 };

        address.sin_family = AF_INET;
        address.sin_addr.s_addr = welcome->members[i].address;
        address.sin_port = welcome->members[i].port;
        peer_add(welcome->members[i].number, &address);
    }
}

/* A rider asking to join our lobby: welcomed with its other members, or told why not. */
static void asked_to_join(uint32_t number, const struct sockaddr_in* from)
{
    struct peer* peer = peer_numbered(number);
    lan_welcome welcome;
    int i;

    if (!peer) {
        tell_failed refused = { 0 };

        if (closed)
            strcpy(refused.why, "the race has started");
        else if (1 + peer_count >= (int) hosted.most)
            strcpy(refused.why, "the lobby is full");
        if (refused.why[0]) {
            send_packet(from, LAN_REFUSE, &refused, sizeof refused);
            return;
        }
        peer = peer_add(number, from);
        if (!peer)
            return;
    }
    peer->address = *from;                      /* asked again: our welcome was lost */
    memset(&welcome, 0, sizeof welcome);
    memcpy(welcome.lobby, lobby_id, sizeof welcome.lobby);
    for (i = 0; i < peer_count; i++) {
        if (peers[i].number == number)
            continue;
        welcome.members[welcome.count].number = peers[i].number;
        welcome.members[welcome.count].address = peers[i].address.sin_addr.s_addr;
        welcome.members[welcome.count].port = peers[i].address.sin_port;
        welcome.count++;
    }
    send_packet(from, LAN_WELCOME, &welcome,
                (uint32_t) (offsetof(lan_welcome, members) + welcome.count * sizeof(lan_member)));
}

static void heard(SOCKET on, const uint8_t* packet, uint32_t size, const struct sockaddr_in* from)
{
    const lan_head* head = (const lan_head*) packet;
    const uint8_t* fields = packet + sizeof *head;
    struct peer* peer;

    if (size < sizeof *head || head->magic != LAN_MAGIC || head->version != LAN_VERSION || head->from == self)
        return;
    size -= sizeof *head;
    if (on == find_socket) {
        if (head->kind == LAN_FIND && hosting) {
            tell_session session;

            session_of_ours(&session);
            send_packet(from, LAN_SESSION, &session, sizeof session);
        }
        return;
    }
    if (head->kind == LAN_SESSION) {
        tell_session session;
        int i;

        if (!finding || size < sizeof session)
            return;
        memcpy(&session, fields, sizeof session);
        session_end_texts(&session);
        for (i = 0; i < found_count && strcmp(found[i].session.lobby, session.lobby); i++)
            ;
        if (i == found_count) {
            if (found_count == FOUND_MOST)
                return;
            found_count++;
        }
        found[i].session = session;
        found[i].address = *from;
        found[i].heard_at = GetTickCount();
        return;
    }
    if (!head->lobby || head->lobby != lobby_key)
        return;
    switch (head->kind) {
    case LAN_JOIN:
        if (hosting)
            asked_to_join(head->from, from);
        return;
    case LAN_WELCOME:
        if (joining.on)
            joined(head, (const lan_welcome*) fields, size, from);
        return;
    case LAN_REFUSE:
        if (joining.on && size >= sizeof(tell_failed)) {
            tell_failed refused;

            memcpy(&refused, fields, sizeof refused);
            refused.why[sizeof refused.why - 1] = 0;
            lobby_clear();
            tell_failure(refused.why);
        }
        return;
    }
    if (!lobby_id[0])
        return;
    peer = peer_numbered(head->from);
    /* A member we have not heard of yet has joined since: the host's welcome told it of us. */
    if (!peer && head->kind == LAN_PING && !hosting)
        peer = peer_add(head->from, from);
    if (!peer)
        return;
    peer->address = *from;
    peer->heard_at = GetTickCount();
    switch (head->kind) {
    case LAN_PING:
        if (!peer->named && size >= sizeof(lan_ping) && fields[0]) {
            tell_peer told = { 0 };

            told.peer = peer->number;
            memcpy(peer->name, fields, EOS_NAME_SIZE - 1);
            peer->name[EOS_NAME_SIZE - 1] = 0;
            peer->named = TRUE;
            memcpy(told.name, peer->name, sizeof told.name);
            say("peer %08X is %s", peer->number, peer->name);
            tell(TELL_PEER, &told, sizeof told, NULL, 0);
        }
        break;
    case LAN_DATA:
        {
            tell_data told = { 0 };

            told.from = peer->number;
            told.size = size;
            tell(TELL_DATA, &told, sizeof told, fields, size);
        }
        break;
    case LAN_LEAVE:
        peer_remove(peer, "left");
        break;
    }
}

static void receive_all(SOCKET on)
{
    static uint8_t packet[sizeof(lan_head) + EOS_DATA_MOST];

    while (on != INVALID_SOCKET) {
        struct sockaddr_in from;
        int from_size = sizeof from;
        int size = recvfrom(on, (char*) packet, sizeof packet, 0, (struct sockaddr*) &from, &from_size);

        if (size < 0) {
            if (WSAGetLastError() == WSAEMSGSIZE || WSAGetLastError() == WSAECONNRESET)
                continue;                       /* too big; or a port that refused a packet of ours */
            return;
        }
        heard(on, packet, (uint32_t) size, &from);
    }
}

/* What is due: pings, the members gone quiet, a join resent or given up, a search's answers. */
static void tick(void)
{
    DWORD now = GetTickCount();
    int i;

    if (lobby_id[0] && now - pinged_at >= PING_EVERY_MS) {
        pinged_at = now;
        for (i = 0; i < peer_count; i++)
            ping(&peers[i]);
    }
    for (i = 0; lobby_id[0] && i < peer_count; i++)
        if (now - peers[i].heard_at > GONE_AFTER_MS)
            peer_remove(&peers[i--], "went quiet");
    if (joining.on && now - joining.started > JOIN_WAIT_MS) {
        lobby_clear();
        tell_failure("the host did not answer");
    } else if (joining.on && now - joining.sent_at >= JOIN_EVERY_MS) {
        joining.sent_at = now;
        send_packet(&joining.host, LAN_JOIN, NULL, 0);
    }
    if (finding && now - find_sent_at >= FIND_WAIT_MS)
        find_answer();
}

/* ---- The game's asks ---- */

static void send_data(uint32_t to, const uint8_t* data, uint32_t size)
{
    int i;

    for (i = 0; i < peer_count; i++)
        if (!to || peers[i].number == to)
            send_packet(&peers[i].address, LAN_DATA, data, size);
}

static void answer(struct ask* ask)
{
    int i;

    switch (ask->kind) {
    case ASK_HOST:
        if (ask->size >= sizeof(ask_host))
            host((const ask_host*) ask->fields);
        break;
    case ASK_FIND:
        find();
        break;
    case ASK_JOIN:
        if (ask->size >= sizeof(ask_join))
            join((const ask_join*) ask->fields);
        break;
    case ASK_LEAVE:
        leave();
        break;
    case ASK_PLAYER:
        if (ask->size >= sizeof(ask_player)) {
            memcpy(player_name, ((const ask_player*) ask->fields)->name, EOS_NAME_SIZE - 1);
            for (i = 0; i < peer_count; i++)
                ping(&peers[i]);
        }
        break;
    case ASK_SEND:
        if (ask->size >= sizeof(ask_send) && lobby_id[0]) {
            const ask_send* send = (const ask_send*) ask->fields;

            if (send->size <= ask->size - sizeof *send && send->size <= EOS_DATA_MOST)
                send_data(send->to, ask->fields + sizeof *send, send->size);
        }
        break;
    case ASK_CLOSED:
    case ASK_OPENED:
        closed = ask->kind == ASK_CLOSED;
        break;
    case ASK_SETTINGS:
        if (ask->size >= sizeof(ask_settings)) {
            memcpy(&race, ask->fields, sizeof race);
            race.track[sizeof race.track - 1] = 0;
        }
        break;
    case ASK_CHAT:
        if (ask->size >= sizeof(ask_chat))
            memcpy(&chat, ask->fields, sizeof chat);
        break;
    }
}

/* ---- Start ---- */

/* Our peer number: any but 0 and the host group's, and unlike the other computers'. */
static uint32_t number_made(void)
{
    LARGE_INTEGER counter;
    uint32_t hash = 2166136261u;
    uint32_t parts[3];
    int i;

    QueryPerformanceCounter(&counter);
    parts[0] = counter.LowPart;
    parts[1] = GetCurrentProcessId();
    parts[2] = GetTickCount();
    for (i = 0; i < (int) sizeof parts; i++)
        hash = (hash ^ ((const uint8_t*) parts)[i]) * 16777619u;
    return hash && hash != GROUP ? hash : 1;
}

static BOOL sockets_start(void)
{
    WSADATA data;
    struct sockaddr_in at = { 0 };
    u_long nonblocking = 1;
    BOOL yes = TRUE;
    int buffer = BUFFER_SIZE;

    if (WSAStartup(MAKEWORD(2, 2), &data))
        return FALSE;
    game_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (game_socket == INVALID_SOCKET)
        return FALSE;
    at.sin_family = AF_INET;
    at.sin_addr.s_addr = htonl(INADDR_ANY);
    setsockopt(game_socket, SOL_SOCKET, SO_BROADCAST, (const char*) &yes, sizeof yes);
    setsockopt(game_socket, SOL_SOCKET, SO_RCVBUF, (const char*) &buffer, sizeof buffer);
    setsockopt(game_socket, SOL_SOCKET, SO_SNDBUF, (const char*) &buffer, sizeof buffer);
    if (bind(game_socket, (const struct sockaddr*) &at, sizeof at)
        || ioctlsocket(game_socket, FIONBIO, &nonblocking))
        return FALSE;
    {
        int size = sizeof at;

        getsockname(game_socket, (struct sockaddr*) &at, &size);
        say("on UDP port %u", ntohs(at.sin_port));
    }
    return TRUE;
}

int main(int argc, char** argv)
{
    char path[MAX_PATH];
    char* slash;

    if (argc < 3)
        return 2;
    GetModuleFileNameA(NULL, path, sizeof path);
    slash = strrchr(path, '\\');
    if (slash)
        strcpy(slash + 1, "mcm2-lan.log");
    log_file = fopen(path, "w");
    strncpy(self_name, argv[2], sizeof self_name - 1);

    pipe = CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        say("the pipe %s would not open (%lu)", argv[1], GetLastError());
        return 4;
    }
    InitializeCriticalSection(&asks_lock);
    if (!sockets_start()) {
        char why[64];

        snprintf(why, sizeof why, "the network would not start (%d)", WSAGetLastError());
        tell_failure(why);
        return 5;
    }
    self = number_made();
    {
        tell_ready ready = { 0 };

        ready.self = self;
        say("ready as peer %08X", self);
        tell(TELL_READY, &ready, sizeof ready, NULL, 0);
    }
    CloseHandle(CreateThread(NULL, 0, pipe_reader, NULL, 0, NULL));

    while (!quitting) {
        struct timeval wait = { 0, 2000 };
        struct ask* ask;
        fd_set readable;

        while ((ask = next_ask()) != NULL) {
            answer(ask);
            free(ask);
        }
        FD_ZERO(&readable);
        FD_SET(game_socket, &readable);
        if (find_socket != INVALID_SOCKET)
            FD_SET(find_socket, &readable);
        select(0, &readable, NULL, NULL, &wait);
        receive_all(game_socket);
        receive_all(find_socket);
        tick();
    }
    leave();
    closesocket(game_socket);
    WSACleanup();
    say("done");
    return 0;
}
