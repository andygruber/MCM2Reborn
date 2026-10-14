/*
    OpenMCM2 - the menus' GUI classes (GUIManager.cpp in the original): their layouts, and the
    game's functions around them that ported code calls.
*/

#pragma once

#include "game.h"
#include <unknwn.h>

typedef struct gui_manager gui_manager;
typedef struct gui_user gui_user;
typedef struct tooltip tooltip;

/* UIFrame, 36 bytes (a BaseObject). */
typedef struct ui_frame {
    void** vtable;
    DWORD  unknown_04;
    DWORD  is_sound;        /* 1: a .wav */
    int    width;
    int    height;
    void*  sound;
    BYTE*  picture;
    DWORD  owns_sound;
    DWORD  texture;         /* passed to the picture's constructor */
} ui_frame;

_Static_assert(sizeof(ui_frame) == 36, "UIFrame is 36 bytes");

/* UIAnim, 248 bytes (a BaseObject). */
typedef struct ui_anim {
    void**    vtable;
    DWORD     unknown_04;
    int       frame;            /* the one showing */
    DWORD     restart;          /* the next step starts it over */
    int       count;
    int       repeats;          /* FOREVER, or how many times it plays */
    int       repeats_left;
    int       delay;            /* milliseconds a frame shows */
    DWORD     last_step;
    DWORD     backwards;
    ui_frame* frames[50];
    DWORD     texture;
    DWORD     unknown_f4;
} ui_anim;

_Static_assert(sizeof(ui_anim) == 248, "UIAnim is 248 bytes");

/* GUICursor, 96 bytes: a GameCursor showing an animation. */
typedef struct gui_cursor {
    BYTE  game_cursor[0x38];
    BYTE* picture;                      /* GameCursor's: the picture it draws */
    BYTE  unknown_3c[0x20];
    ui_anim* anim;                      /* the animation it shows, or NULL */
} gui_cursor;

LAYOUT(gui_cursor, picture, 0x38);
LAYOUT(gui_cursor, anim, 0x5C);
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
    float       font_grow;              /* added to every dialog's font height */
    int         font_bold;              /* used with font_name */
    int         font_italic;
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
    DWORD          keys[32];            /* which keys and buttons are down */
    gui_manager*   manager;
    game_object*   tooltip;
    int            unknown_c4;
    char           cursor_image[0x80];
    char           wait_image[0x80];
    ui_anim*       own_anim;            /* the cursor shows it when given no other */
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

/* Step an animation, sounding its sound frames; the picture reached (ui.c). */
BYTE* __fastcall anim_next_picture(ui_anim* self);

/*
    BackgroundImage: the surface the menus are drawn on, with slots for the parts that change
    (backgroundimage.c).
*/
BYTE* __fastcall background_construct(BYTE* self, void* edx_unused, DWORD flag);
void  __fastcall background_set_picture(BYTE* self, void* edx_unused, void* picture);
int   __fastcall background_new_slot(BYTE* self, void* edx_unused, DWORD keep);
void  __fastcall background_free_slot(BYTE* self, void* edx_unused, int slot);
void  __fastcall background_slot_redraw(BYTE* self, void* edx_unused, int slot, RECT* area);
int   __fastcall background_draw(BYTE* self, void* edx_unused, void* picture, RECT* area,
                                 RECT* source, DWORD flags, int slot, DWORD lasting,
                                 int* passes, DWORD blend);
int   __fastcall background_draw_at(BYTE* self, void* edx_unused, void* picture, int x, int y,
                                    RECT* source, DWORD flags, int slot, DWORD lasting,
                                    int* passes, DWORD blend);
int   __fastcall background_get_dc(BYTE* self, void* edx_unused, HDC* dc, RECT* area, int slot,
                                   DWORD lasting, int* passes, int* again, RECT* clip);
int   __fastcall background_release_dc(BYTE* self);
void  __fastcall background_slot_clear(BYTE* self, void* edx_unused, int slot);
void  __fastcall background_invalidate(BYTE* self);
#ifdef OPENMCM2_DEV
void  __fastcall background_resize(BYTE* self);         /* the widescreen menus' mode switch */
#endif

/* The screen's own draw: vtable slot 3 of object_screen(). */
typedef int (__fastcall* screen_draw)(void* screen, void* edx_unused, RECT* area, void* picture,
                                      void* source, DWORD flags);

/* A picture (128 bytes), and its vtable slot 4 that fills it from pixels. */
void* __fastcall game_FUN_004ca650(void* self, void* edx_unused, int a, DWORD flag);
typedef int (__fastcall* picture_fill)(void* picture, void* edx_unused, BYTE* pixels, int width,
                                       int height, int a, int b, int format, int mode, DWORD c,
                                       DWORD d, DWORD e, DWORD f, DWORD g, DWORD h, DWORD i,
                                       DWORD j, DWORD k, DWORD key);

#define video_mode() FIELD(FIELD(game_main, 0x10, BYTE*), 0x28, int)

