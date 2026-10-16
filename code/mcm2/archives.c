/*
    OpenMCM2 - a resource archive (RES\*.res, "FAOE") mounted: its entries joined to the one table
    every name is looked up in, a later archive's entry taking the place of an earlier one's.

    In the dev build, after the game's last archive (Eco.res) every archive in RES\ADDONS\ is
    mounted too, in name order. Custom tracks need art and ecosystem models the community added
    to its own GLOBAL.RES and ECO.RES; packed in an addon archive of their own instead
    (tools/res.py split), they reach the game without the retail archives being replaced.
*/

#include "runtime.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

int __fastcall game_orig_FUN_004eddc0(BYTE* self, void* edx_unused, const char* path, int flags);

#ifdef OPENMCM2_DEV
#define ADDONS_FOLDER "ADDONS\\"

typedef HANDLE (WINAPI* find_first_fn)(LPCSTR pattern, WIN32_FIND_DATAA* found);
typedef BOOL   (WINAPI* find_next_fn)(HANDLE search, WIN32_FIND_DATAA* found);
typedef BOOL   (WINAPI* find_close_fn)(HANDLE search);

/* The length of `path` up to and with its last backslash: its folder. */
static int folder_length(const char* path)
{
    int i, folder = 0;

    for (i = 0; path[i]; i++)
        if (path[i] == '\\' || path[i] == '/')
            folder = i + 1;
    return folder;
}

static void mount_addons(BYTE* self, const char* eco_path)
{
    find_first_fn find_first = (find_first_fn) api("kernel32.dll", "FindFirstFileA");
    find_next_fn find_next = (find_next_fn) api("kernel32.dll", "FindNextFileA");
    find_close_fn find_close = (find_close_fn) api("kernel32.dll", "FindClose");
    char folder[MAX_PATH], pattern[MAX_PATH], path[MAX_PATH];
    int length = folder_length(eco_path), mounted = 0;
    WIN32_FIND_DATAA found;
    HANDLE search;

    if (!find_first || !find_next || !find_close || length + lstrlenA(ADDONS_FOLDER) + 8 >= MAX_PATH)
        return;
    lstrcpynA(folder, eco_path, length + 1);
    lstrcpyA(folder + length, ADDONS_FOLDER);
    wsprintfA(pattern, "%s*.res", folder);
    search = find_first(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
        return;
    do {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            || lstrlenA(folder) + lstrlenA(found.cFileName) >= MAX_PATH)
            continue;
        wsprintfA(path, "%s%s", folder, found.cFileName);
        game_orig_FUN_004eddc0(self, NULL, path, 0);
        log_line("dev build: addon archive %s mounted", path);
        mounted++;
    } while (find_next(search, &found));
    find_close(search);
    log_line("dev build: %d addon archive(s) from %s", mounted, folder);
}
#endif

PORT(0x004EDDC0)
int __fastcall archive_mount(BYTE* self, void* edx_unused, const char* path, int flags)
{
    int result = game_orig_FUN_004eddc0(self, edx_unused, path, flags);

#ifdef OPENMCM2_DEV
    {
        int name = folder_length(path);

        if (lstrcmpiA(path + name, "Eco.res") == 0)
            mount_addons(self, path);
    }
#endif
    return result;
}
