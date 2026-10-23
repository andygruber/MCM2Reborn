/* Drive real mcm2-lan.exe helpers over their pipes, as the game's eosplay.c does, on this computer's
   own network: hosting, finding, joining, names, packets both ways and between members, a closed
   lobby, leaving, and the host's going. mcm2-lan.exe is the one in the folder given. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../code/eos/protocol.h"

static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #x); exit(1); } } while (0)

typedef struct helper {
    HANDLE pipe, process;
    uint32_t self;
} helper;

static const char* folder;

static BOOL io(HANDLE pipe, BOOL write, void* data, DWORD size, DWORD wait_ms)
{
    OVERLAPPED o = { 0 };
    DWORD done = 0;
    BOOL ok;

    o.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    ok = write ? WriteFile(pipe, data, size, NULL, &o) : ReadFile(pipe, data, size, NULL, &o);
    if (!ok && GetLastError() == ERROR_IO_PENDING) {
        if (WaitForSingleObject(o.hEvent, wait_ms) != WAIT_OBJECT_0) {
            CancelIoEx(pipe, &o);
            GetOverlappedResult(pipe, &o, &done, TRUE);
            CloseHandle(o.hEvent);
            return FALSE;
        }
        ok = TRUE;
    }
    ok = ok && GetOverlappedResult(pipe, &o, &done, TRUE) && done == size;
    CloseHandle(o.hEvent);
    return ok;
}

static void ask(helper* h, BYTE kind, const void* fields, DWORD size, const void* more, DWORD more_size)
{
    static BYTE frame[5 + sizeof(ask_send) + EOS_DATA_MOST];
    DWORD length = 1 + size + more_size;

    memcpy(frame, &length, 4);
    frame[4] = kind;
    memcpy(frame + 5, fields, size);
    memcpy(frame + 5 + size, more, more_size);
    CHECK(io(h->pipe, TRUE, frame, 4 + length, 2000));
}

/* The next news of `kind`, others passed over; FALSE when none comes in time. */
static BOOL hear(helper* h, BYTE kind, BYTE* fields, DWORD* size, DWORD wait_ms)
{
    DWORD until = GetTickCount() + wait_ms;

    for (;;) {
        DWORD length, left = until - GetTickCount();
        BYTE got;

        if ((int) left <= 0 || !io(h->pipe, FALSE, &length, 4, left) || !io(h->pipe, FALSE, &got, 1, 2000)
            || length - 1 > sizeof(tell_data) + EOS_DATA_MOST
            || (length > 1 && !io(h->pipe, FALSE, fields, length - 1, 2000)))
            return FALSE;
        if (got == kind) {
            *size = length - 1;
            return TRUE;
        }
    }
}

static void start(helper* h, const char* rider)
{
    static int made;
    char name[64], command[MAX_PATH + 128];
    STARTUPINFOA startup = { sizeof startup };
    PROCESS_INFORMATION process;
    OVERLAPPED o = { 0 };
    BYTE fields[sizeof(tell_ready)];
    DWORD size, done;

    sprintf(name, EOS_PIPE_FORMAT, "lantest", GetCurrentProcessId(), (unsigned long) ++made);
    h->pipe = CreateNamedPipeA(name, PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED, PIPE_TYPE_BYTE | PIPE_WAIT, 1,
                               0x10000, 0x10000, 0, NULL);
    CHECK(h->pipe != INVALID_HANDLE_VALUE);
    sprintf(command, "\"%s\\mcm2-lan.exe\" %s \"%s\"", folder, name, rider);
    CHECK(CreateProcessA(NULL, command, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, folder, &startup, &process));
    CloseHandle(process.hThread);
    h->process = process.hProcess;
    o.hEvent = CreateEventA(NULL, TRUE, FALSE, NULL);
    if (!ConnectNamedPipe(h->pipe, &o) && GetLastError() == ERROR_IO_PENDING)
        CHECK(WaitForSingleObject(o.hEvent, 10000) == WAIT_OBJECT_0 && GetOverlappedResult(h->pipe, &o, &done, FALSE));
    CloseHandle(o.hEvent);
    CHECK(hear(h, TELL_READY, fields, &size, 5000));
    h->self = ((tell_ready*) fields)->self;
    CHECK(h->self != 0);
}

static void stop(helper* h)
{
    CloseHandle(h->pipe);
    CHECK(WaitForSingleObject(h->process, 5000) == WAIT_OBJECT_0);
    CloseHandle(h->process);
}

static void player(helper* h, const char* name)
{
    ask_player p = { 0 };

    strcpy(p.name, name);
    ask(h, ASK_PLAYER, &p, sizeof p, NULL, 0);
}

/* A search's answer: the lobby named `name`, found within a few searches. */
static BOOL find(helper* h, const char* name, tell_session* session)
{
    static BYTE fields[sizeof(tell_data) + EOS_DATA_MOST];
    int tries;

    for (tries = 0; tries < 5; tries++) {
        DWORD size, count, i;

        ask(h, ASK_FIND, NULL, 0, NULL, 0);
        CHECK(hear(h, TELL_SESSIONS, fields, &size, 3000));
        memcpy(&count, fields, 4);
        CHECK(4 + count * sizeof(tell_session) <= size);
        for (i = 0; i < count; i++) {
            memcpy(session, fields + 4 + i * sizeof *session, sizeof *session);
            if (!strcmp(session->name, name))
                return TRUE;
        }
    }
    return FALSE;
}

