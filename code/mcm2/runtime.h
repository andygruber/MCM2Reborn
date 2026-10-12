/*
    OpenMCM2 - runtime support for the code compiled into the image.

    There is no CRT here and no import table of our own: Win32 calls go through the game's own
    import slots (see tools/link.py), and anything the game does not import is looked up at
    runtime with api().
*/

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* openmcm2.log in the game folder, truncated at start-up. */
void log_open(void);
void log_line(const char* fmt, ...);

/* A function the game does not import, or NULL. */
FARPROC api(const char* dll, const char* name);

/* Zeroed memory from the process heap, and its release. */
void* mem_alloc(SIZE_T size);
void  mem_free(void* p);

/* Copy into memory the loader made read-only, such as the import table or a vtable. */
BOOL write_protected(void* to, const void* from, SIZE_T size);
