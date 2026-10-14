/*
    OpenMCM2 - the network screens, ported from the game (NetProcs.cpp in the original): the
    sessions screen (HostJoin.dtm) and the modem's host-or-join prompt, the serial cable's
    settings, the TCP/IP address and player name prompts, and the notices of a failed join and a
    lost connection. Also the game's network leaving and making, and the cable's DirectPlay
    address. Each function here replaces the game's at the address in its PORT line, and was
    checked against the original's instructions.

    The network is the object at game_main + 8. The sessions it finds (five at most, 0x10C bytes
    each) are kept at game_main + 0x1010.
*/

#include "menus.h"

BYTE* __fastcall dialog_construct(BYTE* self, void* edx_unused, DWORD flag, const char* name);
void  __fastcall dialog_end(BYTE* self, void* edx_unused, DWORD code);
void  __fastcall dialog_notify_controls(BYTE* self, void* edx_unused, DWORD flag);
void  __fastcall dialog_add_timer(BYTE* self, void* edx_unused, DWORD id, DWORD period,
                                  BYTE* target);
void  __fastcall dialog_remove_timer(BYTE* self, void* edx_unused, DWORD id);
int   __fastcall dialog_queue_focus(BYTE* self, void* edx_unused, BYTE* control, BYTE** old,
                                    BOOL now);
BYTE* __fastcall gui_manager_open(gui_manager* self, void* edx_unused, BYTE* dialog, DWORD a,
                                  DWORD flags, DWORD area, BYTE* parent, DWORD arg, DWORD c,
                                  DWORD wait);
int   __fastcall control_set_colour(BYTE* self, void* edx_unused, COLORREF colour);
int   __fastcall control_set_align(BYTE* self, void* edx_unused, DWORD align);
BOOL  __fastcall option_set_count(BYTE* self, void* edx_unused, int count);
int   __fastcall option_load_text(BYTE* self, void* edx_unused, int index, HINSTANCE module,
                                  UINT id);
int   __fastcall option_set_text(BYTE* self, void* edx_unused, int index, const char* text);
DWORD __fastcall list_data(BYTE* self, void* edx_unused, int index);
int   __fastcall list_select_data(BYTE* self, void* edx_unused, DWORD data);
int   __fastcall list_set_text_colour(BYTE* self, void* edx_unused, COLORREF colour, int index);
int   __fastcall list_set_208(BYTE* self, void* edx_unused, DWORD value);
int   __fastcall list_set_colour_2(BYTE* self, void* edx_unused, COLORREF colour);
int   __fastcall edit_set_capacity(BYTE* self, void* edx_unused, int length);
int   __fastcall edit_set_text(BYTE* self, void* edx_unused, const char* text);
void  __fastcall edit_set_allowed(BYTE* self, void* edx_unused, const char* allowed);
void  __fastcall session_go_to(BYTE* self, void* edx_unused, int screen);
void  __cdecl    result_back(int code, ui_event* event);
void  __cdecl    result_confirm(int code, ui_event* event);

/*
    The network (Net.cpp): destroyed; a DirectPlay address for a provider, and connecting
    through one; the sessions found (an HRESULT); hosting one by name and joining one by its
    instance. The flags are DirectPlay's: enumeration 1 available, 0x10 async, 0x80 return
    status; opening 0x80 return status.
*/
void    __fastcall game_FUN_004afc60(BYTE* net);
BOOL    __fastcall game_FUN_004b0600(BYTE* net, void* edx_unused, GUID provider,
                                     const char* address, const char* b, void* settings,
                                     void** data, DWORD* size);
int     __fastcall game_FUN_004b04e0(BYTE* net, void* edx_unused, void* data, DWORD size,
                                     DWORD flags);
HRESULT __fastcall game_FUN_004b26c0(BYTE* net, void* edx_unused, BYTE* sessions, int* count,
                                     DWORD flags);
