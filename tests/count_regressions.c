/* Exercise the production catalog counter against changing installed catalogs.
   Scene metadata/merged directories are fixtures; stock classification is production. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "dirlist.h"
#include "../code/dev/trackfilter.c"
static BYTE main_fixture[0x3400];
#undef game_main
#define game_main main_fixture
#define game_profile ((char*)main_fixture + 0x578)
#define game_resources NULL
#define PATH "%s/%s"
#define ENV_FILES "*.env"
#define ENV "env"
#define SCN "scn"
#define BAJA_FOLDER "Baja"
#define NATIONAL_FOLDER "National"
#define SX_FOLDER "SX"
#define ENDURO_FOLDER "Enduro"
#define CUSTOM_FOLDER FIELD(game_main,0xC18,const char*)
static const char* catalog[6][8] = {
    {"quarry01", "quarry_custom",NULL}, {"baja01","baja_custom",NULL},
    {"nat01","nat_custom",NULL}, {"sx01","sx_custom",NULL},
    {"tag01","tag_custom",NULL}, {"airfield","enduro_custom",NULL}
};
static int context_kind=3, hidden, reads, checks;
#define CHECK(x) do { ++checks; if(!(x)) {fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);} } while(0)
static int profile_event(char* p) {return context_kind;}
static char* profile_set_event(char* p,void* unused,int kind) {
    context_kind=kind;
    FIELD(game_main,0xC18,const char*)=kind==4?"Tag":"Quarry";
    return p;
}
static BOOL track_hidden(const char* name) {return hidden && !strcmp(name,"nat_custom");}
static int game_FUN_0053a11f(char* to,const char* fmt,...) {int n;va_list args;va_start(args,fmt);n=vsprintf(to,fmt,args);va_end(args);return n;}
merged_list* __fastcall merged_list_construct(merged_list* f) {ZeroMemory(f,sizeof *f);return f;}
void __fastcall merged_list_set_folders(merged_list* f,void* unused,const char* own,const char* shared) {strcpy(f->hard_disk,own);strcpy(f->cd,shared);}
void __fastcall dir_list_set_pattern(dir_list* d,void* unused,const char* pattern,BOOL files) {}
BOOL __fastcall merged_list_scan(merged_list* f) {
    int i;f->base.entries=calloc(8,sizeof(dir_entry));
    for(i=0;catalog[context_kind][i];i++) strcpy(f->base.entries[i].name,catalog[context_kind][i]);
    f->base.count=i;return TRUE;
}
BOOL __fastcall dir_list_title(dir_list* d,void* unused,int i,char* name) {
    if(i<0||i>=d->count)return FALSE;strcpy(name,d->entries[i].name);return TRUE;
}
BOOL __fastcall dir_list_current_title(dir_list* d,void* unused,char* name) {return dir_list_title(d,NULL,d->cursor,name);}
BOOL __fastcall dir_list_next_title(dir_list* d,void* unused,char* name) {return dir_list_title(d,NULL,++d->cursor,name);}
void __fastcall merged_list_destruct(merged_list* f) {free(f->base.entries);}
static BOOL profile_find_named_file(char* p,void* unused,const char* folder,const char* name,const char* ext,char* path) {strcpy(path,folder);return TRUE;}
static void game_FUN_004ee910(BYTE* scene,void* unused,const char* path) {}
static void game_FUN_004eeda0(BYTE* scene,void* unused,char* title,const char* name,int variant,const char* ext,void* a,void* b) {
    reads++;FIELD(scene,0x390,int)=track_is_stock(context_kind,name)?3:2;
}
void __fastcall game_FUN_004ee5c0(void* resources,void* unused,void* archive) {}
static void game_FUN_00461cd0(void* archive) {}
void __cdecl game_FUN_004a46b0(void* p) {free(p);}
#include "counts_under_test.inc"
int main(void) {
    int k,read_before;
    CHECK(track_counts_refresh());CHECK(context_kind==3);
    for(k=0;k<6;k++) CHECK(g_track_counts[k][0]==g_track_counts[k][1]+g_track_counts[k][2]);
    CHECK(g_track_counts[0][0]==4);CHECK(g_track_counts[0][1]==2);CHECK(g_track_counts[0][2]==2);
    CHECK(g_track_counts[1][0]==5);CHECK(g_track_counts[5][0]==5);
    read_before=reads;CHECK(!track_counts_refresh());CHECK(reads==read_before);CHECK(g_track_counts[0][0]==4);
    catalog[3][2]="another_sx";CHECK(track_counts_refresh());CHECK(g_track_counts[3][0]==3);CHECK(g_track_counts[3][2]==2);
    catalog[3][2]=NULL;CHECK(track_counts_refresh());CHECK(g_track_counts[3][0]==2);
    hidden=1;CHECK(track_counts_refresh());CHECK(g_track_counts[2][0]==1);CHECK(g_track_counts[2][2]==0);
    hidden=0;CHECK(track_counts_refresh());CHECK(g_track_counts[2][0]==2);
    catalog[5][2]="new_enduro";CHECK(track_counts_refresh());CHECK(g_track_counts[5][0]==7);CHECK(g_track_counts[0][0]==5);
    CHECK(!track_counts_refresh());CHECK(g_track_counts[0][0]==5);CHECK(context_kind==3);
    printf("PASS %d catalog checks: all/base/custom counts, variants, add/remove, hidden tracks, cache and profile restoration\n",checks);
    return 0;
}
