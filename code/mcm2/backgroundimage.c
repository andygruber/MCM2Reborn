/*
    OpenMCM2 - the menus' background, ported from the game (BackgroundImage.cpp in the original).

    Each function here replaces the game's at the address in its PORT line, and was checked against
    the original's instructions. The original's C++ exception frame in the destructor is not
    reproduced, and its file-scope RECTs are locals here: each was only used within one call. The
    file's static initializer (0x00403D30, a jump to 0x00403D40) fills a header's constant vector at
    0x0057D828 that nothing here reads, and is left to the game.

    A BackgroundImage (0x74 bytes, a GameObject) is what the menus are drawn on. It keeps the
    background in a surface of its own, the copy, and lends slots to the things drawn over it that
    move or change. A slot remembers the area it covered in each of the screen's buffers, so when
    that buffer comes round again the background can be put back there from the copy. Drawing
    "lasting" puts something on the background itself: into the copy as well as the screen, until
    every buffer has it. Dialogs keep their manager's background at 0x110.
*/

#include "gui.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

#define BACKGROUNDIMAGE_CPP ((const char*) 0x0056C978)  /* "...\BackgroundImage.cpp" */
#define BACKGROUND_VTABLE   ((void**) 0x005556D8)

/* Its own fields, after GameObject's. */
#define BG_PICTURE         0x2C     /* the picture behind everything, or NULL */
#define BG_SHOWN           0x30     /* the picture is drawn; only ever set */
#define BG_PICTURE_SLOT    0x34
#define BG_PICTURE_PASSES  0x38
#define BG_COPY            0x3C     /* IDirectDrawSurface7*: the background, off screen */
#define BG_COPY_LIVE       0x40     /* frames are restored from the copy, so lasting drawing goes into it */
#define BG_SLOT_COUNT      0x48
#define BG_SLOTS_USED      0x4C
#define BG_SLOTS           0x50     /* bg_slot[BG_SLOT_COUNT] */
#define BG_FULL_REDRAWS    0x58     /* frames left that draw the whole background afresh */
#define BG_PICTURE_DRAWN   0x5C     /* drawn once, so later draws of it are lasting */
#define BG_DC_TO_COPY      0x60     /* background_get_dc, passes left: the next DC is the copy's */
#define BG_DC_SLOT         0x64     /* background_get_dc, by slots: the last slot, or -1 */
#define BG_DC_TO_SCREEN    0x68     /* background_get_dc, by slots: the next DC is the screen's */
#define BG_DC_SURFACE      0x6C     /* the surface whose DC is out, or NULL */
#define BG_DC              0x70

/* The screen's fields used here (object_screen). */
#define SCREEN_VIDEO       0x04     /* its 0x190 is the IDirectDraw7 */
#define SCREEN_VIEW        0x08     /* an object drawn under the menus, or NULL */
#define SCREEN_WIDTH       0x0C
#define SCREEN_HEIGHT      0x10
#define SCREEN_BUFFERS     0x14
#define SCREEN_BUFFER      0x18     /* the one being drawn, 0 to SCREEN_BUFFERS - 1 */
#define SCREEN_FRAME       0x1C
#define SCREEN_DEPTH       0x2C     /* float: what the depth buffer is cleared to */
#define SCREEN_SURFACE     0x48     /* IDirectDrawSurface7*: the buffer being drawn */
#define SCREEN_DEVICE      0x50     /* IDirect3DDevice7* */

/* The view's: shown is GameObject's bit at 0x25. */
#define VIEW_WIDTH         0x1A8
#define VIEW_HEIGHT        0x1AC
#define VIEW_EDGE_REDRAWS  0x1CC    /* set as well when it is shorter than the screen */
#define VIEW_REDRAWS       0x1D0

#define DESC_FLAGS         0x04
#define DESC_CAPS          0x68

#define DDSD_CAPS             0x1
#define DDSD_HEIGHT           0x2
#define DDSD_WIDTH            0x4
#define DDSD_PIXELFORMAT      0x1000
#define DDSCAPS_SYSTEMMEMORY  0x800
#define DDSCAPS_OWNDC         0x2000
#define DDBLTFAST_WAIT        0x10
#define DDBLT_WAIT            0x1000000
#define D3DCLEAR_ZBUFFER      0x2