BOOL    __fastcall game_FUN_004b0ab0(BYTE* net, void* edx_unused, const char* name, DWORD flags);
BOOL    __fastcall game_FUN_004b0b70(BYTE* net, void* edx_unused, const GUID* instance,
                                     DWORD flags);
void    __cdecl    game_FUN_004b1c90(HRESULT result, const char* file, int line); /* formats it, no more */

/* Out of line: the original calls these. */
__declspec(noinline) DWORD __cdecl serial_baud_rate(int choice);
__declspec(noinline) DWORD __cdecl serial_stop_bits(int choice);
__declspec(noinline) DWORD __cdecl serial_parity(int choice);
__declspec(noinline) DWORD __cdecl serial_flow_control(int choice);
__declspec(noinline) void  __cdecl session_join(ui_event* event);
__declspec(noinline) void  __cdecl net_leave(void);
__declspec(noinline) BOOL  __cdecl net_make(void);

#define NETPROCS_CPP    NAME(0x00574C6C)    /* "D:\aardvark\VC\krusty2\NetProcs.cpp" */
#define MESSAGE_1       NAME(0x005700BC)    /* "messbox1.dtm" */
#define LIST_SESSIONS   NAME(0x00574C44)    /* "LstSessions" */
#define BUT_HOST        NAME(0x00574C90)    /* "ButHost" */
#define BUT_JOIN        NAME(0x00574C98)    /* "ButJoin" */
#define BUT_OK          NAME(0x00574CA0)    /* "ButOk" */
#define BUT_FLOW        NAME(0x00574CA8)    /* "ButFlow" */
#define BUT_PARITY      NAME(0x00574CB0)    /* "ButParity" */
#define BUT_STOP        NAME(0x00574CBC)    /* "ButStop" */
#define BUT_BAUD        NAME(0x00574CC4)    /* "ButBaud" */
#define BUT_PORT        NAME(0x00574CCC)    /* "ButPort" */
#define BUT_CANCEL      NAME(0x0056F83C)    /* "ButCancel" */
#define BUTTON_LEFT     NAME(0x0056F888)    /* "ButLeft" */
#define BUTTON_MIDDLE   NAME(0x0056F87C)    /* "ButMiddle" */
#define BUTTON_RIGHT    NAME(0x0056F870)    /* "ButRight" */
#define TITLE_TEXT      NAME(0x0056F864)    /* "TitleText" */
#define PROMPT_TEXT     NAME(0x0056F858)    /* "TxtPrompt" */
#define EDIT_BOX        NAME(0x0056FE58)    /* "EditBox" */
#define OK_BUTTON       NAME(0x0056FE60)    /* "OkButton" */
#define OK_BUTTON_WORDS NAME(0x0056FF54)    /* "OKButton" */
#define CANCEL_BUTTON   NAME(0x0056FF44)    /* "CancelButton" */
#define NAME_CHARS      NAME(0x00574CD4)    /* the characters a player's name may hold */

#define EVENT_DOUBLE_CLICK 13       /* a list's entry clicked twice (list_tell in ui.c) */
#define KEY_RETURN      0x1C
#define GREY            0x808080
#define WHITE           0xFFFFFF
#define SESSION_COLOUR  0xFEB97A
#define NAME_PROMPT_VTABLE 0x0055A69C

/* The sessions screen's id (0xC4) when the main menu chose TCP/IP by address. */
#define BY_ADDRESS      0x85D

#define game_net        FIELD(game_main, 0x8, BYTE*)
#define game_window     FIELD(game_main, 0x31C, HWND)
#define player_name     ((char*) game_main + 0x588)     /* char[0x80] */
#define PROVIDER_TCPIP  (*(const GUID*) 0x0055BA70)     /* DPSPGUID_TCPIP */

/* The sessions found: each a vtable, its name (+4) and its instance (+0x108). */
#define game_sessions       (game_main + 0x1010)
#define SESSION_SIZE        0x10C
#define session_name(i)     ((const char*) game_main + 0x1014 + (i) * SESSION_SIZE)
#define session_instance(i) FIELD(game_main, 0x1118 + (i) * SESSION_SIZE, const GUID*)
#define session_chosen      FIELD(game_main, 0x154C, int)   /* the sessions list's choice */
#define session_count       (*(int*) 0x0068E80C)

