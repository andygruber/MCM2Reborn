/*
    mcm2-eos.exe - MCM2:Reborn's Epic Online Services helper: mcm2-eos <pipe name> <rider's name>

    The game is 32-bit and Epic's SDK is 64-bit only, so the game starts this and talks to it
    over the pipe it names (protocol.h). It signs in with a device id (no Epic account), makes,
    finds and joins lobbies, and carries the game's packets to the lobby's members over P2P:
    small ones as they are (the game resends what is lost), larger ones in pieces on a
    reliable channel, put back together here. It quits when the pipe closes.

    Settings come from eos.ini beside it: product, sandbox, deployment, client_id, client_secret.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "eos_sdk.h"
#include "eos_logging.h"
#include "eos_connect.h"
#include "eos_lobby.h"
#include "eos_p2p.h"

#include "protocol.h"

#define BUCKET          "mcm2reborn:1"      /* one game, one version of its netcode */
#define ATTR_NAME       "NAME"
#define ATTR_CLOSED     "CLOSED"
#define ATTR_RIDER      "RIDER"             /* a member's: its rider's name */
#define ATTR_KIND       "KIND"              /* the race the host has set up */
#define ATTR_LAPS       "LAPS"
#define ATTR_TREES      "TREES"
#define ATTR_TRACK      "TRACK"
#define ATTR_CHAT       "CHAT"              /* its lines, each a name, a tab, the text and a new line */
#define SOCKET_NAME     "MCM2"
#define CHANNEL_SMALL   0                   /* a whole packet, unreliable */
#define CHANNEL_PIECES  1                   /* a larger one's pieces, reliable and in order */
#define CHANNEL_HELLO   2                   /* a peer's player name, reliable */
#define PIECE_DATA      1100                /* under EOS_P2P_MAX_PACKET_SIZE, with a header */
#define PEERS_MOST      16

static FILE* log_file;
static HANDLE pipe = INVALID_HANDLE_VALUE;
static CRITICAL_SECTION asks_lock;
static BOOL quitting;

static EOS_HPlatform platform;
static EOS_HConnect connect_api;
static EOS_HLobby lobby_api;
static EOS_HP2P p2p;
static EOS_ProductUserId self;
static char self_name[EOS_NAME_SIZE];
static char player_name[EOS_NAME_SIZE];   /* the game's player, once it has made it */
static char lobby_id[EOS_LOBBY_ID_SIZE];
static EOS_P2P_SocketId socket_id = { EOS_P2P_SOCKETID_API_LATEST, SOCKET_NAME };

/* The lobby's other members. */
static struct peer {
    EOS_ProductUserId user;
    uint32_t number;
    BOOL     named;          /* its player told us its name */
    uint8_t  pieces[EOS_DATA_MOST];
    uint32_t pieces_have, pieces_want;
} peers[PEERS_MOST];
static int peer_count;

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

/* ---- The pipe ---- */

/* One read or write, waited for. The pipe is opened overlapped: on a synchronous handle the
   reader thread's waiting read would hold up every write. */
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

/* The asks read off the pipe, waiting for the main loop (which alone calls EOS). */
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

/* ---- Peers ---- */

static uint32_t number_of(EOS_ProductUserId user)
{
    char text[EOS_PRODUCTUSERID_MAX_LENGTH + 1];
    int32_t size = sizeof text;
    uint32_t hash = 2166136261u;
    const char* c;

    if (EOS_ProductUserId_ToString(user, text, &size) != EOS_Success)
        return 0;
    for (c = text; *c; c++)
        hash = (hash ^ (uint8_t) *c) * 16777619u;
    return hash ? hash : 1;
}

static struct peer* peer_numbered(uint32_t number);

static struct peer* peer_of(EOS_ProductUserId user)
{
    int i;

    for (i = 0; i < peer_count; i++)
        if (peers[i].user == user)
            return &peers[i];
    return peer_numbered(number_of(user));
}

static struct peer* peer_numbered(uint32_t number)
{
    int i;

    for (i = 0; i < peer_count; i++)
        if (peers[i].number == number)
            return &peers[i];
    return NULL;
}

