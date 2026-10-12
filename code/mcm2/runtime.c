/*
    OpenMCM2 - runtime support: the log, runtime-resolved functions, and protected writes.
*/

#include "runtime.h"

#include <intrin.h>
#include <stdarg.h>

/* The compiler marks any object that uses floating point by referring to this; the CRT that
   normally defines it is not linked. */
int _fltused = 0x9875;

static HANDLE g_log = INVALID_HANDLE_VALUE;

void log_open(void)
{
    typedef BOOL (WINAPI *move_fn)(LPCSTR, LPCSTR, DWORD);
    move_fn move = (move_fn) api("kernel32.dll", "MoveFileExA");

    /* The previous run's log is kept, so a quick relaunch cannot wipe what a long session found. */
    if (move)
        move("openmcm2.log", "openmcm2.prev.log", MOVEFILE_REPLACE_EXISTING);

    g_log = CreateFileA("openmcm2.log", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, NULL);
}

void log_line(const char* fmt, ...)
{
    char line[1024];
    DWORD written;
    va_list args;
    int n;

    if (g_log == INVALID_HANDLE_VALUE)
        return;

    va_start(args, fmt);
    n = wvsprintfA(line, fmt, args);    /* stops at 1024 characters, the size of the buffer */
    va_end(args);

    if (n < 0)
        return;

    if (n > (int) sizeof line - 3)
        n = (int) sizeof line - 3;

    line[n++] = '\r';
    line[n++] = '\n';
    WriteFile(g_log, line, (DWORD) n, &written, NULL);
}

FARPROC api(const char* dll, const char* name)
{
    HMODULE module = LoadLibraryA(dll);

    return module ? GetProcAddress(module, name) : NULL;
}

static HANDLE heap(void)
{
    typedef HANDLE (WINAPI *heap_fn)(void);
    static HANDLE process_heap;

    if (!process_heap)
        process_heap = ((heap_fn) api("kernel32.dll", "GetProcessHeap"))();
    return process_heap;
}

void* mem_alloc(SIZE_T size)
{
    return HeapAlloc(heap(), HEAP_ZERO_MEMORY, size);
}

void mem_free(void* p)
{
    if (p)
        HeapFree(heap(), 0, p);
}

BOOL write_protected(void* to, const void* from, SIZE_T size)
{
    typedef BOOL (WINAPI *protect_fn)(void*, SIZE_T, DWORD, DWORD*);
    static protect_fn protect;
    DWORD old;

    if (!protect)
        protect = (protect_fn) api("kernel32.dll", "VirtualProtect");

    /* Execute stays on: a vtable can share its page with code. */
    if (!protect || !protect(to, size, PAGE_EXECUTE_READWRITE, &old))
        return FALSE;

    __movsb((unsigned char*) to, (const unsigned char*) from, size);
    protect(to, size, old, &old);
    return TRUE;
}
