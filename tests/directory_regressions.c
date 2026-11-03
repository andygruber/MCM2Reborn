#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dirlist.h"
#define DIRLIST_CPP "directory regression"
#define DIR_LIST_VTABLE ((void**)1)
#define NO_TEXT ""
#define HERE "."
#define PARENT ".."
static int checks,live,allocations,fail_at,firsts,closes,position,which,read_error;
static BOOL mocked=TRUE;
static int amounts[2];
static char names[2][160][MAX_PATH];
#define CHECK(x) do {checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
void* __cdecl game_FUN_004a4600(size_t size,const char* file,int line) {
    size_t* p;
    if(++allocations==fail_at)return NULL;
    p=malloc(size+sizeof(size_t)+sizeof(DWORD));CHECK(p!=NULL);*p=size;
    {DWORD guard=0x1234ABCD;memcpy((BYTE*)(p+1)+size,&guard,sizeof guard);}
    live++;return p+1;
}
void __cdecl game_FUN_004a46b0(void* object) {
    size_t* p;DWORD guard;if(!object)return;
    p=(size_t*)object-1;memcpy(&guard,(BYTE*)object+*p,sizeof guard);
    CHECK(guard==0x1234ABCD);live--;free(p);
}
char* __cdecl game__strncpy(char* to,const char* from,size_t count){return strncpy(to,from,count);}
static int game_FUN_0053b6e0(const char* a,const char* b){return _stricmp(a,b);}
static int __cdecl compare(const void* a,const void* b){return _stricmp(a,b);}
static void game_FUN_00539a96(void* p,size_t count,size_t size,int (__cdecl* ignored)(const void*,const void*)){qsort(p,count,size,compare);}
#define dir_entries_by_name compare
static void found(WIN32_FIND_DATAA* data){ZeroMemory(data,sizeof *data);strcpy(data->cFileName,names[which][position]);data->dwFileAttributes=FILE_ATTRIBUTE_NORMAL;}
static HANDLE WINAPI test_first(LPCSTR path,LPWIN32_FIND_DATAA data){
    if(!mocked)return FindFirstFileA(path,data);
    firsts++;position=0;which=strstr(path,"shared")!=NULL;
    if(!strstr(path,"*.env")||!amounts[which]){SetLastError(ERROR_FILE_NOT_FOUND);return INVALID_HANDLE_VALUE;}
    found(data);return (HANDLE)1;
}
static BOOL WINAPI test_next(HANDLE h,LPWIN32_FIND_DATAA data){
    if(!mocked)return FindNextFileA(h,data);
    if(++position==read_error){SetLastError(ERROR_ACCESS_DENIED);return FALSE;}
    if(position>=amounts[which]){SetLastError(ERROR_NO_MORE_FILES);return FALSE;}
    found(data);return TRUE;
}
static BOOL WINAPI test_close(HANDLE h){if(!mocked)return FindClose(h);closes++;return TRUE;}
#define FindFirstFileA test_first
#define FindNextFileA test_next
#define FindClose test_close
#include "directory_under_test.inc"
static void reset(void){CHECK(!live);allocations=fail_at=firsts=closes=read_error=0;amounts[0]=amounts[1]=0;}
static dir_list list(void){dir_list d={0};strcpy(d.folder,"local");strcpy(d.pattern,"*.env");d.files_too=TRUE;return d;}
static merged_list merged(void){merged_list m={0};strcpy(m.hard_disk,"local");strcpy(m.cd,"shared");strcpy(m.base.pattern,"*.env");return m;}
static void actual_directories(void){
    char temp[MAX_PATH],root[MAX_PATH],a[MAX_PATH],b[MAX_PATH],path[MAX_PATH];
    const char* files[2][3]={{"a.env","shared.env","local.env"},{"a.env","SHARED.ENV","z.env"}};
    merged_list m={0};int side,i;
    CHECK(GetTempPathA(MAX_PATH,temp)>0);CHECK(GetTempFileNameA(temp,"mcm",0,root));CHECK(DeleteFileA(root));
    CHECK(CreateDirectoryA(root,NULL));sprintf(a,"%s\\local",root);sprintf(b,"%s\\shared",root);
    CHECK(CreateDirectoryA(a,NULL));CHECK(CreateDirectoryA(b,NULL));
    for(side=0;side<2;side++)for(i=0;i<3;i++){
        HANDLE f;sprintf(path,"%s\\%s",side?b:a,files[side][i]);
        f=CreateFileA(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
        CHECK(f!=INVALID_HANDLE_VALUE);CloseHandle(f);
    }
    strcpy(m.hard_disk,a);strcpy(m.cd,b);strcpy(m.base.pattern,"*.env");mocked=FALSE;
    CHECK(merged_list_scan(&m));CHECK(m.base.count==4);CHECK(!strcmp(m.base.entries[2].name,"shared.env"));
    CHECK(m.base.entries[2].attributes==DRIVE_FIXED);CHECK(m.base.entries[3].attributes==DRIVE_CDROM);
    dir_list_destruct(&m.base);mocked=TRUE;
    for(side=0;side<2;side++)for(i=0;i<3;i++){
        sprintf(path,"%s\\%s",side?b:a,files[side][i]);CHECK(DeleteFileA(path));
    }
    CHECK(RemoveDirectoryA(a));CHECK(RemoveDirectoryA(b));CHECK(RemoveDirectoryA(root));CHECK(!live);
}
int main(void){
    dir_list d;merged_list m;int i;
    reset();d=list();amounts[0]=160;
    for(i=0;i<160;i++)sprintf(names[0][i],"track%03d.env",i);
    CHECK(dir_list_scan(&d));CHECK(d.count==160);CHECK(firsts==1);CHECK(closes==1);
    CHECK(!strcmp(d.entries[159].name,"track159.env"));dir_list_destruct(&d);
    for(i=1;i<=4;i++){
        reset();d=list();amounts[0]=160;fail_at=i;CHECK(!dir_list_scan(&d));
        CHECK(!d.count&&!d.entries);CHECK(closes==1);CHECK(!live);
    }
    reset();d=list();amounts[0]=160;read_error=20;CHECK(!dir_list_scan(&d));CHECK(!d.count&&!d.entries);CHECK(!live);
    reset();d=list();memset(d.folder,'x',MAX_PATH-2);d.folder[MAX_PATH-2]=0;
    CHECK(!dir_list_scan(&d));CHECK(!firsts);CHECK(!d.entries);
    reset();d=list();CHECK(dir_list_scan(&d));CHECK(!d.count&&!d.entries);
    reset();d=list();d.folder[0]=0;amounts[0]=1;CHECK(dir_list_scan(&d));CHECK(d.count==1);dir_list_destruct(&d);
    reset();amounts[0]=3;amounts[1]=3;
    strcpy(names[0][0],"shared.env");strcpy(names[0][1],"LOCAL.env");strcpy(names[0][2],"a.env");
    strcpy(names[1][0],"SHARED.ENV");strcpy(names[1][1],"z.env");strcpy(names[1][2],"a.env");
    m=merged();CHECK(merged_list_scan(&m));CHECK(m.base.count==4);
    CHECK(!strcmp(m.base.entries[0].name,"a.env"));CHECK(!strcmp(m.base.entries[1].name,"LOCAL.env"));
    CHECK(!strcmp(m.base.entries[2].name,"shared.env"));CHECK(!strcmp(m.base.entries[3].name,"z.env"));
    CHECK(m.base.entries[2].attributes==DRIVE_FIXED);CHECK(m.base.entries[3].attributes==DRIVE_CDROM);
    CHECK(merged_list_scan(&m));CHECK(m.base.count==4);dir_list_destruct(&m.base);CHECK(!live);
    for(i=1;i<=3;i++){
        reset();amounts[0]=amounts[1]=2;m=merged();fail_at=i;
        CHECK(!merged_list_scan(&m));CHECK(!m.base.count&&!m.base.entries);CHECK(!live);
    }
    reset();amounts[0]=2;m=merged();CHECK(merged_list_scan(&m));CHECK(m.base.count==2);dir_list_destruct(&m.base);
    reset();amounts[1]=2;m=merged();CHECK(merged_list_scan(&m));CHECK(m.base.count==2);dir_list_destruct(&m.base);
    reset();m=merged();CHECK(merged_list_scan(&m));CHECK(!m.base.count&&!m.base.entries);
    actual_directories();CHECK(!live);printf("PASS %d directory checks: growth, duplicates, ordering, source tags, missing folders and failure cleanup\n",checks);
    return 0;
}