static void send_to(struct peer* peer, uint8_t channel, const void* data, uint32_t size,
                    EOS_EPacketReliability reliability)
{
    EOS_P2P_SendPacketOptions send = { 0 };

    send.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
    send.LocalUserId = self;
    send.RemoteUserId = peer->user;
    send.SocketId = &socket_id;
    send.Channel = channel;
    send.DataLengthBytes = size;
    send.Data = data;
    send.bAllowDelayedDelivery = EOS_TRUE;
    send.Reliability = reliability;
    send.bDisableAutoAcceptConnection = EOS_FALSE;
    EOS_P2P_SendPacket(p2p, &send);
}

/* Our player's name to `peer`, once the game has made the player. */
static void hello(struct peer* peer)
{
    if (player_name[0])
        send_to(peer, CHANNEL_HELLO, player_name, (uint32_t) strlen(player_name) + 1, EOS_PR_ReliableOrdered);
}

static void peer_add(EOS_ProductUserId user)
{
    struct peer* peer;

    if (user == self || number_of(user) == number_of(self) || peer_of(user) || peer_count == PEERS_MOST)
        return;
    peer = &peers[peer_count++];
    memset(peer, 0, sizeof *peer);
    peer->user = user;
    peer->number = number_of(user);
    say("peer %08X joined the lobby", peer->number);
    hello(peer);
}

static void peer_remove(EOS_ProductUserId user)
{
    struct peer* peer = peer_of(user);
    uint32_t number;

    if (!peer)
        return;
    number = peer->number;
    if (peer->named)
        tell(TELL_GONE, &number, 4, NULL, 0);
    *peer = peers[--peer_count];
    say("peer %08X left", number);
}

/* ---- Signing in ---- */

static void EOS_CALL on_login(const EOS_Connect_LoginCallbackInfo* info);

static void login(void)
{
    EOS_Connect_Credentials credentials = { EOS_CONNECT_CREDENTIALS_API_LATEST, NULL,
                                            EOS_ECT_DEVICEID_ACCESS_TOKEN };
    EOS_Connect_UserLoginInfo user = { EOS_CONNECT_USERLOGININFO_API_LATEST, self_name, NULL };
    EOS_Connect_LoginOptions options = { EOS_CONNECT_LOGIN_API_LATEST, &credentials, &user };

    EOS_Connect_Login(connect_api, &options, NULL, on_login);
}

static void EOS_CALL on_created_user(const EOS_Connect_CreateUserCallbackInfo* info)
{
    if (info->ResultCode != EOS_Success) {
        tell_failure("could not make an EOS user");
        return;
    }
    on_login(&(EOS_Connect_LoginCallbackInfo) { EOS_Success, NULL, info->LocalUserId, NULL });
}

static void EOS_CALL on_incoming(const EOS_P2P_OnIncomingConnectionRequestInfo* info);

static void EOS_CALL on_login(const EOS_Connect_LoginCallbackInfo* info)
{
    if (info->ResultCode == EOS_InvalidUser && info->ContinuanceToken) {
        EOS_Connect_CreateUserOptions create = { EOS_CONNECT_CREATEUSER_API_LATEST, info->ContinuanceToken };

        EOS_Connect_CreateUser(connect_api, &create, NULL, on_created_user);
        return;
    }
    if (info->ResultCode != EOS_Success) {
        tell_failure(EOS_EResult_ToString(info->ResultCode));
        return;
    }
    self = info->LocalUserId;
    {
        EOS_P2P_AddNotifyPeerConnectionRequestOptions incoming = {
            EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST, self, &socket_id };
        tell_ready ready = { number_of(self) };

        EOS_P2P_AddNotifyPeerConnectionRequest(p2p, &incoming, NULL, on_incoming);
        say("signed in as peer %08X", ready.self);
        tell(TELL_READY, &ready, sizeof ready, NULL, 0);
    }
}

static void EOS_CALL on_device_id(const EOS_Connect_CreateDeviceIdCallbackInfo* info)
{
    /* Made now, or made on an earlier run: either way, sign in with it. */
    if (info->ResultCode != EOS_Success && info->ResultCode != EOS_DuplicateNotAllowed) {
        tell_failure(EOS_EResult_ToString(info->ResultCode));
        return;
    }
    login();
}

/* ---- P2P ---- */

static void EOS_CALL on_incoming(const EOS_P2P_OnIncomingConnectionRequestInfo* info)
{
    EOS_P2P_AcceptConnectionOptions accept = { EOS_P2P_ACCEPTCONNECTION_API_LATEST, self, info->RemoteUserId,
                                               &socket_id };

    if (peer_of(info->RemoteUserId))            /* only the lobby's members */
        EOS_P2P_AcceptConnection(p2p, &accept);
}