/* The cable's settings as SerPopup.dtm's choices show them, and the baud rates it offers. */
#define serial_settings ((int*) 0x0068E7F8)
#define SERIAL_PORT     0
#define SERIAL_BAUD     1
#define SERIAL_STOP     2
#define SERIAL_PARITY   3
#define SERIAL_FLOW     4
#define baud_rates      ((const int*) 0x00574C1C)       /* 4800 to 256000, ten */

static BYTE* event_dialog(const ui_event* event)
{
    return (BYTE*) event->sender;
}

/* End the dialog that sent the event, marking the event handled. */
static void event_end(ui_event* event, DWORD code)
{
    dialog_end(event_dialog(event), NULL, code);
    event->e = 1;
}

static void go_to(int screen)
{
    session_go_to(game_session, NULL, screen);
}

/* In a session: the lobby next. */
static void lobby_enter(ui_event* event)
{
    go_to(0x866);
    event_end(event, 0x63);
}

static void control_enable(BYTE* control, DWORD enabled)
{
    VIRTUAL(control, 0xC4, method_1)(control, NULL, enabled);
}

static void load_text(BYTE* self, const char* name, int type, UINT id)
{
    control_load_text(dialog_find_control(self, NULL, name, type), NULL, game_language, id);
}

/* The prompt's text, wrapped (0x1E8). */
static void load_prompt(BYTE* self, UINT id)
{
    BYTE* prompt = dialog_find_control(self, NULL, PROMPT_TEXT, 12);

    FIELD(prompt, 0x1E8, DWORD) = 1;
    control_load_text(prompt, NULL, game_language, id);
}

/* The player name prompt over `self`, which hears its end (0x14 when confirmed). */
static void name_prompt_open(BYTE* self, int line)
{
    BYTE* prompt = game_new(0x7F58, NETPROCS_CPP, line);

    if (prompt) {
        dialog_construct(prompt, NULL, 1, MESSAGE_1);
        *(DWORD*) prompt = NAME_PROMPT_VTABLE;
    }
    gui_manager_open(FIELD(self, 0x30, gui_manager*), NULL, prompt, 0x12C, 4, 0, self, 0, 0, 1);
}

/* ---- The cable's DirectPlay address ---- */

PORT(0x004B29C0)
DWORD __cdecl serial_baud_rate(int choice)
{
    switch (choice) {
    case 0: return 4800;
    case 1: return 9600;
    case 2: return 14400;
    case 3: return 19200;
    case 4: return 38400;
    case 5: return 56000;
    case 6: return 57600;
    case 7: return 115200;
    case 8: return 128000;
    case 9: return 256000;
    }
    return 0;
}

/* One, one and a half or two: the choices are DirectPlay's own numbers. */
PORT(0x004B2A40)
DWORD __cdecl serial_stop_bits(int choice)
{
    return choice == 1 || choice == 2 ? choice : 0;
}

/* The choices are none, even, odd and mark; DirectPlay counts odd before even. */
PORT(0x004B2A60)
DWORD __cdecl serial_parity(int choice)
{
    switch (choice) {
    case 1: return 2;
    case 2: return 1;
    case 3: return 3;
    }
    return 0;
}

/* None, XON/XOFF, RTS, DTR, RTS and DTR: DirectPlay's own numbers. */
PORT(0x004B2AA0)
DWORD __cdecl serial_flow_control(int choice)
{
    return choice >= 1 && choice <= 4 ? choice : 0;
}

/* A DPCOMPORTADDRESS from the settings; the ports are counted from COM1. */
PORT(0x004B2AE0)
void __cdecl serial_address(const int* settings, DWORD* address)
{
    address[0] = settings[SERIAL_PORT] + 1;
    address[1] = serial_baud_rate(settings[SERIAL_BAUD]);
    address[2] = serial_stop_bits(settings[SERIAL_STOP]);
    address[3] = serial_parity(settings[SERIAL_PARITY]);
    address[4] = serial_flow_control(settings[SERIAL_FLOW]);
}