/* Something drawn over the background, 64 bytes. */
typedef struct bg_slot {
    int   life;         /* 0 when free; see background_new_slot */
    RECT  area[3];      /* where it was drawn, by the screen's buffer */
    int   drawn_in;     /* the buffer it was drawn into as part of the background, or -1 */
    DWORD frame;        /* the screen's frame when it was last placed */
    DWORD keep;         /* its area is never put back */
} bg_slot;

LAYOUT(bg_slot, drawn_in, 0x34);
LAYOUT(bg_slot, keep, 0x3C);
_Static_assert(sizeof(bg_slot) == 64, "a slot is 64 bytes");

/* COM calls, by vtable offset. */
typedef ULONG   (__stdcall* com_release)(void* self);
typedef HRESULT (__stdcall* surface_get_desc)(void* surface, void* desc);
typedef HRESULT (__stdcall* surface_blt_fast)(void* surface, DWORD x, DWORD y, void* from,
                                              RECT* source, DWORD flags);
typedef HRESULT (__stdcall* ddraw_create_surface)(void* ddraw, void* desc, void** surface,
                                                  void* outer);
typedef HRESULT (__stdcall* device_clear)(void* device, DWORD count, RECT* rects, DWORD flags,
                                          DWORD colour, float z, DWORD stencil);

/* Vtable slot 27: told which surface drawing onto the background goes to. */
typedef int (__fastcall* set_target)(BYTE* self, void* edx_unused, void* surface);

/* The game's tracked realloc. */
void* __cdecl game_FUN_004a44b0(void* p, size_t size, const char* file, int line);

/*
    A picture's blits of `source` from its own surface onto `to`: a Blt into `area`, or a BltFast
    to (x, y). With `blend` the first does nothing and the second blends by hand.
*/
int __fastcall game_FUN_004cc250(void* picture, void* edx_unused, RECT* area, void* to,
                                 RECT* source, DWORD flags, DWORD blend);
int __fastcall game_FUN_004cc290(void* picture, void* edx_unused, int x, int y, void* to,
                                 RECT* source, DWORD flags, DWORD blend);
#define picture_blit    game_FUN_004cc250
#define picture_blit_at game_FUN_004cc290

void __fastcall background_destruct(BYTE* self);
int  __fastcall background_restore_slots(BYTE* self);
int  __fastcall background_clear_depth(BYTE* self);
void __fastcall background_forget_drawn(BYTE* self);
void __fastcall background_slot_reset(BYTE* self, void* edx_unused, int slot);

static bg_slot* slot_at(BYTE* self, int slot)
{
    return &FIELD(self, BG_SLOTS, bg_slot*)[slot];
}

/* The part of `a` inside `b`: empty when right <= left or bottom <= top. */
static RECT overlap(const RECT* a, const RECT* b)
{
    RECT r;

    r.left = a->left > b->left ? a->left : b->left;
    r.right = a->right < b->right ? a->right : b->right;
    r.top = a->top > b->top ? a->top : b->top;
    r.bottom = a->bottom < b->bottom ? a->bottom : b->bottom;
    return r;
}

/* `r` measured from the top left of `origin`. */
static RECT relative(const RECT* r, const RECT* origin)
{
    RECT moved;

    moved.left = r->left - origin->left;
    moved.top = r->top - origin->top;
    moved.right = r->right - origin->left;
    moved.bottom = r->bottom - origin->top;
    return moved;
}

static void empty(RECT* r)
{
    r->left = r->top = r->right = r->bottom = 0;
}

/* The view is shown at the display mode's size with frames of its own still to draw. */
static BOOL view_redrawing(BYTE* view)
{
    BYTE* video;
    int* mode;

    if (!view || !(FIELD(view, 0x25, BYTE) & 1) || !FIELD(view, VIEW_REDRAWS, int))
        return FALSE;
    video = FIELD(game_main, 0xC, BYTE*);
    mode = FIELD(video, 0x10, int*) + 9 * FIELD(video, 0xC, int);   /* 36-byte modes */
    return FIELD(view, VIEW_HEIGHT, int) == mode[1] && FIELD(view, VIEW_WIDTH, int) == mode[0];
}