/* A piece's header: where it goes in the whole, and the whole's size. */
typedef struct piece_head {
    uint32_t at, whole;
} piece_head;

static void send_data(uint32_t to, const uint8_t* data, uint32_t size)
{
    int i;

    for (i = 0; i < peer_count; i++) {
        struct peer* peer = &peers[i];
        uint32_t at;

        if (to && peer->number != to)
            continue;
        if (size <= PIECE_DATA) {
            send_to(peer, CHANNEL_SMALL, data, size, EOS_PR_UnreliableUnordered);
            continue;
        }
        for (at = 0; at < size; at += PIECE_DATA) {
            uint8_t piece[sizeof(piece_head) + PIECE_DATA];
            uint32_t part = size - at < PIECE_DATA ? size - at : PIECE_DATA;
            piece_head head = { at, size };

            memcpy(piece, &head, sizeof head);
            memcpy(piece + sizeof head, data + at, part);
            send_to(peer, CHANNEL_PIECES, piece, sizeof head + part, EOS_PR_ReliableOrdered);
        }
    }
}

static void receive_all(void)
{
    uint8_t data[EOS_P2P_MAX_PACKET_SIZE];

    for (;;) {
        EOS_P2P_ReceivePacketOptions options = { EOS_P2P_RECEIVEPACKET_API_LATEST, self, sizeof data, NULL };
        EOS_ProductUserId from;
        EOS_P2P_SocketId socket;
        uint8_t channel;
        uint32_t size;
        struct peer* peer;

        if (EOS_P2P_ReceivePacket(p2p, &options, &from, &socket, &channel, data, &size) != EOS_Success)
            return;
        peer = peer_of(from);
        if (!peer)
            continue;
        if (channel == CHANNEL_HELLO) {
            tell_peer told = { peer->number };

            if (peer->named)
                continue;
            peer->named = TRUE;
            memcpy(told.name, data, size < EOS_NAME_SIZE ? size : EOS_NAME_SIZE - 1);
            say("peer %08X is %s", peer->number, told.name);
            tell(TELL_PEER, &told, sizeof told, NULL, 0);
        } else if (channel == CHANNEL_SMALL) {
            tell_data told = { peer->number, size };

            tell(TELL_DATA, &told, sizeof told, data, size);
        } else if (channel == CHANNEL_PIECES && size > sizeof(piece_head)) {
            piece_head head;
            uint32_t part = size - sizeof head;

            memcpy(&head, data, sizeof head);
            if (head.whole > EOS_DATA_MOST || head.at + part > head.whole)
                continue;
            if (head.at == 0)
                peer->pieces_have = 0;
            memcpy(peer->pieces + head.at, data + sizeof head, part);
            peer->pieces_have += part;
            if (peer->pieces_have == head.whole) {
                tell_data told = { peer->number, head.whole };

                tell(TELL_DATA, &told, sizeof told, peer->pieces, head.whole);
                peer->pieces_have = 0;
            }
        }
    }
}

/* ---- Lobbies ---- */

static void members_add(void)
{
    EOS_Lobby_CopyLobbyDetailsHandleOptions copy = { EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST, lobby_id, self };
    EOS_LobbyDetails_GetMemberCountOptions count_options = { EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST };
    EOS_HLobbyDetails details;
    uint32_t count, i;

    if (EOS_Lobby_CopyLobbyDetailsHandle(lobby_api, &copy, &details) != EOS_Success)
        return;
    count = EOS_LobbyDetails_GetMemberCount(details, &count_options);
    for (i = 0; i < count; i++) {
        EOS_LobbyDetails_GetMemberByIndexOptions member = { EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST, i };

        peer_add(EOS_LobbyDetails_GetMemberByIndex(details, &member));
    }
    EOS_LobbyDetails_Release(details);
}

static void EOS_CALL on_member_status(const EOS_Lobby_LobbyMemberStatusReceivedCallbackInfo* info)
{
    if (strcmp(info->LobbyId, lobby_id))
        return;
    switch (info->CurrentStatus) {
    case EOS_LMS_JOINED:
        peer_add(info->TargetUserId);
        break;
    case EOS_LMS_LEFT:
    case EOS_LMS_DISCONNECTED:
    case EOS_LMS_KICKED:
        peer_remove(info->TargetUserId);
        break;
    case EOS_LMS_CLOSED:
        say("the lobby closed");
        lobby_id[0] = 0;
        while (peer_count)
            peer_remove(peers[0].user);
        tell(TELL_LOST, NULL, 0, NULL, 0);
        break;
    default:
        break;
    }
}

