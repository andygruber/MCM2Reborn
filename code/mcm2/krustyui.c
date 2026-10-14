/*
    OpenMCM2 - the menus' session, ported from the game (krustyui.cpp in the original).

    The session is the object at game_main + 0x56C. It owns the menus' GUIManager, opens their
    screens, holds the bikes, riders and engine presets they offer, and builds the 3D garage
    shown behind some of them. Each function here replaces the game's at the address in its
    PORT line, and was checked against the original's instructions.
*/

#include "gui.h"

#define NAME(address) ((const char*) (address))
#define KRUSTYUI_CPP  NAME(0x005736FC)      /* "D:\aardvark\VC\krusty2\krustyui.cpp" */
#define READ_MODE     NAME(0x0056CA38)      /* "r" */

#define game_net       FIELD(game_main, 0x8, BYTE*)
#define game_display   ((DWORD*) (game_main + 0x1C))    /* 6 DWORDs, the first the display */
#define game_profile   ((char*) game_main + 0x578)
#define game_resources (*(void* const*) 0x00578C84)

/* The session's fields. */
#define SESSION_VTABLE         ((void**) 0x00559944)
#define session_gui(s)         FIELD(s, 0x2C, gui_manager*)
#define session_before_d0(s)   FIELD(s, 0x34, int)      /* the screen 0xD0 was opened over */
#define session_showing(s)     FIELD(s, 0x3C, int)
#define session_makers(s)      FIELD(s, 0x48, BYTE*)
#define session_maker_count(s) FIELD(s, 0x4C, int)
#define session_bikes(s)       FIELD(s, 0x50, BYTE*)
#define session_bike_count(s)  FIELD(s, 0x54, int)
#define session_riders(s)      FIELD(s, 0x58, BYTE*)
#define session_rider_count(s) FIELD(s, 0x5C, int)
#define session_tracks(s)      FIELD(s, 0x60, BYTE*)
#define session_track_count(s) FIELD(s, 0x64, int)
#define session_scene(s)       FIELD(s, 0x464, BYTE*)   /* the garage; NULL until built */
#define session_camera(s)      FIELD(s, 0x468, BYTE*)
#define session_rider_model(s) FIELD(s, 0x46C, BYTE*)
#define session_bike_model(s)  FIELD(s, 0x470, BYTE*)
#define session_centre(s)      FIELD(s, 0x474, vector3)
#define session_rider_place(s) FIELD(s, 0x480, vector3)
#define session_loading(s)     FIELD(s, 0x490, BYTE*)   /* the loading screen while it is open */
#define session_allow_ime(s)   FIELD(s, 0x498, DWORD)

/*
    A maker (200 bytes, from bikes.pb): its name, model files, the model the garage shows for it
    and who may ride its bikes (3: the computer riders too).
*/
#define MAKER_SIZE         200
#define MAKER_MODEL        0x40
#define MAKER_UI_MODEL     0x80
#define MAKER_SHOWN        0xC0
#define AVAILABILITY       0xC4     /* a maker's or a rider's */
#define FOR_EVERYONE       3

/* A bike (0x94 bytes): its maker's index, name, look, and the player's own or not. */
#define BIKE_SIZE          0x94
#define BIKE_MAKER         0x00
#define BIKE_NAME          0x04
#define BIKE_LOOK          0x48
#define BIKE_OWN           0x88
#define BIKE_ENGINE        0x8C     /* cc */
#define BIKE_FOUR_STROKE   0x90

/* A rider (200 bytes). */
#define RIDER_SIZE         200
#define RIDER_LOOK         0x40
#define RIDER_OWN          0xC0

#define TRACK_SIZE         0x54

static BYTE* maker_at(BYTE* self, int i)
{
    return session_makers(self) + i * MAKER_SIZE;
}

static BYTE* bike_at(BYTE* self, int i)
{
    return session_bikes(self) + i * BIKE_SIZE;
}

static BYTE* rider_at(BYTE* self, int i)
{
    return session_riders(self) + i * RIDER_SIZE;
}

/* The game's: sprintf, atoi, atof, _strnicmp, strrchr, rand, srand, atexit, the time, calloc. */
int    __cdecl game_FUN_0053a11f(char* to, const char* format, ...);
int    __cdecl game_FUN_0053b6c7(const char* text);
double __cdecl game_FUN_0053b990(const char* text);
int    __cdecl game_FUN_0054cd80(const char* a, const char* b, size_t count);
char*  __cdecl game__strrchr(const char* text, int c);
int    __cdecl game__rand(void);
void   __cdecl game_FUN_0053a27e(unsigned seed);
int    __cdecl game_FUN_0053aaa0(void (__cdecl* at_exit)(void));
DWORD  __cdecl game_FUN_004c41d0(void);
void*  __cdecl game_FUN_004a45b0(size_t count, size_t size, const char* file, int line);
void*  __cdecl ui_realloc(void* block, size_t size);

/* A string from the game's language module, into `text` of `size` bytes; FALSE when missing. */
BOOL __fastcall game_FUN_00526fc0(BYTE* game, void* edx_unused, UINT id, char* text, int size);
#define load_string(id, text) game_FUN_00526fc0(game_main, NULL, id, text, sizeof text)

/* The game's settings (the registry, judging by the names): a number and a text. */
typedef DWORD (__fastcall* setting_number)(void* game, void* edx_unused, const char* name,
                                           DWORD fallback);
typedef int   (__fastcall* setting_text)(void* game, void* edx_unused, const char* name,
                                         const char* fallback, char* text, DWORD* size);
#define GAME_SETTING_NUMBER 0x58
#define GAME_SETTING_TEXT   0x5C

/* The resources' look-up and add, and the game's file object (308 bytes). */
BYTE* __fastcall game_FUN_004ee0f0(void* resources, void* edx_unused, const char* name, DWORD a);
void* __fastcall game_FUN_004eddc0(void* resources, void* edx_unused, const char* path, DWORD a);
void* __fastcall game_FUN_00461c80(void* self, void* edx_unused, void* resources);
BOOL  __fastcall game_FUN_00461ec0(void* self, void* edx_unused, const char* name, const char* mode,
                                   DWORD a);
void  __fastcall game_FUN_00461cd0(void* self);

/* A file object on the game's resources (NULL when out of memory, which is opened anyway). */
static void* file_new(int line)
{
    void* file = game_new(0x134, KRUSTYUI_CPP, line);

    if (file)
        file = game_FUN_00461c80(file, NULL, game_resources);
    return file;
}

static void file_delete(void* file)
{
    if (file) {
        game_FUN_00461cd0(file);
        game_delete(file);
    }
}

/* The GUIManager and dialog functions these use, ported in gui.c and ui.c. */
gui_manager* __fastcall gui_manager_construct(gui_manager* self, void* edx_unused, DWORD flag);
gui_manager* __fastcall gui_manager_setup(gui_manager* self, void* edx_unused, void* screen,
                                          int a, int b, BYTE* background, DWORD load_sound,
                                          void* font_object, const char* name, short font_size,
                                          const char* cursor_image, int c);
void  __fastcall gui_manager_make_background(gui_manager* self);
void  __fastcall gui_manager_set_folder(gui_manager* self, void* edx_unused, const char* folder);
void  __fastcall gui_manager_set_wait_image(gui_manager* self, void* edx_unused, const char* image);
void  __fastcall gui_manager_show_cursors(gui_manager* self, void* edx_unused, BOOL show);
void  __fastcall gui_manager_set_font(gui_manager* self, void* edx_unused, const char* face);
BOOL  __fastcall gui_manager_make_message(gui_manager* self, void* edx_unused, const char* name);
void  __fastcall gui_manager_drop_message(gui_manager* self);
BOOL  __fastcall gui_manager_set_palette(gui_manager* self, void* edx_unused, DWORD palette);
BYTE* __fastcall gui_manager_open(gui_manager* self, void* edx_unused, BYTE* dialog, DWORD a,
                                  DWORD flags, DWORD area, BYTE* parent, DWORD arg, DWORD c,
                                  DWORD wait);
void  __fastcall gui_user_release_inputs(gui_user* self);
BOOL  __fastcall gui_user_claim(gui_user* self, void* edx_unused, gui_input* input);
BYTE* __fastcall dialog_construct(BYTE* self, void* edx_unused, DWORD flag, const char* name);
BYTE* __fastcall dialog_find_control(BYTE* self, void* edx_unused, const char* name, int type);
void  __fastcall progress_step(BYTE* self, void* edx_unused, BOOL refresh);