/* ---- HostJoin.dtm: the sessions ---- */

/* The session chosen in the sender's list, joined; the lobby next. */
PORT(0x004B2B30)
void __cdecl session_join(ui_event* event)
{
    BYTE* dialog = event_dialog(event);
    const GUID* instance;

    session_chosen = list_selected(dialog_find_control(dialog, NULL, LIST_SESSIONS, 0));
    /* The dialog is checked only after it was used (the original's). */
    if (session_chosen == -1 || !dialog || FIELD(dialog, 0xC4, int) == BY_ADDRESS)
        return;
    instance = session_instance(session_chosen);
    if (instance && game_FUN_004b0b70(game_net, NULL, instance, 0x80))
        lobby_enter(event);
}

/* Every second: the sessions found again, keeping the entry chosen. Join needs one. */
static void sessions_refresh(BYTE* self, ui_event* event)
{
    BYTE* list = dialog_find_control(self, NULL, LIST_SESSIONS, 0);
    DWORD chosen = list_data(list, NULL, -1);
    BYTE* join;
    int i;

    list_clear(list);
    session_count = 0;
    if (game_FUN_004b26c0(game_net, NULL, game_sessions, &session_count, 0x91) < 0) {
        net_leave();
        event_end(event, 0xD);
        return;
    }
    for (i = 0; i < session_count; i++)
        list_add_text(list, NULL, session_name(i), (DWORD) session_name(i), 0);
    list_select_data(list, NULL, chosen);
    join = dialog_find_control(self, NULL, BUT_JOIN, 0);
    control_enable(join, session_count != 0);
    control_set_colour(join, NULL, session_count ? WHITE : GREY);
}

/* The player named: a session hosted under the name, by address first when so chosen. */
static void session_host(BYTE* self, ui_event* event)
{
    if (FIELD(self, 0xC4, int) == BY_ADDRESS) {
        void* address;
        DWORD size;

        if (!game_FUN_004b0600(game_net, NULL, PROVIDER_TCPIP, NO_TEXT, NO_TEXT, NULL, &address,
                               &size))
            return;
        if (!game_FUN_004b04e0(game_net, NULL, address, size, 4))
            return;                     /* the address is not freed (the original's) */
        game_FUN_004a4450(address, NETPROCS_CPP, 0xB3);
    }
    if (game_FUN_004b0ab0(game_net, NULL, player_name, 0x80))
        lobby_enter(event);
}

/* Connected to an address: its first session joined, or the network made anew and 0x1F9. */
static void address_join(BYTE* self, ui_event* event)
{
    gui_manager* manager = FIELD(self, 0x30, gui_manager*);
    HRESULT result;

    gui_manager_set_cursors(manager, NULL, manager->wait_image, 1);
    session_count = 0;
    result = game_FUN_004b26c0(game_net, NULL, game_sessions, &session_count, 0x81);
    if (session_count > 0 && result >= 0
        && game_FUN_004b0b70(game_net, NULL, session_instance(0), 0)) {
        gui_manager_set_cursors(manager, NULL, manager->cursor_image, 1);
        lobby_enter(event);
        return;
    }
    if (result < 0)
        game_FUN_004b1c90(result, NETPROCS_CPP, 0xCF);
    net_leave();
    net_make();
    gui_manager_set_cursors(manager, NULL, manager->cursor_image, 1);
    go_to(0x1F9);
}