static void set_attribute(EOS_HLobbyModification change, const char* key, const char* text, int64_t number)
{
    EOS_Lobby_AttributeData data = { EOS_LOBBY_ATTRIBUTEDATA_API_LATEST, key };
    EOS_LobbyModification_AddAttributeOptions add = { EOS_LOBBYMODIFICATION_ADDATTRIBUTE_API_LATEST, &data,
                                                      EOS_LAT_PUBLIC };

    if (text) {
        data.Value.AsUtf8 = text;
        data.ValueType = EOS_AT_STRING;
    } else {
        data.Value.AsInt64 = number;
        data.ValueType = EOS_AT_INT64;
    }
    EOS_LobbyModification_AddAttribute(change, &add);
}

static void EOS_CALL on_updated(const EOS_Lobby_UpdateLobbyCallbackInfo* info)
{
    if (info->ResultCode != EOS_Success)
        say("the lobby would not change: %s", EOS_EResult_ToString(info->ResultCode));
}

/* The chat's lines as one attribute's text, into `to` (big enough for all of them). */
static void chat_encode(const ask_chat* chat, char* to)
{
    int i;

    to[0] = 0;
    for (i = 0; i < EOS_CHAT_LINES; i++) {
        const chat_line* line = &chat->lines[i];

        if (!line->name[0])
            continue;
        strncat(to, line->name, sizeof line->name - 1);
        strcat(to, "\t");
        strncat(to, line->text, sizeof line->text - 1);
        strcat(to, "\n");
    }
}

static void chat_decode(const char* from, ask_chat* chat)
{
    int i;

    memset(chat, 0, sizeof *chat);
    for (i = 0; i < EOS_CHAT_LINES && *from; i++) {
        const char* tab = strchr(from, '\t');
        const char* end = strchr(from, '\n');

        if (!tab || !end || tab > end)
            break;
        memcpy(chat->lines[i].name, from, min(tab - from, (ptrdiff_t) sizeof chat->lines[i].name - 1));
        memcpy(chat->lines[i].text, tab + 1, min(end - tab - 1, (ptrdiff_t) sizeof chat->lines[i].text - 1));
        from = end + 1;
    }
}

/*
    The lobby changed in one update: the attributes given (the host's alone: a member can change
    only its own), and always our rider's name on our membership, for searches to show who is in it.
*/
static void lobby_change(const char* name, int closed, const ask_settings* race, const ask_chat* chat)
{
    EOS_Lobby_UpdateLobbyModificationOptions options = { EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST, self,
                                                         lobby_id };
    EOS_Lobby_AttributeData rider = { EOS_LOBBY_ATTRIBUTEDATA_API_LATEST, ATTR_RIDER };
    EOS_LobbyModification_AddMemberAttributeOptions add = { EOS_LOBBYMODIFICATION_ADDMEMBERATTRIBUTE_API_LATEST,
                                                            &rider, EOS_LAT_PUBLIC };
    EOS_HLobbyModification change;

    if (!lobby_id[0] || EOS_Lobby_UpdateLobbyModification(lobby_api, &options, &change) != EOS_Success)
        return;
    rider.Value.AsUtf8 = player_name[0] ? player_name : self_name;
    rider.ValueType = EOS_AT_STRING;
    EOS_LobbyModification_AddMemberAttribute(change, &add);
    if (name)
        set_attribute(change, ATTR_NAME, name, 0);
    if (closed >= 0)
        set_attribute(change, ATTR_CLOSED, NULL, closed);
    if (race) {
        char track[EOS_TRACK_SIZE];

        memcpy(track, race->track, sizeof track);
        track[sizeof track - 1] = 0;
        set_attribute(change, ATTR_KIND, NULL, race->kind);
        set_attribute(change, ATTR_LAPS, NULL, race->laps);
        set_attribute(change, ATTR_TREES, NULL, race->trees);
        set_attribute(change, ATTR_TRACK, track, 0);
    }
    if (chat) {
        char text[EOS_CHAT_LINES * sizeof(chat_line) + 1];

        chat_encode(chat, text);
        set_attribute(change, ATTR_CHAT, text, 0);
    }
    {
        EOS_Lobby_UpdateLobbyOptions update = { EOS_LOBBY_UPDATELOBBY_API_LATEST, change };

        EOS_Lobby_UpdateLobby(lobby_api, &update, NULL, on_updated);
    }
    EOS_LobbyModification_Release(change);
}