/* A dialog of `size` bytes read from `name`, given its kind's vtable (NULL when out of memory). */
static BYTE* dialog_new(size_t size, int line, const char* name, DWORD vtable)
{
    BYTE* dialog = game_new(size, KRUSTYUI_CPP, line);

    if (dialog) {
        dialog_construct(dialog, NULL, 1, name);
        *(DWORD*) dialog = vtable;
    }
    return dialog;
}

__declspec(noinline) void __fastcall session_load_bikes(BYTE* self);
__declspec(noinline) void __fastcall session_load_presets(BYTE* self);

/* ---- Set-up ---- */

#define menu_words ((char (*)[16]) 0x006905D8)  /* strings 5000-5035, 15 characters at most */
#define MENU_WORDS 36

#define FONT_ARIAL     NAME(0x00572444)    /* "Arial" */
#define FONT_NONE      NAME(0x0056ED48)    /* "NONE" */
#define CURSOR_IMAGE   NAME(0x0056E044)    /* "ui\cursor.tga" */
#define UI_FOLDER      NAME(0x0056FE48)    /* "ui" */
#define WAIT_IMAGE     NAME(0x005736F0)    /* "ui\wait.tga" */
#define UI_RESOURCES   NAME(0x005736E0)    /* "ui\uires.res" */
#define KEY_PROFILE    NAME(0x005736D4)    /* "MRUProfile" */
#define KEY_JOYSTICK   NAME(0x005736C4)    /* "JoystickFilter" */
#define INTRO          NAME(0x005736B8)    /* "intro1.dtm" */
#define SAVE_PROFILE   0x0049D370          /* the manager's callback: session_save_profile */

void __fastcall game_FUN_005294a0(char* profile);                       /* reset it */
void __fastcall game_FUN_00528820(char* profile);
void __fastcall game_FUN_004c8230(BYTE* input, void* edx_unused, DWORD filter);

/* Equal as `test ah, 40h` tests it (C3 set): unordered counts too. */
static BOOL x87_equal(double a, double b)
{
    return !(a < b) && !(a > b);
}

/*
    The session set up on `screen`: the menus' words, their GUIManager and its font, the profile
    last used, the session's bikes and presets, then the intro unless asked not to (or in a
    lobby's game). NULL if it could not join the tree, in which case it destroyed itself.
*/
PORT(0x00499850)
BYTE* __fastcall session_setup(BYTE* self, void* edx_unused, BYTE* screen, BOOL intro)
{
    char font[0x80];
    char text[0x80];
    DWORD profile_size;
    float font_grow;
    gui_manager* gui;
    int i;

    (void) edx_unused;
    object_setup(self, NULL, screen);
    profile_size = 16;
    for (i = 0; i < MENU_WORDS; i++) {
        load_string(5000 + i, text);
        text_copy_most(menu_words[i], text, 15);
    }

    font_grow = 0.0f;
    if (load_string(0xFED, text))
        font_grow = (float) game_FUN_0053b990(text);
    text_copy_most(font, load_string(0xFF2, text) ? text : FONT_ARIAL, 0x7F);

    gui = game_new(sizeof(gui_manager), KRUSTYUI_CPP, 0x7E);
    if (gui)
        gui = gui_manager_construct(gui, NULL, 1);
    session_gui(self) = gui;
    gui = gui_manager_setup(gui, NULL, object_screen(self),
                            FIELD(FIELD(game_main, 0xC, BYTE*), 0x64, int),
                            FIELD(game_main, 0x3C, int), NULL, 0, NULL, FONT_ARIAL, 15,
                            CURSOR_IMAGE, SAVE_PROFILE);
    if (!object_add_sibling(self, NULL, gui, -1)) {
        object_destroy(self);
        return NULL;
    }

    if (font[0] && game_FUN_0054cd80(font, FONT_NONE, 4)) {
        gui_manager_set_font(session_gui(self), NULL, font);
        if (load_string(0x1469, text))
            session_gui(self)->font_bold = game_FUN_0053b6c7(text);
    }
    if (!x87_equal(font_grow, 0.0f))
        session_gui(self)->font_grow = font_grow;

    gui_manager_make_background(session_gui(self));
    gui_manager_set_folder(session_gui(self), NULL, UI_FOLDER);
    gui_manager_set_wait_image(session_gui(self), NULL, WAIT_IMAGE);
    gui_user_release_inputs(gui_manager_user(session_gui(self), NULL, 0));
    gui = session_gui(self);
    gui_user_claim(gui_manager_user(gui, NULL, 0), NULL, gui->keyboard);
    gui_manager_show_cursors(session_gui(self), NULL, FALSE);
    game_FUN_004eddc0(game_resources, NULL, UI_RESOURCES, 0);

    game_FUN_005294a0(game_profile);
    VIRTUAL(game_main, GAME_SETTING_TEXT, setting_text)(game_main, NULL, KEY_PROFILE, NO_TEXT,
                                                        game_profile, &profile_size);
    game_FUN_00528820(game_profile);

    session_gui(self)->unknown_ec = FIELD(game_main, 0xC40, int);
    if (!FIELD(game_main, 0xC4C, int))
        ((tooltip*) gui_manager_user(session_gui(self), NULL, 0)->tooltip)->enabled = 0;
    if (FIELD(game_inputs, 0xC, BYTE*)) {
        DWORD filter = VIRTUAL(game_main, GAME_SETTING_NUMBER, setting_number)(game_main, NULL,
                                                                               KEY_JOYSTICK, 0);

        game_FUN_004c8230(FIELD(game_inputs, 0xC, BYTE*), NULL, filter);
    }

    session_bikes(self) = NULL;
    session_riders(self) = NULL;
    session_makers(self) = NULL;
    session_load_bikes(self);
    session_load_presets(self);
    session_gui(self)->unknown_34c =
        (FIELD(game_main, 0xF9C, int) ? (FIELD(game_main, 0xFB4, int) - 100) * 25 : -10000) - 200;

    if ((!game_net || !FIELD(game_net, 0x14, int)) && intro)
        gui_manager_open(session_gui(self), NULL, dialog_new(0x7F5C, 0xCE, INTRO, 0x005599B4), 0,
                         2, 0, NULL, 0, 0, 1);
    return self;
}

/* ---- The garage ---- */

/* The function's statics: built and dressed, its lights, shadow, and where they stand. */
#define scene_statics    (*(BYTE*) 0x006825DC)     /* which of the six vectors are registered */
#define scene_lights     (*(BYTE**) 0x006825E0)
#define scene_light      (*(BYTE**) 0x006825E4)    /* the last light made */
#define scene_built      (*(int*) 0x006825E8)
#define scene_dressed    (*(int*) 0x006825EC)
#define scene_shadow     (*(BYTE**) 0x006825CC)
#define shadow_drawer    (*(BYTE**) 0x006825AC)
#define key_place        (*(vector3*) 0x006825D0)
#define key_direction    (*(vector3*) 0x00682550)
#define fill_place       (*(vector3*) 0x00682590)  /* worked out, never used */
#define fill_direction   (*(vector3*) 0x00682580)
#define shadow_place     (*(vector3*) 0x006825B0)  /* likewise */
#define shadow_direction (*(vector3*) 0x006825C0)
#define scene_origin     (*(const vector3*) 0x00682560)

#define ALLOW_IME       NAME(0x00573770)    /* "AllowIME" */
#define GLOBAL_DTM      NAME(0x00573764)    /* "global.dtm" */
#define GARAGE_MODEL    NAME(0x00573754)    /* "UIgarage.mcf" */
#define RIDER_MODEL     NAME(0x00573740)    /* "UIRider.mcf" */
#define ANIM_SPIN       NAME(0x0057374C)    /* "Spin" */
#define ANIM_RIDER_WAIT NAME(0x0056FC58)    /* "WaitR" */
#define ANIM_BIKE_WAIT  NAME(0x00573720)    /* "WaitB" */
#define BIKE_ANIMATIONS NAME(0x00573728)    /* "Animations\UIbike\%s" */

#define model_frame(m) FIELD(m, 0x1A0, BYTE*)

typedef BYTE* (__fastcall* setup_on)(void* self, void* edx_unused, BYTE* screen);
typedef BYTE* (__fastcall* model_load)(void* model, void* edx_unused, BYTE* screen,
                                       const char* file, BYTE* lights, DWORD* display, DWORD a,
                                       DWORD b);
#define SETUP_ON   0x20
#define MODEL_LOAD 0x2C