static void join(helper* h, const tell_session* session, uint32_t owner)
{
    BYTE fields[sizeof(tell_failed)];
    ask_join j = { 0 };
    DWORD size;

    memcpy(j.lobby, session->lobby, sizeof j.lobby);
    ask(h, ASK_JOIN, &j, sizeof j, NULL, 0);
    CHECK(hear(h, TELL_JOINED, fields, &size, 6000));
    CHECK(!strcmp(((tell_lobby*) fields)->lobby, session->lobby));
    CHECK(((tell_lobby*) fields)->owner == owner);
}

static void named(helper* h, uint32_t peer, const char* name)
{
    BYTE fields[sizeof(tell_peer)];
    DWORD size;

    CHECK(hear(h, TELL_PEER, fields, &size, 5000));
    CHECK(((tell_peer*) fields)->peer == peer);
    CHECK(!strcmp(((tell_peer*) fields)->name, name));
}

static void sent(helper* from, helper* to, uint32_t send_to, DWORD length, BYTE seed)
{
    static BYTE data[EOS_DATA_MOST], fields[sizeof(tell_data) + EOS_DATA_MOST];
    ask_send s = { 0 };
    DWORD size, i;

    for (i = 0; i < length; i++)
        data[i] = (BYTE) (seed + i * 7);
    s.to = send_to;
    s.size = length;
    ask(from, ASK_SEND, &s, sizeof s, data, length);
    CHECK(hear(to, TELL_DATA, fields, &size, 3000));
    CHECK(((tell_data*) fields)->from == from->self);
    CHECK(((tell_data*) fields)->size == length && size == sizeof(tell_data) + length);
    CHECK(!memcmp(fields + sizeof(tell_data), data, length));
}

int main(int argc, char** argv)
{
    helper host, bob, cat, dan;
    ask_host hosting = { 3, "Test race" };
    ask_settings race = { 3, 5, 1, "sx01" };
    tell_session session;
    BYTE fields[sizeof(tell_data) + EOS_DATA_MOST];
    DWORD size;

    if (argc < 2)
        return 2;
    folder = argv[1];
    start(&host, "Alice");
    ask(&host, ASK_HOST, &hosting, sizeof hosting, NULL, 0);
    CHECK(hear(&host, TELL_HOSTING, fields, &size, 3000));
    CHECK(((tell_lobby*) fields)->owner == host.self);
    ask(&host, ASK_SETTINGS, &race, sizeof race, NULL, 0);
    player(&host, "Alice");

    /* Found with the race, the host's rider and room for two more, then joined. */
    start(&bob, "Bob");
    CHECK(bob.self != host.self);
    CHECK(find(&bob, "Test race", &session));
    CHECK(session.players == 1 && session.most == 3 && !session.closed);
    CHECK(!strcmp(session.host, "Alice") && !strcmp(session.riders[0], "Alice"));
    CHECK(session.race.kind == 3 && session.race.laps == 5 && session.race.trees && !strcmp(session.race.track, "sx01"));
    join(&bob, &session, host.self);
    player(&bob, "Bob");
    named(&host, bob.self, "Bob");
    named(&bob, host.self, "Alice");

    /* Packets both ways: one larger than an EOS P2P packet, one to the host alone. */
    sent(&host, &bob, 0, 3000, 1);
    sent(&bob, &host, host.self, 10, 2);
    sent(&host, &bob, bob.self, EOS_DATA_MOST, 3);

    /* A third rider meets the second through the host's welcome, and they send to each other. */
    start(&cat, "Cat");
    CHECK(find(&cat, "Test race", &session));
    CHECK(session.players == 2 && !strcmp(session.riders[1], "Bob"));
    join(&cat, &session, host.self);
    player(&cat, "Cat");
    named(&host, cat.self, "Cat");
    named(&bob, cat.self, "Cat");
    sent(&cat, &bob, bob.self, 64, 4);
    sent(&bob, &cat, cat.self, 64, 5);

    /* Full, then racing: a fourth is told why it cannot join. */
    start(&dan, "Dan");
    CHECK(find(&dan, "Test race", &session));
    CHECK(session.players == 3);
    {
        ask_join j = { 0 };

        memcpy(j.lobby, session.lobby, sizeof j.lobby);
        ask(&dan, ASK_JOIN, &j, sizeof j, NULL, 0);
        CHECK(hear(&dan, TELL_FAILED, fields, &size, 6000));
        CHECK(!strcmp(((tell_failed*) fields)->why, "the lobby is full"));
    }

    /* Bob leaves: the others are told, and the lobby has room again until the race starts. */
    ask(&bob, ASK_LEAVE, NULL, 0, NULL, 0);
    CHECK(hear(&host, TELL_GONE, fields, &size, 3000) && *(uint32_t*) fields == bob.self);
    CHECK(hear(&cat, TELL_GONE, fields, &size, 3000) && *(uint32_t*) fields == bob.self);
    ask(&host, ASK_CLOSED, NULL, 0, NULL, 0);
    CHECK(find(&dan, "Test race", &session));
    CHECK(session.players == 2 && session.closed);
    {
        ask_join j = { 0 };

        memcpy(j.lobby, session.lobby, sizeof j.lobby);
        ask(&dan, ASK_JOIN, &j, sizeof j, NULL, 0);
        CHECK(hear(&dan, TELL_FAILED, fields, &size, 6000));
        CHECK(!strcmp(((tell_failed*) fields)->why, "the race has started"));
    }

    /* The host's game shuts: its helper quits, and the lobby is lost to the rider left in it. */
    stop(&host);
    CHECK(hear(&cat, TELL_LOST, fields, &size, 3000));
    stop(&cat);
    stop(&bob);
    stop(&dan);
    printf("PASS %d LAN checks: hosting, finding, joining, names, packets to one and all, members meeting, "
           "full and racing lobbies, leaving and the host's going\n", checks);
    return 0;
}