static char hosting_name[EOS_SESSION_NAME_SIZE];

static void EOS_CALL on_created(const EOS_Lobby_CreateLobbyCallbackInfo* info)
{
    tell_lobby lobby = { 0 };

    if (info->ResultCode != EOS_Success) {
        tell_failure(EOS_EResult_ToString(info->ResultCode));
        return;
    }
    strncpy(lobby_id, info->LobbyId, sizeof lobby_id - 1);
    lobby_change(hosting_name, 0, NULL, NULL);
    strncpy(lobby.lobby, lobby_id, sizeof lobby.lobby - 1);
    lobby.owner = number_of(self);
    say("hosting lobby %s", lobby_id);
    tell(TELL_HOSTING, &lobby, sizeof lobby, NULL, 0);
}

static void host(const ask_host* ask)
{
    EOS_Lobby_CreateLobbyOptions create = { 0 };

    strncpy(hosting_name, ask->name, sizeof hosting_name - 1);
    create.ApiVersion = EOS_LOBBY_CREATELOBBY_API_LATEST;
    create.LocalUserId = self;
    create.MaxLobbyMembers = ask->most ? ask->most : 8;
    create.PermissionLevel = EOS_LPL_PUBLICADVERTISED;
    create.bPresenceEnabled = EOS_FALSE;
    create.bAllowInvites = EOS_TRUE;
    create.BucketId = BUCKET;
    create.bDisableHostMigration = EOS_TRUE;        /* the game's host owns the race */
    create.bEnableJoinById = EOS_TRUE;
    EOS_Lobby_CreateLobby(lobby_api, &create, NULL, on_created);
}

static EOS_HLobbySearch search;

/* A member's rider name, into `name` (EOS_NAME_SIZE): TRUE when it has one. */
static BOOL rider_name(EOS_HLobbyDetails details, EOS_ProductUserId member, char* name)
{
    EOS_LobbyDetails_CopyMemberAttributeByKeyOptions by_key = {
        EOS_LOBBYDETAILS_COPYMEMBERATTRIBUTEBYKEY_API_LATEST, member, ATTR_RIDER };
    EOS_Lobby_Attribute* attribute;
    BOOL named = FALSE;

    if (EOS_LobbyDetails_CopyMemberAttributeByKey(details, &by_key, &attribute) == EOS_Success) {
        if (attribute->Data->ValueType == EOS_AT_STRING) {
            strncpy(name, attribute->Data->Value.AsUtf8, EOS_NAME_SIZE - 1);
            named = TRUE;
        }
        EOS_Lobby_Attribute_Release(attribute);
    }
    return named;
}

/* The riders in a found lobby, the host first. */
static void read_riders(EOS_HLobbyDetails details, tell_session* session)
{
    EOS_LobbyDetails_GetLobbyOwnerOptions owner_options = { EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST };
    EOS_LobbyDetails_GetMemberCountOptions count_options = { EOS_LOBBYDETAILS_GETMEMBERCOUNT_API_LATEST };
    EOS_ProductUserId owner = EOS_LobbyDetails_GetLobbyOwner(details, &owner_options);
    uint32_t count = EOS_LobbyDetails_GetMemberCount(details, &count_options), i, riders = 0;

    if (owner && rider_name(details, owner, session->host))
        memcpy(session->riders[riders++], session->host, EOS_NAME_SIZE);
    for (i = 0; i < count && riders < EOS_RIDERS_MOST; i++) {
        EOS_LobbyDetails_GetMemberByIndexOptions by_index = { EOS_LOBBYDETAILS_GETMEMBERBYINDEX_API_LATEST, i };
        EOS_ProductUserId member = EOS_LobbyDetails_GetMemberByIndex(details, &by_index);

        if (member != owner && rider_name(details, member, session->riders[riders]))
            riders++;
    }
}

/* A lobby's number attribute, or `none`. */
static int64_t number_attribute(EOS_HLobbyDetails details, const char* key, int64_t none)
{
    EOS_LobbyDetails_CopyAttributeByKeyOptions by_key = { EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST, key };
    EOS_Lobby_Attribute* attribute;
    int64_t value = none;

    if (EOS_LobbyDetails_CopyAttributeByKey(details, &by_key, &attribute) == EOS_Success) {
        if (attribute->Data->ValueType == EOS_AT_INT64)
            value = attribute->Data->Value.AsInt64;
        EOS_Lobby_Attribute_Release(attribute);
    }
    return value;
}