BOOL  __cdecl    game_FUN_0050ab20(BOOL unhook);        /* the keyboard hook: 1 removes it */
BYTE* __fastcall game_FUN_004c34d0(BYTE* self, void* edx_unused, DWORD flag);     /* camera */
BYTE* __fastcall game_FUN_0049f940(BYTE* self, void* edx_unused, DWORD flag);     /* lights */
BYTE* __fastcall game_FUN_0049f460(BYTE* self, void* edx_unused, DWORD flag);     /* a light */
BYTE* __fastcall game_FUN_0049f7e0(BYTE* light, void* edx_unused, BYTE* screen, int kind,
                                   COLORREF colour, const vector3* place, const vector3* direction,
                                   DWORD a, DWORD b, DWORD c, DWORD d, DWORD e, DWORD f);
void  __fastcall game_FUN_0049fa20(BYTE* lights, void* edx_unused, BYTE* light);
BYTE* __fastcall game_FUN_00446550(BYTE* self, void* edx_unused, DWORD a, DWORD flag);  /* model */
void  __fastcall game_FUN_00445460(BYTE* frame, void* edx_unused, DWORD value);
void  __fastcall game_FUN_005014a0(BYTE* frame, void* edx_unused, const vector3* place);
void  __fastcall game_FUN_00500b50(BYTE* frame, void* edx_unused, vector3 forward, vector3 up,
                                   DWORD a, DWORD b);
void  __fastcall game_FUN_004aa170(BYTE* model, void* edx_unused, const char* animation);
void  __fastcall game_FUN_004a8210(BYTE* model, void* edx_unused, float seconds, DWORD a, DWORD b);
BYTE* __fastcall game_FUN_004ded60(BYTE* self, void* edx_unused, DWORD flag);     /* shadow */
BYTE* __fastcall game_FUN_004defa0(BYTE* shadow, void* edx_unused, BYTE* screen, DWORD a);
BYTE* __fastcall game_FUN_004477e0(BYTE* self, void* edx_unused, DWORD flag);     /* its drawer */
BYTE* __fastcall game_FUN_00447830(BYTE* self, void* edx_unused, BYTE* screen, BYTE* frame,
                                   BYTE* shadow);
void  __fastcall game_FUN_004df2f0(BYTE* shadow, void* edx_unused, BYTE* frame);  /* a caster */
void  __fastcall game_FUN_004df640(BYTE* shadow, void* edx_unused, const vector3* place,
                                   const vector3* direction, DWORD a);
void  __fastcall game_FUN_004df4d0(BYTE* shadow, void* edx_unused, DWORD value);
void  __fastcall game_FUN_004df620(BYTE* shadow, void* edx_unused, DWORD value);
void  __fastcall game_FUN_004eda80(BYTE* screen, void* edx_unused, BYTE* camera);
void  __fastcall game_FUN_00418300(DWORD* scope, void* edx_unused, BYTE* display);
void  __fastcall game_FUN_00418470(DWORD* scope, void* edx_unused, BYTE* frame, DWORD value);
void  __fastcall game_FUN_00418370(DWORD* scope);
int   __fastcall game_FUN_0046a0d0(void* self);                                   /* unlink */
int   __fastcall game_vf_24_46A550(void* self, void* edx_unused, DWORD message, DWORD a, DWORD b,
                                   DWORD c, DWORD d);

/* The destructors of the six vectors, registered with atexit: empty, as the class's is. */
PORT(0x0049A8D0)
void __cdecl scene_vector_end_6(void) { }
PORT(0x0049A8E0)
void __cdecl scene_vector_end_5(void) { }
PORT(0x0049A8F0)
void __cdecl scene_vector_end_4(void) { }
/* Ghidra never defined these three (0x0049A900-20) as functions, so they cannot be ports. */
void __cdecl scene_vector_end_3(void) { }
void __cdecl scene_vector_end_2(void) { }
void __cdecl scene_vector_end_1(void) { }

static void scene_static(BYTE bit, void (__cdecl* end)(void))
{
    if (!(scene_statics & bit)) {
        scene_statics |= bit;
        game_FUN_0053aaa0(end);
    }
}

static void towards(vector3* direction, const vector3* centre, const vector3* from)
{
    direction->x = centre->x - from->x;
    direction->y = centre->y - from->y;
    direction->z = centre->z - from->z;
}

/* A light of `kind` in the garage, and in its light group when it could be made. */
static void scene_add_light(BYTE* self, int line, int kind, COLORREF colour, const vector3* place,
                            const vector3* direction)
{
    BYTE* light = game_new(0xAC, KRUSTYUI_CPP, line);

    if (light)
        light = game_FUN_0049f460(light, NULL, 1);
    scene_light = game_FUN_0049f7e0(light, NULL, object_screen(self), kind, colour, place,
                                    direction, 0, 0, 0, 0, 0, 0);
    object_add_child(session_scene(self), NULL, scene_light, -1);
    if (scene_light)
        game_FUN_0049fa20(scene_lights, NULL, scene_light);
}

/* A model of 576 bytes, loaded from `file` into the garage. */
static BYTE* scene_add_model(BYTE* self, int line, DWORD a, const char* file, DWORD* display,
                             BYTE** to)
{
    BYTE* model = game_new(0x240, KRUSTYUI_CPP, line);

    if (model)
        model = game_FUN_00446550(model, NULL, a, 1);
    *to = model;
    object_add_child(session_scene(self), NULL,
                     VIRTUAL(model, MODEL_LOAD, model_load)(model, NULL, object_screen(self), file,
                                                            scene_lights, display, 1, 1), -1);
    return model;
}

/* A model stands at `place` facing -z, playing `animation`. */
static void model_place(BYTE* model, DWORD value, const vector3* place, const char* animation)
{
    static const vector3 forward = { 0.0f, 0.0f, -1.0f };
    static const vector3 up = { 0.0f, 1.0f, 0.0f };

    game_FUN_00445460(model_frame(model), NULL, value);
    game_FUN_005014a0(model_frame(model), NULL, place);
    game_FUN_00500b50(model_frame(model), NULL, forward, up, 0, 1);
    game_FUN_004aa170(model, NULL, animation);
    game_FUN_004a8210(model, NULL, 0.03f, 0, 0);
}

/*
    Whether the models are told video memory is short: under 5 MB left once the screen's
    surfaces are counted (or 2-4 MB set aside on cards that do not report it).
*/
static BOOL short_of_memory(BYTE* screen)
{
    BYTE* device = FIELD(screen, 0x4, BYTE*);
    int memory = FIELD(device, 0x54, int);
    int left = memory - game_FUN_00517050(FIELD(screen, 0x28, int))
                        * (FIELD(screen, 0x14, int) + 1) * FIELD(screen, 0x10, int)
                        * FIELD(screen, 0xC, int);

    if (FIELD(game_main, 0x434, int) != 2 && !(FIELD(device, 0x1B9, BYTE) & 4))
        left = (FIELD(screen, 0x164, DWORD) & 0x4000) ? memory - 0x400000 : memory - 0x200000;
    return !FIELD(FIELD(game_main, 0xC, BYTE*), 0x9F0, int) && left < 0x500000;
}

