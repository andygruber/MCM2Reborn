/*
    OpenMCM2 - the menus' dialogs and controls, ported from the game (gameui.cpp in the original).

    Each function here replaces the game's at the address in its PORT line, and was checked against
    the original's instructions. C++ exception frames are not reproduced (see gui.c).

    A UIDialog (32,600 bytes) is one menu screen, loaded from a dialog file. Its controls live under
    a UICtlContainer; it belongs to the GUIManager and to the GUIUser it was opened for. The layout
    of most of it is not known yet, so its fields are named by offset below.
*/

#include "gui.h"

/* UIDialog fields, by offset. */
#define DLG_PARENT        0x2C      /* the dialog it was opened over, or NULL */
#define DLG_MANAGER       0x30      /* gui_manager* */
#define DLG_USER          0x34      /* gui_user* */
#define DLG_NAME          0x38      /* char[], its class name ("UIDDL", ...) */
#define DLG_RESOURCE      0xB8      /* its entry in the resource manager */
#define DLG_SHOWN         0xBC
#define DLG_FONT          0xD8      /* HFONT */
#define DLG_SCREEN_SIZE   0xDC
#define DLG_FACE          0xE0      /* char[], its font's face */
#define DLG_BACKGROUND    0x110     /* the manager's BackgroundImage, when it has one */
#define DLG_COLOURS       0x118     /* 10 COLORREFs, white to start with */
#define DLG_BACKDROP      0x140     /* game_object*, shared with the parent when the same */
#define DLG_OPEN_FLAGS    0x144     /* as passed to gui_manager_open */
#define DLG_MODAL         0x148
#define DLG_SWAP          0x14C     /* given to gui_manager_swap when it closes */
#define DLG_SOUND         0x154
#define DLG_SOUND_ID      0x17C
#define DLG_CONTROLS      0x18C     /* game_object*[], DLG_CONTROL_COUNT of them */
#define DLG_CONTROL_COUNT 0x9B0
#define DLG_ENTRY_COUNT   0x7EFC    /* named entries at DLG_ENTRIES, 0x3C bytes each */
#define DLG_ENTRIES       0x9CC
#define DLG_OLD_FONT      0x7F04    /* selected out of the DC by dialog_begin_paint */
#define DLG_CLIP          0x7F00    /* HRGN */
#define DLG_CHILDREN      0x7F24    /* container_list of objects it owns */
#define DLG_CONTAINER     0x7F3C    /* the UICtlContainer its controls are under */

#define UIDIALOG_VTABLE     ((void**) 0x00557B54)
#define UICTLCONTAINER_VTABLE ((void**) 0x00557BD8)
#define UICONTROL_CLASS     ((const char*) 0x005717A0)  /* "UIControl" */

/* The game's resource manager, at 0x00578C84, and its lookup and release. */
#define game_resources (*(void* const*) 0x00578C84)
BYTE* __fastcall game_FUN_004ee0f0(void* resources, void* edx_unused, const char* name, DWORD a);
void  __fastcall game_FUN_004ee5c0(void* resources, void* edx_unused, void* entry);

/* The C library's _stricmp, as the game links it. */
int __cdecl game_FUN_0053b6e0(const char* a, const char* b);

/* A walk over an object's descendants of one class (also in gui.c). */
void  __fastcall game_FUN_0046a880(void* walk, void* edx_unused, void* root, DWORD deep,
                                   const char* class_name);
BYTE* __fastcall game_FUN_0046a980(void* walk);
void  __fastcall game_FUN_0046a970(void* walk);
#define walk_begin game_FUN_0046a880
#define walk_next  game_FUN_0046a980
#define walk_end   game_FUN_0046a970

/* GameObject's own versions of the slots UIDialog overrides. */
int __fastcall game_vf_2_46A5F0(void* self);

/* The rest of UIDialog and UIControl, not ported yet. */
void* __fastcall game_FUN_0046ac10(void* self, void* edx_unused, void* dialog);
int   __fastcall game_FUN_0046b850(BYTE* self, void* edx_unused, void* data, DWORD size);
void  __fastcall game_FUN_00470050(BYTE* self);
void  __fastcall game_FUN_00470d70(BYTE* self, void* edx_unused, DWORD key);
void  __fastcall game_FUN_00470ea0(BYTE* self, void* edx_unused, DWORD sound, DWORD kind);
void  __fastcall game_FUN_00471590(BYTE* control, void* edx_unused, DWORD enabled, DWORD redraw);
void  __fastcall game_FUN_004040f0(BYTE* background, void* edx_unused, DWORD slot);
int   __fastcall game_FUN_00404a10(BYTE* background, void* edx_unused, HDC* dc, const RECT* area,
                                   DWORD slot, DWORD a, void* b, void* out, RECT* clip);
const char* __fastcall game_FUN_00471d20(BYTE* control);              /* its name */

/* What a dialog's vtable slot 0x74 is told: the dialog closing (6) or slot 28 (5). */
typedef struct ui_event {
    DWORD        a;
    DWORD        b;
    DWORD        type;
    void*        sender;
    gui_manager* manager;
    DWORD        c;
    DWORD        param;
    DWORD        d;
    DWORD        e;
} ui_event;

_Static_assert(sizeof(ui_event) == 0x24, "the event is 36 bytes");

typedef int (__fastcall* dialog_handle)(void* self, void* edx_unused, ui_event* event);

/* ---- UIDialog: life cycle ----------------------------------------------------------------- */

PORT(0x0046ACE0)
BYTE* __fastcall dialog_construct(BYTE* self, void* edx_unused, DWORD flag, const char* name)
{
    container_list* children = &FIELD(self, DLG_CHILDREN, container_list);
    int i;

    (void) edx_unused;
    object_construct(self, NULL, flag);
    children->items = NULL;
    children->count = 0;
    children->grow_by = 0;
    children->capacity = 0;
    FIELD(self, 0x7F34, BYTE) &= ~1;
    *(void***) self = UIDIALOG_VTABLE;
    game_FUN_0046ac10(self, NULL, self);
    text_copy((char*) self + DLG_NAME, name ? name : NO_TEXT);

    FIELD(self, DLG_PARENT, void*) = NULL;
    FIELD(self, DLG_MANAGER, void*) = NULL;
    FIELD(self, DLG_SOUND_ID, DWORD) = 0;
    FIELD(self, DLG_FONT, HFONT) = NULL;
    FIELD(self, 0x108, DWORD) = 0;
    FIELD(self, 0x10C, DWORD) = 0;
    FIELD(self, DLG_FACE, char) = 0;
    FIELD(self, 0x174, DWORD) = 640;
    FIELD(self, 0x178, DWORD) = 480;
    FIELD(self, DLG_SHOWN, DWORD) = 1;
    FIELD(self, 0xC4, DWORD) = 0;
    FIELD(self, 0xCC, DWORD) = 0;
    FIELD(self, 0xD0, DWORD) = 0;
    FIELD(self, 0xD4, DWORD) = 0;
    FIELD(self, 0x95C, DWORD) = 0;
    FIELD(self, 0x184, DWORD) = 0;
    FIELD(self, 0x188, DWORD) = 0;
    FIELD(self, DLG_CONTROL_COUNT, DWORD) = 0;
    FIELD(self, 0x9B4, DWORD) = 0;
    FIELD(self, 0x7F20, DWORD) = 0;
    FIELD(self, DLG_BACKGROUND, BYTE*) = NULL;
    FIELD(self, 0x114, DWORD) = 0;
    FIELD(self, 0x7F14, DWORD) = 0;
    FIELD(self, 0xC0, DWORD) = 0;
    FIELD(self, DLG_MODAL, DWORD) = 0;
    for (i = 0; i < 10; i++)
        FIELD(self, DLG_COLOURS + 4 * i, COLORREF) = 0xFFFFFF;
    FIELD(self, 0x9C8, float) = 1.0f;
    FIELD(self, 0x9C4, float) = 1.0f;
    FIELD(self, DLG_BACKDROP, void*) = NULL;
    FIELD(self, 0x7F0C, DWORD) = 0;
    FIELD(self, 0x9B8, DWORD) = 0;
    FIELD(self, 0x7F10, DWORD) = 0;
    FIELD(self, 0x158, DWORD) = 1;
    FIELD(self, 0x15C, DWORD) = 0;
    FIELD(self, 0x170, DWORD) = 0x12;
    FIELD(self, 0x180, DWORD) = 7;
    FIELD(self, 0xC8, DWORD) = 1;
    __stosd((unsigned long*) (self + DLG_ENTRIES), 0, 0x1D4C);
    FIELD(self, DLG_ENTRY_COUNT, DWORD) = 0;
    FIELD(self, DLG_SOUND, DWORD) = 1;
    FIELD(self, 0x9C0, DWORD) = 0;
    FIELD(self, 0x7F18, DWORD) = 1;

    children->items = game_new(8, CONTAINER_LIST_H, 0x3B);
    children->grow_by = 1;
    children->capacity = 2;
    FIELD(self, 0x7F34, BYTE) |= 1;
    FIELD(self, 0x7F1C, DWORD) = 1;
    FIELD(self, DLG_USER, void*) = NULL;
    FIELD(self, DLG_RESOURCE, void*) = NULL;
    FIELD(self, DLG_CONTAINER, void*) = NULL;
    FIELD(self, DLG_SWAP, DWORD) = 0;
    FIELD(self, 0x7F44, DWORD) = 0;
    FIELD(self, DLG_OPEN_FLAGS, DWORD) = 0;
    FIELD(self, 0x7F50, DWORD) = 0;
    FIELD(self, 0x7F48, int) = -1;
    return self;
}

