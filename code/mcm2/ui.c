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
            game_FUN_00470fa0(FIELD(self, DLG_PARENT, BYTE*), NULL, 0, 0, NULL);
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