/*
    Stage 1 builds the garage: camera, lights, the turning bike and the rider, the shadow. Stage 2
    adds a bike for each maker and points the shadow; -1 does both. Once dressed, stage 2 does
    nothing until the garage is built again.
*/
PORT(0x00499CA0)
void __fastcall session_build_scene(BYTE* self, void* edx_unused, int stage)
{
    vector3 sun_place;
    vector3 sun_direction;
    DWORD display[6];
    DWORD scope[11];
    char text[0x104];
    BYTE* made;
    int i;

    (void) edx_unused;
    scene_static(0x01, scene_vector_end_1);
    scene_static(0x02, scene_vector_end_2);
    scene_static(0x04, scene_vector_end_3);
    scene_static(0x08, scene_vector_end_4);
    scene_static(0x10, scene_vector_end_5);
    scene_static(0x20, scene_vector_end_6);

    FIELD(self, 0x4AC, DWORD) = 0;
    FIELD(self, 0x4A8, DWORD) = 0;
    FIELD(object_screen(self), 0x34, DWORD) = 0;
    FIELD(game_main, 0x2D5, BYTE) |= 4;

    if (!session_scene(self) && (stage == 1 || stage == -1)) {
        game_FUN_0050ab20(TRUE);
        session_allow_ime(self) = 0;
        if (load_string(0x1438, text) && game_FUN_0053b6c7(text))
            session_allow_ime(self) = 1;
        session_allow_ime(self) = VIRTUAL(game_main, GAME_SETTING_NUMBER, setting_number)(
            game_main, NULL, ALLOW_IME, session_allow_ime(self));
        if (session_allow_ime(self))
            gui_manager_set_palette(session_gui(self), NULL, 1);
        gui_manager_make_message(session_gui(self), NULL, GLOBAL_DTM);
        session_centre(self) = scene_origin;
        session_rider_place(self) = scene_origin;
        session_rider_place(self).y += 0.1f;

        made = game_new(sizeof(game_object), KRUSTYUI_CPP, 0x11B);
        if (made)
            object_construct(made, NULL, 1);
        session_scene(self) = made;

        made = game_new(0x220, KRUSTYUI_CPP, 0x11E);
        if (made)
            made = game_FUN_004c34d0(made, NULL, 1);
        session_camera(self) = VIRTUAL(made, SETUP_ON, setup_on)(made, NULL, object_screen(self));
        object_add_child(session_scene(self), NULL, session_camera(self), -1);
        FIELD(session_camera(self), 0x1D8, DWORD) = 0;
        FIELD(session_camera(self), 0x1D4, DWORD) = 0;

        made = game_new(0x11C, KRUSTYUI_CPP, 0x127);
        if (made)
            made = game_FUN_0049f940(made, NULL, 1);
        scene_lights = VIRTUAL(made, SETUP_ON, setup_on)(made, NULL, object_screen(self));
        object_add_child(session_scene(self), NULL, scene_lights, -1);

        scene_add_light(self, 0x12C, 6, 0x606060, NULL, NULL);
        sun_place.x = -1800.0f;
        sun_place.y = 200.0f;
        sun_place.z = 0.0f;
        towards(&sun_direction, &session_centre(self), &sun_place);
        scene_add_light(self, 0x137, 4, 0xFFFFFF, &sun_place, &sun_direction);

        key_place = session_centre(self);
        key_place.y += 8.0f;
        key_place.x += 12.0f;
        towards(&key_direction, &session_centre(self), &key_place);
        fill_place = key_place;
        fill_place.x = key_place.x - 24.0f;
        towards(&fill_direction, &session_centre(self), &fill_place);
        scene_add_light(self, 0x146, 1, 0x8C8C8C, &key_place, &key_direction);

        __movsd((unsigned long*) display, (unsigned long*) game_display, 6);
        display[5] = !FIELD(game_main, 0x2D0, int) && short_of_memory(object_screen(self));

        scene_add_model(self, 0x18A, 1, GARAGE_MODEL, display, &session_bike_model(self));
        model_place(session_bike_model(self), 0, &session_centre(self), ANIM_SPIN);

        made = game_new(0x134, KRUSTYUI_CPP, 0x198);
        if (made)
            made = game_FUN_004ded60(made, NULL, 1);
        scene_shadow = game_FUN_004defa0(made, NULL, object_screen(self),
                                         FIELD(game_main, 0x3C, DWORD));
        made = game_new(0x48, KRUSTYUI_CPP, 0x19C);
        if (made)
            made = game_FUN_004477e0(made, NULL, 1);
        shadow_drawer = game_FUN_00447830(made, NULL, object_screen(self),
                                          model_frame(session_bike_model(self)), scene_shadow);
        object_add_child(session_scene(self), NULL, shadow_drawer, -1);
        if (scene_shadow)
            object_add_child(session_scene(self), NULL, scene_shadow, -1);

        scene_add_model(self, 0x1AA, 1, RIDER_MODEL, game_display, &session_rider_model(self));
        model_place(session_rider_model(self), 1, &session_rider_place(self), ANIM_RIDER_WAIT);

        game_FUN_004eda80(object_screen(self), NULL, NULL);
        scene_built = 1;
        scene_dressed = 0;
    } else if (!scene_built || scene_dressed) {
        return;
    }
    if (stage != 2 && stage != -1)
        return;

    game_FUN_00418300(scope, NULL, FIELD(game_main, 0x1C, BYTE*));
    for (i = 0; i < session_maker_count(self); i++) {
        BYTE* maker = maker_at(self, i);
        BYTE* model;

        game_FUN_0053a11f(text, BIKE_ANIMATIONS, (const char*) maker + MAKER_UI_MODEL);
        model = scene_add_model(self, 0x1CA, 0, text, game_display,
                                &FIELD(maker, MAKER_SHOWN, BYTE*));
        model_place(model, 1, &session_centre(self), ANIM_BIKE_WAIT);
        game_FUN_00418470(scope, NULL, model_frame(model), FIELD(game_main, 0x2144, DWORD));
    }
    /* With no shadow it is never dressed, and the makers' bikes are made again each time. */
    if (scene_shadow) {
        game_FUN_004df2f0(scene_shadow, NULL,
                          model_frame(FIELD(maker_at(self, 0), MAKER_SHOWN, BYTE*)));
        game_FUN_004df2f0(scene_shadow, NULL, model_frame(session_rider_model(self)));
        shadow_place = session_centre(self);
        shadow_place.y += 40.0f;
        shadow_place.x += 15.0f;
        towards(&shadow_direction, &session_centre(self), &shadow_place);
        game_FUN_004df640(scene_shadow, NULL, &key_place, &key_direction, 3);
        game_FUN_004df4d0(scene_shadow, NULL, 0x20);
        game_FUN_004df620(scene_shadow, NULL, 0x20);
        scene_built = 0;
        scene_dressed = 1;
    }
    game_FUN_00418370(scope);
}

/* Slot 25: passed on to the garage too, which is not the session's child. */
PORT(0x0049A930)
int __fastcall session_slot_25(BYTE* self, void* edx_unused, DWORD a)
{
    (void) edx_unused;
    game_vf_25_46A650(self, NULL, a);
    if (session_scene(self))
        VIRTUAL(session_scene(self), 0x64, method_1)(session_scene(self), NULL, a);
    return 1;
}

/* Leaving the menus: the keyboard hook back, the message dialog and the garage gone. */
PORT(0x0049A960)
void __fastcall session_drop_scene(BYTE* self)
{
    game_FUN_0050ab20(FALSE);
    if (!(FIELD(game_main, 0x2D5, BYTE) & 2))
        gui_manager_drop_message(session_gui(self));
    if (session_scene(self)) {
        object_destroy(session_scene(self));
        session_scene(self) = NULL;
    }
}

/* The garage behind `dialog`, seen through the session's camera. */
PORT(0x0049A9A0)
void __fastcall session_show_scene(BYTE* self, void* edx_unused, BYTE* dialog)
{
    (void) edx_unused;
    if (session_scene(self)) {
        object_add_child(dialog, NULL, session_scene(self), -1);
        game_FUN_004eda80(object_screen(self), NULL, session_camera(self));
    }
}

PORT(0x0049A9D0)
void __fastcall session_hide_scene(BYTE* self)
{
    if (session_scene(self)) {
        game_FUN_0046a0d0(session_scene(self));
        game_FUN_004eda80(object_screen(self), NULL, NULL);
    }
}

/* Slot 23: input clears 0x4A4 (an idle count, it seems). */
PORT(0x0049A9F0)
int __fastcall session_slot_23(BYTE* self, void* edx_unused, DWORD a, DWORD b)
{
    (void) edx_unused;
    FIELD(self, 0x4A4, DWORD) = 0;
    return game_vf_23_46A500(self, NULL, a, b) != 0;
}

/* Slot 24: message 0x101 that nothing took sets the network's 0x10. */
PORT(0x0049AA20)
int __fastcall session_slot_24(BYTE* self, void* edx_unused, DWORD message, DWORD a, DWORD b,
                               DWORD c, DWORD d)
{
    (void) edx_unused;
    if (game_vf_24_46A550(self, NULL, message, a, b, c, d))
        return 1;
    if (message == 0x101)
        FIELD(game_net, 0x10, DWORD) = 1;
    return 0;
}

PORT(0x0049AA70)
int __fastcall session_show(BYTE* self)
{
    return game_vf_5_469F90(self);
}

PORT(0x0049AA80)
void __fastcall session_show_cursors(BYTE* self)
{
    if (session_gui(self))
        gui_manager_show_cursors(session_gui(self), NULL, TRUE);
}

PORT(0x0049AA90)
void __fastcall session_hide_cursors(BYTE* self)
{
    if (session_gui(self))
        gui_manager_show_cursors(session_gui(self), NULL, FALSE);
}

/* ---- Screens ---- */