PORT(0x0046AFA0)
void __fastcall dialog_destruct(BYTE* self)
{
    gui_manager* manager = FIELD(self, DLG_MANAGER, gui_manager*);
    BYTE* parent;
    BYTE* backdrop;
    container_list* children;
    int i;

    *(void***) self = UIDIALOG_VTABLE;
    if (manager) {
        if (FIELD(self, DLG_MODAL, int))
            gui_manager_swap(manager, NULL, FIELD(self, DLG_SWAP, int));
        if (FIELD(FIELD(game_main, 0x10, BYTE*), 0x28, int) == 8) {
            manager = FIELD(self, DLG_MANAGER, gui_manager*);
            if (!manager || !gui_manager_unknown_1f8(manager))
                game_vf_28_483430(FIELD(self, DLG_MANAGER, gui_manager*));
        }
        if (FIELD(self, DLG_MODAL, int) && FIELD(self, DLG_PARENT, BYTE*))
            dialog_freeze(FIELD(self, DLG_PARENT, BYTE*), NULL, 0, 0, NULL);
        if ((FIELD(self, DLG_OPEN_FLAGS, DWORD) & 4) && (FIELD(self, DLG_OPEN_FLAGS, DWORD) & 0x10))
            gui_manager_drop_backdrop(FIELD(self, DLG_MANAGER, gui_manager*));
    }
    if (FIELD(self, DLG_BACKGROUND, BYTE*) && !(FIELD(game_main, 0x2D5, BYTE) & 2))
        background_invalidate(FIELD(self, DLG_BACKGROUND, BYTE*));

    parent = FIELD(self, DLG_PARENT, BYTE*);
    if (parent) {
        void* parent_backdrop = FIELD(parent, DLG_BACKDROP, void*);

        if (parent_backdrop && parent_backdrop != FIELD(self, DLG_BACKDROP, void*)
            && FIELD(parent, DLG_MANAGER, gui_manager*)
            && !gui_manager_unknown_1f8(FIELD(parent, DLG_MANAGER, gui_manager*))) {
            parent = FIELD(self, DLG_PARENT, BYTE*);
            gui_manager_set_busy(FIELD(parent, DLG_MANAGER, gui_manager*), NULL,
                                 FIELD(parent, DLG_BACKDROP, int));
        }
        if (FIELD(self, DLG_SOUND, int))
            game_FUN_00470ea0(self, NULL, FIELD(self, DLG_SOUND_ID, DWORD), 9);
    }
    if (FIELD(self, DLG_FONT, HFONT))
        DeleteObject(FIELD(self, DLG_FONT, HFONT));
    if (FIELD(self, DLG_CONTROL_COUNT, int)) {
        for (i = 0; i < FIELD(self, DLG_CONTROL_COUNT, int); i++) {
            void* control = FIELD(self, DLG_CONTROLS + 4 * i, void*);

            if (control)
                object_destroy(control);
        }
    }
    if (!(FIELD(game_main, 0x2D5, BYTE) & 2) && game_resources && FIELD(self, DLG_RESOURCE, void*))
        game_FUN_004ee5c0(game_resources, NULL, FIELD(self, DLG_RESOURCE, void*));

    backdrop = FIELD(self, DLG_BACKDROP, BYTE*);
    parent = FIELD(self, DLG_PARENT, BYTE*);
    if ((FIELD(self, 0x7F0C, int) && backdrop && !parent)
        || (backdrop && parent && FIELD(parent, DLG_BACKDROP, BYTE*) != backdrop))
        object_destroy(backdrop);

    if (!(FIELD(game_main, 0x2D5, BYTE) & 2) && FIELD(self, 0x7F10, int) && FIELD(self, 0x9B8, int)) {
        object_destroy(FIELD(FIELD(self, DLG_BACKGROUND, BYTE*), 0x2C, void*));
        game_FUN_004040f0(FIELD(self, DLG_BACKGROUND, BYTE*), NULL, FIELD(self, 0x9B8, DWORD));
    }
    parent = FIELD(self, DLG_PARENT, BYTE*);
    if (!(FIELD(game_main, 0x2D5, BYTE) & 2) && parent && FIELD(parent, DLG_BACKGROUND, BYTE*))
        background_invalidate(FIELD(parent, DLG_BACKGROUND, BYTE*));

    children = &FIELD(self, DLG_CHILDREN, container_list);
    for (i = 0; children->count > 0 && children->items && i < children->count; i++) {
        if (!children->items[i])
            break;
        object_destroy(children->items[i]);
    }
    game_delete(children->items);
    object_destruct(self);
}

