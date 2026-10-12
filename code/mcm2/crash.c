/*
    OpenMCM2 - a record of crashes in openmcm2.log.

    An access violation, or a jump to nowhere, is logged with its address, the registers and the
    top of the stack, which holds the return addresses that lead to it. Only the first few are
    logged: the game handles some faults itself.
*/

#include "runtime.h"

#define FAULTS_LOGGED 4
#define STACK_WORDS   48

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

static LONG WINAPI on_fault(EXCEPTION_POINTERS* fault)
{
    const EXCEPTION_RECORD* what = fault->ExceptionRecord;
    const CONTEXT* at = fault->ContextRecord;
    const DWORD* stack = (const DWORD*) at->Esp;
    int i;

    if (what->ExceptionCode != EXCEPTION_ACCESS_VIOLATION
        && what->ExceptionCode != EXCEPTION_ILLEGAL_INSTRUCTION)
        return EXCEPTION_CONTINUE_SEARCH;
    if (InterlockedIncrement(&faults) > FAULTS_LOGGED)
        return EXCEPTION_CONTINUE_SEARCH;

    log_line("fault %08X at %08X, %s %08X", what->ExceptionCode, at->Eip,
             what->NumberParameters >= 2 && what->ExceptionInformation[0] ? "writing" : "reading",
             what->NumberParameters >= 2 ? (DWORD) what->ExceptionInformation[1] : 0);
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
    return EXCEPTION_CONTINUE_SEARCH;
}

void crash_install(void)
{
    PVOID (WINAPI* add_handler)(ULONG, PVECTORED_EXCEPTION_HANDLER) =
        (PVOID (WINAPI*)(ULONG, PVECTORED_EXCEPTION_HANDLER)) api("kernel32.dll", "AddVectoredExceptionHandler");

    if (add_handler)
        add_handler(1, on_fault);
}