#define MAIN_MENU   NAME(0x0057392C)    /* "mainmenu.dtm" */
#define SP_BASE     NAME(0x0056F660)    /* "SPBase.dtm" */
#define OPT_BASE    NAME(0x0056F690)    /* "OptBase.dtm" */
#define LOADING     NAME(0x0056FD04)    /* "loading.dtm" */
#define HOST_JOIN   NAME(0x0057390C)    /* "HostJoin.dtm" */
#define MP_BASE     NAME(0x00573900)    /* "MPBase.dtm" */
#define SERIAL      NAME(0x0057391C)    /* "SerPopup.dtm" */
#define MESSAGE_1   NAME(0x005700BC)    /* "messbox1.dtm" */
#define MESSAGE_2   NAME(0x0056F734)    /* "messbox2.dtm" */
#define PC_CHOOSE   NAME(0x005738EC)    /* "PCC.dtm" */
#define PC_RACE     NAME(0x005738F4)    /* "PCLRace.dtm" */
#define HIGH_SCORES NAME(0x0056F584)    /* "HiScores.dtm" */

void* __fastcall game_FUN_004f7da0(void* self);            /* clears MPBase's table */
void  __fastcall game_FUN_00469760(BYTE* game);
void  __fastcall game_FUN_00469cb0(void* scene, void* edx_unused, const char* class_name);
void  __fastcall game_FUN_004d88f0(BYTE* net);
void  __cdecl    game_FUN_004d92f0(void);

/* MPBase has a table of its own, cleared before its vtable is set. */
static BYTE* multiplayer_new(int line)
{
    BYTE* dialog = game_new(0x8110, KRUSTYUI_CPP, line);

    if (dialog) {
        dialog_construct(dialog, NULL, 1, MP_BASE);
        game_FUN_004f7da0(dialog + 0x7F68);
        *(DWORD*) dialog = 0x00559BCC;
    }
    return dialog;
}

/* `dialog` opened with `id`; one `over` the screen showing is that screen's child. */
static void screen_open(BYTE* self, BYTE* dialog, int id, DWORD flags, BOOL over)
{
    BYTE* parent = over ? gui_manager_active_dialog(session_gui(self)) : NULL;

    gui_manager_open(session_gui(self), NULL, dialog, id, flags, 0, parent, 0, 0, 1);
}

/* The menus' screen numbered `screen`: the bases, their pop-ups and the message boxes. */
PORT(0x0049AAA0)
void __fastcall session_go_to(BYTE* self, void* edx_unused, int screen)
{
    BYTE* net = FIELD(game_main, 0x3440, BYTE*);

    (void) edx_unused;
    if (screen == 0xD0)
        session_before_d0(self) = session_showing(self);
    session_showing(self) = screen;

    switch (screen) {
    case 100:
        screen_open(self, dialog_new(0x7F80, 0x2B8, MAIN_MENU, 0x0055A070), screen, 2, FALSE);
        game_FUN_00469760(game_main);
        break;
    case 101:
        screen_open(self, dialog_new(0x7F64, 0x2BF, SP_BASE, 0x00556B4C), screen, 2, FALSE);
        break;
    case 102:
        screen_open(self, dialog_new(0x7F70, 0x2C3, OPT_BASE, 0x00556BD0), screen, 2, FALSE);
        break;
    case 104:
        if (FIELD(game_main, 0xFC4, DWORD) != FIELD(FIELD(game_main, 0xC, BYTE*), 0xC, DWORD))
            VIRTUAL(game_main, 0x4C, method_1)(game_main, NULL, FIELD(game_main, 0xFC4, DWORD));
        screen_open(self, dialog_new(0x7F5C, 0x2CD, LOADING, 0x00559FEC), screen, 2, FALSE);
        break;
    case 0xD8:
        screen_open(self, dialog_new(0x7F58, 0x2F7, SERIAL, 0x00559F68), screen, 4, TRUE);
        break;
    case 0xD9:
        screen_open(self, dialog_new(0x7F58, 0x2FB, MESSAGE_1, 0x00559EE4), screen, 4, TRUE);
        break;
    case 0xFC:
        screen_open(self, dialog_new(0x7F58, 0x31C, MESSAGE_2, 0x00559E60), screen, 4, TRUE);
        break;
    case 0x12F:
        screen_open(self, dialog_new(0x7F58, 0x338, MESSAGE_2, 0x005569C0), screen, 4, TRUE);
        break;
    case 0x190:
        screen_open(self, dialog_new(0x7F58, 0x34C, MESSAGE_2, 0x00559DDC), screen, 0x1C, FALSE);
        break;
    case 0x191:
        screen_open(self, dialog_new(0x7F58, 0x347, MESSAGE_2, 0x00559D58), screen, 0x1C, FALSE);
        break;
    case 0x1F9:
        screen_open(self, dialog_new(0x7F58, 0x33C, MESSAGE_2, 0x00559CD4), screen, 4, TRUE);
        break;
    case 0x1FA:
        screen_open(self, dialog_new(0x7F58, 0x340, MESSAGE_2, 0x00556C54), screen, 4, TRUE);
        break;
    case 0x85C:
    case 0x85D:
        screen_open(self, dialog_new(0x7F58, 0x2ED, HOST_JOIN, 0x00559C50), screen, 4, TRUE);
        break;
    case 0x866:
    case 0x867:
    case 0x868:
        screen_open(self, multiplayer_new(0x2F3), screen, 2, FALSE);
        break;
    case 0x88E:
        /* PCC opens with 0 for its id, here and for 0x88F (the original's). */
        if (!net) {
            if (FIELD(game_main, 0x18, int) == 1)
                screen_open(self, dialog_new(0x7F64, 0x315, SP_BASE, 0x00556B4C), screen, 2,
                            FALSE);
            else
                screen_open(self, multiplayer_new(0x317), screen, 2, FALSE);
        } else if (FIELD(net, 0x128D + FIELD(net, 0x40, int) * 16, int) == FIELD(net, 0x44, int)) {
            game_FUN_004d88f0(net);
            game_FUN_004d92f0();
        } else if (FIELD(game_main, 0x2D70, int) == 1) {
            screen_open(self, dialog_new(0x7F58, 0x30E, PC_RACE, 0x00559B48), screen, 2, FALSE);
        } else {
            screen_open(self, dialog_new(0x7F64, 0x311, PC_CHOOSE, 0x00559AC4), 0, 2, FALSE);
        }
        break;
    case 0x88F:
        if (net)
            screen_open(self, dialog_new(0x7F64, 0x301, PC_CHOOSE, 0x00559AC4), 0, 2, FALSE);
        break;
    case 0x910:
    case 0x911:
        screen_open(self, dialog_new(0x7F68, 0x32A, HIGH_SCORES, 0x00556834), screen, 2, FALSE);
        break;
    case 0xBB9:
    case 0xBBA:
    case 0xBBB:
        screen_open(self, dialog_new(0x7F58, 0x334, MESSAGE_1, 0x00559A40), screen, 0xC, TRUE);
        break;
    }
}

/* Quitting: the media controls and the session hidden, and the game's window told to close. */
PORT(0x0049B420)
void __fastcall session_quit(BYTE* self)
{
    game_FUN_00469cb0(self, NULL, NAME(0x0057393C));                        /* "MediaControl" */
    VIRTUAL(self, 0x10, method_0)(self);
    PostMessageA(FIELD(FIELD(game_main, 0xC, BYTE*), 0x1B0, HWND), WM_CLOSE, 0, 0);
}

/* ---- Engine presets ---- */

/*
    Five engine categories, each an RPM range read in 11 steps: the category's power curve, and
    three preset curves.
*/
#define CATEGORIES   5
#define PRESETS      3
#define CURVE_POINTS 11

#define preset_curves(s, c)  ((int*) ((s) + 0x68) + (c) * PRESETS * CURVE_POINTS)
#define category_curve(s, c) ((int*) ((s) + 0x324) + (c) * CURVE_POINTS)
#define rpm_low(s, c)        FIELD(s, 0x2FC + (c) * 4, int)
#define rpm_high(s, c)       FIELD(s, 0x310 + (c) * 4, int)
#define hp_sum(s, c)         FIELD(s, 0x400 + (c) * 4, int)
#define rpm_step(s, c)       FIELD(s, 0x414 + (c) * 4, int)
#define weight(s, c)         FIELD(s, 0x428 + (c) * 4, int)
#define min_range(s, c)      FIELD(s, 0x43C + (c) * 4, int)
#define curve_peak(s, c)     FIELD(s, 0x450 + (c) * 4, int)

#define PRESETS_PB      NAME(0x005739B4)    /* "presets.pb" */
#define CATEGORY        NAME(0x005739A8)    /* "Category_%d" */
#define CATEGORY_PRESET NAME(0x0057394C)    /* "Category_%dPreset_%d" */
#define RPM_KEY         NAME(0x00573964)    /* "RPM%05d" */
#define KEY_RPM_LOW     NAME(0x00573998)    /* "RPMLowerLimit" */
#define KEY_RPM_HIGH    NAME(0x00573988)    /* "RPMUpperLimit" */
#define KEY_HP_SUM      NAME(0x00573980)    /* "HPSum" */
#define KEY_MIN_RANGE   NAME(0x00573974)    /* "MinRange" */
#define KEY_WEIGHT      NAME(0x0057396C)    /* "Weight" */