PORT(0x0046AF20)
BYTE* __fastcall dialog_delete(BYTE* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    dialog_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

void __fastcall dialog_notify_controls(BYTE* self, void* edx_unused, DWORD flag);

/* Vtable slot 2: close. The controls and the dialog are told (slot 0x7C), then its handler. */
PORT(0x0046AF40)
int __fastcall dialog_close(BYTE* self)
{
    ui_event event;

    dialog_notify_controls(self, NULL, 1);
    VIRTUAL(self, 0x7C, method_1)(self, NULL, 1);
    event.a = 0;
    event.b = 0;
    event.type = 6;
    event.sender = self;
    event.manager = FIELD(self, DLG_MANAGER, gui_manager*);
    event.c = 0;
    event.e = 0;
    VIRTUAL(self, 0x74, dialog_handle)(self, NULL, &event);
    return game_vf_2_46A5F0(self);
}

/* Vtable slot 28: tell the dialog's handler, with `param`. */
PORT(0x0046F7F0)
int __fastcall dialog_slot_28(BYTE* self, void* edx_unused, DWORD param)
{
    ui_event event;

    (void) edx_unused;
    event.e = 0;
    event.a = 0;
    event.b = 0;
    event.c = 0;
    event.manager = FIELD(self, DLG_MANAGER, gui_manager*);
    event.param = param;
    event.type = 5;
    event.sender = self;
    return VIRTUAL(self, 0x74, dialog_handle)(self, NULL, &event);
}

/* ---- UIDialog: input ---------------------------------------------------------------------- */

/* A key or button event: its code, and the device kind (1 is the mouse). */
typedef struct key_event {
    DWORD code;
    DWORD device;
} key_event;

/* Vtable slot 23: a key pressed, recorded in the user's key table unless the dialog is frozen. */
PORT(0x0046B6B0)
int __fastcall dialog_press(BYTE* self, void* edx_unused, key_event* event, DWORD arg)
{
    (void) edx_unused;
    if (event->device && event->code < 32)
        FIELD(self, DLG_USER, gui_user*)->keys[event->code] = 1;
    if (FIELD(self, 0x25, BYTE) & 4)
        return 0;
    if (event->device == 1)
        game_FUN_00470050(self);
    return game_vf_23_46A500(self, NULL, (DWORD) event, arg);
}

/* Vtable slot 22: a key released. */
PORT(0x0046B700)
int __fastcall dialog_release(BYTE* self, void* edx_unused, key_event* event, DWORD arg)
{
    (void) edx_unused;
    if (event->device && event->code < 32)
        FIELD(self, DLG_USER, gui_user*)->keys[event->code] = 0;
    if (FIELD(self, 0x25, BYTE) & 4)
        return 0;
    if (event->device == 1)
        game_FUN_00470050(self);
    if (event->device) {
        game_FUN_00470d70(self, NULL, 0x101);
        game_FUN_00470d70(self, NULL, 0x102);
    }
    return game_vf_22_46A4B0(self, NULL, (DWORD) event, arg);
}

/* ---- UIDialog: controls ------------------------------------------------------------------- */

/* Give `control` the dialog's screen, manager and size, and the id `id`. */
PORT(0x0046B770)
BYTE* __fastcall dialog_attach(BYTE* self, void* edx_unused, BYTE* control, DWORD id, DWORD keep)
{
    (void) edx_unused;
    if (!control)
        return NULL;
    FIELD(control, 0x18, void*) = object_screen(self);
    FIELD(control, 0x1DC, DWORD) = FIELD(self, 0x7F18, DWORD);
    FIELD(control, 0xBC, void*) = FIELD(self, DLG_MANAGER, void*);
    FIELD(control, 0x78, DWORD) = id;
    FIELD(control, 0x160, DWORD) = FIELD(self, DLG_SCREEN_SIZE, DWORD);
    if (FIELD(self, DLG_BACKGROUND, BYTE*))
        FIELD(control, 0x1B8, int) = background_new_slot(FIELD(self, DLG_BACKGROUND, BYTE*), NULL,
                                                          keep);
    return control;
}

/* Attach `control` and put it under the dialog's control container. */
PORT(0x0046B7D0)
BYTE* __fastcall dialog_add(BYTE* self, void* edx_unused, BYTE* control, DWORD id, DWORD keep)
{
    (void) edx_unused;
    if (!control)
        return NULL;
    dialog_attach(self, NULL, control, id, keep);
    object_add_child(FIELD(self, DLG_CONTAINER, void*), NULL, control, -1);
    return control;
}

/* Load the dialog file named `name` from the game's resources. FALSE when there is none. */
PORT(0x0046B810)
int __fastcall dialog_load(BYTE* self, void* edx_unused, const char* name)
{
    BYTE* entry = game_FUN_004ee0f0(game_resources, NULL, name, 1);

    (void) edx_unused;
    if (!entry)
        return 0;
    return game_FUN_0046b850(self, NULL, FIELD(entry, 0x14, void*), FIELD(entry, 0x18, DWORD));
}

/* Hidden, or shown: vtable slot 4 or 5 of the object. */
PORT(0x0046F990)
int __fastcall dialog_show(void* dialog, void* edx_unused, DWORD shown)
{
    (void) edx_unused;
    FIELD(dialog, DLG_SHOWN, DWORD) = shown;
    return shown ? game_vf_5_469F90(dialog) : game_vf_4_469F80(dialog);
}

/* The value of the entry named `name`, or of the manager's message dialog's; 0 when none. */
PORT(0x0046F8D0)
int __fastcall dialog_find(void* dialog, void* edx_unused, const char* name)
{
    BYTE* self = dialog;
    int i;

    (void) edx_unused;
    if (!name)
        return 0;
    for (i = 0; i < FIELD(self, DLG_ENTRY_COUNT, int); i++) {
        if (!game_FUN_0053b6e0(name, (const char*) self + DLG_ENTRIES + 0x3C * i))
            return FIELD(self, DLG_ENTRIES + 0x3C * i + 0x34, int);
    }
    if (FIELD(self, DLG_MANAGER, gui_manager*))
        return gui_manager_find_message(FIELD(self, DLG_MANAGER, gui_manager*), NULL, name);
    return 0;
}

/* Every control with id `id`: vtable slot 0xC4 with `value`. */
PORT(0x0046F9B0)
void __fastcall dialog_set_controls(BYTE* self, void* edx_unused, DWORD id, DWORD value)
{
    BYTE walk[0x94];
    BYTE* control;

    (void) edx_unused;
    walk_begin(walk, NULL, FIELD(self, DLG_CONTAINER, void*), 1, UICONTROL_CLASS);
    for (control = walk_next(walk); control; control = walk_next(walk)) {
        if (FIELD(control, 0x78, DWORD) == id)
            VIRTUAL(control, 0xC4, method_1)(control, NULL, value);
    }
    walk_end(walk);
}

/* Enable or disable every control with id `id`; the background is redrawn after disabling. */
PORT(0x0046FA60)
void __fastcall dialog_enable_controls(BYTE* self, void* edx_unused, DWORD id, DWORD enabled)
{
    BYTE walk[0x94];
    BYTE* control;

    (void) edx_unused;
    walk_begin(walk, NULL, FIELD(self, DLG_CONTAINER, void*), 1, UICONTROL_CLASS);
    for (control = walk_next(walk); control; control = walk_next(walk)) {
        if (FIELD(control, 0x78, DWORD) == id)
            game_FUN_00471590(control, NULL, enabled, 1);
    }
    if (!enabled && FIELD(self, DLG_BACKGROUND, BYTE*))
        background_invalidate(FIELD(self, DLG_BACKGROUND, BYTE*));
    walk_end(walk);
}

/*
    The control named `name` (any case), if it is of `type`: any with 0, and type 8 also accepts
    a 7. NULL when there is none, or the first one by that name is of another type.
*/
PORT(0x0046FB20)
BYTE* __fastcall dialog_find_control(BYTE* self, void* edx_unused, const char* name, int type)
{
    BYTE walk[0x94];
    BYTE* control;
    BYTE* found = NULL;

    (void) edx_unused;
    walk_begin(walk, NULL, FIELD(self, DLG_CONTAINER, void*), 1, UICONTROL_CLASS);
    for (control = walk_next(walk); control; control = walk_next(walk)) {
        if (!game_FUN_0053b6e0(game_FUN_00471d20(control), name)) {
            int kind = FIELD(control, 0x5C, int);

            if (!type || kind == type || (type == 8 && kind == 7))
                found = control;
            break;
        }
    }
    walk_end(walk);
    return found;
}

/* Tell every control that wants it (0x1B4) through its slot 0xEC, then the dialog (slot 0x7C). */
PORT(0x0046FBF0)
void __fastcall dialog_notify_controls(BYTE* self, void* edx_unused, DWORD flag)
{
    BYTE walk[0x94];
    BYTE* control;

    (void) edx_unused;
    walk_begin(walk, NULL, FIELD(self, DLG_CONTAINER, void*), 1, UICONTROL_CLASS);
    for (control = walk_next(walk); control; control = walk_next(walk)) {
        if (FIELD(control, 0x1B4, int))
            VIRTUAL(control, 0xEC, method_1)(control, NULL, flag);
    }
    VIRTUAL(self, 0x7C, method_1)(self, NULL, flag);
    walk_end(walk);
}

/* ---- UIDialog: painting ------------------------------------------------------------------- */

/* The clip rectangle and result of the last dialog_begin_paint, in the game's data. */
#define paint_clip    (*(RECT**) 0x006616D0)
#define paint_area    (*(RECT*) 0x00661738)
#define paint_result  (*(int*) 0x00661734)

/*
    A DC to paint `area` with, into `dc`: the background's, for `control`'s slot when given, or the
    screen's own. With a control, its font (or the dialog's) is selected and the DC clipped to the
    area. Returns what the background said, or 1 for the screen.
*/
PORT(0x0046FCA0)
int __fastcall dialog_begin_paint(BYTE* self, void* edx_unused, HDC* dc, RECT* area, void* out,
                                  BYTE* control)
{
    (void) edx_unused;
    paint_clip = area;
    *(DWORD*) out = 0;
    FIELD(self, DLG_OLD_FONT, HGDIOBJ) = NULL;
    if (FIELD(self, DLG_BACKGROUND, BYTE*)) {
        paint_clip = &paint_area;
        paint_area = *area;
        if (control) {
            paint_result = game_FUN_00404a10(FIELD(self, DLG_BACKGROUND, BYTE*), NULL, dc, area,
                                             FIELD(control, 0x1B8, DWORD),
                                             FIELD(control, 0x1C0, DWORD), control + 0x1BC, out,
                                             &paint_area);
        } else {
            DWORD whole = 1;

            paint_result = game_FUN_00404a10(FIELD(self, DLG_BACKGROUND, BYTE*), NULL, dc, area,
                                             (DWORD) -1, 0, &whole, out, &paint_area);
        }
    } else {
        void* surface = FIELD(object_screen(self), 0x48, void*);

        VIRTUAL(surface, 0x44, HRESULT (__stdcall*)(void*, HDC*))(surface, dc);
        paint_result = 1;
    }

    if (*dc && control) {
        HGDIOBJ font = FIELD(control, 0x128, HGDIOBJ);

        FIELD(self, DLG_OLD_FONT, HGDIOBJ) = SelectObject(*dc, font ? font : FIELD(self, DLG_FONT, HGDIOBJ));
        FIELD(self, DLG_CLIP, HRGN) = CreateRectRgn(paint_clip->left, paint_clip->top,
                                                    paint_clip->right, paint_clip->bottom);
        SelectClipRgn(*dc, FIELD(self, DLG_CLIP, HRGN));
        return paint_result;
    }
    FIELD(self, DLG_CLIP, HRGN) = NULL;
    return paint_result;
}

PORT(0x0046FDE0)
void __fastcall dialog_end_paint(BYTE* self, void* edx_unused, HDC dc)
{
    (void) edx_unused;
    if (FIELD(self, DLG_OLD_FONT, HGDIOBJ))
        SelectObject(dc, FIELD(self, DLG_OLD_FONT, HGDIOBJ));
    if (!FIELD(self, DLG_BACKGROUND, BYTE*) && dc) {
        void* surface = FIELD(object_screen(self), 0x48, void*);

        VIRTUAL(surface, 0x68, HRESULT (__stdcall*)(void*, HDC))(surface, dc);
    }
    if (FIELD(self, DLG_CLIP, HRGN))
        DeleteObject(FIELD(self, DLG_CLIP, HRGN));
}

/* ---- UICtlContainer ----------------------------------------------------------------------- */

PORT(0x0046F790)
void __fastcall control_container_destruct(game_object* self)
{
    self->vtable = UICTLCONTAINER_VTABLE;
    object_destruct(self);
}

PORT(0x0046F770)
game_object* __fastcall control_container_delete(game_object* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    control_container_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

/* ---- The unit vectors, set up by the C runtime's start-up --------------------------------- */

static void set_vector(float* v, float x, float y, float z)
{
    v[0] = x;
    v[1] = y;
    v[2] = z;
}

PORT(0x0046F7B0)
void __cdecl vector_zero_init(void)
{
    set_vector((float*) 0x006616B0, 0.0f, 0.0f, 0.0f);
}

PORT(0x0046F840)
void __cdecl vector_x_init(void)
{
    set_vector((float*) 0x006616C0, 1.0f, 0.0f, 0.0f);
}

PORT(0x0046F890)
void __cdecl vector_y_init(void)
{
    set_vector((float*) 0x006616D8, 0.0f, 1.0f, 0.0f);
}

PORT(0x0046F950)
void __cdecl vector_z_init(void)
{
    set_vector((float*) 0x00661698, 0.0f, 0.0f, 1.0f);
}

/* The start-up table points at these jumps to them. */
PORT(0x0046F7A0)
void __cdecl vector_zero_init_thunk(void)
{
    vector_zero_init();
}

PORT(0x0046F830)
void __cdecl vector_x_init_thunk(void)
{
    vector_x_init();
}

PORT(0x0046F880)
void __cdecl vector_y_init_thunk(void)
{
    vector_y_init();
}

PORT(0x0046F940)
void __cdecl vector_z_init_thunk(void)
{
    vector_z_init();
}

/* ---- UIDialog: opening -------------------------------------------------------------------- */

#define GAMEUI_CPP   ((const char*) 0x005717B8)     /* "...\gameui.cpp" */
#define DAT_FORMAT   ((const char*) 0x005717AC)     /* "%s%s.dat" */
#define PATH_FORMAT  ((const char*) 0x0056ED34)     /* "%s%s" */
#define READ_MODE    ((const char*) 0x0056CA38)     /* "r" */

/* The C library, as the game links it. */
int   __cdecl game_FUN_0053a11f(char* to, const char* format, ...);      /* sprintf */
char* __cdecl game__strrchr(const char* text, int c);
char* __cdecl game__strncpy(char* to, const char* from, size_t count);

/* The resource manager's add, and the game's file object (308 bytes). */
void* __fastcall game_FUN_004eddc0(void* resources, void* edx_unused, const char* path, DWORD a);
void* __fastcall game_FUN_00461c80(void* self, void* edx_unused, void* resources);
int   __fastcall game_FUN_00461ec0(void* self, void* edx_unused, const char* path, const char* mode,
                                   DWORD a);
void  __fastcall game_FUN_00461cd0(void* self);

int __fastcall dialog_queue_focus(BYTE* self, void* edx_unused, BYTE* control, BYTE** old, BOOL now);
int __fastcall dialog_give_up(BYTE* self);

static void file_delete(void* file)
{
    if (file) {
        game_FUN_00461cd0(file);
        game_delete(file);
    }
}

/*
    Vtable slot 27: lay the dialog out and load it. Its .dat file comes from `folder` (named by
    `file`, or after the dialog without its extension), or failing that from the resources. With
    a user that has no pointing device, the first control gets focus. NULL when it cannot load.
*/
PORT(0x0046B230)
BYTE* __fastcall dialog_open(BYTE* self, void* edx_unused, void* screen, const RECT* area, DWORD a,
                             DWORD flags, void* font_object, DWORD b, const char* folder,
                             BYTE* parent, BYTE* background, const char* face, int font_size,
                             gui_manager* manager, gui_user* user, const char* file)
{
    char name[0x100];
    char path[0x100];
    char* dialog_name = (char*) self + DLG_NAME;
    game_object* container;
    ui_event event;

    (void) edx_unused;
    object_setup(self, NULL, screen);
    if (area) {
        FIELD(self, 0x160, RECT) = *area;
    } else {
        FIELD(self, 0x164, LONG) = 0;
        FIELD(self, 0x160, LONG) = 0;
        FIELD(self, 0x168, LONG) = FIELD(object_screen(self), 0xC, LONG);
        FIELD(self, 0x16C, LONG) = FIELD(object_screen(self), 0x10, LONG);
    }
    FIELD(self, DLG_PARENT, BYTE*) = parent;
    FIELD(self, DLG_MODAL, DWORD) = (flags & 6) ? 1 : 0;
    FIELD(self, DLG_MANAGER, gui_manager*) = manager;
    if (manager && FIELD(self, DLG_MODAL, DWORD))
        FIELD(self, DLG_SWAP, int) = gui_manager_swap(manager, NULL, (int) self);
    if (parent)
        FIELD(self, DLG_BACKDROP, void*) = FIELD(parent, DLG_BACKDROP, void*);
    FIELD(self, 0xC4, DWORD) = a;
    FIELD(self, 0x114, void*) = font_object;
    FIELD(self, DLG_BACKGROUND, BYTE*) = background;
    FIELD(self, 0x7F20, DWORD) = b;
    FIELD(self, DLG_SCREEN_SIZE, int) = font_size;
    FIELD(self, DLG_USER, gui_user*) = user;
    text_copy((char*) self + DLG_FACE, face);
    FIELD(self, DLG_FONT, HFONT) = CreateFontA(FIELD(self, DLG_SCREEN_SIZE, int), 0, 0, 0, FW_MEDIUM, 0,
                                               0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                                               CLIP_DEFAULT_PRECIS, PROOF_QUALITY, VARIABLE_PITCH,
                                               (char*) self + DLG_FACE);
    FIELD(self, 0x108, DWORD) = 0;
    FIELD(self, 0x10C, DWORD) = 0;

    container = game_new(sizeof(game_object), GAMEUI_CPP, 0x16E);
    if (container) {
        object_construct(container, NULL, 1);
        container->vtable = UICTLCONTAINER_VTABLE;
    }
    FIELD(self, DLG_CONTAINER, void*) = object_add_child(self, NULL, container, -1);

    event.e = 0;
    event.type = 4;
    event.a = 0;
    event.b = 0;
    event.sender = self;
    event.c = 0;
    event.manager = FIELD(self, DLG_MANAGER, gui_manager*);
    VIRTUAL(self, 0x74, dialog_handle)(self, NULL, &event);

    if (dialog_name[0]) {
        char* dot = game__strrchr(dialog_name, '.');
        void* data;

        if (!file) {
            if (dot) {
                size_t length = dot - dialog_name;

                game__strncpy(name, dialog_name, length);
                name[length] = 0;
                game_FUN_0053a11f(path, DAT_FORMAT, folder ? folder : NO_TEXT, name);
            } else {
                game_FUN_0053a11f(path, DAT_FORMAT, folder ? folder : NO_TEXT, dialog_name);
            }
        } else {
            game_FUN_0053a11f(path, PATH_FORMAT, folder ? folder : NO_TEXT, file);
        }
        FIELD(self, DLG_RESOURCE, void*) = game_FUN_004eddc0(game_resources, NULL, path, 0);
        game_FUN_0053a11f(path, PATH_FORMAT, folder ? folder : NO_TEXT, dialog_name);

        data = game_new(0x134, GAMEUI_CPP, 0x194);
        if (data)
            data = game_FUN_00461c80(data, NULL, game_resources);
        if (game_FUN_00461ec0(data, NULL, path, READ_MODE, 0)) {
            int loaded = game_FUN_0046b850(self, NULL, data, 0);

            file_delete(data);
            if (!loaded) {
                dialog_give_up(self);
                return NULL;
            }
        } else {
            file_delete(data);
            if (!dialog_load(self, NULL, dialog_name)) {
                dialog_give_up(self);
                return NULL;
            }
        }
    }

    if (parent && !FIELD(self, DLG_BACKGROUND, BYTE*))
        FIELD(self, DLG_BACKGROUND, BYTE*) = FIELD(parent, DLG_BACKGROUND, BYTE*);
    if (FIELD(self, DLG_BACKGROUND, BYTE*))
        background_invalidate(FIELD(self, DLG_BACKGROUND, BYTE*));
    if (user && !user->input) {
        BYTE walk[0x94];

        walk_begin(walk, NULL, FIELD(self, DLG_CONTAINER, void*), 1, UICONTROL_CLASS);
        dialog_queue_focus(self, NULL, walk_next(walk), NULL, 0);
        walk_end(walk);
    }
    dialog_notify_controls(self, NULL, 0);
    FIELD(self, 0x7F40, DWORD) = 1;
    return self;
}

/* Give up on opening: no closing sound, and vtable slot 0x68. */
PORT(0x00470E90)
int __fastcall dialog_give_up(BYTE* self)
{
    FIELD(self, DLG_SOUND, DWORD) = 0;
    return VIRTUAL(self, 0x68, method_0)(self);
}

/* Queue `control` for focus with the dialog's user. */
PORT(0x00470F30)
int __fastcall dialog_queue_focus(BYTE* self, void* edx_unused, BYTE* control, BYTE** old, BOOL now)
{
    (void) edx_unused;
    return gui_user_queue_focus(FIELD(self, DLG_USER, gui_user*), NULL, control, old, now);
}

/* ---- UIDialog: every frame ---------------------------------------------------------------- */

/* A UITimer under the dialog: counts up to its period, then calls its target or tells the dialog. */
typedef struct ui_timer {
    void**       vtable;
    DWORD        unknown_04;
    DWORD        id;
    DWORD        elapsed;               /* milliseconds */
    DWORD        period;
    BYTE*        target;                /* told through its slot 0xF0, or NULL */
} ui_timer;

#define MILLISECONDS (*(const float*) 0x00555FFC)  /* 1000.0f */

BYTE* __fastcall game_FUN_004736f0(BYTE* control);     /* the control before it */
BYTE* __fastcall game_FUN_00473740(BYTE* control);     /* the control after it */
BYTE* __fastcall game_FUN_004733b0(BYTE* control, void* edx_unused, POINT* at);
void  __fastcall dialog_track_pointer(BYTE* self);

/* Equal to zero as the x87 compare tests it (C3 set): NaN counts too. */
static BOOL x87_zero(float value)
{
    return !(value < 0.0f || value > 0.0f);
}

/*
    Vtable slot 10, the frame's tick: run the timers, follow the focused control with the cursor,
    and move focus between controls with a joystick's left and right.
*/
PORT(0x0046FE30)
int __fastcall dialog_tick(BYTE* self, void* edx_unused, float seconds)
{
    gui_user* user;
    BYTE* focus;
    gui_input* input;
    int i;

    (void) edx_unused;
    if (x87_zero(FIELD(self, 0x9C0, float)))
        FIELD(self, 0x9C0, DWORD) = 0x3A83126F;               /* 0.001: the first frame */
    else
        FIELD(self, 0x9C0, float) = seconds;

    if (FIELD(self, 0x7F40, int)) {
        ui_event event;

        event.manager = FIELD(self, DLG_MANAGER, gui_manager*);
        event.e = 0;
        event.type = 0x12;
        event.b = 0;
        event.sender = self;
        event.c = 0;
        VIRTUAL(self, 0x74, dialog_handle)(self, NULL, &event);
        FIELD(self, 0x7F40, DWORD) = 0;
    }
    if ((FIELD(self, 0x25, BYTE) & 4) || FIELD(self, 0x7F50, int))
        return 1;

    for (i = 0;; i++) {
        container_list* children = &FIELD(self, DLG_CHILDREN, container_list);
        ui_timer* timer;

        if (children->count <= 0 || !children->items || i >= children->count)
            break;
        timer = children->items[i];
        if (!timer)
            break;
        timer->elapsed += (int) ((double) seconds * MILLISECONDS);
        if (timer->elapsed < timer->period)
            continue;
        timer->elapsed = 0;
        if (timer->target) {
            VIRTUAL(timer->target, 0xF0, method_1)(timer->target, NULL, (DWORD) timer);
        } else {
            ui_event event;

            event.a = timer->id;
            event.manager = FIELD(self, DLG_MANAGER, gui_manager*);
            event.e = 0;
            event.type = 7;
            event.b = 0;
            event.sender = self;
            event.c = 0;
            VIRTUAL(self, 0x74, dialog_handle)(self, NULL, &event);
            if (event.e)
                return 1;
        }
    }

    user = FIELD(self, DLG_USER, gui_user*);
    focus = user->focus;
    gui_user_cursor_follow(user, NULL, focus && FIELD(focus, 0x180, BYTE*) ? FIELD(focus, 0x180, BYTE*)
                                                                          : FIELD(self, 0x95C, BYTE*));
    for (i = 0; (input = gui_user_claimed(FIELD(self, DLG_USER, gui_user*), NULL, i)) != NULL; i++) {
        BYTE* next = NULL;
        int x, y;

        if (FIELD(input->device, 0xC, int) != 2 || !focus)
            continue;
        x = input->x;
        y = input->y;
        if (x <= -1) {
            if (!FIELD(self, 0x7F1C, int))
                continue;
            next = game_FUN_004736f0(focus);
        } else if (x >= 1) {
            if (!FIELD(self, 0x7F1C, int))
                continue;
            next = game_FUN_00473740(focus);
        } else {
            if (y <= -1 || y >= 1) {
                if (FIELD(self, 0x7F1C, int))
                    FIELD(self, 0x7F1C, DWORD) = 0;
            } else {
                FIELD(self, 0x7F1C, DWORD) = 1;
            }
            continue;
        }
        FIELD(self, 0x7F1C, DWORD) = 0;
        if (next)
            dialog_queue_focus(self, NULL, next, NULL, 0);
    }
    dialog_track_pointer(self);
    game_vf_10_46A300(self, NULL, FIELD(self, 0x9C0, DWORD));
    return 1;
}

/*
    With the pointing device off every control, focus goes to the dialog itself: the user's focus
    is queued to NULL when it is on one of this dialog's controls and nothing else is waiting.
*/
PORT(0x00470050)
void __fastcall dialog_track_pointer(BYTE* self)
{
    gui_user* user = FIELD(self, DLG_USER, gui_user*);
    BYTE* container;
    BYTE* focus;
    POINT at;

    if (!user->input)
        return;
    container = FIELD(self, DLG_CONTAINER, BYTE*);
    if (!FIELD(container, 0x10, BYTE*))
        return;
    focus = user->focus;
    if (focus && FIELD(focus, 0xB8, BYTE*) != self)
        focus = NULL;
    at.x = user->input->x;
    at.y = user->input->y;
    if (!FIELD(container, 0x10, BYTE*))
        return;
    FIELD(user, 0x1DC, DWORD) = 0;
    if (!game_FUN_004733b0(FIELD(FIELD(self, DLG_CONTAINER, BYTE*), 0x10, BYTE*), NULL, &at) && focus
        && !FIELD(self, DLG_USER, gui_user*)->next_focus)
        gui_user_queue_focus(FIELD(self, DLG_USER, gui_user*), NULL, NULL, NULL, 1);
}

/* ---- UIDialog: drawing -------------------------------------------------------------------- */

/* The frozen dialog's picture (0x7F44), drawn at its place through the background or the screen. */
static void draw_frozen(BYTE* self, void* picture)
{
    if (FIELD(self, DLG_BACKGROUND, BYTE*))
        background_draw(FIELD(self, DLG_BACKGROUND, BYTE*), NULL, picture, &FIELD(self, 0x160, RECT),
                        NULL, 0x1000000, FIELD(self, 0x7F48, int), 1, &FIELD(self, 0x7F4C, int), 0);
    else
        VIRTUAL(object_screen(self), 0xC, screen_draw)(object_screen(self), NULL,
                                                      &FIELD(self, 0x160, RECT), picture, NULL,
                                                      0x1000000);
}

/*
    Vtable slot 13: draw over. The controls' background slots are redrawn first. A frozen dialog
    (bit 2 of byte 0x25) draws its picture instead, when it has one and 0x7F18 is clear.
*/
PORT(0x004700F0)
int __fastcall dialog_draw_over(BYTE* self)
{
    if (!(FIELD(self, 0x25, BYTE) & 4)) {
        BYTE walk[0x94];

        walk_begin(walk, NULL, FIELD(self, DLG_CONTAINER, void*), 1, UICONTROL_CLASS);
        if (FIELD(self, DLG_BACKGROUND, BYTE*)) {
            BYTE* control;

            for (control = walk_next(walk); control; control = walk_next(walk)) {
                int slot = FIELD(control, 0x1B8, int);

                if (slot >= 0 && (!FIELD(control, 0x1C0, int) || FIELD(control, 0x1BC, int)))
                    background_slot_redraw(FIELD(self, DLG_BACKGROUND, BYTE*), NULL, slot,
                                           (RECT*) (control + 0x2C));
            }
        }
        game_vf_13_46A260(self);
        walk_end(walk);
    } else if (FIELD(self, 0x7F44, void*) && !FIELD(self, 0x7F18, int)) {
        draw_frozen(self, FIELD(self, 0x7F44, void*));
    }
    return 1;
}

/* Vtable slot 14: nothing while frozen. */
PORT(0x00470230)
int __fastcall dialog_slot_14(BYTE* self)
{
    if (FIELD(self, 0x25, BYTE) & 4)
        return 1;
    return game_vf_14_46A290(self);
}

/* Vtable slot 15: draw; a frozen dialog draws its picture when 0x7F18 is set. Slot 0x68 follows a
   screen change while 0x7F50 is set. */
PORT(0x00470250)
int __fastcall dialog_draw(BYTE* self)
{
    if (!(FIELD(self, 0x25, BYTE) & 4))
        game_vf_15_46A2D0(self);
    else if (FIELD(self, 0x7F44, void*) && FIELD(self, 0x7F18, int))
        draw_frozen(self, FIELD(self, 0x7F44, void*));
    if (FIELD(self, 0x7F50, int) && FIELD(self, 0x7F54, DWORD) != FIELD(object_screen(self), 0x1C, DWORD))
        VIRTUAL(self, 0x68, method_0)(self);
    return 1;
}

/* ---- Text --------------------------------------------------------------------------------- */

/*
    A run of a dialog's text, as the markup parser makes them: plain text, a colour change (~N) or
    a tab stop (|X). The array ends with a run whose text is NULL.
*/
typedef struct text_run {
    const char* text;
    int         kind;               /* 1: switch to `colour`, 2: move to `tab` from the start */
    int         length;
    COLORREF    colour;
    int         width;
    int         tab;
} text_run;

_Static_assert(sizeof(text_run) == 24, "a text run is 24 bytes");

/* Where the text being drawn has got to, in the game's data. */
#define text_run_index  (*(int*) 0x0066174C)
#define text_x          (*(int*) 0x006616AC)
#define text_colour     (*(COLORREF*) 0x006616BC)
#define text_size       (*(SIZE*) 0x00661728)
#define text_left       (*(int*) 0x006616F0)
#define text_top        (*(int*) 0x006616CC)

/* `text` in `colour`, over a shadow one pixel up and left when `shadow` is set. */
PORT(0x004702F0)
void __fastcall text_out(void* self, void* edx_unused, HDC dc, int x, int y, const char* text,
                         int length, COLORREF colour, BOOL shadow, COLORREF shadow_colour)
{
    (void) self;
    (void) edx_unused;
    if (shadow) {
        SetTextColor(dc, shadow_colour);
        TextOutA(dc, x - 1, y - 1, text, length);
    }
    SetTextColor(dc, colour);
    TextOutA(dc, x, y, text, length);
}

/*
    Draw text split into runs from (x, y): colour runs change the colour, tab runs move to their
    stop. A shadow goes one pixel down and right of each run when `shadow` is set.
*/
PORT(0x00470350)
void __fastcall text_out_runs(void* self, void* edx_unused, HDC dc, int x, int y, const char* text,
                              int length, const text_run* runs, COLORREF colour, BOOL shadow,
                              COLORREF shadow_colour)
{
    (void) self;
    (void) edx_unused;
    (void) text;
    (void) length;
    text_run_index = 0;
    text_x = x;
    SetTextColor(dc, colour);
    text_colour = colour;
    while (runs[text_run_index].text) {
        const text_run* run = &runs[text_run_index];

        if (run->kind == 2)
            text_x = x + run->tab;
        if (shadow) {
            SetTextColor(dc, shadow_colour);
            run = &runs[text_run_index];
            TextOutA(dc, text_x + 1, y + 1, run->text, run->length);
            SetTextColor(dc, text_colour);
        }
        run = &runs[text_run_index];
        if (run->kind == 1) {
            text_colour = run->colour;
            SetTextColor(dc, text_colour);
        }
        run = &runs[text_run_index];
        TextOutA(dc, text_x, y, run->text, run->length);
        text_x += runs[text_run_index].width;
        text_run_index++;
    }
}

/*
    Draw `text` in `area`, placed by `align`: 2 centred across, 4 right; 0x10 centred down, 0x20 at
    the bottom (`height` high, or the dialog's font size). Align 9 keeps the last measurement.
    `control` measures it, when given; `runs` draws it as runs.
*/
PORT(0x00470480)
void __fastcall dialog_draw_text(BYTE* self, void* edx_unused, HDC dc, const RECT* area,
                                 const char* text, int length, COLORREF colour, int height,
                                 const text_run* runs, BOOL shadow, COLORREF shadow_colour,
                                 DWORD align, BOOL transparent, BYTE* control)
{
    (void) edx_unused;
    SetBkMode(dc, transparent ? TRANSPARENT : OPAQUE);
    if (align != 9) {
        if (control) {
            text_size.cx = VIRTUAL(control, 0x94, SIZE* (__fastcall*)(void*))(control)->cy;
            text_size.cy = FIELD(control, 0x160, LONG);
        } else {
            GetTextExtentPoint32A(dc, text, length, &text_size);
        }
    }
    if (align & 2)
        text_left = area->left + (area->right - area->left) / 2 - text_size.cx / 2;
    else if (align & 4)
        text_left = area->right - text_size.cx;
    else
        text_left = area->left;
    if (!height)
        height = FIELD(self, DLG_SCREEN_SIZE, int);
    if (align & 0x10)
        text_top = area->top + (area->bottom - area->top) / 2 - height / 2;
    else if (align & 0x20)
        text_top = area->bottom - height;
    else
        text_top = area->top;

    if (runs)
        text_out_runs(self, NULL, dc, text_left, text_top, text, length, runs, colour, shadow,
                      shadow_colour);
    else
        text_out(self, NULL, dc, text_left, text_top, text, length, colour, shadow, shadow_colour);
}

/* What a control's text slots give: text, length, runs, and the measured size (0x88 to 0x94). */
#define control_text(c)    VIRTUAL(c, 0x88, const char* (__fastcall*)(void*))(c)
#define control_length(c)  VIRTUAL(c, 0x8C, int (__fastcall*)(void*))(c)
#define control_runs(c)    VIRTUAL(c, 0x90, const text_run* (__fastcall*)(void*))(c)
#define control_extent(c)  VIRTUAL(c, 0x94, SIZE* (__fastcall*)(void*))(c)

#define control_text_left  (*(int*) 0x00661730)
#define control_text_top   (*(int*) 0x00661708)
#define control_text_size  (*(SIZE*) 0x006616E8)

/* Draw `control`'s text inside its padded rectangle, placed by its alignment (0xD8). */
PORT(0x004705F0)
void __fastcall dialog_draw_control_text(BYTE* self, void* edx_unused, HDC dc, BYTE* control,
                                         BOOL transparent)
{
    RECT box = *(RECT*) (control + 0x2C);
    int pad_x = FIELD(control, 0xE8, int);
    int pad_y = FIELD(control, 0xEC, int);
    int left = box.left + pad_x;
    int top = box.top + pad_y;
    int right = box.right - pad_x;
    int bottom = box.bottom - pad_y;
    DWORD align;
    int width;

    (void) edx_unused;
    SetBkMode(dc, transparent ? TRANSPARENT : OPAQUE);
    if (FIELD(control, 0xD8, DWORD) != 9) {
        width = control_extent(control)->cy;
        control_text_size.cx = width;
        control_text_size.cy = FIELD(control, 0x160, LONG);
    } else {
        width = control_text_size.cx;
    }
    align = FIELD(control, 0xD8, DWORD);
    if (align & 2)
        control_text_left = left + (right - left) / 2 - width / 2;
    else if (align & 4)
        control_text_left = right - width;
    else
        control_text_left = left;
    align = FIELD(control, 0xD8, DWORD);
    if (align & 0x10)
        control_text_top = top + (bottom - top) / 2 - FIELD(self, DLG_SCREEN_SIZE, int) / 2;
    else if (align & 0x20)
        control_text_top = bottom - FIELD(self, DLG_SCREEN_SIZE, int);
    else
        control_text_top = top;

    if (control_runs(control)) {
        COLORREF shadow_colour = FIELD(control, 0xC8, COLORREF);
        COLORREF colour = FIELD(control, 0xC4, COLORREF);
        const text_run* runs = control_runs(control);
        int length = control_length(control);

        text_out_runs(self, NULL, dc, control_text_left, control_text_top, control_text(control),
                      length, runs, colour, colour, shadow_colour);
    } else {
        COLORREF shadow_colour = FIELD(control, 0xC8, COLORREF);
        BOOL shadow = FIELD(control, 0xD4, BOOL);
        COLORREF colour = FIELD(control, 0xC4, COLORREF);
        int length = control_length(control);

        text_out(self, NULL, dc, control_text_left, control_text_top, control_text(control),
                 length, colour, shadow, shadow_colour);
    }
}

/* ---- Text markup -------------------------------------------------------------------------- */

char* __cdecl game_FUN_0053b580(const char* text, int c);               /* strchr */

/* Measure `text` in `font` (or the dialog's) on the screen's DC. FALSE when there is no DC. */
static BOOL measure_text(BYTE* self, HFONT font, const char* text, int length, SIZE* size)
{
    void* surface = FIELD(object_screen(self), 0x48, void*);
    HDC dc;
    HGDIOBJ old;

    if (VIRTUAL(surface, 0x44, HRESULT (__stdcall*)(void*, HDC*))(surface, &dc))
        return FALSE;
    old = SelectObject(dc, font ? font : FIELD(self, DLG_FONT, HFONT));
    GetTextExtentPoint32A(dc, text, length, size);
    SelectObject(dc, old);
    surface = FIELD(object_screen(self), 0x48, void*);
    VIRTUAL(surface, 0x68, HRESULT (__stdcall*)(void*, HDC))(surface, dc);
    return TRUE;
}

/*
    Split `text` into runs at its markup: "~N" switches to the dialog's colour N (0-9), "|X" moves
    to tab stop X (0-9, then a-z or A-Z for 10 on) of `tabs`. Each run is measured in `font`. With
    `extent`, [0] gets the length without the markup and [1] the total width. Returns the runs on
    the heap, ended by an empty run, or NULL when the text has no markup (or no tabs for its "|"),
    or while the game's flag 0x538 is set.

    The original is split in two at 0x00470822, which only this function jumps to.
*/
PORT(0x004707C0)
text_run* __fastcall dialog_parse_runs(BYTE* self, void* edx_unused, const char* text,
                                       COLORREF colour, HFONT font, const int* tabs, int* extent)
{
    text_run runs[100];
    const char* pending = NULL;           /* where the markup before the current run began */
    const char* start;
    SIZE size = { 0, 0 };                 /* the original leaves it unset until measured */
    int length, i = 0, count = 0, at = 2;
    text_run* copy;

    (void) edx_unused;
    if (FIELD(game_main, 0x538, int))
        return NULL;
    if (!game_FUN_0053b580(text, '~') && (!game_FUN_0053b580(text, '|') || !tabs))
        return NULL;

    length = (int) text_length(text);
    if (text[0] != '~' && text[0] != '|') {
        runs[0].text = text;
        runs[0].colour = colour;
        runs[0].kind = 1;
        count = 1;
    }
    start = text;
    if (text[0]) {
        do {
            char mark = text[i];
            text_run* run;

            if (mark != '~' && mark != '|') {
                at++;
                i++;
                continue;
            }
            if (!pending)
                pending = &text[i];
            if (at >= length)
                break;
            if (text[i + 2] == '~' || text[i + 2] == '|') {
                i += 2;
                at += 2;
                continue;
            }

            run = &runs[count];
            run->text = &text[i + 2];
            if (mark == '~') {
                int n = text[i + 1] - '0';

                run->kind = 1;
                if (n > 9)
                    n = 9;
                else if (n < 0)
                    n = 0;
                run->colour = FIELD(self, DLG_COLOURS + 4 * n, COLORREF);
            } else {
                run->kind = 2;
                if (tabs) {
                    char stop = text[i + 1];
                    int n;

                    if (stop >= '0' && stop <= '9')
                        n = stop - '0';
                    else if (stop >= 'a' && stop <= 'z')
                        n = stop - 'a' + 10;
                    else if (stop >= 'A' && stop <= 'Z')
                        n = stop - 'A' + 10;
                    else
                        n = 0;
                    run->tab = tabs[n];
                } else {
                    run->tab = 0;
                }
            }
            at += 2;
            i += 2;
            if (count) {
                text_run* previous = &runs[count - 1];

                previous->length = pending ? (int) (pending - start) : (int) (run->text - start) - 2;
                measure_text(self, font, previous->text, previous->length, &size);
                previous->width = size.cx;
            }
            start = run->text;
            count++;
            pending = NULL;
        } while (text[i]);
    }

    if (count) {
        text_run* last = &runs[count - 1];

        last->length = pending ? (int) (pending - last->text) : (int) (text + length - last->text);
        if (measure_text(self, font, last->text, last->length, &size))
            last->width = size.cx;
        else
            last->width = 0;
    }
    if (extent) {
        int n;

        extent[1] = 0;
        extent[0] = length - 2 * count;
        for (n = 0; n < count; n++)
            extent[1] += runs[n].width;
    }
    runs[count].text = NULL;
    runs[count].length = 0;
    runs[count].kind = 0;

    copy = game_FUN_004a4410((count + 1) * sizeof(text_run), GAMEUI_CPP, 0xA5B);
    if (copy)
        __movsb((BYTE*) copy, (BYTE*) runs, (count + 1) * sizeof(text_run));
    return copy;
}

/* ---- UIDialog: timers, children, freezing ------------------------------------------------- */

#define UITIMER_VTABLE ((void**) 0x00557C48)

/* BaseObject's constructor and destructor: UITimer is not a GameObject. */
void __fastcall game_FUN_00405f40(void* self);
void __fastcall game_FUN_00405f70(void* self);
int  __fastcall game_vf_16_46A3B0(void* self, void* edx_unused, DWORD frozen);

/*
    Whether `at` is on a double-byte character of `text` (its lead or trail byte), looking at
    no more than `count` bytes; FALSE once a single-byte character past it is reached.
*/
PORT(0x00470BB0)
BOOL __fastcall text_on_double_byte(void* self, void* edx_unused, const char* text, const char* at,
                                    int count)
{
    const char* p = text;
    int i;

    (void) self;
    (void) edx_unused;
    for (i = 0; i < count; i++, p++) {
        if (IsDBCSLeadByte((BYTE) *p)) {
            if (p == at || text + i + 1 == at)
                return TRUE;
            i++;
            p++;
        } else if (p > at) {
            return FALSE;
        }
    }
    return FALSE;
}

/* Add a timer that fires every `period` milliseconds: at `target` (slot 0xF0), or as event 7. */
PORT(0x00470C10)
void __fastcall dialog_add_timer(BYTE* self, void* edx_unused, DWORD id, DWORD period, BYTE* target)
{
    ui_timer* timer;

    (void) edx_unused;
    if (!period)
        return;
    timer = game_new(sizeof(ui_timer), GAMEUI_CPP, 0xA76);
    if (timer) {
        game_FUN_00405f40(timer);
        timer->vtable = UITIMER_VTABLE;
        timer->id = id;
        timer->period = period;
        timer->elapsed = 0;
        timer->target = target;
    }
    list_add(&FIELD(self, DLG_CHILDREN, container_list), timer);
}

PORT(0x00470D60)
void __fastcall ui_timer_destruct(ui_timer* self)
{
    self->vtable = UITIMER_VTABLE;
    game_FUN_00405f70(self);
}

PORT(0x00470D40)
ui_timer* __fastcall ui_timer_delete(ui_timer* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    ui_timer_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

/*
    Take `item` out of the children by moving the last one into its place. The original does not
    destroy it, and the walk then goes on past the moved one.
*/
static void children_remove(container_list* children, void* item)
{
    int j;

    for (j = 0; j < children->count; j++) {
        if (children->items[j] == item) {
            children->items[j] = children->items[children->count - 1];
            children->count--;
            return;
        }
    }
}

/* Take every timer with `id` out of the dialog's children. */
PORT(0x00470D70)
void __fastcall dialog_remove_timer(BYTE* self, void* edx_unused, DWORD id)
{
    container_list* children = &FIELD(self, DLG_CHILDREN, container_list);
    int i;

    (void) edx_unused;
    for (i = 0; children->count > 0 && children->items && i < children->count; i++) {
        ui_timer* timer = children->items[i];

        if (!timer)
            break;
        if (timer->id == id && children->count > 0)
            children_remove(children, timer);
    }
}

/* Take `object` out of the dialog's children. */
PORT(0x00470DF0)
void __fastcall dialog_remove_child(BYTE* self, void* edx_unused, void* object)
{
    container_list* children = &FIELD(self, DLG_CHILDREN, container_list);
    int i;

    (void) edx_unused;
    for (i = 0; children->count > 0 && children->items && i < children->count; i++) {
        void* item = children->items[i];

        if (!item)
            break;
        if (item == object && children->count > 0)
            children_remove(children, object);
    }
}

/* Close at the next screen change, with `sound` (see dialog_draw). Only the first call counts. */
PORT(0x00470E60)
void __fastcall dialog_end(BYTE* self, void* edx_unused, DWORD sound)
{
    (void) edx_unused;
    if (FIELD(self, 0x7F50, int))
        return;
    FIELD(self, DLG_SOUND_ID, DWORD) = sound;
    FIELD(self, 0x7F50, DWORD) = 1;
    FIELD(self, 0x7F54, DWORD) = FIELD(object_screen(self), 0x1C, DWORD);
}

/* Tell the parent dialog's handler: event `kind` from this one, with `sound`. */
PORT(0x00470EA0)
void __fastcall dialog_tell_parent(BYTE* self, void* edx_unused, DWORD sound, DWORD kind)
{
    BYTE* parent = FIELD(self, DLG_PARENT, BYTE*);
    ui_event event;

    (void) edx_unused;
    if (!parent)
        return;
    event.type = kind;
    event.a = sound;
    event.e = 0;
    event.b = 0;
    event.sender = parent;
    event.c = (DWORD) self;
    event.manager = FIELD(parent, DLG_MANAGER, gui_manager*);
    VIRTUAL(parent, 0x74, dialog_handle)(parent, NULL, &event);
}

PORT(0x00470EF0)
void __fastcall dialog_set_170(BYTE* self, void* edx_unused, DWORD value)
{
    (void) edx_unused;
    FIELD(self, 0x170, DWORD) = value;
}

/* The background, for this dialog and every parent up the chain. */
PORT(0x00470F00)
void __fastcall dialog_set_background(BYTE* self, void* edx_unused, BYTE* background)
{
    (void) edx_unused;
    if (FIELD(self, DLG_PARENT, BYTE*))
        dialog_set_background(FIELD(self, DLG_PARENT, BYTE*), NULL, background);
    FIELD(self, DLG_BACKGROUND, BYTE*) = background;
}

/* Vtable slots 29 to 31: the dialog's callbacks, when it has them. */
typedef void (__cdecl* callback_arg)(DWORD arg);
typedef int  (__fastcall* callback_self)(BYTE* self);
typedef void (__cdecl* callback_self_arg)(BYTE* self, DWORD arg);

PORT(0x00470F50)
void __fastcall dialog_slot_29(BYTE* self, void* edx_unused, DWORD arg)
{
    callback_arg callback = FIELD(self, 0xCC, callback_arg);

    (void) edx_unused;
    if (callback)
        callback(arg);
}

PORT(0x00470F70)
int __fastcall dialog_slot_30(BYTE* self)
{
    callback_self callback = FIELD(self, 0xD4, callback_self);

    return callback ? callback(self) : 0;
}

PORT(0x00470F80)
void __fastcall dialog_slot_31(BYTE* self, void* edx_unused, DWORD arg)
{
    callback_self_arg callback = FIELD(self, 0xD0, callback_self_arg);

    (void) edx_unused;
    if (callback)
        callback(self, arg);
}

/* Drop the frozen picture and its background slot. */
PORT(0x00471040)
void __fastcall dialog_thaw_picture(BYTE* self)
{
    BYTE* background;

    if (FIELD(self, 0x7F44, void*))
        object_destroy(FIELD(self, 0x7F44, void*));
    background = FIELD(self, DLG_BACKGROUND, BYTE*);
    if (background && FIELD(self, 0x7F48, int) >= 0) {
        background_free_slot(background, NULL, FIELD(self, 0x7F48, int));
        FIELD(FIELD(self, DLG_BACKGROUND, BYTE*), 0x30, int) = 1;
    }
    FIELD(self, 0x7F44, void*) = NULL;
    FIELD(self, 0x7F48, int) = -1;
    FIELD(self, 0x7F4C, int) = 0;
}

/* Take a picture of the screen behind the dialog (`area`, or its own), darkened with `darken`. */
PORT(0x00470FE0)
void __fastcall dialog_freeze_picture(BYTE* self, void* edx_unused, BOOL darken, const RECT* area)
{
    BYTE* background;

    (void) edx_unused;
    dialog_thaw_picture(self);
    FIELD(self, 0x7F44, void*) = gui_manager_capture_screen(FIELD(self, DLG_MANAGER, gui_manager*),
                                                            NULL, darken,
                                                            area ? area : &FIELD(self, 0x160, RECT));
    background = FIELD(self, DLG_BACKGROUND, BYTE*);
    if (background) {
        FIELD(self, 0x7F48, int) = background_new_slot(background, NULL, 1);
        background_invalidate(FIELD(self, DLG_BACKGROUND, BYTE*));
        FIELD(FIELD(self, DLG_BACKGROUND, BYTE*), 0x30, int) = 0;
    }
}

/* Freeze the dialog behind a picture of itself, or thaw it; GameObject's slot 16 follows. */
PORT(0x00470FA0)
void __fastcall dialog_freeze(void* dialog, void* edx_unused, DWORD frozen, DWORD darken, RECT* area)
{
    BYTE* self = dialog;

    (void) edx_unused;
    if (frozen)
        dialog_freeze_picture(self, NULL, darken, area);
    else
        dialog_thaw_picture(self);
    game_vf_16_46A3B0(self, NULL, frozen);
}
