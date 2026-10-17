/*
    OpenMCM2 - a record of crashes in openmcm2.log.

    An access violation, or a jump to nowhere, is logged with its address, the registers and the
    top of the stack, which holds the return addresses that lead to it; and, deeper, each word of
    the stack that is the return address of a call, by its module (a DLL's, or the game's own
    addresses as they are). Only the first few are logged: the game handles some faults itself.
*/

#include "runtime.h"

#define FAULTS_LOGGED 4
#define STACK_WORDS   48
#define CALLS_SEARCHED 1024             /* stack words looked through for return addresses */
#define CALLS_LOGGED  24

static LONG faults;

/* Whether `size` bytes at `p` can be read. */
static BOOL readable(const void* p, SIZE_T size)
{
    MEMORY_BASIC_INFORMATION info;

    if (!VirtualQuery(p, &info, sizeof info) || info.State != MEM_COMMIT)
        return FALSE;
    if (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))
        return FALSE;
    return (const BYTE*) p + size <= (const BYTE*) info.BaseAddress + info.RegionSize;
}

/* Whether `p` follows a call instruction (E8 rel32, or FF /2 of 2, 3, 6 or 7 bytes). */
static BOOL after_call(const BYTE* p)
{
    if (!readable(p - 7, 7))
        return FALSE;
    return p[-5] == 0xE8 || (p[-2] == 0xFF && (p[-1] & 0x38) == 0x10) || (p[-3] == 0xFF && (p[-2] & 0x38) == 0x10)
           || (p[-6] == 0xFF && (p[-5] & 0x38) == 0x10) || (p[-7] == 0xFF && (p[-6] & 0x38) == 0x10);
}

/* `address` as "module+offset" for a DLL's, else as it is. */
static void name_address(DWORD address, char* to)
{
    typedef BOOL (WINAPI* module_of_fn)(DWORD, LPCSTR, HMODULE*);
    typedef DWORD (WINAPI* module_name_fn)(HMODULE, LPSTR, DWORD);
    static module_of_fn module_of;
    static module_name_fn module_name;
    HMODULE module = NULL;
    char path[MAX_PATH];
    const char* name;
    int i;

    if (!module_of) {
        module_of = (module_of_fn) api("kernel32.dll", "GetModuleHandleExA");
        module_name = (module_name_fn) api("kernel32.dll", "GetModuleFileNameA");
    }
    if (module_of && module_name && module_of(6, (LPCSTR) address, &module) && module
        && module != GetModuleHandleA(NULL) && module_name(module, path, MAX_PATH)) {
        for (name = path, i = 0; path[i]; i++)
            if (path[i] == '\\')
                name = path + i + 1;
        wsprintfA(to, "%s+%X", name, address - (DWORD) module);
        return;
    }
    wsprintfA(to, "%08X", address);
}

static LONG WINAPI on_fault(EXCEPTION_POINTERS* fault)
{
    const EXCEPTION_RECORD* what = fault->ExceptionRecord;
    const CONTEXT* at = fault->ContextRecord;
    const DWORD* stack = (const DWORD*) at->Esp;
    int i, n_calls;

    if (what->ExceptionCode != EXCEPTION_ACCESS_VIOLATION
        && what->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&faults) > FAULTS_LOGGED)
        return EXCEPTION_CONTINUE_SEARCH;

    {
        char where[MAX_PATH + 16];
        ULONG_PTR kind = what->NumberParameters >= 2 ? what->ExceptionInformation[0] : 0;

        name_address(at->Eip, where);
        log_line("fault %08X at %s, %s %08X", what->ExceptionCode, where,
                 kind == 8 ? "running" : kind ? "writing" : "reading",
                 what->NumberParameters >= 2 ? (DWORD) what->ExceptionInformation[1] : 0);
    }
    log_line("  eax %08X ebx %08X ecx %08X edx %08X", at->Eax, at->Ebx, at->Ecx, at->Edx);
    log_line("  esi %08X edi %08X ebp %08X esp %08X", at->Esi, at->Edi, at->Ebp, at->Esp);
    for (i = 0; i < STACK_WORDS && readable(stack + i, 4); i += 4) {
        int n;
        char line[96];
        char* to = line;

        for (n = i; n < i + 4 && readable(stack + n, 4); n++)
            to += wsprintfA(to, " %08X", stack[n]);
        log_line("  [esp+%02X]%s", i * 4, line);
    }
    for (i = 0, n_calls = 0; i < CALLS_SEARCHED && n_calls < CALLS_LOGGED && readable(stack + i, 4); i++) {
        char name[MAX_PATH + 16];

        if (stack[i] < 0x10000 || !after_call((const BYTE*) stack[i]))
            continue;
        name_address(stack[i], name);
        log_line("  called from %s (esp+%X)", name, i * 4);
        n_calls++;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

void crash_install(void)
{
    PVOID (WINAPI* add_handler)(ULONG, PVECTORED_EXCEPTION_HANDLER) =
        (PVOID (WINAPI*)(ULONG, PVECTORED_EXCEPTION_HANDLER)) api("kernel32.dll", "AddVectoredExceptionHandler");

    if (add_handler)
        add_handler(1, on_fault);
}