#define rpm_at(s, c, point) (rpm_step(s, c) * (point) + rpm_low(s, c))

/* The categories from presets.pb, or defaults (an HP sum of 100, not the file's 400) without it. */
PORT(0x0049B460)
void __fastcall session_load_presets(BYTE* self)
{
    param_file params;
    void* file;
    char key[0x80];
    int category, preset, point;

    param_construct(&params);
    file = file_new(0x363);
    if (game_FUN_00461ec0(file, NULL, PRESETS_PB, READ_MODE, 0)) {
        param_open(&params, NULL, file, 0, TRUE);
        for (category = 0; category < CATEGORIES; category++) {
            int* curve = category_curve(self, category);
            int* values = preset_curves(self, category);
            int peak = 0;

            game_FUN_0053a11f(key, CATEGORY, category + 1);
            param_select(&params, NULL, key);
            param_int(&params, NULL, KEY_RPM_LOW, 5000, &rpm_low(self, category));
            param_int(&params, NULL, KEY_RPM_HIGH, 10000, &rpm_high(self, category));
            rpm_step(self, category) = (rpm_high(self, category) - rpm_low(self, category)) / 10;
            param_int(&params, NULL, KEY_HP_SUM, 400, &hp_sum(self, category));
            param_int(&params, NULL, KEY_MIN_RANGE, 10, &min_range(self, category));
            param_int(&params, NULL, KEY_WEIGHT, 200, &weight(self, category));
            for (point = 0; point < CURVE_POINTS; point++) {
                game_FUN_0053a11f(key, RPM_KEY, rpm_at(self, category, point));
                param_int(&params, NULL, key, 10, &curve[point]);
                if (peak <= curve[point])
                    peak = curve[point];
            }
            curve_peak(self, category) = peak;
            for (preset = 1; preset <= PRESETS; preset++) {
                game_FUN_0053a11f(key, CATEGORY_PRESET, category + 1, preset);
                param_select(&params, NULL, key);
                for (point = 0; point < CURVE_POINTS; point++) {
                    game_FUN_0053a11f(key, RPM_KEY, rpm_at(self, category, point));
                    param_int(&params, NULL, key, 10, values++);
                }
            }
        }
    } else {
        /* MinRange, Weight and the peak are left as they were. */
        for (category = 0; category < CATEGORIES; category++) {
            int* values = preset_curves(self, category);

            rpm_low(self, category) = 5000;
            rpm_high(self, category) = 10000;
            hp_sum(self, category) = 100;
            rpm_step(self, category) = (rpm_high(self, category) - rpm_low(self, category)) / 10;
            for (preset = 0; preset < PRESETS; preset++) {
                for (point = 0; point < CURVE_POINTS; point++) {
                    *values++ = 10;
                    category_curve(self, category)[point] = 40;
                }
            }
        }
    }
    file_delete(file);
    param_destruct(&params);
}

/* ---- Bikes and riders ---- */

#define KEY_NAME      NAME(0x0056ED14)      /* "Name" */
#define KEY_FILE      NAME(0x00571BB0)      /* "File" */
#define KEY_AVAILABLE NAME(0x00573A90)      /* "Availability" */
#define INFORMATION   NAME(0x00573B00)      /* "Information" */
#define NO_NAME       NAME(0x00573AB8)      /* "(no name)" */
#define WHOLE_TEXT    (-1)

/* A found file's name without its extension. */
static void file_title(char* to, const char* file)
{
    char* dot;

    text_copy_most(to, file, 0xFF);
    dot = game__strrchr(to, '.');
    if (dot)
        *dot = 0;
}

/* The next maker's place; it is counted once its bikes are in. */
static BYTE* maker_add(BYTE* self)
{
    session_makers(self) = ui_realloc(session_makers(self),
                                      (session_maker_count(self) + 1) * MAKER_SIZE);
    return maker_at(self, session_maker_count(self));
}

/* A bike of the maker being read; `own` marks one of the player's own textures. */
static BYTE* bike_add(BYTE* self, DWORD own)
{
    BYTE* bike;

    session_bikes(self) = ui_realloc(session_bikes(self),
                                     (session_bike_count(self) + 1) * BIKE_SIZE);
    bike = bike_at(self, session_bike_count(self));
    FIELD(bike, BIKE_MAKER, int) = session_maker_count(self);
    FIELD(bike, BIKE_OWN, DWORD) = own;
    return bike;
}

static BYTE* rider_add(BYTE* self, DWORD own)
{
    BYTE* rider;

    session_riders(self) = ui_realloc(session_riders(self),
                                      (session_rider_count(self) + 1) * RIDER_SIZE);
    rider = rider_at(self, session_rider_count(self));
    FIELD(rider, RIDER_OWN, DWORD) = own;
    return rider;
}

/*
    The makers and their bikes from bikes.pb, then the player's own bike textures (Bikes\*.tga)
    under a "(User Map)" maker; the riders from riders.pb, then the player's own (Riders\*.tga).
*/
PORT(0x0049B7D0)
void __fastcall session_load_bikes(BYTE* self)
{
    param_file params;
    WIN32_FIND_DATAA found;
    HANDLE search;
    void* file;
    char text[0x100];
    char name[0x104];
    char look[0x104];
    char maker_name[0x104];
    char model[0x104];
    char ui_model[0x104];
    int count, bikes, available, i, j;

    param_construct(&params);
    file = file_new(0x3A1);
    /* The makers are freed but not forgotten (the original's); its caller has cleared them. */
    if (session_makers(self))
        game_FUN_004a4450(session_makers(self), KRUSTYUI_CPP, 0x3A6);
    if (session_bikes(self))
        game_FUN_004a4450(session_bikes(self), KRUSTYUI_CPP, 0x3A7);
    if (session_riders(self))
        game_FUN_004a4450(session_riders(self), KRUSTYUI_CPP, 0x3A8);
    session_bikes(self) = NULL;
    session_riders(self) = NULL;
    session_rider_count(self) = 0;
    session_bike_count(self) = 0;
    session_maker_count(self) = 0;

    if (game_FUN_00461ec0(file, NULL, NAME(0x00573B0C), READ_MODE, 0)) {        /* bikes.pb */
        param_open(&params, NULL, file, 0, TRUE);
        param_select(&params, NULL, INFORMATION);
        param_int(&params, NULL, NAME(0x00573AE8), 0, &count);          /* NumberOfManufacturers */
        for (i = 1; i <= count; i++) {
            BYTE* maker;

            game_FUN_0053a11f(text, NAME(0x00573AD4), i);                   /* Manufacturer_%02d */
            param_select(&params, NULL, text);
            param_int(&params, NULL, NAME(0x00573AC4), 0, &bikes);              /* NumberOfBikes */
            param_text(&params, NULL, KEY_NAME, NO_NAME, maker_name, WHOLE_TEXT);
            param_text(&params, NULL, NAME(0x00573AAC), NO_TEXT, ui_model, WHOLE_TEXT);
            param_text(&params, NULL, NAME(0x00573AA0), NO_TEXT, model, WHOLE_TEXT);
            param_int(&params, NULL, KEY_AVAILABLE, 1, &available);
            maker = maker_add(self);
            text_copy_most((char*) maker, maker_name, 63);
            text_copy_most((char*) maker + MAKER_MODEL, model, 63);
            text_copy_most((char*) maker + MAKER_UI_MODEL, ui_model, 63);
            FIELD(maker, AVAILABILITY, int) = available;
            for (j = 1; j <= bikes; j++) {
                BYTE* bike;

                game_FUN_0053a11f(text, NAME(0x00573A80), j);                   /* BikeName_%02d */
                param_text(&params, NULL, text, NO_NAME, name, WHOLE_TEXT);
                game_FUN_0053a11f(text, NAME(0x00573A6C), j);                /* BikeTexture_%02d */
                param_text(&params, NULL, text, NO_TEXT, look, WHOLE_TEXT);
                bike = bike_add(self, FALSE);
                text_copy_most((char*) bike + BIKE_NAME, name, 63);
                text_copy_most((char*) bike + BIKE_LOOK, look, 63);
                game_FUN_0053a11f(text, NAME(0x00573A58), j);             /* BikeEngineSize_%02d */
                param_int(&params, NULL, text, 250, &FIELD(bike, BIKE_ENGINE, int));
                game_FUN_0053a11f(text, NAME(0x00573A44), j);             /* BikeFourStroke_%02d */
                param_int(&params, NULL, text, 0, &FIELD(bike, BIKE_FOUR_STROKE, int));
                session_bike_count(self)++;
            }
            session_maker_count(self)++;
        }
    }

    /* The player's own bikes keep whatever engine size and stroke the reallocation left. */
    search = FindFirstFileA(NAME(0x00573A38), &found);                            /* Bikes\*.tga */
    if (search != INVALID_HANDLE_VALUE) {
        BYTE* maker = maker_add(self);

        text_copy_most((char*) maker, NAME(0x00573A2C), 63);                       /* (User Map) */
        text_copy_most((char*) maker + MAKER_MODEL, NAME(0x00573A20), 63);         /* yamaha.mcf */
        text_copy_most((char*) maker + MAKER_UI_MODEL, NAME(0x00573A10), 63);    /* UIYamaha.mcf */
        FIELD(maker, AVAILABILITY, int) = 1;
        do {
            BYTE* bike;

            file_title(text, found.cFileName);
            bike = bike_add(self, TRUE);
            text_copy_most((char*) bike + BIKE_NAME, text, 63);
            game_FUN_0053a11f(text, NAME(0x00573A04), found.cFileName);              /* Bikes\%s */
            text_copy_most((char*) bike + BIKE_LOOK, text, 63);
            session_bike_count(self)++;
        } while (FindNextFileA(search, &found));
        session_maker_count(self)++;
        FindClose(search);
    }

    if (game_FUN_00461ec0(file, NULL, NAME(0x005739F8), READ_MODE, 0)) {       /* riders.pb */
        param_open(&params, NULL, file, 0, TRUE);
        param_select(&params, NULL, INFORMATION);
        param_int(&params, NULL, NAME(0x005739E8), 0, &count);                 /* NumberOfRiders */
        for (i = 1; i <= count; i++) {
            BYTE* rider;

            game_FUN_0053a11f(text, NAME(0x005739DC), i);                          /* Rider_%02d */
            param_select(&params, NULL, text);
            param_text(&params, NULL, KEY_NAME, NO_NAME, name, WHOLE_TEXT);
            param_text(&params, NULL, KEY_FILE, NO_TEXT, look, WHOLE_TEXT);
            param_int(&params, NULL, KEY_AVAILABLE, 1, &available);
            rider = rider_add(self, FALSE);
            text_copy_most((char*) rider, name, 63);
            text_copy_most((char*) rider + RIDER_LOOK, look, 0x7F);
            FIELD(rider, AVAILABILITY, int) = available;
            session_rider_count(self)++;
        }
    }
    file_delete(file);

    search = FindFirstFileA(NAME(0x005739CC), &found);                           /* Riders\*.tga */
    if (search != INVALID_HANDLE_VALUE) {
        do {
            BYTE* rider;

            file_title(text, found.cFileName);
            rider = rider_add(self, TRUE);
            text_copy_most((char*) rider, text, 63);
            game_FUN_0053a11f(text, NAME(0x005739C0), found.cFileName);             /* Riders\%s */
            text_copy_most((char*) rider + RIDER_LOOK, text, 0x7F);
            FIELD(rider, AVAILABILITY, int) = 1;
            session_rider_count(self)++;
        } while (FindNextFileA(search, &found));
        FindClose(search);
    }
    param_destruct(&params);
}