PORT(0x00403D80)
BYTE* __fastcall background_construct(BYTE* self, void* edx_unused, DWORD flag)
{
    (void) edx_unused;
    object_construct(self, NULL, flag);
    FIELD(self, BG_COPY_LIVE, DWORD) = 0;
    FIELD(self, BG_PICTURE, void*) = NULL;
    FIELD(self, BG_SLOTS, bg_slot*) = NULL;
    FIELD(self, BG_SLOT_COUNT, int) = 0;
    FIELD(self, BG_SLOTS_USED, int) = 0;
    FIELD(self, BG_FULL_REDRAWS, int) = 0;
    FIELD(self, BG_DC_SURFACE, void*) = NULL;
    FIELD(self, BG_DC, HDC) = NULL;
    FIELD(self, BG_PICTURE_DRAWN, DWORD) = 0;
    FIELD(self, BG_PICTURE_SLOT, int) = -1;
    FIELD(self, BG_DC_SLOT, int) = -1;
    *(void***) self = BACKGROUND_VTABLE;
    FIELD(self, BG_SHOWN, DWORD) = 1;
    FIELD(self, BG_DC_TO_COPY, DWORD) = 1;
    FIELD(self, BG_DC_TO_SCREEN, DWORD) = 1;
    /* BG_COPY is not cleared: a set-up that fails before making the copy destroys the object,
       and the destructor releases whatever was there. Kept. */
    return self;
}

