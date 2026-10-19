/* Native regression checks use production AI/pad/radar code and menu helpers,
   with the game's objects and hardware responses supplied by small fixtures. */
#define DIRECTINPUT_VERSION 0x0800
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdarg.h>
#include "runtime.h"
#include <mmsystem.h>
#include <dinput.h>
#include "gui.h"
#include "../code/dev/dev.h"
#include "../code/dev/padstate.h"
static DWORD clock_ms = 1000;
static DWORD WINAPI test_time(void) { return clock_ms; }
#define timeGetTime test_time
#include "../code/mcm2/bikeai.c"
#include "../code/dev/pad.c"
#include "../code/dev/trackfilter.c"
#include "../code/mcm2/overlays.c"
#include "../code/mcm2/racebikes.c"

dev_switches dev_settings = {0};
reborn_options g_reborn = {0};
static int checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
void log_line(const char* fmt, ...) { (void) fmt; }
FARPROC api(const char* dll, const char* name) { return NULL; }
BOOL write_protected(void* to, const void* from, SIZE_T size) { return TRUE; }
float square_root(float x) { return sqrtf(x); }
float exp_minus(float x) { return expf(-x); }
void __fastcall game_FUN_00500690(BYTE* object) { }
void __fastcall game_orig_FUN_00414000(BYTE* self, void* unused, float dt) { FIELD(self,0x800,int)++; }
int __fastcall game_orig_vf_89_48F390(BYTE* self, void* unused, DWORD a) { return 0; }
int __fastcall game_orig_FUN_0041a770(BYTE* self, void* unused, DWORD a,DWORD b,DWORD c,DWORD d,DWORD e)
{ CHECK(a==1 && b==2 && c==3 && d==4 && e==5); return 7; }
void hud_overlays_drawn(void) { }
void leaders_watch(void) { }
int __fastcall game_orig_vf_14_51DFC0(BYTE* self) { return 1; }
int __fastcall game_orig_vf_14_51F000(BYTE* self) { return 1; }
int __fastcall game_orig_vf_14_521370(BYTE* self) { return 1; }
static int radar_seen;
void __fastcall game_orig_FUN_00521520(BYTE* self, void* unused, int kind) { radar_seen=FIELD(self,0x160,int); }

static BOOL connected[4];
static xinput_state hardware;
static DWORD WINAPI get_pad(DWORD user, xinput_state* state)
{ if (!connected[user]) return ERROR_DEVICE_NOT_CONNECTED; *state=hardware; return ERROR_SUCCESS; }
static void pad_next(void) { clock_ms += 1100; }
static void test_pad(void)
{
    float x,y;
    g_loaded=TRUE; g_get=get_pad; g_index=-1; g_sony=FALSE;
    CHECK(!pad_connected());
    connected[2]=TRUE; pad_next(); CHECK(pad_connected()); CHECK(g_index==2);
    hardware.buttons=0x2000; pad_next(); CHECK(pad_back_down()); CHECK(!pad_start_down());
    hardware.buttons=0x1000; pad_next(); CHECK(pad_accept_down());
    hardware.buttons=0x0009; pad_next(); CHECK(pad_menu_axes(&x,&y)); CHECK(x>0 && y<0);
    hardware.buttons=0; hardware.left_x=-32768; hardware.left_y=-32768;
    pad_next(); CHECK(pad_menu_axes(&x,&y)); CHECK(x<0 && y>0);
    connected[2]=FALSE; pad_next(); CHECK(!pad_connected()); CHECK(!pad_accept_down()); CHECK(!pad_back_down());
    connected[0]=TRUE; ZeroMemory(&hardware,sizeof hardware); pad_next(); CHECK(pad_connected()); CHECK(g_index==0);
    puts("PASS controller startup, unplug/reconnect, buttons, D-pad and sticks");
}
static void test_sony(void)
{
    DIJOYSTATE joy; xinput_state state;
    ZeroMemory(&joy,sizeof joy); joy.lRx=joy.lRy=-32767; joy.rgdwPOV[0]=(DWORD)-1;
    sony_pad_convert(&joy,&state); CHECK(!state.buttons && !state.left_trigger && !state.right_trigger);
    joy.rgbButtons[2]=0x80; sony_pad_convert(&joy,&state); CHECK(state.buttons==0x2000);
    joy.rgbButtons[2]=0; joy.rgbButtons[1]=0x80; joy.rgdwPOV[0]=4500;
    joy.lX=32767; joy.lY=-32767; joy.lZ=-32767; joy.lRz=32767; joy.lRx=32767;
    sony_pad_convert(&joy,&state); CHECK((state.buttons&0x1009)==0x1009);
    CHECK(state.left_x==32767 && state.left_y==32767 && state.right_x==-32767 && state.right_y==-32767);
    CHECK(state.left_trigger==255 && state.right_trigger==0);
    puts("PASS native PlayStation Circle/Cross, diagonal D-pad, axes and independent triggers");
}
static void test_ai(void)
{
    static BYTE bikes[29][0x900]; int race,i,frame;
    dev_settings.fix_ai_rate=30;
    for(race=0;race<20;race++) {
        for(i=0;i<32;i++) ai_bike_of((BYTE*)(UINT_PTR)(0x10000+race*0x20000+i*16));
        CHECK(race_load_bikes(NULL,NULL,1,2,3,4,5)==7);
        for(i=0;i<32;i++) CHECK(!g_ai_bikes[i].bike);
        ZeroMemory(bikes,sizeof bikes);
        for(frame=0;frame<288;frame++) {
            clock_ms+=7;
            for(i=0;i<29;i++) ai_bike_step(bikes[i],NULL,1.0f/144);
        }
        for(i=0;i<29;i++) CHECK(FIELD(bikes[i],0x800,int)>=59);
    }
    puts("PASS 20 successive 30-rider events at 144 FPS: every AI takes steps");
}
static void test_radar(void)
{
    BYTE radar[0x300]={0}; int n;
    for(n=0;n<=32;n++) {
        FIELD(radar,0x160,int)=n; radar_update_markers(radar,NULL,2);
        CHECK(radar_seen==(n>11?11:n)); CHECK(FIELD(radar,0x160,int)==n);
    }
    puts("PASS radar update caps all 0..32 rider counts and restores the real count");
}
static void test_tracks(void)
{
    CHECK(track_is_stock(0,"QUARRY05")); CHECK(!track_is_stock(0,"QUARRY06"));
    CHECK(track_is_stock(1,"Baja01")); CHECK(track_is_stock(2,"NAT12"));
    CHECK(!track_is_stock(2,"NAT13")); CHECK(!track_is_stock(2,"MCM1_Belas"));
    CHECK(track_is_stock(3,"sx17")); CHECK(!track_is_stock(3,"sx18"));
    CHECK(track_is_stock(4,"tag03")); CHECK(track_is_stock(5,"AIRFIELD"));
    CHECK(!track_is_stock(5,"airfield_custom")); CHECK(!track_is_stock(3,"sx"));
    CHECK(!track_is_stock(2,"nat01_custom")); CHECK(!track_is_stock(6,"nat01"));
    puts("PASS stock/custom classification for every event and strict stem matching");
}