/* ---- The computer riders ---- */

#define rider_names  ((char (*)[16]) 0x006905D8)    /* the menu words double as their names */
#define engine_sizes ((const int*) 0x00572BEC)      /* 125, 250, 400, 500, 600 */
#define RAND_SCALE   (1.0f / 32768)
#define NEVER        (-1)                           /* in a taken list: not to be picked */

/* A random index below `range` (rand / 32768 * range, cut): `range` itself never comes up. */
static int random_index(int range)
{
    return (int) (game__rand() * (double) RAND_SCALE * range);
}

/* The next free slot after `from`, wrapping; `from` again when there is none. */
static int next_free(const int* taken, int from, int count)
{
    int i = from;

    do {
        if (++i == count)
            i = 0;
    } while (i != from && taken[i]);
    return i;
}

/*
    An engine size's class: 0 up to 125cc, 4 from 600cc, else the first k of 1-3 with the size
    strictly between sizes k-1 and k+1, and 1 when none is (the windows overlap; kept).
*/
static int engine_class(int size)
{
    int k;

    if (size <= engine_sizes[0])
        return 0;
    if (size >= engine_sizes[4])
        return 4;
    for (k = 1; k < 4; k++) {
        if (size > engine_sizes[k - 1] && size < engine_sizes[k + 1])
            return k;
    }
    return 1;
}

/* Bikes of the player's class, or the class paired with it (1 and 2, 3 and 4), race him. */
static BOOL class_matches(int player, int bike)
{
    return bike == player || (player == 1 && bike == 2) || (player == 2 && bike == 1)
           || (player == 3 && bike == 4) || (player == 4 && bike == 3);
}

/* A free bike picked at random stays: class 1 four times in five, class 2 one in five, others
   always. */
static BOOL bike_kept(BYTE* self, int pick)
{
    double chance = game__rand() * (double) RAND_SCALE;

    switch (engine_class(FIELD(bike_at(self, pick), BIKE_ENGINE, int))) {
    case 1:
        return chance > 0.2f;
    case 2:
        return !(chance > 0.2f);
    default:
        return TRUE;    /* classes 3 and 4 are compared with 0.5, and the result ignored */
    }
}

/* `count` different names for the computer riders, at random. */
PORT(0x0049C610)
void __fastcall session_pick_names(BYTE* self, void* edx_unused, const char** names, int count)
{
    int* taken = game_FUN_004a45b0(MENU_WORDS, sizeof(int), KRUSTYUI_CPP, 0x458);
    int i;

    (void) self;
    (void) edx_unused;
    game_FUN_0053a27e(game_FUN_004c41d0());
    for (i = 0; i < count; i++) {
        int pick = (int) (game__rand() * (double) RAND_SCALE * 36.0f);

        if (pick >= MENU_WORDS - 1)
            pick = MENU_WORDS - 1;
        if (taken[pick])
            pick = next_free(taken, pick, MENU_WORDS);
        taken[pick] = 1;
        names[i] = rider_names[pick];
    }
    game_FUN_004a4450(taken, KRUSTYUI_CPP, 0x47D);
}

/*
    A bike near the player's class and a rider for each of `count` computer riders, from those
    everyone may ride. With one left, or none, the pick never ends (the original's).
*/
PORT(0x0049C6C0)
void __fastcall session_pick_opponents(BYTE* self, void* edx_unused, int* bikes, int count,
                                       int* riders)
{
    int* bike_taken = game_FUN_004a45b0(session_bike_count(self), sizeof(int), KRUSTYUI_CPP,
                                        0x484);
    int* rider_taken = game_FUN_004a45b0(session_rider_count(self), sizeof(int), KRUSTYUI_CPP,
                                         0x485);
    int player = engine_class(FIELD(game_main, 0x1550, int));
    int i;

    (void) edx_unused;
    game_FUN_0053a27e(game_FUN_004c41d0());
    for (i = 0; i < session_rider_count(self); i++) {
        if (FIELD(rider_at(self, i), AVAILABILITY, int) != FOR_EVERYONE)
            rider_taken[i] = NEVER;
    }
    for (i = 0; i < session_bike_count(self); i++) {
        BYTE* bike = bike_at(self, i);

        if (FIELD(maker_at(self, FIELD(bike, BIKE_MAKER, int)), AVAILABILITY, int)
                != FOR_EVERYONE
            || !class_matches(player, engine_class(FIELD(bike, BIKE_ENGINE, int))))
            bike_taken[i] = NEVER;
    }
    for (i = 0; i < count; i++) {
        int pick;

        do
            pick = random_index(session_bike_count(self) - 1);
        while (bike_taken[pick] == NEVER);
        if (bike_taken[pick] || !bike_kept(self, pick))
            pick = next_free(bike_taken, pick, session_bike_count(self));
        bike_taken[pick] = 1;
        bikes[i] = pick;

        do
            pick = random_index(session_rider_count(self) - 1);
        while (rider_taken[pick] == NEVER);
        if (rider_taken[pick])
            pick = next_free(rider_taken, pick, session_rider_count(self));
        rider_taken[pick] = 1;
        riders[i] = pick;
    }
    game_FUN_004a4450(bike_taken, KRUSTYUI_CPP, 0x509);
    game_FUN_004a4450(rider_taken, KRUSTYUI_CPP, 0x50A);
}