/* A lobby's text attribute, into `to` (`size` bytes); empty when it has none. */
static void text_attribute(EOS_HLobbyDetails details, const char* key, char* to, size_t size)
{
    EOS_LobbyDetails_CopyAttributeByKeyOptions by_key = { EOS_LOBBYDETAILS_COPYATTRIBUTEBYKEY_API_LATEST, key };
    EOS_Lobby_Attribute* attribute;

    to[0] = 0;
    if (EOS_LobbyDetails_CopyAttributeByKey(details, &by_key, &attribute) == EOS_Success) {
        if (attribute->Data->ValueType == EOS_AT_STRING)
            strncpy(to, attribute->Data->Value.AsUtf8, size - 1);
        EOS_Lobby_Attribute_Release(attribute);
    }
}

static void read_session(EOS_HLobbyDetails details, tell_session* session)
{
    EOS_LobbyDetails_CopyInfoOptions info_options = { EOS_LOBBYDETAILS_COPYINFO_API_LATEST };
    EOS_LobbyDetails_Info* info;

    if (EOS_LobbyDetails_CopyInfo(details, &info_options, &info) == EOS_Success) {
        strncpy(session->lobby, info->LobbyId, sizeof session->lobby - 1);
        session->most = info->MaxMembers;
        session->players = info->MaxMembers - info->AvailableSlots;
        EOS_LobbyDetails_Info_Release(info);
    }
    text_attribute(details, ATTR_NAME, session->name, sizeof session->name);
    session->closed = number_attribute(details, ATTR_CLOSED, 0) != 0;
    session->race.kind = (int32_t) number_attribute(details, ATTR_KIND, -1);
    session->race.laps = (uint32_t) number_attribute(details, ATTR_LAPS, 0);
    session->race.trees = number_attribute(details, ATTR_TREES, 0) != 0;
    text_attribute(details, ATTR_TRACK, session->race.track, sizeof session->race.track);
    {
        char chat[EOS_CHAT_LINES * sizeof(chat_line) + 1];

        text_attribute(details, ATTR_CHAT, chat, sizeof chat);
        chat_decode(chat, &session->chat);
    }
    read_riders(details, session);
}

static void EOS_CALL on_found(const EOS_LobbySearch_FindCallbackInfo* info)
{
    EOS_LobbySearch_GetSearchResultCountOptions count_options = { EOS_LOBBYSEARCH_GETSEARCHRESULTCOUNT_API_LATEST };
    tell_session sessions[EOS_SESSIONS_MOST];
    uint32_t count = 0, found, i;

    found = info->ResultCode == EOS_Success ? EOS_LobbySearch_GetSearchResultCount(search, &count_options) : 0;
    memset(sessions, 0, sizeof sessions);
    for (i = 0; i < found && count < EOS_SESSIONS_MOST; i++) {
        EOS_LobbySearch_CopySearchResultByIndexOptions result = { EOS_LOBBYSEARCH_COPYSEARCHRESULTBYINDEX_API_LATEST,
                                                                  i };
        EOS_HLobbyDetails details;

        if (EOS_LobbySearch_CopySearchResultByIndex(search, &result, &details) != EOS_Success)
            continue;
        read_session(details, &sessions[count]);
        EOS_LobbyDetails_Release(details);
        if (sessions[count].lobby[0])
            count++;
    }
    say("found %u lobbies", count);
    {
        uint8_t message[4 + sizeof sessions];

        memcpy(message, &count, 4);
        memcpy(message + 4, sessions, count * sizeof *sessions);
        tell(TELL_SESSIONS, message, 4 + count * sizeof *sessions, NULL, 0);
    }
}