/*
    HostJoin.dtm (vtable 0x00559C50), screens 0x85C and 0x85D. The sessions are listed and
    refreshed every second; by address (0x85D) none are, and Join asks for the address (0xD9).
    Host asks for the player's name and hosts once it is given (0x14); the address prompt
    answers 0xF once connected.
*/
PORT(0x004B2BD0)
void __fastcall sessions_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    BYTE* list;
    BYTE* join;

    (void) edx_unused;
    switch (event->type) {
    case EVENT_CLICK:
        if (clicked(event, BUT_HOST)) {
            name_prompt_open(self, 0x6E);
        } else if (clicked(event, BUT_JOIN)) {
            dialog_notify_controls(self, NULL, 1);
            if (FIELD(self, 0xC4, int) != BY_ADDRESS)
                session_join(event);
            else
                go_to(0xD9);
        } else if (clicked(event, BUT_CANCEL)) {
            event_end(event, 0);
            net_leave();
            net_make();
        }
        break;
    case EVENT_OPEN:
        join = dialog_find_control(self, NULL, BUT_JOIN, 0);
        if (FIELD(self, 0xC4, int) != BY_ADDRESS) {
            control_enable(join, FALSE);
            control_set_colour(join, NULL, GREY);
        }
        list = dialog_find_control(self, NULL, LIST_SESSIONS, 0);
        list_set_text_colour(list, NULL, SESSION_COLOUR, -1);
        list_set_208(list, NULL, WHITE);
        list_set_colour_2(list, NULL, SESSION_COLOUR);
        session_chosen = 0;
        control_bind(list, &session_chosen);
        dialog_notify_controls(self, NULL, 0);
        list_select_all(list, NULL, 0);
        dialog_queue_focus(self, NULL, list, NULL, FALSE);
        if (FIELD(self, 0xC4, int) != BY_ADDRESS)
            dialog_add_timer(self, NULL, 0, 1000, NULL);
        break;
    case EVENT_CLOSE:
        dialog_remove_timer(self, NULL, 0);
        break;
    case EVENT_TIMER:
        sessions_refresh(self, event);
        break;
    case EVENT_CHILD_ENDED:
        if (event->a == 0x14)
            session_host(self, event);
        else if (event->a == 0xF)
            address_join(self, event);
        else if (event->a == 0xD)
            dialog_end(self, NULL, 0xD);
        else if (event->a == 0x63)
            event_end(event, 0x63);
        break;
    case EVENT_DOUBLE_CLICK:
        if (clicked(event, LIST_SESSIONS))
            session_join(event);
        break;
    }
}

/* ---- SerPopup.dtm: the serial cable ---- */

/* A choice control (kind 2) offering the strings given. */
static BYTE* choices_fill(BYTE* self, const char* name, const UINT* ids, int count)
{
    BYTE* control = dialog_find_control(self, NULL, name, 2);
    int i;

    option_set_count(control, NULL, count);
    for (i = 0; i < count; i++)
        option_load_text(control, NULL, i, game_language, ids[i]);
    return control;
}

static void serial_open(BYTE* self)
{
    static const UINT ports[] = { 0x147B, 0x147C, 0x147D, 0x147E };
    static const UINT stops[] = { 0x1478, 0x1479, 0x147A };
    static const UINT parities[] = { 0x146F, 0x1470, 0x1471, 0x1472 };
    static const UINT flows[] = { 0x1474, 0x1473, 0x1476, 0x1477, 0x1475 };
    char text[0x80];
    BYTE* control;
    int i;

    control = choices_fill(self, BUT_PORT, ports, 4);
    serial_settings[SERIAL_PORT] = 0;
    control_bind(control, &serial_settings[SERIAL_PORT]);

    control = dialog_find_control(self, NULL, BUT_BAUD, 2);
    option_set_count(control, NULL, 10);
    for (i = 0; i < 10; i++) {
        game_FUN_0053a11f(text, ONE_NUMBER, baud_rates[i]);
        option_set_text(control, NULL, i, text);
    }
    serial_settings[SERIAL_BAUD] = 6;                       /* 57600 */
    control_bind(control, &serial_settings[SERIAL_BAUD]);

    control = choices_fill(self, BUT_STOP, stops, 3);
    serial_settings[SERIAL_STOP] = 0;
    control_bind(control, &serial_settings[SERIAL_STOP]);

    control = choices_fill(self, BUT_PARITY, parities, 4);
    serial_settings[SERIAL_PARITY] = 0;
    control_bind(control, &serial_settings[SERIAL_PARITY]);

    control = choices_fill(self, BUT_FLOW, flows, 5);
    serial_settings[SERIAL_FLOW] = 4;                       /* RTS and DTR */
    control_bind(control, &serial_settings[SERIAL_FLOW]);

    dialog_notify_controls(self, NULL, 0);
}

