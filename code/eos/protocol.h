/*
    OpenMCM2 - what the game and its EOS helper (mcm2-eos.exe) say to each other over the pipe.

    The game is 32-bit and Epic's SDK is 64-bit only, so online play goes through a helper of
    its own: the game starts it, and the two talk over the named pipe \\.\pipe\mcm2-eos-<the
    game's process id>. Each message is a 4-byte length (of what follows), a 1-byte kind and
    its fields. Both sides are little-endian x86, so the fields are plain structs.

    Peers are numbered by the helper (a hash of their EOS product user id, never 0); a peer's
    number is the DirectPlay player id the game sees for it.
*/

#pragma once

#include <stdint.h>

#define EOS_PIPE_FORMAT     "\\\\.\\pipe\\mcm2-eos-%lu"
#define EOS_NAME_SIZE       32          /* a rider's name, as the lobby shows it */
#define EOS_LOBBY_ID_SIZE   64          /* an EOS lobby id, as text */
#define EOS_SESSION_NAME_SIZE 64
#define EOS_SESSIONS_MOST   5           /* the most a search returns (a TELL_SESSIONS fits a frame) */
#define EOS_DATA_MOST       0x2000      /* the largest message the game sends is 0x8DC bytes */
#define EOS_RIDERS_MOST     30          /* a lobby's members, the most there may be */

#pragma pack(push, 1)

/* The game's asks. */
enum {
    ASK_HOST = 1,       /* ask_host: make a lobby and own it */
    ASK_FIND,           /* no fields: the lobbies being hosted, as TELL_SESSIONS */
    ASK_JOIN,           /* ask_join */
    ASK_LEAVE,          /* no fields */
    ASK_PLAYER,         /* ask_player: the game made its player; tell the others */
    ASK_SEND,           /* ask_send, then the data */
    ASK_CLOSED,         /* no fields: joining is closed (the race has started) or open again */
    ASK_OPENED,
    ASK_SETTINGS,       /* ask_settings: the race the host has set up, for the lobby to show */
    ASK_CHAT,           /* ask_chat: the host's lobby chat, its newest lines, for the lobby to show */
};

typedef struct ask_host {
    uint32_t most;                          /* players at most, the host's included */
    char     name[EOS_SESSION_NAME_SIZE];   /* the session's */
} ask_host;

typedef struct ask_join {
    char lobby[EOS_LOBBY_ID_SIZE];
} ask_join;

typedef struct ask_player {
    char name[EOS_NAME_SIZE];
} ask_player;

#define EOS_TRACK_SIZE 48

typedef struct ask_settings {
    int32_t  kind;                          /* the game's: 0 quarry, 1 Baja, 2 national, 3 SX, 4 tag, 5 enduro */
    uint32_t laps;
    uint32_t trees;                         /* tree collision on */
    char     track[EOS_TRACK_SIZE];
} ask_settings;

#define EOS_CHAT_LINES 4

typedef struct chat_line {
    char name[24];
    char text[76];
} chat_line;

typedef struct ask_chat {
    chat_line lines[EOS_CHAT_LINES];        /* the oldest first; a line with no name is none */
} ask_chat;

typedef struct ask_send {
    uint32_t to;                            /* a peer, or 0 for every one */
    uint32_t size;                          /* the data's, after this */
} ask_send;

/* The helper's answers and news. */
enum {
    TELL_READY = 1,     /* tell_ready: signed in */
    TELL_FAILED,        /* tell_failed: signing in, or the last ask, failed */
    TELL_SESSIONS,      /* uint32_t count, then that many tell_session */
    TELL_HOSTING,       /* tell_lobby: the lobby made */
    TELL_JOINED,        /* tell_lobby: in the lobby; its peers follow as TELL_PEER */
    TELL_PEER,          /* tell_peer: a peer's player (its name known) */
    TELL_GONE,          /* uint32_t peer: it left */
    TELL_DATA,          /* tell_data, then the data */
    TELL_LOST,          /* no fields: the lobby closed under us */
};

typedef struct tell_ready {
    uint32_t self;                          /* this computer's peer number */
} tell_ready;

typedef struct tell_failed {
    char why[128];
} tell_failed;

typedef struct tell_session {
    char     lobby[EOS_LOBBY_ID_SIZE];
    char     name[EOS_SESSION_NAME_SIZE];
    uint32_t players;
    uint32_t most;
    uint32_t closed;                        /* joining closed */
    char     host[EOS_NAME_SIZE];
    char     riders[EOS_RIDERS_MOST][EOS_NAME_SIZE];    /* its members' names, the host's first */
    ask_settings race;                      /* kind -1 until the host has published it */
    ask_chat chat;
} tell_session;

/* A search's answer fits one frame. */
_Static_assert(4 + EOS_SESSIONS_MOST * sizeof(tell_session) <= EOS_DATA_MOST, "TELL_SESSIONS is too big");

typedef struct tell_lobby {
    char     lobby[EOS_LOBBY_ID_SIZE];
    uint32_t owner;                         /* the host's peer number */
} tell_lobby;

typedef struct tell_peer {
    uint32_t peer;
    char     name[EOS_NAME_SIZE];
} tell_peer;

typedef struct tell_data {
    uint32_t from;
    uint32_t size;
} tell_data;

#pragma pack(pop)