static void find(void)
{
    EOS_Lobby_CreateLobbySearchOptions create = { EOS_LOBBY_CREATELOBBYSEARCH_API_LATEST, EOS_SESSIONS_MOST };
    EOS_Lobby_AttributeData bucket = { EOS_LOBBY_ATTRIBUTEDATA_API_LATEST, EOS_LOBBY_SEARCH_BUCKET_ID };
    EOS_LobbySearch_SetParameterOptions parameter = { EOS_LOBBYSEARCH_SETPARAMETER_API_LATEST, &bucket, EOS_CO_EQUAL };
    EOS_LobbySearch_FindOptions options = { EOS_LOBBYSEARCH_FIND_API_LATEST, self };

    if (search)
        EOS_LobbySearch_Release(search);
    search = NULL;
    if (EOS_Lobby_CreateLobbySearch(lobby_api, &create, &search) != EOS_Success) {
        tell_failure("could not search for lobbies");
        return;
    }
    bucket.Value.AsUtf8 = BUCKET;
    bucket.ValueType = EOS_AT_STRING;
    EOS_LobbySearch_SetParameter(search, &parameter);
    EOS_LobbySearch_Find(search, &options, NULL, on_found);
}

static void EOS_CALL on_joined(const EOS_Lobby_JoinLobbyByIdCallbackInfo* info)
{
    EOS_Lobby_CopyLobbyDetailsHandleOptions copy = { EOS_LOBBY_COPYLOBBYDETAILSHANDLE_API_LATEST, info->LobbyId,
                                                     self };
    EOS_LobbyDetails_GetLobbyOwnerOptions owner_options = { EOS_LOBBYDETAILS_GETLOBBYOWNER_API_LATEST };
    EOS_HLobbyDetails details;
    tell_lobby lobby = { 0 };

    if (info->ResultCode != EOS_Success) {
        tell_failure(EOS_EResult_ToString(info->ResultCode));
        return;
    }
    strncpy(lobby_id, info->LobbyId, sizeof lobby_id - 1);
    strncpy(lobby.lobby, lobby_id, sizeof lobby.lobby - 1);
    if (EOS_Lobby_CopyLobbyDetailsHandle(lobby_api, &copy, &details) == EOS_Success) {
        lobby.owner = number_of(EOS_LobbyDetails_GetLobbyOwner(details, &owner_options));
        EOS_LobbyDetails_Release(details);
    }
    lobby_change(NULL, -1, NULL, NULL);
    say("joined lobby %s, owned by %08X", lobby_id, lobby.owner);
    tell(TELL_JOINED, &lobby, sizeof lobby, NULL, 0);
    members_add();
}

static void join(const ask_join* ask)
{
    EOS_Lobby_JoinLobbyByIdOptions options = { 0 };

    options.ApiVersion = EOS_LOBBY_JOINLOBBYBYID_API_LATEST;
    options.LobbyId = ask->lobby;
    options.LocalUserId = self;
    options.bPresenceEnabled = EOS_FALSE;
    EOS_Lobby_JoinLobbyById(lobby_api, &options, NULL, on_joined);
}

static void EOS_CALL on_left(const EOS_Lobby_LeaveLobbyCallbackInfo* info)
{
    say("left the lobby: %s", EOS_EResult_ToString(info->ResultCode));
}

static void leave(void)
{
    EOS_Lobby_LeaveLobbyOptions options = { EOS_LOBBY_LEAVELOBBY_API_LATEST, self, lobby_id };
    EOS_P2P_CloseConnectionsOptions close = { EOS_P2P_CLOSECONNECTIONS_API_LATEST, self, &socket_id };

    if (!lobby_id[0])
        return;
    EOS_Lobby_LeaveLobby(lobby_api, &options, NULL, on_left);
    EOS_P2P_CloseConnections(p2p, &close);
    lobby_id[0] = 0;
    peer_count = 0;
}

/* ---- The game's asks ---- */

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
                hello(&peers[i]);
            lobby_change(NULL, -1, NULL, NULL);
        }
        break;
    case ASK_SEND:
        if (ask->size >= sizeof(ask_send)) {
            const ask_send* send = (const ask_send*) ask->fields;

            if (send->size <= ask->size - sizeof *send && send->size <= EOS_DATA_MOST)
                send_data(send->to, ask->fields + sizeof *send, send->size);
        }
        break;
    case ASK_CLOSED:
    case ASK_OPENED:
        lobby_change(NULL, ask->kind == ASK_CLOSED, NULL, NULL);
        break;
    case ASK_SETTINGS:
        if (ask->size >= sizeof(ask_settings))
            lobby_change(NULL, -1, (const ask_settings*) ask->fields, NULL);
        break;
    case ASK_CHAT:
        if (ask->size >= sizeof(ask_chat))
            lobby_change(NULL, -1, NULL, (const ask_chat*) ask->fields);
        break;
    }
}

/* ---- Start ---- */