/* SerPopup.dtm (vtable 0x00559F68), screen 0xD8: OK connects through result_confirm. */
PORT(0x004B3130)
void __fastcall serial_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUT_CANCEL)) {
            event_end(event, 0);
        } else if (clicked(event, BUT_OK)) {
            dialog_notify_controls(self, NULL, 1);
            result_confirm(0xD8, event);
        }
    } else if (event->type == EVENT_OPEN) {
        serial_open(self);
    } else if (event->type == EVENT_CHILD_ENDED) {
        if (event->a == 0x63)
            event_end(event, 0x63);
        else if (event->a == 0xD)
            net_make();
    }
}

/* ---- Prompts ---- */

/* messbox1.dtm (vtable 0x00559EE4), screen 0xD9: the TCP/IP address to join. */
PORT(0x004B34E0)
void __fastcall tcp_address_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, CANCEL_BUTTON))
            result_back(0xD9, event);
        else if (clicked(event, OK_BUTTON))
            result_confirm(0xD9, event);
    } else if (event->type == EVENT_OPEN) {
        load_text(self, TITLE_TEXT, 0, 0x898);
        load_text(self, OK_BUTTON, 0, 0x13E9);
        load_text(self, CANCEL_BUTTON, 0, 0x13DC);
        load_prompt(self, 0x899);
        gui_user_queue_capture(FIELD(self, 0x34, gui_user*), NULL,
                               dialog_find_control(self, NULL, EDIT_BOX, 0), NULL, FALSE);
    }
}

/* The network left and destroyed; the game is single-player again (0x18). */
PORT(0x004B3610)
void __cdecl net_leave(void)
{
    BYTE* net = game_net;

    if (net) {
        game_FUN_004afc60(net);
        game_delete(net);
        game_net = NULL;
    }
    FIELD(game_main, 0x18, int) = 1;
    FIELD(game_main, 0x3424, DWORD) = 0;
    FIELD(game_main, 0x2DA5, BYTE) = 0;
    FIELD(game_main, 0x2D98, DWORD) = 0;
}

/* The network, made (game_main's slot 0x40) unless there is one; left again if that fails. */
PORT(0x004B3670)
BOOL __cdecl net_make(void)
{
    if (!game_net && !VIRTUAL(game_main, 0x40, method_1)(game_main, NULL, 1)) {
        net_leave();
        return FALSE;
    }
    return TRUE;
}

/* messbox1.dtm (vtable 0x0055A69C): the player's name, 11 characters at most. */
PORT(0x004B36A0)
void __fastcall player_name_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, CANCEL_BUTTON))
            result_back(0x12C, event);
        else if (clicked(event, OK_BUTTON_WORDS))
            result_confirm(0x12C, event);
    } else if (event->type == EVENT_OPEN) {
        BYTE* edit;

        load_text(self, TITLE_TEXT, 12, 0x911);
        load_text(self, OK_BUTTON_WORDS, 0, 0x13E9);
        load_text(self, CANCEL_BUTTON, 0, 0x13DC);
        edit = dialog_find_control(self, NULL, EDIT_BOX, 0);
        edit_set_capacity(edit, NULL, 11);
        edit_set_text(edit, NULL, player_name);
        edit_set_allowed(edit, NULL, NAME_CHARS);
        gui_user_queue_capture(FIELD(self, 0x34, gui_user*), NULL, edit, NULL, FALSE);
    }
}

