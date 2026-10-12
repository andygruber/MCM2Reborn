/*
    OpenMCM2 - the menus' GUI objects, ported from the game (GUIManager.cpp in the original).

    Each function here replaces the game's at the address in its PORT line, and was checked against
    the original's instructions. The original's C++ exception frames are not reproduced: they only
    destroy half-built objects when an exception passes through, and the game throws none here.

    The GUIManager runs the menus: it owns the screen's background, the fonts, the input devices
    and up to four GUIUsers. A GUIUser is one player at the menus, with a cursor, a tooltip and the
    controls it has focus on; it claims input devices, at most one user each.
*/

#include "game.h"
#include <unknwn.h>

typedef struct gui_manager gui_manager;
typedef struct gui_user gui_user;
typedef struct tooltip tooltip;

/* GUICursor, 96 bytes: a GameCursor that can follow a pointing device. */
typedef struct gui_cursor {
    BYTE  game_cursor[0x38];
    DWORD position;                     /* GameCursor's: where it is drawn */
    BYTE  unknown_3c[0x20];
    void* follows;                      /* the device it follows, or NULL */
} gui_cursor;

LAYOUT(gui_cursor, position, 0x38);
LAYOUT(gui_cursor, follows, 0x5C);
_Static_assert(sizeof(gui_cursor) == 96, "GUICursor is 96 bytes");

/* GUIInputDevice, 196 bytes. */
typedef struct gui_input {
    game_object base;
    BYTE        axis_x[0x3C];           /* the two axes, each set up by 0x0043DBF0 */
    BYTE        axis_y[0x3C];
    int         x;                      /* where it points, in screen pixels */
    int         y;
    BYTE*       device;                 /* the game's input device */
    float       range[4];               /* x from, x to, y from, y to; all 0 = the whole screen */
    gui_user*   user;                   /* the user that claimed it, or paired with it */
} gui_input;

LAYOUT(gui_input, axis_x, 0x2C);
LAYOUT(gui_input, axis_y, 0x68);
LAYOUT(gui_input, device, 0xAC);
LAYOUT(gui_input, user, 0xC0);
_Static_assert(sizeof(gui_input) == 196, "GUIInputDevice is 196 bytes");

/* GUIManager, 992 bytes. */
struct gui_manager {
    game_object base;
    int         swapped;                /* exchanged by gui_manager_swap */
    void*       message;                /* a dialog of its own, for messages */
    int         unknown_34;
    int         unknown_38;
    BYTE*       background;             /* the BackgroundImage the menus draw on */
    BYTE*       background_pending;     /* invalidated once the countdown ends */
    void*       font_object;
    int         background_made;        /* the background is the manager's own */
    int         font_object_made;       /* font_object is the manager's own */
    char        name[0x80];
    short       font_size;
    BYTE        unknown_d2[2];
    int         unknown_d4;
    int         unknown_d8;
    void*       backdrop;               /* the screen's picture behind the menus */
    int         backdrop_drawn;
    int         backdrop_slot;          /* its slot in the background, or -1 */
    int         unknown_e8;
    int         unknown_ec;
    char        folder[0x100];          /* the menus' folder, ending in a backslash */
    int         busy;
    int         countdown;              /* frames to wait after being busy */
    int         unknown_1f8;
    HFONT       font;
    char        cursor_image[0x80];     /* the users' cursor unless told otherwise */
    char        wait_image[0x84];       /* the cursor while busy */
    int         unknown_304;
    gui_input*  mouse;
    gui_input*  keyboard;
    gui_input*  joysticks[8];           /* input_count() of them */
    gui_user*   users[4];
    int         user_count;
    game_object* group;                 /* the parent of the devices */
    HMODULE     language;               /* uilang.dll */
    int         unknown_34c;
    char        font_name[0x80];
    int         unknown_3d0[3];
    IUnknown*   palette;                /* released through COM */
};

LAYOUT(gui_manager, name, 0x50);
LAYOUT(gui_manager, font_size, 0xD0);
LAYOUT(gui_manager, backdrop, 0xDC);
LAYOUT(gui_manager, folder, 0xF0);
LAYOUT(gui_manager, busy, 0x1F0);
LAYOUT(gui_manager, cursor_image, 0x200);
LAYOUT(gui_manager, wait_image, 0x280);
LAYOUT(gui_manager, mouse, 0x308);
LAYOUT(gui_manager, keyboard, 0x30C);
LAYOUT(gui_manager, users, 0x330);
LAYOUT(gui_manager, user_count, 0x340);
LAYOUT(gui_manager, group, 0x344);
LAYOUT(gui_manager, font_name, 0x350);
LAYOUT(gui_manager, palette, 0x3DC);
_Static_assert(sizeof(gui_manager) == 992, "GUIManager is 992 bytes");

/* GUIUser, 500 bytes. */
struct gui_user {
    game_object    base;
    gui_input*     input;               /* the device it is paired with */
    game_object*   cursor;
    int            cursor_x;
    int            cursor_y;
    BYTE           unknown_3c[0x80];
    gui_manager*   manager;
    game_object*   tooltip;
    int            unknown_c4;
    char           cursor_image[0x80];
    char           wait_image[0x80];
    int            unknown_1c8;
    BYTE*          next_focus;          /* a dialog to take focus */
    BYTE*          next_capture;        /* a dialog to take focus and capture */
    BYTE*          capture;
    BYTE*          focus;
    BYTE           unknown_1dc[4];
    container_list inputs;              /* the devices it claimed */
    BYTE           flags;               /* bit 0: `inputs` owns its array */
    BYTE           unknown_1f1[3];
};

LAYOUT(gui_user, input, 0x2C);
LAYOUT(gui_user, cursor, 0x30);
LAYOUT(gui_user, manager, 0xBC);
LAYOUT(gui_user, tooltip, 0xC0);
LAYOUT(gui_user, cursor_image, 0xC8);
LAYOUT(gui_user, wait_image, 0x148);
LAYOUT(gui_user, next_focus, 0x1CC);
LAYOUT(gui_user, focus, 0x1D8);
LAYOUT(gui_user, inputs, 0x1E0);
_Static_assert(sizeof(gui_user) == 500, "GUIUser is 500 bytes");