static BOOL read_settings(const char* path, char* product, char* sandbox, char* deployment, char* client,
                          char* secret)
{
    GetPrivateProfileStringA("eos", "product", "", product, 64, path);
    GetPrivateProfileStringA("eos", "sandbox", "", sandbox, 64, path);
    GetPrivateProfileStringA("eos", "deployment", "", deployment, 64, path);
    GetPrivateProfileStringA("eos", "client_id", "", client, 128, path);
    GetPrivateProfileStringA("eos", "client_secret", "", secret, 128, path);
    return product[0] && sandbox[0] && deployment[0] && client[0] && secret[0];
}

static void EOS_CALL on_log(const EOS_LogMessage* message)
{
    say("eos %s: %s", message->Category, message->Message);
}

int main(int argc, char** argv)
{
    char folder[MAX_PATH], path[MAX_PATH], cache[MAX_PATH];
    char product[64], sandbox[64], deployment[64], client[128], secret[128];
    char* slash;
    EOS_InitializeOptions init = { 0 };
    EOS_Platform_Options options = { 0 };

    if (argc < 3)
        return 2;
    GetModuleFileNameA(NULL, folder, sizeof folder);
    slash = strrchr(folder, '\\');
    if (slash)
        slash[1] = 0;
    snprintf(path, sizeof path, "%smcm2-eos.log", folder);
    log_file = fopen(path, "w");
    strncpy(self_name, argv[2], sizeof self_name - 1);

    snprintf(path, sizeof path, "%seos.ini", folder);
    if (!read_settings(path, product, sandbox, deployment, client, secret)) {
        say("eos.ini is missing settings");
        return 3;
    }
    pipe = CreateFileA(argv[1], GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, NULL);
    if (pipe == INVALID_HANDLE_VALUE) {
        say("the pipe %s would not open (%lu)", argv[1], GetLastError());
        return 4;
    }
    InitializeCriticalSection(&asks_lock);

    init.ApiVersion = EOS_INITIALIZE_API_LATEST;
    init.ProductName = "MCM2 Reborn";
    init.ProductVersion = "0.1";
    if (EOS_Initialize(&init) != EOS_Success) {
        tell_failure("EOS would not start");
        return 5;
    }
    EOS_Logging_SetCallback(on_log);
    EOS_Logging_SetLogLevel(EOS_LC_ALL_CATEGORIES, EOS_LOG_Warning);
    GetTempPathA(sizeof cache, cache);
    strncat(cache, "mcm2-eos", sizeof cache - strlen(cache) - 1);
    CreateDirectoryA(cache, NULL);

    options.ApiVersion = EOS_PLATFORM_OPTIONS_API_LATEST;
    options.ProductId = product;
    options.SandboxId = sandbox;
    options.DeploymentId = deployment;
    options.ClientCredentials.ClientId = client;
    options.ClientCredentials.ClientSecret = secret;
    options.Flags = EOS_PF_DISABLE_OVERLAY;
    options.CacheDirectory = cache;
    platform = EOS_Platform_Create(&options);
    if (!platform) {
        tell_failure("the EOS platform would not start");
        return 6;
    }
    connect_api = EOS_Platform_GetConnectInterface(platform);
    lobby_api = EOS_Platform_GetLobbyInterface(platform);
    p2p = EOS_Platform_GetP2PInterface(platform);
    {
        EOS_Lobby_AddNotifyLobbyMemberStatusReceivedOptions status = {
            EOS_LOBBY_ADDNOTIFYLOBBYMEMBERSTATUSRECEIVED_API_LATEST };
        EOS_Connect_CreateDeviceIdOptions device = { EOS_CONNECT_CREATEDEVICEID_API_LATEST, "PC" };

        EOS_Lobby_AddNotifyLobbyMemberStatusReceived(lobby_api, &status, NULL, on_member_status);
        EOS_Connect_CreateDeviceId(connect_api, &device, NULL, on_device_id);
    }
    CloseHandle(CreateThread(NULL, 0, pipe_reader, NULL, 0, NULL));

    while (!quitting) {
        struct ask* ask;

        while ((ask = next_ask()) != NULL) {
            if (self || ask->kind == ASK_LEAVE)
                answer(ask);
            free(ask);
        }
        EOS_Platform_Tick(platform);
        if (self)
            receive_all();
        Sleep(2);
    }
    leave();
    EOS_Platform_Tick(platform);
    EOS_Platform_Release(platform);
    EOS_Shutdown();
    say("done");
    return 0;
}