/* Menu integration fixtures: production helper code is included by the runner. */
#define DLG_PARENT 0x2C
#define DLG_SHOWN 0xBC
#define DLG_MODAL 0x148
#define CTL_RECT 0x2C
#define CTL_DESIGN 0x3C
#define CTL_TYPE 0x5C
#define CTL_DIALOG 0xB8
#define SLIDER_RANGE 0x200
#define COMBO_OPEN 0x208
#define COMBO_LIST 0x1FC
#define UICONTROL_CLASS "UIControl"
static BYTE dialog_fixture[0x8000], controls_fixture[6][0x400];
static BYTE* controls[6]; static BOOL binding; static int clicks, list_updates;
static void walk_begin(BYTE* walk,void* unused,void* root,int depth,const char* kind) { FIELD(walk,0,int)=0; }
static BYTE* walk_next(BYTE* walk) { int i=FIELD(walk,0,int)++; return i<6?controls[i]:NULL; }
static void walk_end(BYTE* walk) { }
static BOOL takes_focus(BYTE* c) { return FIELD(c,0x6C,int)&&FIELD(c,0x70,int); }
BOOL pad_binding_active(void) { return binding; }
BOOL input_in_foreground(void) { return TRUE; }
BOOL pad_menus_active(void) { return TRUE; }
BYTE* __fastcall gui_manager_active_dialog(gui_manager* manager) { return dialog_fixture; }
int __fastcall gui_user_set_capture(gui_user* u,void* unused,BYTE* c,BYTE** old) { u->capture=u->focus=c; return 1; }
void __fastcall gui_user_apply_focus(gui_user* u,void* unused,BOOL force) { }
static int __fastcall can_focus(void* self) { return 1; }
static int __fastcall press(void* self,void* unused,DWORD a,DWORD b) { return 1; }
static int __fastcall release(void* self,void* unused,DWORD a,DWORD b) { clicks++; return 1; }
static int __fastcall stepped(void* self,void* unused,int* done) { list_updates++; return 1; }
static int list_selected(BYTE* self) { return FIELD(self,0x200,int); }
static int list_count(BYTE* self) { return FIELD(self,0x1EC,int); }
static int list_select_all(BYTE* self,void* unused,int n) { FIELD(self,0x200,int)=n; return 1; }
typedef int (__fastcall* list_stepped_slot)(void*,void*,int*);
typedef int (__fastcall* control_two)(void*,void*,DWORD,DWORD);
#define slider_value(s) FIELD(s,0x300,int)
#define slider_steps(s) FIELD(s,0x1F0,unsigned)
#define slider_set(s,v,r) (FIELD(s,0x300,int)=(v))
static void control_tell(BYTE* s,void* event,DWORD kind) { list_updates++; }
static void combo_open(BYTE* s,void* unused,BOOL open) { FIELD(s,COMBO_OPEN,BOOL)=open; }
#include "menu_under_test.inc"
#define MENU_MOVIE 0x7F58
#include "movie_under_test.inc"
static int video_paused;
static int __fastcall pause_video(void* self,void* unused,DWORD paused) { video_paused+=(int)paused; return 1; }
static void test_movie(void)
{
    BYTE dialog[0x8000]={0},movie[0x200]={0},video[4]={0}; void* table[17]={0};
    menu_movie_pause(dialog,1); FIELD(dialog,MENU_MOVIE,BYTE*)=movie;
    menu_movie_pause(dialog,1); table[16]=(void*)pause_video; *(void***)video=table;
    FIELD(movie,0x1EC,void*)=video; menu_movie_pause(dialog,1); CHECK(video_paused==1);
    puts("PASS missing movie, missing decoder and valid menu preview video");
}
static void test_menu(void)
{
    static gui_manager manager; static gui_user user; static gui_input mouse;
    static void* table[80]; int i; BYTE* focus;
    manager.user_count=1; manager.users[0]=&user; manager.mouse=&mouse;
    FIELD(dialog_fixture,DLG_SHOWN,int)=1;
    table[0x78/4]=(void*)can_focus; table[0xDC/4]=(void*)press;
    table[0xE0/4]=(void*)release; table[0x108/4]=(void*)stepped;
    for(i=0;i<6;i++) {
        controls[i]=controls_fixture[i]; *(void***)controls[i]=table;
        FIELD(controls[i],CTL_DIALOG,BYTE*)=dialog_fixture;
        FIELD(controls[i],0x6C,int)=FIELD(controls[i],0x70,int)=1;
        FIELD(controls[i],CTL_TYPE,int)=1;
        SetRect(&FIELD(controls[i],CTL_RECT,RECT),100,100+i*100,400,180+i*100);
        FIELD(controls[i],CTL_DESIGN,RECT)=FIELD(controls[i],CTL_RECT,RECT);
    }
    hardware.buttons=2; hardware.left_x=hardware.left_y=0; pad_next(); pad_menu_tick(&manager);
    CHECK(user.focus==controls[0]); CHECK(pad_navigation_active);
    pad_next(); pad_menu_tick(&manager); CHECK(user.focus==controls[1]);
    focus=user.focus; hardware.buttons=0; pad_next(); pad_menu_tick(&manager); CHECK(user.focus==focus);
    hardware.buttons=0x1000; pad_next(); pad_menu_tick(&manager); CHECK(clicks==1);
    pad_next(); pad_menu_tick(&manager); CHECK(clicks==1);
    binding=TRUE; hardware.buttons=2; pad_next(); pad_menu_tick(&manager); CHECK(user.focus==focus);
    binding=FALSE; FIELD(focus,CTL_TYPE,int)=3; FIELD(focus,0x1EC,int)=3;
    hardware.buttons=0; pad_next(); pad_menu_tick(&manager);
    hardware.buttons=2; pad_next(); pad_menu_tick(&manager); CHECK(FIELD(focus,0x200,int)==1); CHECK(list_updates==1);
    FIELD(focus,CTL_TYPE,int)=8; FIELD(focus,0x1F0,unsigned)=10;
    hardware.buttons=8; pad_next(); pad_menu_tick(&manager); CHECK(FIELD(focus,0x300,int)==1);
    FIELD(focus,CTL_TYPE,int)=6; FIELD(focus,COMBO_LIST,BYTE*)=controls[5];
    FIELD(controls[5],0x1EC,int)=4;
    hardware.buttons=0; pad_next(); pad_menu_tick(&manager);
    hardware.buttons=8; pad_next(); pad_menu_tick(&manager); CHECK(FIELD(controls[5],0x200,int)==1);
    FIELD(focus,COMBO_OPEN,BOOL)=TRUE;
    hardware.buttons=2; pad_next(); pad_menu_tick(&manager); CHECK(FIELD(controls[5],0x200,int)==2);
    CHECK(pad_close_dropdown(focus)); CHECK(!FIELD(focus,COMBO_OPEN,BOOL));
    CHECK(!pad_close_dropdown(focus));
    hardware.buttons=0; mouse.x=25; pad_next(); pad_menu_tick(&manager); CHECK(!pad_navigation_active);
    FIELD(controls[2],0x70,int)=0; CHECK(!pad_focusable(controls[2]));
    { static BYTE parent[0x8000];
      FIELD(parent,DLG_SHOWN,int)=1; FIELD(parent,0x25,BYTE)=4;
      FIELD(dialog_fixture,DLG_PARENT,BYTE*)=parent;
      FIELD(dialog_fixture,DLG_MODAL,int)=1; CHECK(pad_focusable(controls[0]));
      FIELD(dialog_fixture,DLG_MODAL,int)=0; CHECK(!pad_focusable(controls[0]));
      FIELD(dialog_fixture,DLG_PARENT,BYTE*)=NULL;
    }
    FIELD(dialog_fixture,DLG_SHOWN,int)=0; CHECK(!pad_focusable(controls[0]));
    puts("PASS controller focus without startup joystick, A activation/debounce, rebinding, lists, sliders and mouse handoff");
}
int main(void)
{
    test_ai(); test_radar(); test_tracks(); test_movie(); test_pad(); test_sony(); test_menu();
    printf("%d native checks passed\n",checks); return 0;
}