/* The bytes per pixel of a video mode. */
int   __cdecl game_FUN_00517050(int mode);

/* DirectDraw surface calls, by vtable offset: GetDC, ReleaseDC, Lock and Unlock. */
typedef HRESULT (__stdcall* surface_get_dc)(void* surface, HDC* dc);
typedef HRESULT (__stdcall* surface_release_dc)(void* surface, HDC dc);
typedef HRESULT (__stdcall* surface_lock)(void* surface, RECT* area, void* desc, DWORD flags,
                                          HANDLE event);
typedef HRESULT (__stdcall* surface_unlock)(void* surface, RECT* area);

/* What DDSURFACEDESC2 fields the ports read, by offset; the whole is 0x7C bytes. */
#define DESC_SIZE    0x7C
#define DESC_HEIGHT  0x08
#define DESC_WIDTH   0x0C
#define DESC_PITCH   0x10
#define DESC_PIXELS  0x24

/* A dialog (ui.c): shown or hidden, and its controls. */
int  __fastcall dialog_show(void* dialog, void* edx_unused, DWORD shown);
int  __fastcall dialog_find(void* dialog, void* edx_unused, const char* name);
void __fastcall dialog_freeze(void* dialog, void* edx_unused, DWORD frozen, DWORD darken,
                              RECT* area);

/* The C library's strcmp, as the game links it. */
int __cdecl game_FUN_0053b910(const char* a, const char* b);

/* The GUIManager.cpp ports (gui.c) that other files call. */
int   __fastcall gui_manager_swap(gui_manager* self, void* edx_unused, int value);
int   __fastcall gui_manager_unknown_1f8(gui_manager* self);
void  __fastcall gui_manager_set_busy(gui_manager* self, void* edx_unused, int value);
int   __fastcall gui_manager_find_message(gui_manager* self, void* edx_unused, const char* name);
void* __fastcall gui_manager_capture_screen(gui_manager* self, void* edx_unused, BOOL darken,
                                            const RECT* area);
void  __fastcall gui_user_set_ime(gui_user* self, void* edx_unused, BOOL on);
void  __fastcall gui_input_reset(gui_input* self);
void  __fastcall game_vf_28_483430(gui_manager* self);

void __fastcall gui_manager_drop_backdrop(gui_manager* self);
void __fastcall gui_manager_load_backdrop(gui_manager* self, void* edx_unused, DWORD flag);
gui_user* __fastcall gui_manager_user(gui_manager* self, void* edx_unused, int index);
void __fastcall gui_user_drop_tooltip(gui_user* self);
void __fastcall gui_user_make_tooltip(gui_user* self, void* edx_unused, void* screen,
                                      gui_manager* manager);
BYTE* __fastcall gui_manager_active_dialog(gui_manager* self);
void __fastcall gui_manager_refresh(gui_manager* self);
void __fastcall gui_user_make_cursor(gui_user* self, void* edx_unused, const char* image,
                                     DWORD arg);
void __fastcall gui_user_set_cursor(gui_user* self, void* edx_unused, const char* image,
                                    BOOL refresh);
void __fastcall gui_user_drop_cursor(gui_user* self);
void __fastcall gui_user_set_cursor_anim(gui_user* self, void* edx_unused, ui_anim* anim);
int  __fastcall gui_user_release_capture(gui_user* self, void* edx_unused, BYTE** old);
void __fastcall tooltip_set_text(tooltip* self, void* edx_unused, const char* text,
                                 const POINT* at, float delay);
gui_input* __fastcall gui_user_claimed(gui_user* self, void* edx_unused, int index);
int  __fastcall gui_user_queue_focus(gui_user* self, void* edx_unused, BYTE* dialog, BYTE** old,
                                     BOOL now);
int  __fastcall gui_user_queue_capture(gui_user* self, void* edx_unused, BYTE* dialog, BYTE** old,
                                       BOOL now);
void __fastcall gui_manager_drop_cursors(gui_manager* self);
void __fastcall gui_manager_set_cursors(gui_manager* self, void* edx_unused, const char* image,
                                        DWORD arg);

/* What a dialog's vtable slot 0x74 is told: the dialog closing (6), slot 28 (5), a control's events. */
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

/* What an event is (its `type`). */
enum {
    EVENT_CLICK = 1,
    EVENT_SELECT = 2,           /* a list's selection moved; c is the list */
    EVENT_SLIDE = 3,            /* a slider moved */
    EVENT_OPEN = 5,
    EVENT_CLOSE = 6,
    EVENT_TIMER = 7,
    EVENT_CHILD_ENDED = 9,      /* a dialog this one opened ended, its code in a */
    EVENT_EDIT_DONE = 10,
    EVENT_KEY = 11,
    EVENT_KEY_UP = 12,
    EVENT_SLIDE_END = 16,       /* a slider let go */
    EVENT_SLIDE_HOLD = 17,      /* the pointer rested on a slider */
    EVENT_EDIT_CHANGED = 19,
};