/* The session's destructor: its tables are freed, but not forgotten. */
PORT(0x0049CA40)
void __fastcall session_destruct(BYTE* self)
{
    *(void***) self = SESSION_VTABLE;
    session_drop_scene(self);
    if (session_makers(self))
        game_FUN_004a4450(session_makers(self), KRUSTYUI_CPP, 0x510);
    if (session_bikes(self))
        game_FUN_004a4450(session_bikes(self), KRUSTYUI_CPP, 0x511);
    if (session_riders(self))
        game_FUN_004a4450(session_riders(self), KRUSTYUI_CPP, 0x512);
    if (session_tracks(self))
        game_FUN_004a4450(session_tracks(self), KRUSTYUI_CPP, 0x513);
    object_destruct(self);
}

/* The loading screen's progress bar, a step on. */
PORT(0x0049CB00)
void __fastcall session_progress(BYTE* self)
{
    BYTE* bar;

    if (!session_loading(self))
        return;
    bar = dialog_find_control(session_loading(self), NULL, NAME(0x0056FDF4), 0); /* ProgressBar */
    if (bar)
        progress_step(bar, NULL, TRUE);
}

/* ---- Looks ---- */

#define RES_PATH   NAME(0x0056C7DC)         /* "%s\%s" */
#define RES_FOLDER NAME(0x0056C7E4)         /* "Res" */

/* `from` into `to` of `size` bytes, ended within it. */
static void text_copy_fit(char* to, const char* from, size_t size)
{
    size_t length;

    game__strncpy(to, from, size);
    length = text_length(from);
    to[length < size - 1 ? length : size - 1] = 0;
}

/*
    Whether a look is there: loaded already, or as a file (under Res when it names no folder).
    `out` gets the name, or the path it was found at.
*/
static BOOL look_find(const char* wanted, char* out, size_t size, int line)
{
    char path[0x80];
    void* file;

    text_copy_fit(out, wanted, size);
    if (game_FUN_004ee0f0(game_resources, NULL, wanted, 1))
        return TRUE;
    if (!game__strrchr(wanted, '\\'))
        game_FUN_0053a11f(path, RES_PATH, RES_FOLDER, wanted);
    else
        text_copy_most(path, wanted, 0x7F);
    file = file_new(line);
    if (game_FUN_00461ec0(file, NULL, path, READ_MODE, 0)) {
        file_delete(file);
        text_copy_fit(out, path, size);
        return TRUE;
    }
    file_delete(file);
    return FALSE;
}

/* A bike look: `wanted` when it is there, else a random stock bike's (and FALSE). */
PORT(0x0049CB30)
BOOL __fastcall session_bike_look(BYTE* self, void* edx_unused, const char* wanted, char* out,
                                  size_t size)
{
    BYTE* bike;

    (void) edx_unused;
    if (look_find(wanted, out, size, 0x54F))
        return TRUE;
    do
        bike = bike_at(self, random_index(session_bike_count(self) - 1));
    while (FIELD(bike, BIKE_OWN, DWORD)
           || FIELD(maker_at(self, FIELD(bike, BIKE_MAKER, int)), AVAILABILITY, int)
                  != FOR_EVERYONE);
    text_copy_fit(out, (const char*) bike + BIKE_LOOK, size);
    return FALSE;
}

/* A rider look: `wanted` when it is there, else a random stock rider's (and FALSE). */
PORT(0x0049CDC0)
BOOL __fastcall session_rider_look(BYTE* self, void* edx_unused, const char* wanted, char* out,
                                   size_t size)
{
    BYTE* rider;

    (void) edx_unused;
    if (look_find(wanted, out, size, 0x579))
        return TRUE;
    do
        rider = rider_at(self, random_index(session_rider_count(self) - 1));
    while (FIELD(rider, RIDER_OWN, DWORD) || FIELD(rider, AVAILABILITY, int) != FOR_EVERYONE);
    text_copy_fit(out, (const char*) rider + RIDER_LOOK, size);
    return FALSE;
}

/* ---- Tracks and best results ---- */

/* A track added to the session's list; its index. */
PORT(0x0049D040)
int __fastcall session_add_track(BYTE* self, void* edx_unused, DWORD first, DWORD kind, DWORD id,
                                 const char* name, DWORD fourth, DWORD fifth)
{
    BYTE* track;

    (void) edx_unused;
    session_tracks(self) = ui_realloc(session_tracks(self),
                                      (session_track_count(self) + 1) * TRACK_SIZE);
    track = session_tracks(self) + session_track_count(self) * TRACK_SIZE;
    FIELD(track, 0x0, DWORD) = first;
    FIELD(track, 0x4, DWORD) = kind;
    FIELD(track, 0x8, DWORD) = id;
    text_copy_most((char*) track + 0x14, name, 0x3F);
    FIELD(track, 0xC, DWORD) = fourth;
    FIELD(track, 0x10, DWORD) = fifth;
    return session_track_count(self)++;
}

PORT(0x0049D150)
void __fastcall session_drop_tracks(BYTE* self)
{
    if (session_tracks(self))
        game_FUN_004a4450(session_tracks(self), KRUSTYUI_CPP, 0x5A3);
    session_tracks(self) = NULL;
    session_track_count(self) = 0;
}

#define race_settings     (game_main + 0x2D70)
#define race_kind         FIELD(game_main, 0x2D74, int)     /* 0 quarry ... 4 tag, 5 enduro */
#define race_trees        FIELD(race_settings, 0x10, int)
#define race_laps         FIELD(race_settings, 0x20, int)
#define race_random_gates FIELD(race_settings, 0x2C, BYTE)
#define race_variant      FIELD(race_settings, 0x34, BYTE)
#define race_track        ((const char*) race_settings + 0x36)
#define race_minutes      FIELD(race_settings, 0x140, float)
#define game_stats        FIELD(game_main, 0x3400, BYTE*)
#define TRACK_NUMBERED    NAME(0x00571C64)                  /* "%s%d" */

/* The best results' tables: a track's opened, a player's improvement, the track kept. */
void  __fastcall game_FUN_00524700(BYTE* stats, void* edx_unused, short kind, const char* track);
BOOL  __fastcall game_FUN_00524a10(BYTE* stats, void* edx_unused, short kind, int player);
void  __fastcall game_FUN_005248b0(BYTE* stats, void* edx_unused, short kind, const char* track,
                                   DWORD value);
void  __fastcall game_FUN_00528bb0(char* profile);                       /* save it */
DWORD __fastcall game_FUN_00529750(char* profile);

__declspec(noinline) void __fastcall session_record(BYTE* self, void* edx_unused, int table);

/* After a race: table 0 always, 1 for 5 laps (or minutes), 2 for 10; Baja with random gates not. */
PORT(0x0049D180)
void __fastcall session_record_race(BYTE* self)
{
    if (race_kind == 1 && race_random_gates)
        return;
    session_record(self, NULL, 0);
    if (race_kind != 0 && race_kind != 4) {
        if (race_laps == 5)
            session_record(self, NULL, 1);
        if (race_laps == 10)
            session_record(self, NULL, 2);
    } else {
        if (race_minutes == 5.0f)
            session_record(self, NULL, 1);
        if (race_minutes == 10.0f)
            session_record(self, NULL, 2);
    }
}

/* The race in best-results table `table`, when trees were on and a player improved on it. */
PORT(0x0049D220)
void __fastcall session_record(BYTE* self, void* edx_unused, int table)
{
    char track[0x80];
    BOOL improved = FALSE;
    int i;

    (void) self;
    (void) edx_unused;
    if (!race_trees)
        return;
    FIELD(game_stats, 0, int) = table;
    if (race_kind == 1 || race_kind == 5)
        game_FUN_0053a11f(track, TRACK_NUMBERED, race_track, race_variant);
    else
        text_copy_most(track, race_track, 0x7F);
    game_FUN_00524700(game_stats, NULL, (short) race_kind, track);
    if (FIELD(game_main, 0x18, int) == 1) {
        improved = game_FUN_00524a10(game_stats, NULL, (short) race_kind, 0);
    } else {
        for (i = 0; i < FIELD(game_main, 0x18, int); i++) {
            if (game_FUN_00524a10(game_stats, NULL, (short) race_kind, i))
                improved = TRUE;
        }
    }
    if (improved)
        game_FUN_005248b0(game_stats, NULL, (short) race_kind, track,
                          game_FUN_00529750(game_profile));
}

/* The GUIManager's callback (given it in session_setup): the profile saved. */
PORT(0x0049D370)
void __cdecl session_save_profile(void)
{
    game_FUN_00528bb0(game_profile);
}