/*
    messbox2.dtm (vtable 0x00559E60), screen 0xFC: the modem's host or join. DirectPlay shows
    dialogs of its own meanwhile, so the game's window is minimized around the calls.
*/
PORT(0x004B37D0)
void __fastcall host_join_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_RIGHT)) {
            event_end(event, 0);
            net_leave();
            net_make();
        } else if (clicked(event, BUTTON_LEFT)) {
            name_prompt_open(self, 0x1C0);
        } else if (clicked(event, BUTTON_MIDDLE)) {
            BOOL joined;

            SendMessageA(game_window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
            session_count = 0;
            game_FUN_004b26c0(game_net, NULL, game_sessions, &session_count, 1);
            SendMessageA(game_window, WM_SYSCOMMAND, SC_RESTORE, 0);
            /* The sessions list's last choice, unchecked against the count (the original's). */
            joined = session_count >= 1
                && game_FUN_004b0b70(game_net, NULL, session_instance(session_chosen), 0);
            if (joined)
                go_to(0x866);
            event_end(event, joined ? 0x63 : 0xD);
        }
    } else if (event->type == EVENT_OPEN) {
        load_text(self, TITLE_TEXT, 12, 0x952);
        load_text(self, BUTTON_RIGHT, 0, 0x13DC);
        load_text(self, BUTTON_LEFT, 0, 0x950);
        load_text(self, BUTTON_MIDDLE, 0, 0x951);
    } else if (event->type == EVENT_CHILD_ENDED && event->a == 0x14) {
        BOOL hosted;

        SendMessageA(game_window, WM_SYSCOMMAND, SC_MINIMIZE, 0);
        hosted = game_FUN_004b0ab0(game_net, NULL, player_name, 0);
        SendMessageA(game_window, WM_SYSCOMMAND, SC_RESTORE, 0);
        if (hosted)
            go_to(0x866);
        event_end(event, hosted ? 0x63 : 0xD);
    }
}

/* messbox2.dtm (vtable 0x00559CD4), screen 0x1F9: no session at the address. One button. */
PORT(0x004B3B40)
void __fastcall session_notice_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_MIDDLE))
            result_confirm(0x1F9, event);
    } else if (event->type == EVENT_OPEN) {
        BYTE* control = dialog_find_control(self, NULL, TITLE_TEXT, 0);

        control_set_align(control, NULL, 10);
        control_load_text(control, NULL, game_language, 0x13C2);
        control = dialog_find_control(self, NULL, BUTTON_LEFT, 1);
        control_show(control, NULL, FALSE, TRUE);
        FIELD(control, 0x1D8, DWORD) = 0;
        control_show(dialog_find_control(self, NULL, BUTTON_RIGHT, 0), NULL, FALSE, TRUE);
        control = dialog_find_control(self, NULL, BUTTON_MIDDLE, 0);
        control_load_text(control, NULL, game_language, 0x13E9);
        FIELD(control, 0x1D8, DWORD) = KEY_RETURN;
        load_prompt(self, 0x13C9);
    }
}

/* messbox2.dtm (vtable 0x00556C54), screen 0x1FA: the connection lost; the network is left. */
PORT(0x004B3C60)
void __fastcall connection_lost_handle(BYTE* self, void* edx_unused, ui_event* event)
{
    (void) edx_unused;
    if (event->type == EVENT_CLICK) {
        if (clicked(event, BUTTON_RIGHT)) {
            net_leave();
            event_end(event, 0);
        }
    } else if (event->type == EVENT_OPEN) {
        BYTE* button = dialog_find_control(self, NULL, BUTTON_LEFT, 1);

        control_show(button, NULL, FALSE, TRUE);
        FIELD(button, 0x1D8, DWORD) = 0;
        control_show(dialog_find_control(self, NULL, BUTTON_MIDDLE, 1), NULL, FALSE, TRUE);
        button = dialog_find_control(self, NULL, BUTTON_RIGHT, 1);
        control_load_text(button, NULL, game_language, 0x13E9);
        FIELD(button, 0x1D8, DWORD) = KEY_RETURN;
        load_text(self, TITLE_TEXT, 12, 0x13D1);
    }
}
