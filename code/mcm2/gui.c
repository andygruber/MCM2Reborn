/*
    OpenMCM2 - the menus' GUI objects, ported from the game (GUIManager.cpp in the original).

    Each function here replaces the game's at the address in its PORT line, and was checked against
    the original's instructions.

    A GUIUser is one player at the menus. It claims input devices, at most one user each, and is
    paired with one of them; it has a cursor and a tooltip. The GUIManager holds the users and the
    devices.
*/

#include "game.h"

typedef struct gui_user gui_user;

/* GUIInputDevice, 196 bytes. */
typedef struct gui_input {
    game_object base;
    BYTE        unknown_2c[0x94];
    gui_user*   user;                   /* the user that claimed it, or paired with it */
} gui_input;

LAYOUT(gui_input, user, 0xC0);
_Static_assert(sizeof(gui_input) == 196, "GUIInputDevice is 196 bytes");

/* GUIManager, 992 bytes. */
typedef struct gui_manager {
    game_object base;
    BYTE        unknown_2c[0x1D4];
    BYTE        cursor_area[0x10C];     /* where the users' cursors go unless told otherwise */
    gui_input*  primary_input;          /* claimed first, by whichever user gets it */
    gui_input*  inputs[8];              /* input_count() of them */
    gui_user*   users[4];
    int         user_count;
    BYTE        unknown_344[0x9C];
} gui_manager;

LAYOUT(gui_manager, cursor_area, 0x200);
LAYOUT(gui_manager, primary_input, 0x30C);
LAYOUT(gui_manager, users, 0x330);
LAYOUT(gui_manager, user_count, 0x340);
_Static_assert(sizeof(gui_manager) == 992, "GUIManager is 992 bytes");

/* GUIUser, 500 bytes. */
struct gui_user {
    game_object    base;
    gui_input*     input;               /* the device it is paired with */
    game_object*   cursor;
    BYTE           unknown_34[0x88];
    gui_manager*   manager;
    game_object*   tooltip;
    BYTE           unknown_c4[4];
    BYTE           cursor_area[0x118];  /* the default for its cursor; the layout is unknown */
    container_list inputs;              /* the devices it claimed */
    BYTE           unknown_1f0[4];
};

LAYOUT(gui_user, input, 0x2C);
LAYOUT(gui_user, cursor, 0x30);
LAYOUT(gui_user, manager, 0xBC);
LAYOUT(gui_user, tooltip, 0xC0);
LAYOUT(gui_user, cursor_area, 0xC8);
LAYOUT(gui_user, inputs, 0x1E0);
_Static_assert(sizeof(gui_user) == 500, "GUIUser is 500 bytes");

/* Give the user a cursor in `area`, or in its own default area, unless it has one. Not ported. */
void __fastcall game_FUN_00488d60(gui_user* self, void* edx_unused, void* area, DWORD cursor_arg);

/*
    Pair the user with `input`, or unpair it with NULL. TRUE when the two end up paired, and also
    when they already were; FALSE, changing nothing, when `input` already belongs to another user.
*/
PORT(0x004890B0)
BOOL __fastcall gui_user_pair(gui_user* self, void* edx_unused, gui_input* input)
{
    gui_user* owner = input ? input->user : NULL;

    (void) edx_unused;
    if (owner)
        return owner == self;

    self->input = input;
    if (input)
        input->user = self;
    return TRUE;
}

/*
    Claim `input` for the user. FALSE when another user has it. When the list of claimed devices
    cannot grow, the device is still marked as the user's, as the game does.
*/
PORT(0x00488BC0)
BOOL __fastcall gui_user_claim(gui_user* self, void* edx_unused, gui_input* input)
{
    (void) edx_unused;
    if (input->user)
        return input->user == self;

    list_add(&self->inputs, input);
    input->user = self;
    return TRUE;
}

/* Claim the manager's primary device, if there is one, and then every other device it has. */
PORT(0x00488C90)
void __fastcall gui_user_claim_inputs(gui_user* self)
{
    int i;

    if (self->manager->primary_input)
        gui_user_claim(self, NULL, self->manager->primary_input);
    for (i = 0; i < input_count(); i++)
        gui_user_claim(self, NULL, self->manager->inputs[i]);
}

PORT(0x004886A0)
void __fastcall gui_user_drop_tooltip(gui_user* self)
{
    if (self->tooltip)
        object_destroy(self->tooltip);
    self->tooltip = NULL;
}

/* Give every user a cursor, in `area` or else the manager's own. `cursor_arg` goes to the cursor. */
PORT(0x00487520)
void __fastcall gui_manager_add_cursors(gui_manager* self, void* edx_unused, void* area, DWORD cursor_arg)
{
    int i;

    (void) edx_unused;
    for (i = 0; i < self->user_count; i++) {
        if (self->users[i])
            game_FUN_00488d60(self->users[i], NULL, area ? area : self->cursor_area, cursor_arg);
    }
}