/* ToolTip, 104 bytes: the text that appears under a user's cursor. */
struct tooltip {
    game_object  base;
    gui_manager* manager;
    game_object* text;                  /* the drawn text, or NULL */
    int          background_slot;       /* its slot in the background, or -1 */
    int          redraws;               /* frames to keep redrawing that slot */
    RECT         place;                 /* where it is on the screen */
    RECT         text_area;
    int          enabled;
    float        delay;                 /* seconds before it shows; -1 when there is none */
    int          shown;
};

LAYOUT(tooltip, manager, 0x2C);
LAYOUT(tooltip, place, 0x3C);
LAYOUT(tooltip, text_area, 0x4C);
LAYOUT(tooltip, enabled, 0x5C);
LAYOUT(tooltip, shown, 0x64);
_Static_assert(sizeof(tooltip) == 104, "ToolTip is 104 bytes");

/* The vtables of the GUI classes, in the game's .rdata. */
#define GUICURSOR_VTABLE   ((void**) 0x00558F88)
#define GUIMANAGER_VTABLE  ((void**) 0x00558FF8)
#define GROUP_VTABLE       ((void**) 0x00559068)
#define TOOLTIP_VTABLE     ((void**) 0x005590D8)
#define GUIINPUT_VTABLE    ((void**) 0x00559148)
#define GUIUSER_VTABLE     ((void**) 0x005591B8)

#define WAIT_TGA   ((const char*) 0x00572408)       /* "wait.tga" */
#define CURSOR_TGA ((const char*) 0x005723FC)       /* "cursor.tga" */
#define UIDDL      ((const char*) 0x005724E8)       /* "UIDDL", a drop-down list's class */
#define DIGITS     ((const char*) 0x0056FA28)       /* "0123456789" */
#define NO_TEXT    ((const char*) 0x0057D888)       /* an empty string */

/* GameCursor, the base of GUICursor. */
void __fastcall game_FUN_0043f9a0(void* self, void* edx_unused, DWORD arg);
void __fastcall game_FUN_0043fb00(void* self);
int  __fastcall game_vf_15_43FCE0(void* self);

/* The pointing device's position, read for the cursor that follows it. */
DWORD __fastcall game_FUN_00473fe0(void* device);

/*
    BackgroundImage: the surface the menus are drawn on, with slots for the parts that change.
*/
void __fastcall game_FUN_00404cc0(BYTE* background);                          /* release its DC */
void __fastcall game_FUN_00404de0(BYTE* background);                          /* redraw it */
int  __fastcall game_FUN_00404130(BYTE* background, void* edx_unused, DWORD keep);  /* new slot */
void __fastcall game_FUN_00404240(BYTE* background, void* edx_unused, int slot);    /* free one */
void __fastcall game_FUN_00404280(BYTE* background, void* edx_unused, int slot, RECT* area);
void __fastcall game_FUN_00404cf0(BYTE* background, void* edx_unused, int slot);
void __fastcall game_FUN_004044c0(BYTE* background, void* edx_unused, void* picture, RECT* area,
                                  void* source, DWORD flags, int slot, DWORD a, int* redraws,
                                  DWORD b);
#define background_release_dc  game_FUN_00404cc0
#define background_invalidate  game_FUN_00404de0
#define background_new_slot    game_FUN_00404130
#define background_free_slot   game_FUN_00404240
#define background_slot_redraw game_FUN_00404280
#define background_slot_clear  game_FUN_00404cf0
#define background_draw        game_FUN_004044c0

/* The screen's own draw: vtable slot 3 of object_screen(). */
typedef int (__fastcall* screen_draw)(void* screen, void* edx_unused, RECT* area, void* picture,
                                      void* source, DWORD flags);

/* A dialog: shown or hidden, and its controls. */
void __fastcall game_FUN_0046f990(void* dialog, void* edx_unused, DWORD shown);
int  __fastcall game_FUN_0046f8d0(void* dialog, void* edx_unused, const char* name);
void __fastcall game_FUN_00470fa0(void* dialog, void* edx_unused, DWORD a, DWORD b, RECT* area);
#define dialog_show game_FUN_0046f990
#define dialog_find game_FUN_0046f8d0

/* The C library's strcmp, as the game links it. */
int __cdecl game_FUN_0053b910(const char* a, const char* b);

/* Not ported yet. */
void* __fastcall game_FUN_00486d80(gui_manager* self);
void* __fastcall game_FUN_00487100(gui_manager* self, void* edx_unused, DWORD a, DWORD b);
void* __fastcall game_FUN_00403d80(void* self, void* edx_unused, DWORD flag);
void  __fastcall game_FUN_00488920(gui_user* self, void* edx_unused, DWORD numeric);
void  __fastcall game_FUN_00488d60(gui_user* self, void* edx_unused, const char* image, DWORD arg);
void  __fastcall game_FUN_00488fa0(gui_user* self, void* edx_unused, const char* image, DWORD arg);
void  __fastcall game_FUN_00489050(gui_user* self);
void  __fastcall game_vf_28_483430(gui_manager* self);
int   __fastcall game_FUN_004880e0(gui_input* self);

/* Ported below, and called before they are defined. */
void __fastcall gui_manager_drop_backdrop(gui_manager* self);
void __fastcall gui_manager_load_backdrop(gui_manager* self, void* edx_unused, DWORD flag);
gui_user* __fastcall gui_manager_user(gui_manager* self, void* edx_unused, int index);
void __fastcall gui_user_drop_tooltip(gui_user* self);
void __fastcall gui_user_make_tooltip(gui_user* self, void* edx_unused, void* screen,
                                      gui_manager* manager);