PORT(0x00403DD0)
BYTE* __fastcall background_delete(BYTE* self, void* edx_unused, BYTE flags)
{
    (void) edx_unused;
    background_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

/* Vtable slot 13: before the menus draw, put the background back where the slots were, or have
   it drawn whole while it is invalid or a full-screen view is still drawing. */
PORT(0x00403DF0)
int __fastcall background_restore(BYTE* self)
{
    int i;

    if (!FIELD(self, BG_FULL_REDRAWS, int)
        && !view_redrawing(FIELD(object_screen(self), SCREEN_VIEW, BYTE*))) {
        if (!FIELD(self, BG_COPY_LIVE, DWORD)) {
            VIRTUAL(self, 0x6C, set_target)(self, NULL, FIELD(self, BG_COPY, void*));
            FIELD(self, BG_COPY_LIVE, DWORD) = 1;
        }
        background_forget_drawn(self);
        if (FIELD(object_screen(self), SCREEN_VIEW, void*))
            background_clear_depth(self);
        background_restore_slots(self);
        return 1;
    }

    VIRTUAL(self, 0x6C, set_target)(self, NULL, FIELD(object_screen(self), SCREEN_SURFACE, void*));
    FIELD(self, BG_COPY_LIVE, DWORD) = 0;
    if (FIELD(object_screen(self), SCREEN_VIEW, void*))
        background_clear_depth(self);
    if (FIELD(self, BG_SLOTS_USED, int))
        for (i = 0; i < FIELD(self, BG_SLOT_COUNT, int); i++)
            background_slot_reset(self, NULL, i);
    if (--FIELD(self, BG_FULL_REDRAWS, int) < 0)
        FIELD(self, BG_FULL_REDRAWS, int) = 0;
    return 1;
}

/* Vtable slot 15: draw the picture, whole; once it has been drawn, as lasting. */
PORT(0x00403EF0)
int __fastcall background_draw_picture(BYTE* self)
{
    BYTE* picture = FIELD(self, BG_PICTURE, BYTE*);
    RECT whole;

    if (picture && FIELD(self, BG_SHOWN, DWORD)) {
        whole.left = 0;
        whole.top = 0;
        whole.right = FIELD(picture, 0x14, LONG);
        whole.bottom = FIELD(picture, 0x18, LONG);
#ifdef OPENMCM2_DEV
        /* The widescreen menus' backdrop covers the screen, centred, its overhang cut off. */
        if (dev_settings.menus_widescreen && whole.right > 0 && whole.bottom > 0) {
            LONG width = FIELD(object_screen(self), SCREEN_WIDTH, LONG);
            LONG height = FIELD(object_screen(self), SCREEN_HEIGHT, LONG);
            LONG fit_w = width;
            LONG fit_h = width * whole.bottom / whole.right;

            if (fit_h < height) {
                fit_h = height;
                fit_w = height * whole.right / whole.bottom;
            }
            SetRect(&whole, (width - fit_w) / 2, (height - fit_h) / 2, (width + fit_w) / 2,
                    (height + fit_h) / 2);
        }
#endif
        background_draw(self, NULL, picture, &whole, NULL, DDBLT_WAIT,
                        FIELD(self, BG_PICTURE_SLOT, int), FIELD(self, BG_PICTURE_DRAWN, DWORD),
                        &FIELD(self, BG_PICTURE_PASSES, int), 0);
        FIELD(self, BG_PICTURE_DRAWN, DWORD) = 1;
    }
    return 1;
}

/* Vtable slot 27: nothing to do with the surface here. */
PORT(0x00403F60)
int __fastcall background_use_surface(BYTE* self, void* edx_unused, void* surface)
{
    (void) self;
    (void) edx_unused;
    (void) surface;
    return 1;
}

/* Vtable slot 8: set up on `screen`, making the copy like its back buffer and four slots. On
   failure the object destroys itself and NULL is returned. */
PORT(0x00403F70)
BYTE* __fastcall background_setup(BYTE* self, void* edx_unused, void* screen)
{
    BYTE desc[DESC_SIZE];
    void* back;
    void* ddraw;
    bg_slot* slots;
    int i;

    (void) edx_unused;
    object_setup(self, NULL, screen);
    __stosd((unsigned long*) desc, 0, DESC_SIZE / 4);
    FIELD(desc, 0, DWORD) = DESC_SIZE;
    FIELD(desc, DESC_FLAGS, DWORD) = DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    back = FIELD(object_screen(self), SCREEN_SURFACE, void*);
    if (VIRTUAL(back, 0x58, surface_get_desc)(back, desc))
        goto failed;
    FIELD(desc, DESC_FLAGS, DWORD) = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH | DDSD_PIXELFORMAT;
    FIELD(desc, DESC_CAPS, DWORD) = DDSCAPS_SYSTEMMEMORY | DDSCAPS_OWNDC;
    ddraw = FIELD(FIELD(object_screen(self), SCREEN_VIDEO, BYTE*), 0x190, void*);
    if (VIRTUAL(ddraw, 0x18, ddraw_create_surface)(ddraw, desc, &FIELD(self, BG_COPY, void*), NULL))
        goto failed;

    FIELD(self, 0x44, DWORD) = 0;
    FIELD(self, BG_SLOT_COUNT, int) = 4;
    FIELD(self, BG_SLOTS_USED, int) = 0;
    /* Not checked, and only `life` is set: the rest of each slot is whatever malloc left. Kept. */
    slots = game_FUN_004a4410(4 * sizeof(bg_slot), BACKGROUNDIMAGE_CPP, 0xD4);
    FIELD(self, BG_SLOTS, bg_slot*) = slots;
    for (i = 0; i < FIELD(self, BG_SLOT_COUNT, int); i++)
        slots[i].life = 0;
    return self;

failed:
    object_destroy(self);
    return NULL;
}

PORT(0x00404050)
void __fastcall background_destruct(BYTE* self)
{
    void* copy;

    *(void***) self = BACKGROUND_VTABLE;
    if (FIELD(self, BG_DC, HDC) && FIELD(self, BG_DC_SURFACE, void*))
        VIRTUAL(FIELD(self, BG_DC_SURFACE, void*), 0x68, surface_release_dc)(
            FIELD(self, BG_DC_SURFACE, void*), FIELD(self, BG_DC, HDC));
    copy = FIELD(self, BG_COPY, void*);
    if (copy) {
        VIRTUAL(copy, 0x08, com_release)(copy);
        FIELD(self, BG_COPY, void*) = NULL;
    }
    if (FIELD(self, BG_PICTURE, void*)) {
        object_destroy(FIELD(self, BG_PICTURE, void*));
        FIELD(self, BG_PICTURE, void*) = NULL;
    }
    if (FIELD(self, BG_SLOTS, bg_slot*))
        game_FUN_004a4450(FIELD(self, BG_SLOTS, bg_slot*), BACKGROUNDIMAGE_CPP, 0xE6);
    object_destruct(self);
}

/* Show `picture` behind everything, or with NULL none; the old one is the caller's to destroy. */
PORT(0x004040F0)
void __fastcall background_set_picture(BYTE* self, void* edx_unused, void* picture)
{
    (void) edx_unused;
    if (picture) {
        if (!FIELD(self, BG_PICTURE, void*))
            FIELD(self, BG_PICTURE_SLOT, int) = background_new_slot(self, NULL, 0);
        FIELD(self, BG_PICTURE, void*) = picture;
    } else {
        /* The slot number stays, so clearing twice frees it twice. Kept. */
        background_free_slot(self, NULL, FIELD(self, BG_PICTURE_SLOT, int));
        FIELD(self, BG_PICTURE, void*) = NULL;
    }
}

/*
    A slot, or -1. Its life starts at the buffer count + 1 and drops to the buffer count at the
    first restore, where it stays; freeing it takes one off, and the restores count it down to 0,
    one buffer each, putting the background back as they go. `keep` slots are never put back.
*/
PORT(0x00404130)
int __fastcall background_new_slot(BYTE* self, void* edx_unused, DWORD keep)
{
    BYTE* screen = object_screen(self);
    int count = FIELD(self, BG_SLOT_COUNT, int);
    bg_slot* slots;
    int i;

    (void) edx_unused;
    if (count > FIELD(self, BG_SLOTS_USED, int)) {
        slots = FIELD(self, BG_SLOTS, bg_slot*);
        for (i = 0; i < count; i++) {
            if (slots[i].life == 0) {
                /* drawn_in and frame are left from the slot's last use, or never set. Kept. */
                slots[i].life = FIELD(screen, SCREEN_BUFFERS, int) + 1;
                slots[i].keep = keep;
                FIELD(self, BG_SLOTS_USED, int)++;
                return i;
            }
        }
        return -1;
    }

    slots = game_FUN_004a44b0(FIELD(self, BG_SLOTS, bg_slot*), (count + 4) * sizeof(bg_slot),
                              BACKGROUNDIMAGE_CPP, 0x10E);
    FIELD(self, BG_SLOTS, bg_slot*) = slots;
    if (!slots)
        return -1;                      /* the old slots are lost with the pointer; kept */
    for (i = count; i < count + 4; i++) {
        slots[i].life = 0;
        slots[i].drawn_in = -1;
        slots[i].keep = 0;
    }
    slots[count].life = FIELD(screen, SCREEN_BUFFERS, int) + 1;
    slots[count].frame = FIELD(screen, SCREEN_FRAME, DWORD);
    slots[count].keep = keep;
    FIELD(self, BG_SLOT_COUNT, int) = count + 4;
    FIELD(self, BG_SLOTS_USED, int)++;
    return count;
}

PORT(0x00404240)
void __fastcall background_free_slot(BYTE* self, void* edx_unused, int slot)
{
    bg_slot* slots = FIELD(self, BG_SLOTS, bg_slot*);

    (void) edx_unused;
    if (!slots || slot >= FIELD(self, BG_SLOT_COUNT, int))
        return;
    /* A negative slot is not turned away, and one already free counts as freed again. A slot
       freed before its first restore sits at the buffer count and is never freed. Kept. */
    if (slots[slot].life)
        slots[slot].life--;
    if (slots[slot].life == 0)
        FIELD(self, BG_SLOTS_USED, int)--;
}

/* The slot was drawn over `area` in the buffer being drawn, clipped to the screen. */
PORT(0x00404280)
void __fastcall background_slot_redraw(BYTE* self, void* edx_unused, int slot, RECT* area)
{
    BYTE* screen = object_screen(self);
    bg_slot* s;
    RECT* r;

    (void) edx_unused;
    /* Meant for an all-zero area, this also skips one placed evenly about 0. Kept. */
    if (area->bottom + area->top == 0 || area->right + area->left == 0)
        return;
    s = slot_at(self, slot);
    if (s->life) {
        r = &s->area[FIELD(screen, SCREEN_BUFFER, int)];
        *r = *area;
        if (r->right > FIELD(screen, SCREEN_WIDTH, LONG))
            r->right = FIELD(screen, SCREEN_WIDTH, LONG);
        if (r->bottom > FIELD(screen, SCREEN_HEIGHT, LONG))
            r->bottom = FIELD(screen, SCREEN_HEIGHT, LONG);
    }
    s->frame = FIELD(screen, SCREEN_FRAME, DWORD);
}

/* Put the background back from the copy wherever a slot was drawn in this buffer. */
PORT(0x00404320)
int __fastcall background_restore_slots(BYTE* self)
{
    int i;

    if (!FIELD(self, BG_SLOTS_USED, int))
        return 1;
    for (i = 0; i < FIELD(self, BG_SLOT_COUNT, int); i++) {
        BYTE* screen = object_screen(self);
        bg_slot* s = slot_at(self, i);
        RECT* r;

        if (!s->life)
            continue;
        r = &s->area[FIELD(screen, SCREEN_BUFFER, int)];
        if (r->right - r->left > 0 && r->bottom - r->top > 0 && !s->keep) {
            void* back = FIELD(screen, SCREEN_SURFACE, void*);

            VIRTUAL(back, 0x1C, surface_blt_fast)(back, r->left, r->top, FIELD(self, BG_COPY, void*),
                                                  r, DDBLTFAST_WAIT);
        }
        if (s->life != FIELD(screen, SCREEN_BUFFERS, int))
            s->life--;
        if (s->life == 0)
            FIELD(self, BG_SLOTS_USED, int)--;
    }
    return 1;
}

/* Clear the depth buffer under each slot's area in the previous buffer; 0 if a clear fails. */
PORT(0x00404400)
int __fastcall background_clear_depth(BYTE* self)
{
    BYTE* screen = object_screen(self);
    int previous, i;

    if (!FIELD(self, BG_SLOTS_USED, int) || !FIELD(screen, SCREEN_VIEW, void*))
        return 1;
    previous = FIELD(screen, SCREEN_BUFFER, int) - 1;
    if (previous < 0)
        previous = FIELD(screen, SCREEN_BUFFERS, int) - 1;
    for (i = 0; i < FIELD(self, BG_SLOT_COUNT, int); i++) {
        bg_slot* s = slot_at(self, i);
        RECT* r = &s->area[previous];

        if (s->life && r->right - r->left > 0 && r->bottom - r->top > 0) {
            void* device = FIELD(object_screen(self), SCREEN_DEVICE, void*);

            if (VIRTUAL(device, 0x28, device_clear)(device, 1, r, D3DCLEAR_ZBUFFER, 0,
                                                    FIELD(object_screen(self), SCREEN_DEPTH, float), 0))
                return 0;
        }
    }
    return 1;
}

/*
    Blt `source` of `picture` into `area`. Not lasting, it goes on the screen and the slot is
    told; lasting, it goes into the copy as well until every buffer has it, and after that only
    the parts this frame's slots overlap are drawn again.
*/
PORT(0x004044C0)
int __fastcall background_draw(BYTE* self, void* edx_unused, void* picture, RECT* area,
                               RECT* source, DWORD flags, int slot, DWORD lasting, int* passes,
                               DWORD blend)
{
    BYTE* screen = object_screen(self);
    int i;

    (void) edx_unused;
    if (!lasting) {
        background_release_dc(self);
        picture_blit(picture, NULL, area, FIELD(screen, SCREEN_SURFACE, void*), source, flags, blend);
        if (slot >= 0)
            background_slot_redraw(self, NULL, slot, area);
        *passes = FIELD(screen, SCREEN_BUFFERS, int);
        return 1;
    }

    if (!FIELD(self, BG_COPY_LIVE, DWORD)) {
        background_release_dc(self);
        picture_blit(picture, NULL, area, FIELD(screen, SCREEN_SURFACE, void*), source, flags, blend);
        *passes = FIELD(screen, SCREEN_BUFFERS, int);
    } else if (*passes) {
        background_release_dc(self);
        picture_blit(picture, NULL, area, FIELD(self, BG_COPY, void*), source, flags, blend);
        picture_blit(picture, NULL, area, FIELD(screen, SCREEN_SURFACE, void*), source, flags, blend);
        (*passes)--;
    } else {
        /* `source` is not used: each part is taken as if `area` were at the picture's corner. Kept. */
        for (i = 0; i < FIELD(self, BG_SLOT_COUNT, int); i++) {
            bg_slot* s = slot_at(self, i);
            RECT clip, part;

            if (s->frame != FIELD(screen, SCREEN_FRAME, DWORD))
                continue;
            clip = overlap(&s->area[FIELD(screen, SCREEN_BUFFER, int)], area);
            if (clip.left >= clip.right || clip.top >= clip.bottom)
                continue;
            background_release_dc(self);
            part = relative(&clip, area);
            picture_blit(picture, NULL, &clip, FIELD(screen, SCREEN_SURFACE, void*), &part, flags, blend);
            picture_blit(picture, NULL, &clip, FIELD(self, BG_COPY, void*), &part, flags, blend);
        }
    }
    if (slot >= 0)
        background_slot_clear(self, NULL, slot);
    return 1;
}

/* background_draw with a BltFast to (x, y): the cursor's. */
PORT(0x00404740)
int __fastcall background_draw_at(BYTE* self, void* edx_unused, void* picture, int x, int y,
                                  RECT* source, DWORD flags, int slot, DWORD lasting, int* passes,
                                  DWORD blend)
{
    BYTE* screen = object_screen(self);
    int i;

    (void) edx_unused;
    if (!lasting) {
        background_release_dc(self);
        picture_blit_at(picture, NULL, x, y, FIELD(screen, SCREEN_SURFACE, void*), source, flags, blend);
        if (slot >= 0) {
            RECT area;

            area.left = x;
            area.top = y;
            area.right = source->right - source->left + x;
            area.bottom = source->bottom - source->top + y;
            background_slot_redraw(self, NULL, slot, &area);
        }
        *passes = FIELD(screen, SCREEN_BUFFERS, int);
        return 1;
    }

    if (!FIELD(self, BG_COPY_LIVE, DWORD)) {
        background_release_dc(self);
        picture_blit_at(picture, NULL, x, y, FIELD(screen, SCREEN_SURFACE, void*), source, flags, blend);
        *passes = FIELD(screen, SCREEN_BUFFERS, int);
    } else if (*passes) {
        background_release_dc(self);
        picture_blit_at(picture, NULL, x, y, FIELD(self, BG_COPY, void*), source, flags, blend);
        picture_blit_at(picture, NULL, x, y, FIELD(screen, SCREEN_SURFACE, void*), source, flags, blend);
        (*passes)--;
    } else {
        /* The slot's screen area is met with `source`, and the part still goes to (x, y)
           unshifted. Kept. */
        for (i = 0; i < FIELD(self, BG_SLOT_COUNT, int); i++) {
            bg_slot* s = slot_at(self, i);
            RECT clip, part;

            if (s->frame != FIELD(screen, SCREEN_FRAME, DWORD))
                continue;
            clip = overlap(&s->area[FIELD(screen, SCREEN_BUFFER, int)], source);
            if (clip.left >= clip.right || clip.top >= clip.bottom)
                continue;
            background_release_dc(self);
            part = relative(&clip, source);
            picture_blit_at(picture, NULL, x, y, FIELD(screen, SCREEN_SURFACE, void*), &part, flags, blend);
            picture_blit_at(picture, NULL, x, y, FIELD(self, BG_COPY, void*), &part, flags, blend);
        }
    }
    if (slot >= 0)
        background_slot_clear(self, NULL, slot);
    return 1;
}

/*
    background_get_dc's surface once every buffer has the drawing: for each slot placed this frame
    that overlaps `area`, the screen's and then the copy's, with `clip` the overlap. NULL when none
    are left, and the walk starts over.
*/
static void* next_part(BYTE* self, RECT* area, int* again, RECT* clip)
{
    BYTE* screen = object_screen(self);
    void* surface;
    int i;

    FIELD(self, BG_DC_TO_COPY, DWORD) = 1;
    for (i = 0; i < FIELD(self, BG_SLOT_COUNT, int); i++) {
        bg_slot* s = slot_at(self, i);
        RECT part;

        if (s->frame != FIELD(screen, SCREEN_FRAME, DWORD))
            continue;
        part = overlap(&s->area[FIELD(screen, SCREEN_BUFFER, int)], area);
        if (part.left >= part.right || part.top >= part.bottom)
            continue;
        if (FIELD(self, BG_DC_TO_SCREEN, DWORD)) {
            if (i <= FIELD(self, BG_DC_SLOT, int))
                continue;
            surface = FIELD(screen, SCREEN_SURFACE, void*);
            FIELD(self, BG_DC_TO_SCREEN, DWORD) = 0;
        } else {
            if (i != FIELD(self, BG_DC_SLOT, int))
                continue;
            surface = FIELD(self, BG_COPY, void*);
            FIELD(self, BG_DC_TO_SCREEN, DWORD) = 1;
        }
        *again = 1;
        *clip = part;
        FIELD(self, BG_DC_SLOT, int) = i;
        return surface;
    }
    FIELD(self, BG_DC_TO_SCREEN, DWORD) = 1;
    FIELD(self, BG_DC_SLOT, int) = -1;
    return NULL;
}

/*
    A DC to paint `area` with, chosen as background_draw chooses its surfaces. `again` says to
    paint again with another DC, `clip` then holding the part to paint. 0 when there is nothing
    to paint.
*/
PORT(0x00404A10)
int __fastcall background_get_dc(BYTE* self, void* edx_unused, HDC* dc, RECT* area, int slot,
                                 DWORD lasting, int* passes, int* again, RECT* clip)
{
    BYTE* screen = object_screen(self);
    void* surface;

    (void) edx_unused;
    *dc = NULL;
    *again = 0;
    if (!lasting) {
        if (slot >= 0)
            background_slot_redraw(self, NULL, slot, area);
        surface = FIELD(screen, SCREEN_SURFACE, void*);
        *passes = FIELD(screen, SCREEN_BUFFERS, int);
    } else {
        if (!FIELD(self, BG_COPY_LIVE, DWORD)) {
            surface = FIELD(screen, SCREEN_SURFACE, void*);
            *passes = FIELD(screen, SCREEN_BUFFERS, int);
        } else if (*passes) {
            if (FIELD(self, BG_DC_TO_COPY, DWORD)) {
                surface = FIELD(self, BG_COPY, void*);
                *again = 1;
                FIELD(self, BG_DC_TO_COPY, DWORD) = 0;
            } else {
                surface = FIELD(screen, SCREEN_SURFACE, void*);
                FIELD(self, BG_DC_TO_COPY, DWORD) = 1;
                (*passes)--;
            }
        } else {
            surface = next_part(self, area, again, clip);
        }
        if (slot >= 0)
            background_slot_clear(self, NULL, slot);
    }

    if (!surface)
        return 0;
    if (surface != FIELD(self, BG_DC_SURFACE, void*)) {
        if (FIELD(self, BG_DC_SURFACE, void*))
            background_release_dc(self);
        FIELD(self, BG_DC_SURFACE, void*) = surface;
        VIRTUAL(surface, 0x44, surface_get_dc)(surface, &FIELD(self, BG_DC, HDC));
    }
    *dc = FIELD(self, BG_DC, HDC);
    return 1;
}

PORT(0x00404CC0)
int __fastcall background_release_dc(BYTE* self)
{
    void* surface = FIELD(self, BG_DC_SURFACE, void*);

    if (surface)
        VIRTUAL(surface, 0x68, surface_release_dc)(surface, FIELD(self, BG_DC, HDC));
    FIELD(self, BG_DC_SURFACE, void*) = NULL;
    FIELD(self, BG_DC, HDC) = NULL;
    return 1;
}

/* The slot was drawn as part of the background in this buffer: the next frame empties its area
   there (background_forget_drawn), there being nothing to put back. */
PORT(0x00404CF0)
void __fastcall background_slot_clear(BYTE* self, void* edx_unused, int slot)
{
    (void) edx_unused;
    slot_at(self, slot)->drawn_in = FIELD(object_screen(self), SCREEN_BUFFER, int);
}

PORT(0x00404D10)
void __fastcall background_forget_drawn(BYTE* self)
{
    int i;

    for (i = 0; i < FIELD(self, BG_SLOT_COUNT, int); i++) {
        bg_slot* s = slot_at(self, i);

        if (s->life > 0 && s->drawn_in != -1) {
            RECT* r = &s->area[s->drawn_in];

            s->drawn_in = -1;
            empty(r);
        }
    }
}

/* Forget where the slot was drawn in this buffer. */
PORT(0x00404D70)
void __fastcall background_slot_reset(BYTE* self, void* edx_unused, int slot)
{
    bg_slot* s = slot_at(self, slot);

    (void) edx_unused;
    s->drawn_in = -1;
    empty(&s->area[FIELD(object_screen(self), SCREEN_BUFFER, int)]);
}

/* Draw the whole background afresh in every buffer, and have the view redrawn too. */
PORT(0x00404DE0)
void __fastcall background_invalidate(BYTE* self)
{
    BYTE* screen = object_screen(self);
    BYTE* view = FIELD(screen, SCREEN_VIEW, BYTE*);

    FIELD(self, BG_FULL_REDRAWS, int) = FIELD(screen, SCREEN_BUFFERS, int) + 1;
    if (view) {
        FIELD(view, VIEW_REDRAWS, int) = FIELD(screen, SCREEN_BUFFERS, int) + 1;
        if (FIELD(view, VIEW_HEIGHT, DWORD) < FIELD(screen, SCREEN_HEIGHT, DWORD))
            FIELD(view, VIEW_EDGE_REDRAWS, int) = FIELD(screen, SCREEN_BUFFERS, int) + 1;
    }
}

/* Vtable slot 18. */
PORT(0x00404E20)
int __fastcall background_refresh(BYTE* self)
{
    background_invalidate(self);
    return 1;
}
