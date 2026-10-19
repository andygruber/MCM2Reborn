#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../code/dev/inputfix.c"

dev_switches dev_settings = {0};
pad_options g_pad = {0};
create_fn _imp__DirectInputCreateEx = NULL;
static BOOL menus=TRUE, start, back, binding, front=TRUE, close_dropdown;
static int driver_reads;
static HWND WINAPI foreground(void) { return front?(HWND)1:NULL; }
static DWORD WINAPI window_process(HWND w,DWORD* owner) { *owner=1; return 1; }
static DWORD WINAPI process(void) { return 1; }
FARPROC api(const char* dll,const char* name)
{
    if (!strcmp(name,"GetForegroundWindow")) return (FARPROC)foreground;
    if (!strcmp(name,"GetWindowThreadProcessId")) return (FARPROC)window_process;
    if (!strcmp(name,"GetCurrentProcessId")) return (FARPROC)process;
    return NULL;
}
void log_line(const char* fmt,...) { }
void dev_trace(const char* area,const char* fmt,...) { }
BOOL write_protected(void* to,const void* from,SIZE_T size) { return TRUE; }
BOOL pad_connected(void) { return TRUE; }
BOOL pad_start_down(void) { return start; }
BOOL pad_back_down(void) { return back; }
BOOL pad_binding_active(void) { return binding; }
BOOL pad_menus_active(void) { return menus; }
BOOL pad_menu_back(void) { return close_dropdown; }
static HRESULT WINAPI driver_data(void* self,DWORD size,object_data* data,DWORD* count,DWORD flags)
{ driver_reads++; *count=0; return DI_OK; }
static HRESULT WINAPI driver_state(void* self,DWORD size,void* state)
{ driver_reads++; ZeroMemory(state,size); return DI_OK; }
static int checks;
#define CHECK(x) do { checks++; if (!(x)) { fprintf(stderr,"FAIL input line %d: %s\n",__LINE__,#x);exit(1); } } while(0)
static DWORD read_keys(void)
{
    object_data data[16]; DWORD count=16;
    CHECK(get_data((void*)2,sizeof data[0],data,&count,0)==DI_OK);
    if(count) { CHECK(count==2);CHECK(data[0].offset==KEY_ESCAPE);CHECK(data[0].data==0x80);CHECK(!data[1].data); }
    return count;
}
int main(void)
{
    object_data data[16]; DWORD count=16; LONG state[20]; int i;
    g_get_data=driver_data;g_get_state=driver_state;
    g_devices[0].self=(void*)1;g_devices[0].kind=JOYSTICK;
    CHECK(get_data((void*)1,sizeof data[0],data,&count,0)==DI_OK);CHECK(!count);CHECK(!driver_reads);
    CHECK(get_state((void*)1,sizeof state,state)==DI_OK);CHECK(!driver_reads);
    for(i=0;i<8;i++)CHECK(state[i]==32767);
    for(i=8;i<12;i++)CHECK(state[i]==-1);
    g_devices[1].self=(void*)2;g_devices[1].kind=KEYBOARD;g_devices[1].state_size=256;
    back=TRUE;CHECK(read_keys()==2);CHECK(!read_keys());back=FALSE;CHECK(!read_keys());
    menus=FALSE;back=TRUE;CHECK(!read_keys());back=FALSE;CHECK(!read_keys());
    start=TRUE;CHECK(read_keys()==2);start=FALSE;CHECK(!read_keys());
    menus=TRUE;back=TRUE;close_dropdown=TRUE;CHECK(!read_keys());back=FALSE;CHECK(!read_keys());
    close_dropdown=FALSE;binding=TRUE;back=TRUE;CHECK(!read_keys());back=FALSE;CHECK(!read_keys());binding=FALSE;
    front=FALSE;back=TRUE;CHECK(!read_keys());back=FALSE;CHECK(!read_keys());
    printf("PASS %d input checks: retired legacy driver bypass; B/Start, dropdown back, capture and foreground gating\n",checks);
    return 0;
}