void __fastcall gui_manager_drop_cursors(gui_manager* self);
void __fastcall gui_manager_set_cursors(gui_manager* self, void* edx_unused, const char* image,
                                        DWORD arg);

/* Two strings, equal: the game inlines strcmp() == 0 like this. */
static BOOL text_equal(const char* a, const char* b)
{
    while (*a == *b) {
        if (!*a)
            return TRUE;
        a++;
        b++;
    }
    return FALSE;
}

/* ---- GUICursor ---------------------------------------------------------------------------- */

PORT(0x00486090)
gui_cursor* __fastcall gui_cursor_construct(gui_cursor* self, void* edx_unused, DWORD arg)
{
    (void) edx_unused;
    game_FUN_0043f9a0(self, NULL, arg);
    *(void***) self = GUICURSOR_VTABLE;
    self->follows = NULL;
    return self;
}

PORT(0x004860D0)
void __fastcall gui_cursor_destruct(gui_cursor* self)
{
    *(void***) self = GUICURSOR_VTABLE;
    game_FUN_0043fb00(self);
}

/* Vtable slot 0: the destructor, freeing the object when bit 0 of `flags` is set. */
PORT(0x004860B0)
gui_cursor* __fastcall gui_cursor_delete(gui_cursor* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    gui_cursor_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

PORT(0x004860E0)
void __fastcall gui_cursor_follow(gui_cursor* self, void* edx_unused, void* device)
{
    (void) edx_unused;
    self->follows = device;
    if (!device)
        self->position = 0;
}

/* Vtable slot 15, once a frame: move to where the device points. */
PORT(0x00486100)
void __fastcall gui_cursor_update(gui_cursor* self)
{
    if (self->follows)
        self->position = game_FUN_00473fe0(self->follows);
    game_vf_15_43FCE0(self);
}

/* ---- GUIManager --------------------------------------------------------------------------- */

PORT(0x00486120)
gui_manager* __fastcall gui_manager_construct(gui_manager* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    object_construct(self, NULL, flag);
    *(void***) self = GUIMANAGER_VTABLE;
    self->message = NULL;
    self->font_object_made = 0;
    self->unknown_d4 = 0;
    self->unknown_d8 = 0;
    self->background_made = 0;
    self->backdrop = NULL;
    self->backdrop_slot = -1;
    self->unknown_e8 = 0;
    self->unknown_ec = 1;
    self->folder[0] = 0;
    self->countdown = 0;
    self->unknown_38 = 0;
    self->unknown_34 = 0;
    self->font_object = NULL;
    self->background = NULL;
    self->font = NULL;
    self->language = NULL;
    self->palette = NULL;
    self->busy = 0;
    self->unknown_1f8 = 0;
    self->background_pending = NULL;
    self->unknown_304 = 0;
    self->cursor_image[0] = 0;
    self->wait_image[0] = 0;
    text_copy(self->wait_image, WAIT_TGA);
    text_copy(self->cursor_image, CURSOR_TGA);
    self->mouse = NULL;
    self->keyboard = NULL;
    __stosd((unsigned long*) self->joysticks, 0, 8);
    self->user_count = 0;
    self->users[0] = self->users[1] = self->users[2] = self->users[3] = NULL;
    self->group = NULL;
    self->swapped = 0;
    self->unknown_34c = 0;
    text_copy(self->font_name, NO_TEXT);
    self->unknown_3d0[0] = self->unknown_3d0[1] = self->unknown_3d0[2] = 0;
    self->backdrop_drawn = 0;
    return self;
}

PORT(0x004862B0)
void __fastcall gui_manager_destruct(gui_manager* self)
{
    *(void***) self = GUIMANAGER_VTABLE;
    gui_manager_drop_cursors(self);
    if (self->message)
        object_destroy(self->message);
    gui_manager_drop_backdrop(self);
    if (self->font)
        DeleteObject(self->font);
    if (self->palette)
        self->palette->lpVtbl->Release(self->palette);
    object_destruct(self);
}

PORT(0x00486290)
gui_manager* __fastcall gui_manager_delete(gui_manager* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    gui_manager_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

/* Vtable slots 22 and 23, charged to the UI's memory. */
PORT(0x004867C0)
int __fastcall gui_manager_slot_22(gui_manager* self, void* edx_unused, DWORD a, DWORD b)
{
    void* zone = zone_enter(ZONE_UI);
    int result = game_vf_22_46A4B0(self, NULL, a, b);

    (void) edx_unused;
    zone_leave(zone);
    return result;
}

PORT(0x00486780)
int __fastcall gui_manager_slot_23(gui_manager* self, void* edx_unused, DWORD a, DWORD b)
{
    void* zone = zone_enter(ZONE_UI);
    int result = game_vf_23_46A500(self, NULL, a, b);

    (void) edx_unused;
    zone_leave(zone);
    return result;
}

/* Vtable slot 10, the frame's tick: the end of a busy spell goes to vtable slot 28. */
PORT(0x00486800)
int __fastcall gui_manager_tick(gui_manager* self, void* edx_unused, DWORD time)
{
    void* zone = zone_enter(ZONE_UI);

    (void) edx_unused;
    game_vf_10_46A300(self, NULL, time);
    if (self->busy) {
        self->busy = 0;
        game_vf_28_483430(self);
    }
    zone_leave(zone);
    return 1;
}

/* Draw the backdrop behind the menus, through the background if there is one. */
static int draw_backdrop(gui_manager* self, BYTE* backdrop)
{
    RECT area;

    area.left = 0;
    area.top = 0;
    area.right = FIELD(backdrop, 0x14, LONG);
    area.bottom = FIELD(backdrop, 0x18, LONG);
    if (self->background) {
        background_draw(self->background, NULL, backdrop, &area, NULL, 0x1000000,
                        self->backdrop_slot, 1, &self->unknown_e8, 0);
        return 1;
    }
    return VIRTUAL(object_screen(self), 0xC, screen_draw)(object_screen(self), NULL, &area, backdrop,
                                                         NULL, 0x1000000);
}

/* Vtable slot 13: draw, with the backdrop first when it has been drawn before. */
PORT(0x00486860)
int __fastcall gui_manager_draw_over(gui_manager* self)
{
    if (self->countdown == 0) {
        if (self->backdrop && self->backdrop_drawn && !draw_backdrop(self, self->backdrop)) {
            game_vf_13_46A260(self);
            return 0;
        }
        game_vf_13_46A260(self);
    }
    return 1;
}

/* Vtable slot 15: draw, with the backdrop first the first time; count down a busy spell. */
PORT(0x00486900)
int __fastcall gui_manager_draw(gui_manager* self)
{
    if (self->countdown == 0) {
        if (self->backdrop && !self->backdrop_drawn && !draw_backdrop(self, self->backdrop)) {
            game_vf_15_46A2D0(self);
            return 0;
        }
        game_vf_15_46A2D0(self);
    } else {
        int left = self->countdown - 1;

        self->countdown = left < 0 ? 0 : left;
        VIRTUAL(FIELD(game_main, 0x10, void*), 0x30, void (__fastcall*)(void*, void*, DWORD, DWORD))(
            FIELD(game_main, 0x10, void*), NULL, 0, 0);
        if (self->countdown == 0 && self->background_pending) {
            background_invalidate(self->background_pending);
            self->background_pending = NULL;
        }
    }
    if (self->background)
        background_release_dc(self->background);
    return 1;
}

/* Vtable slot 0x6C of a dialog: lay it out and open it. Nonzero when it opened. */
typedef int (__fastcall* dialog_open)(void* dialog, void* edx_unused, void* screen, DWORD a,
                                      DWORD b, DWORD flags, void* font_object, int c,
                                      char* folder, void* parent, BYTE* background,
                                      const char* font, int font_size, gui_manager* manager,
                                      gui_user* user, DWORD d);

#define ARIAL ((const char*) 0x00572444)            /* "Arial" */

void* __fastcall game_FUN_0046ace0(void* self, void* edx_unused, DWORD a, DWORD b);

/* A walk over an object's descendants of one class, and what is done to each dialog found. */
#define UIDIALOG ((const char*) 0x00572474)         /* "UIDialog" */
void  __fastcall game_FUN_0046a880(void* walk, void* edx_unused, void* root, DWORD deep,
                                   const char* class_name);
BYTE* __fastcall game_FUN_0046a980(void* walk);
void  __fastcall game_FUN_0046a970(void* walk);
void  __fastcall game_FUN_00470f00(void* dialog, void* edx_unused, DWORD a);
void  __fastcall game_FUN_004eda50(void* video);
void  __fastcall game_FUN_004cfd00(void* video, void* edx_unused, DWORD palette);

/*
    Show `dialog`, opened through gui_manager_open: it goes under its parent, or under the
    manager's group, and the background is redrawn.
*/
PORT(0x00486B60)
void __fastcall gui_manager_show_dialog(gui_manager* self, void* edx_unused, BYTE* dialog,
                                        DWORD arg, DWORD wait)
{
    DWORD shown = FIELD(dialog, 0x25, BYTE) & 1;
    BYTE* parent = FIELD(dialog, 0x2C, BYTE*);

    (void) edx_unused;
    dialog_show(dialog, NULL, 0);
    if (parent && (FIELD(dialog, 0x144, BYTE) & 1))
        object_add_sibling(FIELD(parent, 0x7F3C, void*), NULL, dialog, -1);
    else
        object_add_child(self->group, NULL, dialog, -1);
    VIRTUAL(dialog, 0x70, method_1)(dialog, NULL, arg);
    FIELD(dialog, 0xC8, int) = self->unknown_ec;
    if (self->background)
        background_invalidate(self->background);
    if (wait)
        gui_manager_set_cursors(self, NULL, self->cursor_image, self->busy == 0);
    if (self->busy)
        self->countdown = 3;
    dialog_show(dialog, NULL, shown);
}

/*
    Open `dialog`. Bits of `flags`: 2 and 4 keep what is behind it, 4 with 0x10 loads the backdrop
    (8 passed on), 0x10 over the whole screen. With `wait`, the users have the wait cursor
    meanwhile. Returns the dialog, or NULL when it would not open.
*/
PORT(0x00486A00)
BYTE* __fastcall gui_manager_open(gui_manager* self, void* edx_unused, BYTE* dialog, DWORD a,
                                  DWORD flags, DWORD b, BYTE* parent, DWORD arg, DWORD c,
                                  DWORD wait)
{
    (void) edx_unused;
    if (!(flags & 4) && !(flags & 2)) {
        if (!game_FUN_00486d80(self) || !FIELD(game_FUN_00486d80(self), 0x148, int))
            gui_manager_drop_backdrop(self);
    } else {
        RECT screen;
        RECT* area = NULL;

        if (flags & 0x10) {
            screen.left = 0;
            screen.top = 0;
            screen.right = FIELD(object_screen(self), 0xC, LONG);
            screen.bottom = FIELD(object_screen(self), 0x10, LONG);
            area = &screen;
        }
        if (parent)
            game_FUN_00470fa0(parent, NULL, 1, flags & 8, area);
        else if ((flags & 4) && (flags & 0x10))
            gui_manager_load_backdrop(self, NULL, flags & 8);
    }
    if (wait)
        gui_manager_set_cursors(self, NULL, self->wait_image, self->busy == 0);

    if (VIRTUAL(dialog, 0x6C, dialog_open)(dialog, NULL, object_screen(self), b, a, flags,
                                           self->font_object, self->unknown_d8, self->folder,
                                           parent, self->background, self->name, self->font_size,
                                           self, self->users[0], c)) {
        FIELD(dialog, 0x144, DWORD) = flags;
        gui_manager_show_dialog(self, NULL, dialog, arg, wait);
        return dialog;
    }
    if (wait)
        gui_manager_set_cursors(self, NULL, self->cursor_image, self->busy == 0);
    return NULL;
}

PORT(0x00486CE0)
void __fastcall gui_manager_drop_message(gui_manager* self)
{
    if (self->message) {
        object_destroy(self->message);
        self->message = NULL;
    }
}

/* Make the manager's own message dialog, in 14-point Arial. FALSE when it cannot be made. */
PORT(0x00486C10)
BOOL __fastcall gui_manager_make_message(gui_manager* self, void* edx_unused, DWORD arg)
{
    BYTE* dialog;

    (void) edx_unused;
    gui_manager_drop_message(self);
    dialog = game_new(0x7F58, GUIMANAGER_CPP, 0x20B);
    if (dialog)
        dialog = game_FUN_0046ace0(dialog, NULL, 0, arg);
    self->message = dialog;
    if (!dialog)
        return FALSE;
    VIRTUAL(dialog, 0x6C, dialog_open)(dialog, NULL, object_screen(self), 0, 0, 0,
                                       self->font_object, self->unknown_d8, self->folder, NULL,
                                       NULL, ARIAL, 14, NULL, NULL, 0);
    return TRUE;
}

/* The menus' folder, given a trailing backslash if it has none. */
PORT(0x00486D00)
void __fastcall gui_manager_set_folder(gui_manager* self, void* edx_unused, const char* folder)
{
    size_t last = text_length(folder) - 1;

    (void) edx_unused;
    text_copy(self->folder, folder);
    if (folder[last] != '\\')
        text_append(self->folder, "\\");
}

/* A control of the message dialog, by name. */
PORT(0x00486E50)
int __fastcall gui_manager_find_message(gui_manager* self, void* edx_unused, const char* name)
{
    (void) edx_unused;
    return self->message ? dialog_find(self->message, NULL, name) : 0;
}

PORT(0x00486E70)
int __fastcall gui_manager_swap(gui_manager* self, void* edx_unused, int value)
{
    int old = self->swapped;

    (void) edx_unused;
    self->swapped = value;
    return old;
}

/* Make the background, if there is none, and give every user a tooltip on it. */
PORT(0x00486E80)
void __fastcall gui_manager_make_background(gui_manager* self)
{
    BYTE* background;
    int i;

    if (self->background)
        return;
    background = game_new(0x74, GUIMANAGER_CPP, 0x2A0);
    if (background)
        background = game_FUN_00403d80(background, NULL, 1);
    background = VIRTUAL(background, 0x20, void* (__fastcall*)(void*, void*, void*, int))(
        background, NULL, object_screen(self), -1);
    background = object_add_child(self, NULL, background, -1);
    self->background = background;
    object_move_before(background, NULL, self->group, -1);
    self->background_made = 1;
    gui_manager_set_cursors(self, NULL, NULL, 1);
    for (i = 0; i < self->user_count; i++)
        gui_user_make_tooltip(gui_manager_user(self, NULL, i), NULL, object_screen(self), self);
}

/* Undo gui_manager_make_background: the users' tooltips are made again, without it. */
PORT(0x00486F50)
void __fastcall gui_manager_drop_background(gui_manager* self)
{
    BYTE iterator[0x94];
    BYTE* dialog;
    int i;

    if (!self->background_made)
        return;
    gui_manager_drop_cursors(self);
    if (self->background)
        object_destroy(self->background);
    self->background = NULL;
    gui_manager_set_cursors(self, NULL, NULL, 0);
    game_FUN_0046a880(iterator, NULL, self, 1, UIDIALOG);
    for (dialog = game_FUN_0046a980(iterator); dialog; dialog = game_FUN_0046a980(iterator))
        game_FUN_00470f00(dialog, NULL, 0);
    for (i = 0; i < self->user_count; i++)
        gui_user_make_tooltip(gui_manager_user(self, NULL, i), NULL, object_screen(self), self);
    game_FUN_0046a970(iterator);
}

/* Load the backdrop, and give it a slot in the background. */
PORT(0x00487030)
void __fastcall gui_manager_load_backdrop(gui_manager* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    gui_manager_drop_backdrop(self);
    self->backdrop = game_FUN_00487100(self, NULL, flag, 0);
    if (self->background) {
        self->backdrop_slot = background_new_slot(self->background, NULL, 1);
        background_invalidate(self->background);
        FIELD(self->background, 0x30, int) = 0;
    }
}

PORT(0x00487080)
void __fastcall gui_manager_drop_backdrop(gui_manager* self)
{
    if (self->backdrop)
        object_destroy(self->backdrop);
    if (self->background && self->backdrop_slot >= 0) {
        background_free_slot(self->background, NULL, self->backdrop_slot);
        FIELD(self->background, 0x30, int) = 1;
    }
    self->backdrop = NULL;
    self->backdrop_slot = -1;
    self->unknown_e8 = 0;
}

PORT(0x004870E0)
void __fastcall gui_manager_set_busy(gui_manager* self, void* edx_unused, int value)
{
    (void) edx_unused;
    self->busy = 1;
    self->unknown_34 = value;
}

PORT(0x00487480)
int __fastcall gui_manager_unknown_1f8(gui_manager* self)
{
    return self->unknown_1f8;
}

/* Redraw everything: the background, then the game's screens. */
PORT(0x00487490)
void __fastcall gui_manager_refresh(gui_manager* self)
{
    if (self->background) {
        background_release_dc(self->background);
        background_invalidate(self->background);
    }
    VIRTUAL(game_main, 0x20, method_0)(game_main);
    VIRTUAL(game_main, 0x24, method_0)(game_main);
    game_FUN_004eda50(FIELD(game_main, 0x10, void*));
}

PORT(0x004874D0)
gui_user* __fastcall gui_manager_user(gui_manager* self, void* edx_unused, int index)
{
    (void) edx_unused;
    return index < 4 ? self->users[index] : NULL;
}

PORT(0x004874F0)
void __fastcall gui_manager_set_wait_image(gui_manager* self, void* edx_unused, const char* image)
{
    (void) edx_unused;
    text_copy(self->wait_image, image);
}

/* Give every user `image` as its cursor, or the manager's own cursor image. */
PORT(0x00487570)
void __fastcall gui_manager_set_cursors(gui_manager* self, void* edx_unused, const char* image,
                                        DWORD arg)
{
    int i;

    (void) edx_unused;
    for (i = 0; i < self->user_count; i++) {
        if (self->users[i])
            game_FUN_00488fa0(self->users[i], NULL, image ? image : self->cursor_image, arg);
    }
}

/* Show the users' cursors, or hide them: vtable slot 5 or 4 of each. */
PORT(0x004875C0)
void __fastcall gui_manager_show_cursors(gui_manager* self, void* edx_unused, BOOL show)
{
    int count = self->user_count;
    int i;

    (void) edx_unused;
    for (i = 0; i < count; i++) {
        gui_user* user = gui_manager_user(self, NULL, i);

        if (user && user->cursor) {
            if (show)
                VIRTUAL(user->cursor, 0x14, method_0)(user->cursor);
            else
                VIRTUAL(user->cursor, 0x10, method_0)(user->cursor);
        }
    }
}

PORT(0x00487610)
void __fastcall gui_manager_drop_cursors(gui_manager* self)
{
    int i;

    for (i = 0; i < self->user_count; i++) {
        if (self->users[i])
            game_FUN_00489050(self->users[i]);
    }
}

/* The menus' font: `face`, 12 pixels high. */
PORT(0x00487650)
void __fastcall gui_manager_set_font(gui_manager* self, void* edx_unused, const char* face)
{
    (void) edx_unused;
    text_copy(self->font_name, face);
    if (self->font)
        DeleteObject(self->font);
    self->font = CreateFontA(12, 0, 0, 0, FW_NORMAL, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, PROOF_QUALITY, VARIABLE_PITCH, face);
}

/* COM calls on the video objects, by vtable offset. */
typedef HRESULT (__stdcall* com_1)(void* self, DWORD a);
typedef HRESULT (__stdcall* com_2)(void* self, DWORD a, DWORD b);
typedef HRESULT (__stdcall* com_3)(void* self, DWORD a, DWORD b, DWORD c);

static void palette_release(IUnknown* palette)
{
    VIRTUAL(palette, 0x1C, com_2)(palette, 0, 0);
    palette->lpVtbl->Release(palette);
}

/* Use the palette numbered `palette`, or none with 0. */
PORT(0x00487840)
BOOL __fastcall gui_manager_set_palette(gui_manager* self, void* edx_unused, DWORD palette)
{
    void* primary;

    (void) edx_unused;
    if (palette) {
        void* maker;

        if (self->palette)
            palette_release(self->palette);
        maker = FIELD(FIELD(game_main, 0xC, BYTE*), 0x190, void*);
        VIRTUAL(maker, 0x10, com_3)(maker, 0, (DWORD) &self->palette, 0);
        primary = FIELD(FIELD(game_main, 0xC, BYTE*), 0x19C, void*);
        VIRTUAL(primary, 0x70, com_1)(primary, (DWORD) self->palette);
        if (self->palette)
            VIRTUAL(self->palette, 0x20, com_2)(self->palette, 0, FIELD(game_main, 0x31C, DWORD));
    } else {
        if (self->palette) {
            palette_release(self->palette);
            self->palette = NULL;
        }
        primary = FIELD(FIELD(game_main, 0xC, BYTE*), 0x19C, void*);
        VIRTUAL(primary, 0x70, com_1)(primary, 0);
    }
    game_FUN_004cfd00(FIELD(game_main, 0xC, void*), NULL, palette);
    return TRUE;
}

/* ---- ToolTip ------------------------------------------------------------------------------ */

PORT(0x00487920)
tooltip* __fastcall tooltip_construct(tooltip* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    object_construct(self, NULL, flag);
    *(void***) self = TOOLTIP_VTABLE;
    self->shown = 0;
    self->text = NULL;
    self->redraws = 0;
    self->place.bottom = self->place.right = self->place.top = self->place.left = 0;
    self->text_area.bottom = self->text_area.right = 0;
    self->text_area.top = self->text_area.left = 0;
    self->background_slot = -1;
    self->delay = -1.0f;
    self->enabled = 1;
    return self;
}

PORT(0x004879A0)
tooltip* __fastcall tooltip_setup(tooltip* self, void* edx_unused, void* screen,
                                  gui_manager* manager)
{
    (void) edx_unused;
    object_setup(self, NULL, screen);
    self->manager = manager;
    return self;
}

PORT(0x004879C0)
void __fastcall tooltip_destruct(tooltip* self)
{
    *(void***) self = TOOLTIP_VTABLE;
    if (self->text)
        object_destroy(self->text);
    if (self->manager && self->manager->background && self->background_slot > -1)
        background_free_slot(self->manager->background, NULL, self->background_slot);
    object_destruct(self);
}

PORT(0x00487980)
tooltip* __fastcall tooltip_delete(tooltip* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    tooltip_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

/* Vtable slot 10, the frame's tick: count the delay down, and show the text when it runs out. */
PORT(0x00487A40)
int __fastcall tooltip_tick(tooltip* self, void* edx_unused, float seconds)
{
    (void) edx_unused;
    if (!self->enabled) {
        self->shown = 0;
        return 1;
    }
    if (self->delay > 0.0f) {
        self->delay -= seconds;
        if (!(self->delay > 0.0f)) {
            if (!self->text) {
                self->shown = 0;
                return 1;
            }
            self->shown = 1;
        }
    }
    return 1;
}

/* Vtable slot 15: draw the text, or put back what was under it. */
PORT(0x00487D40)
int __fastcall tooltip_draw(tooltip* self)
{
    BYTE* background = self->manager ? self->manager->background : NULL;

    if (self->shown) {
        RECT area;

        if (background) {
            background_draw(background, NULL, self->text, &self->place, &self->text_area,
                            0x1008000, self->background_slot, 0, &self->redraws, 0);
            return 1;
        }
        area.left = self->place.left;
        area.top = self->place.top;
        area.right = self->place.left + self->text_area.right;
        area.bottom = self->place.top + self->text_area.bottom;
        return VIRTUAL(object_screen(self), 0xC, screen_draw)(object_screen(self), NULL, &area,
                                                             self->text, &self->text_area,
                                                             0x1008000) ? 1 : 0;
    }
    if (background && self->background_slot > -1) {
        if (self->redraws > 0) {
            background_slot_redraw(background, NULL, self->background_slot, &self->place);
            self->redraws--;
            return 1;
        }
        background_slot_clear(background, NULL, self->background_slot);
    }
    return 1;
}

/* ---- GUIInputDevice ----------------------------------------------------------------------- */

/* An axis of an input device: its constructor, destructor, release and set-up. */
void  __fastcall game_FUN_0043dbe0(void* axis);
void  __fastcall game_thunk_FUN_0043dd80(void* axis);
void  __fastcall game_FUN_0043dd80(void* axis);

PORT(0x00487E10)
gui_input* __fastcall gui_input_construct(gui_input* self)
{
    object_construct(self, NULL, 1);
    game_FUN_0043dbe0(self->axis_x);
    game_FUN_0043dbe0(self->axis_y);
    self->range[0] = self->range[1] = self->range[2] = self->range[3] = 0.0f;
    self->user = NULL;
    *(void***) self = GUIINPUT_VTABLE;
    return self;
}

PORT(0x00487EB0)
void __fastcall gui_input_destruct(gui_input* self)
{
    *(void***) self = GUIINPUT_VTABLE;
    if (FIELD(self->device, 0xC, int)) {
        game_FUN_0043dd80(self->axis_x);
        game_FUN_0043dd80(self->axis_y);
    }
    game_thunk_FUN_0043dd80(self->axis_y);
    game_thunk_FUN_0043dd80(self->axis_x);
    object_destruct(self);
}

PORT(0x00487E90)
gui_input* __fastcall gui_input_delete(gui_input* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    gui_input_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

/*
    Vtable slot 10, the frame's tick: where the device points. A device of type 1 that is a
    user's has its cursor's position instead, unless the game is in its pointer mode (bit 1 of
    its flags at 0x2D4).
*/
PORT(0x004882B0)
int __fastcall gui_input_tick(gui_input* self, void* edx_unused, DWORD time)
{
    int type;

    (void) edx_unused;
    game_vf_10_46A300(self, NULL, time);
    type = FIELD(self->device, 0xC, int);
    if (!type)
        return 1;
    if (FIELD(game_main, 0x2D4, BYTE) & 2) {
        self->x = (int) FIELD(self->axis_x, 0x24, float);
        self->y = (int) FIELD(self->axis_y, 0x24, float);
        return 1;
    }
    if (type == 1 && self->user && self->user->cursor) {
        self->x = FIELD(self->user->cursor, 0x54, int);
        self->y = FIELD(self->user->cursor, 0x58, int);
    }
    return 1;
}

PORT(0x00488340)
int __fastcall gui_input_slot_25(gui_input* self, void* edx_unused, DWORD a)
{
    (void) edx_unused;
    game_vf_25_46A650(self, NULL, a);
    game_FUN_004880e0(self);
    return 1;
}

/* ---- GUIUser ------------------------------------------------------------------------------ */

PORT(0x00488360)
gui_user* __fastcall gui_user_construct(gui_user* self)
{
    object_construct(self, NULL, 1);
    self->inputs.items = NULL;
    self->inputs.count = 0;
    self->inputs.grow_by = 0;
    self->inputs.capacity = 0;
    self->flags &= ~1;
    *(void***) self = GUIUSER_VTABLE;
    self->manager = NULL;
    self->tooltip = NULL;
    self->cursor = NULL;
    self->unknown_1c8 = 0;
    self->cursor_image[0] = 0;
    self->wait_image[0] = 0;
    self->unknown_c4 = 0;
    text_copy(self->wait_image, WAIT_TGA);
    text_copy(self->cursor_image, CURSOR_TGA);
    self->capture = NULL;
    self->focus = NULL;
    self->next_focus = NULL;
    self->next_capture = NULL;
    self->inputs.items = game_new(4, CONTAINER_LIST_H, 0x3B);
    self->inputs.grow_by = 1;
    self->inputs.capacity = 1;
    self->flags |= 1;
    self->input = NULL;
    self->cursor_y = 0;
    self->cursor_x = 0;
    __stosd((unsigned long*) self->unknown_3c, 0, sizeof self->unknown_3c / 4);
    return self;
}

PORT(0x004884D0)
void __fastcall gui_user_destruct(gui_user* self)
{
    *(void***) self = GUIUSER_VTABLE;
    game_delete(self->inputs.items);
    object_destruct(self);
}

PORT(0x004884B0)
gui_user* __fastcall gui_user_delete(gui_user* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    gui_user_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

/* Calls a control's vtable takes for focus, by offset. */
typedef int  (__fastcall* control_take)(void* control);
typedef void (__fastcall* control_lose)(void* control, void* edx_unused, void* to);

/* A control that can lose focus: there, not marked (bit 3 of byte 0x25), and not `to`. */
static BOOL loses_focus(BYTE* control, BYTE* to)
{
    return control && !(FIELD(control, 0x25, BYTE) & 8) && control != to;
}

/* Give `control` focus. The old one is told, and returned through `old` if wanted. */
PORT(0x00488790)
int __fastcall gui_user_set_focus(gui_user* self, void* edx_unused, BYTE* control, BYTE** old)
{
    int taken = 1;

    (void) edx_unused;
    if (control && control != self->focus) {
        taken = VIRTUAL(control, 0x7C, control_take)(control);
        if (!taken)
            return 0;
    }
    if (loses_focus(self->focus, control))
        VIRTUAL(self->focus, 0x80, control_lose)(self->focus, NULL, control);
    if (old)
        *old = self->focus;
    self->focus = control;
    return taken;
}

/*
    Give `control` focus and capture. With the game's text input on, a text box (type 11) that
    takes only digits turns it to numbers.
*/
PORT(0x00488800)
int __fastcall gui_user_set_capture(gui_user* self, void* edx_unused, BYTE* control, BYTE** old)
{
    int taken = 1;

    (void) edx_unused;
    if (control && control != self->focus) {
        taken = VIRTUAL(control, 0x7C, control_take)(control);
        if (!taken)
            return 0;
    }
    if (loses_focus(self->focus, control))
        VIRTUAL(self->focus, 0x80, control_lose)(self->focus, NULL, control);
    if (loses_focus(self->capture, control))
        VIRTUAL(self->capture, 0x84, control_lose)(self->capture, NULL, control);
    if (old)
        *old = self->capture;
    self->focus = control;
    self->capture = control;

    if (FIELD(FIELD(game_main, 0xC, BYTE*), 0x6C, int)) {
        if (FIELD(control, 0x5C, int) == 11) {
            const char* allowed = FIELD(control, 0x210, const char*);

            game_FUN_00488920(self, NULL, !(allowed && text_equal(allowed, DIGITS)));
        } else {
            game_FUN_00488920(self, NULL, 0);
        }
    }
    return taken;
}

/* Move focus to the dialog waiting for it; `force` moves it even when none is. */
PORT(0x00488550)
void __fastcall gui_user_apply_focus(gui_user* self, void* edx_unused, BOOL force)
{
    (void) edx_unused;
    if (self->next_capture) {
        gui_user_set_capture(self, NULL, self->next_capture, NULL);
        self->next_focus = NULL;
        self->next_capture = NULL;
    } else if (self->next_focus || force) {
        gui_user_set_focus(self, NULL, self->next_focus, NULL);
        self->next_focus = NULL;
    }
}

/* Vtable slot 10, the frame's tick. */
PORT(0x00488530)
int __fastcall gui_user_tick(gui_user* self, void* edx_unused, DWORD time)
{
    (void) edx_unused;
    gui_user_apply_focus(self, NULL, 0);
    return game_vf_10_46A300(self, NULL, time);
}

/*
    Queue `dialog` for focus. Refused, with 0, while another dialog that is not a drop-down list
    is waiting.
*/
PORT(0x004886C0)
int __fastcall gui_user_queue_focus(gui_user* self, void* edx_unused, BYTE* dialog, BYTE** old,
                                    BOOL now)
{
    (void) edx_unused;
    if (self->next_focus && game_FUN_0053b910(FIELD(self->next_focus, 0x28, const char*), UIDDL))
        return 0;
    if (old)
        *old = self->next_focus;
    self->next_focus = dialog;
    if (now)
        gui_user_apply_focus(self, NULL, 1);
    return 1;
}

PORT(0x00488720)
int __fastcall gui_user_queue_capture(gui_user* self, void* edx_unused, BYTE* dialog, BYTE** old,
                                      BOOL now)
{
    (void) edx_unused;
    if (self->next_capture
        && game_FUN_0053b910(FIELD(self->next_capture, 0x28, const char*), UIDDL))
        return 0;
    if (old)
        *old = self->next_capture;
    self->next_focus = dialog;
    self->next_capture = dialog;
    if (now)
        gui_user_apply_focus(self, NULL, 1);
    return 1;
}

/* Vtable slot 15: the cursor goes where the paired device points. */
PORT(0x004885B0)
int __fastcall gui_user_update(gui_user* self)
{
    game_vf_15_46A2D0(self);
    if (self->input) {
        self->cursor_x = self->input->x;
        self->cursor_y = self->input->y;
    }
    return 1;
}

/* Replace the user's tooltip with a new one. */
PORT(0x00488610)
void __fastcall gui_user_make_tooltip(gui_user* self, void* edx_unused, void* screen,
                                      gui_manager* manager)
{
    tooltip* tip;

    (void) edx_unused;
    gui_user_drop_tooltip(self);
    tip = game_new(sizeof(tooltip), GUIMANAGER_CPP, 0x667);
    if (tip)
        tip = tooltip_construct(tip, NULL, 1);
    self->tooltip = object_add_child(self, NULL, tooltip_setup(tip, NULL, screen, manager), -1);
}

PORT(0x004885E0)
gui_user* __fastcall gui_user_setup(gui_user* self, void* edx_unused, void* screen,
                                    gui_manager* manager)
{
    (void) edx_unused;
    object_setup(self, NULL, screen);
    self->manager = manager;
    gui_user_make_tooltip(self, NULL, screen, manager);
    return self;
}

PORT(0x004886A0)
void __fastcall gui_user_drop_tooltip(gui_user* self)
{
    if (self->tooltip)
        object_destroy(self->tooltip);
    self->tooltip = NULL;
}

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

/* Claim the manager's keyboard, if there is one, and then every joystick. */
PORT(0x00488C90)
void __fastcall gui_user_claim_inputs(gui_user* self)
{
    int i;

    if (self->manager->keyboard)
        gui_user_claim(self, NULL, self->manager->keyboard);
    for (i = 0; i < input_count(); i++)
        gui_user_claim(self, NULL, self->manager->joysticks[i]);
}

/* Give every user a cursor, `image` or else the manager's own. `arg` goes to the cursor. */
PORT(0x00487520)
void __fastcall gui_manager_add_cursors(gui_manager* self, void* edx_unused, const char* image,
                                        DWORD arg)
{
    int i;

    (void) edx_unused;
    for (i = 0; i < self->user_count; i++) {
        if (self->users[i])
            game_FUN_00488d60(self->users[i], NULL, image ? image : self->cursor_image, arg);
    }
}
