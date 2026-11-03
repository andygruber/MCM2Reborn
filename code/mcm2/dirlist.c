/*
    OpenMCM2 - the drive and folder lists, ported from the game (dirlist.cpp in the original).

    The drive list (8 bytes) holds the computer's drives with their volume labels; the profile
    looks for the game's CD in it. A DirectoryList (0x21C bytes) holds the names one folder has
    for a pattern, and a CombinedDirectoryList (0x424) merges two folders' lists, the hard disk's
    and the CD's, for the track screens. Each function here replaces the game's at the address in
    its PORT line, and was checked against the original's instructions. The original's C++
    exception frames are not reproduced.
*/

#include "dirlist.h"

#define NAME(address) ((const char*) (address))
#define DIRLIST_CPP NAME(0x0056F2EC)    /* "D:\aardvark\VC\krusty2\dirlist.cpp" */
#define DRIVE_ROOT  NAME(0x0056F2E4)    /* "%c:\" */
#define PARENT      NAME(0x0056F32C)    /* ".." */
#define HERE        NAME(0x0056F330)    /* "." */
#define EVERYTHING  NAME(0x0056F334)    /* "*" */
#define IN_FOLDER   NAME(0x0056C7DC)    /* "%s\%s" */
#define JOINED      NAME(0x0056ED34)    /* "%s%s" */
#define NO_TEXT     NAME(0x0057D888)    /* "" */

int  __cdecl game_FUN_0053b6e0(const char* a, const char* b);                  /* _stricmp */
int  __cdecl game_FUN_0053a11f(char* to, const char* format, ...);             /* sprintf */
void __cdecl game_FUN_0053b042(const char* path, char* drive, char* folder, char* name,
                               char* extension);                               /* _splitpath */
void __cdecl game_FUN_00539a96(void* base, size_t count, size_t size,
                               int (__cdecl* compare)(const void*, const void*));  /* qsort */

/* Two strings, equal: the game inlines strcmp() == 0 like this. */
static BOOL text_equal(const char* a, const char* b)
{
    while (*a == *b) {
        if (!*a)
            return TRUE;
        a++;
        b++;
    }
    return FALSE;
}

/* ---- The drives ---- */

/* A jump to drives_scan, which the profile calls when it is made. */
PORT(0x0044ADF0)
BOOL __fastcall drives_scan_entry(drive_list* self)
{
    return drives_scan(self);
}

/*
    Lists the drives with their roots and types, and the labels of those kept. Only fixed disks
    are kept: the type is tested against DRIVE_FIXED twice, where the second test was probably
    meant for DRIVE_CDROM (kept), so profile_find_cd, which asks for a CD-ROM, never finds one.
*/
PORT(0x0044AE00)
BOOL __fastcall drives_scan(drive_list* self)
{
    char root[4];
    char file_system[MAX_PATH];
    const char* letter;
    char* letters;
    size_t length = 0;
    DWORD size;
    int count = 0;

    self->count = 0;
    if (self->drives)
        game_delete(self->drives);
    size = GetLogicalDriveStringsA(0, NULL) * 2;
    letters = game_FUN_004a4410(size, DIRLIST_CPP, 0x20);
    if (!letters)
        return FALSE;           /* self->drives still points at the freed array (kept) */
    GetLogicalDriveStringsA(size, letters);

    /* Bytes, four to a drive, used as the count of drives: four times the room (kept). */
    if (*letters)
        do
            length += 4;
        while (letters[length]);
    self->drives = game_new(length * sizeof(drive_entry), DIRLIST_CPP, 0x2C);
    if (!self->drives)
        return FALSE;           /* the letters are leaked (kept) */

    for (letter = letters; *letter; letter += 4) {
        drive_entry* drive = &self->drives[count];

        game_FUN_0053a11f(root, DRIVE_ROOT, *letter);
        text_copy(drive->root, root);
        drive->type = GetDriveTypeA(drive->root);
        if (drive->type == DRIVE_FIXED) {
            GetVolumeInformationA(drive->root, drive->label, MAX_PATH, NULL, NULL, NULL,
                                  file_system, MAX_PATH);
            count++;
        }
    }
    self->count = count;
    game_FUN_004a4450(letters, DIRLIST_CPP, 0x3F);
    return TRUE;
}

PORT(0x0044AF90)
void __fastcall drives_destruct(drive_list* self)
{
    if (self->drives)
        game_delete(self->drives);
}

/* The first drive with this label and, unless `type` is 0, this type; `index` is 0 if none. */
PORT(0x0044AFA0)
BOOL __fastcall drives_find(drive_list* self, void* edx_unused, const char* label, int* index,
                            UINT type)
{
    int i;

    (void) edx_unused;
    *index = 0;
    if (!self->drives)
        return FALSE;
    for (i = 0; i < self->count; i++) {
        if (text_equal(label, self->drives[i].label)
            && (!type || type == self->drives[i].type)) {
            *index = i;
            return TRUE;
        }
    }
    return FALSE;
}

/* A drive's root; the index is not checked (kept). */
PORT(0x0044B040)
BOOL __fastcall drives_root(drive_list* self, void* edx_unused, int index, char* root)
{
    (void) edx_unused;
    text_copy(root, NO_TEXT);
    if (!self->drives)
        return FALSE;
    text_copy(root, self->drives[index].root);
    return TRUE;
}

/* ---- DirectoryList ---- */

#define DIR_LIST_VTABLE ((void**) 0x00556798)

/* `count` empty entries, as the original's new[] makes them. */
static dir_entry* entries_new(int count, int line)
{
    dir_entry* entries = game_new(count * sizeof *entries, DIRLIST_CPP, line);
    int i;

    if (entries) {
        for (i = 0; i < count; i++) {
            entries[i].name[0] = 0;
            entries[i].attributes = FILE_ATTRIBUTE_NORMAL;
            entries[i].drive = DRIVE_FIXED;
        }
    }
    return entries;
}

/* A path's file name without its folder or extension. */
static void copy_title(char* to, const char* path)
{
    char drive[4];
    char folder[0x100];
    char title[0x100];
    char extension[0x100];

    game_FUN_0053b042(path, drive, folder, title, extension);
    text_copy(to, title);
}

/* The current folder, every name, and a scan with the pattern still empty, which finds none. */
PORT(0x0044B0C0)
dir_list* __fastcall dir_list_construct(dir_list* self)
{
    self->vtable = DIR_LIST_VTABLE;
    GetCurrentDirectoryA(MAX_PATH, self->folder);
    text_copy_most(self->pattern, NO_TEXT, MAX_PATH - 1);
    self->entries = NULL;
    self->count = 0;
    self->cursor = 0;
    self->files_too = TRUE;
    dir_list_scan(self);
    return self;
}

/* Vtable slot 0: the destructor, freeing the object when bit 0 of `flags` is set. */
PORT(0x0044B140)
dir_list* __fastcall dir_list_delete(dir_list* self, void* edx_unused, DWORD flags)
{
    (void) edx_unused;
    dir_list_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

PORT(0x0044B160)
void __fastcall dir_list_set_folder(dir_list* self, void* edx_unused, const char* folder)
{
    (void) edx_unused;
    text_copy_most(self->folder, folder, MAX_PATH - 1);
}

PORT(0x0044B1B0)
void __fastcall dir_list_set_pattern(dir_list* self, void* edx_unused, const char* pattern,
                                     BOOL files_too)
{
    (void) edx_unused;
    text_copy_most(self->pattern, pattern, MAX_PATH - 1);
    self->files_too = files_too;
}

PORT(0x0044B200)
BOOL __fastcall dir_list_name(dir_list* self, void* edx_unused, int index, char* name)
{
    (void) edx_unused;
    text_copy(name, NO_TEXT);
    if (!self->entries || index < 0 || index >= self->count)
        return FALSE;
    text_copy(name, self->entries[index].name);
    return TRUE;
}

/* A name without its extension. */
PORT(0x0044B280)
BOOL __fastcall dir_list_title(dir_list* self, void* edx_unused, int index, char* title)
{
    char name[MAX_PATH];

    (void) edx_unused;
    text_copy(title, NO_TEXT);
    if (!dir_list_name(self, NULL, index, name))
        return FALSE;
    copy_title(title, name);
    return TRUE;
}

PORT(0x0044B340)
int __fastcall dir_list_count(dir_list* self)
{
    return self->count;
}

/* The name after the cursor; past the last the cursor stays on it. */
PORT(0x0044B350)
BOOL __fastcall dir_list_next_name(dir_list* self, void* edx_unused, char* name)
{
    (void) edx_unused;
    text_copy(name, NO_TEXT);
    if (++self->cursor >= self->count) {
        self->cursor = self->count - 1;
        return FALSE;
    }
    text_copy(name, self->entries[self->cursor].name);
    return TRUE;
}

PORT(0x0044B3E0)
BOOL __fastcall dir_list_current_name(dir_list* self, void* edx_unused, char* name)
{
    (void) edx_unused;
    text_copy(name, NO_TEXT);
    if (self->count <= 0)
        return FALSE;
    text_copy(name, self->entries[self->cursor].name);
    return TRUE;
}

/* The next name without its extension; `title` is left as it was when there is none. */
PORT(0x0044B460)
BOOL __fastcall dir_list_next_title(dir_list* self, void* edx_unused, char* title)
{
    char name[MAX_PATH];

    (void) edx_unused;
    if (!dir_list_next_name(self, NULL, name))
        return FALSE;
    copy_title(title, name);
    return TRUE;
}

/* The current name without its extension: the first, after a scan. */
PORT(0x0044B4F0)
BOOL __fastcall dir_list_current_title(dir_list* self, void* edx_unused, char* title)
{
    char name[MAX_PATH];

    (void) edx_unused;
    text_copy(title, NO_TEXT);
    if (!dir_list_current_name(self, NULL, name))
        return FALSE;
    copy_title(title, name);
    return TRUE;
}

/* Whether a found name is listed: never "." or "..", and only folders unless files_too. */
PORT(0x0044B5A0)
BOOL __fastcall dir_list_wanted(dir_list* self, void* edx_unused, const char* name,
                                DWORD attributes)
{
    (void) edx_unused;
    if (!game_FUN_0053b6e0(NO_TEXT, name) || !game_FUN_0053b6e0(HERE, name)
        || !game_FUN_0053b6e0(PARENT, name))
        return FALSE;
    if (self->files_too)
        return TRUE;
    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

/* One enumeration with a growable buffer: files added while a scan is running
   must never exceed a count taken by an earlier enumeration. Publish only a
   complete successful scan; missing folders and no matches are empty lists. */
PORT(0x0044B610)
BOOL __fastcall dir_list_scan(dir_list* self)
{
    char path[MAX_PATH];
    WIN32_FIND_DATAA found;
    HANDLE search;
    int capacity = 0;
    size_t folder = text_length(self->folder);
    size_t pattern = text_length(self->pattern);
    BOOL separator = folder && self->folder[folder - 1] != '\\' && self->folder[folder - 1] != '/';
    DWORD error;

    if (self->entries)
        game_delete(self->entries);
    self->entries = NULL;
    self->count = self->cursor = 0;
    if (!pattern)
        return TRUE;
    if (folder + pattern + separator >= MAX_PATH) {
        return FALSE;
    }
    text_copy(path, self->folder);
    if (separator)
        path[folder++] = '\\';
    text_copy(path + folder, self->pattern);
    search = FindFirstFileA(path, &found);
    if (search == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
    }
    do {
        if (dir_list_wanted(self, NULL, found.cFileName, found.dwFileAttributes)) {
            dir_entry* entry;
            if (self->count == capacity) {
                int next;
                dir_entry* grown;
                if ((unsigned) capacity > 0x7FFFFFFFu / sizeof(dir_entry) / 2)
                    goto failed;
                next = capacity ? capacity * 2 : 32;
                grown = entries_new(next, 0x148);
                if (!grown)
                    goto failed;
                if (self->entries) {
                    int i;
                    for (i = 0; i < self->count; i++)
                        grown[i] = self->entries[i];
                    game_delete(self->entries);
                }
                self->entries = grown;
                capacity = next;
            }
            entry = &self->entries[self->count++];
            text_copy_most(entry->name, found.cFileName, MAX_PATH - 1);
            entry->attributes = found.dwFileAttributes;
        }
    } while (FindNextFileA(search, &found));
    error = GetLastError();
    FindClose(search);
    if (error == ERROR_NO_MORE_FILES)
        return TRUE;
    if (self->entries)
        game_delete(self->entries);
    self->entries = NULL;
    self->count = self->cursor = 0;
    return FALSE;

failed:
    FindClose(search);
    if (self->entries)
        game_delete(self->entries);
    self->entries = NULL;
    self->count = self->cursor = 0;
    return FALSE;
}

/* Whether the name is listed, in any case. */
PORT(0x0044B8B0)
BOOL __fastcall dir_list_has(dir_list* self, void* edx_unused, const char* name)
{
    int i;

    (void) edx_unused;
    for (i = 0; i < self->count; i++) {
        if (!game_FUN_0053b6e0(name, self->entries[i].name))
            return TRUE;
    }
    return FALSE;
}

/*
    Deletes a folder and everything in it. A file that cannot be deleted stops it; a subfolder
    that fails is passed over (kept), and then the folder itself fails to go.
*/
PORT(0x0044B900)
BOOL __fastcall dir_list_remove_folder(dir_list* self, void* edx_unused, const char* folder)
{
    char path[MAX_PATH];
    dir_list* inside;
    int i;

    (void) edx_unused;
    inside = game_new(sizeof *inside, DIRLIST_CPP, 0x176);
    inside = inside ? dir_list_construct(inside) : NULL;
    if (!inside)
        return FALSE;
    dir_list_set_folder(inside, NULL, folder);
    dir_list_set_pattern(inside, NULL, EVERYTHING, TRUE);
    VIRTUAL(inside, 4, method_0)(inside);

    for (i = 0; i < inside->count; i++) {
        game_FUN_0053a11f(path, IN_FOLDER, folder, inside->entries[i].name);
        if (inside->entries[i].attributes & FILE_ATTRIBUTE_DIRECTORY) {
            dir_list_remove_folder(self, NULL, path);
        } else if (!DeleteFileA(path)) {
            VIRTUAL(inside, 0, method_1)(inside, NULL, 1);
            return FALSE;
        }
    }
    VIRTUAL(inside, 0, method_1)(inside, NULL, 1);
    return RemoveDirectoryA(folder);
}

/* qsort order: by name, in any case. */
PORT(0x0044BA30)
int __cdecl dir_entries_by_name(const void* a, const void* b)
{
    return game_FUN_0053b6e0((const char*) a, (const char*) b);
}

PORT(0x0044BA50)
void __fastcall dir_list_sort(dir_list* self)
{
    if (self->count > 0)
        game_FUN_00539a96(self->entries, self->count, sizeof(dir_entry), dir_entries_by_name);
}

PORT(0x0044BA80)
void __fastcall dir_list_destruct(dir_list* self)
{
    self->vtable = DIR_LIST_VTABLE;
    if (self->entries) {
        game_delete(self->entries);
        self->entries = NULL;
    }
}

/* ---- CombinedDirectoryList ---- */

#define MERGED_LIST_VTABLE ((void**) 0x005567A4)

PORT(0x0044BAB0)
merged_list* __fastcall merged_list_construct(merged_list* self)
{
    dir_list_construct(&self->base);
    self->base.vtable = MERGED_LIST_VTABLE;
    text_copy(self->hard_disk, NO_TEXT);
    text_copy(self->cd, NO_TEXT);
    return self;
}

PORT(0x0044BB40)
void __fastcall merged_list_destruct(merged_list* self)
{
    self->base.vtable = MERGED_LIST_VTABLE;
    dir_list_destruct(&self->base);
}

/* Vtable slot 0, as the DirectoryList's. */
PORT(0x0044BB20)
merged_list* __fastcall merged_list_delete(merged_list* self, void* edx_unused, DWORD flags)
{
    (void) edx_unused;
    merged_list_destruct(self);
    if (flags & 1)
        game_delete(self);
    return self;
}

PORT(0x0044BB50)
void __fastcall merged_list_set_folders(merged_list* self, void* edx_unused,
                                        const char* hard_disk, const char* cd)
{
    (void) edx_unused;
    text_copy_most(self->hard_disk, hard_disk, MAX_PATH - 1);
    text_copy_most(self->cd, cd, MAX_PATH - 1);
}

/* Merge sorted names once. Windows names are case insensitive; a local copy
   wins a duplicate, without decrementing the number of unrelated entries.
   Merged lists retain the original API's drive tags in attributes. */
PORT(0x0044BBD0)
BOOL __fastcall merged_list_scan(merged_list* self)
{
    dir_list hard_disk, cd;
    dir_list* base = &self->base;
    dir_entry* merged = NULL;
    int local = 0, shared = 0, count = 0, capacity;
    BOOL ok = FALSE;

    if (base->entries)
        game_delete(base->entries);
    base->entries = NULL;
    base->count = base->cursor = 0;
    dir_list_construct(&hard_disk);
    dir_list_construct(&cd);
    dir_list_set_pattern(&hard_disk, NULL, base->pattern, TRUE);
    dir_list_set_folder(&hard_disk, NULL, self->hard_disk);
    dir_list_set_pattern(&cd, NULL, base->pattern, TRUE);
    dir_list_set_folder(&cd, NULL, self->cd);
    if ((self->hard_disk[0] && !dir_list_scan(&hard_disk))
        || (self->cd[0] && !dir_list_scan(&cd)))
        goto done;
    dir_list_sort(&hard_disk);
    dir_list_sort(&cd);
    if ((unsigned)hard_disk.count + (unsigned)cd.count > 0x7FFFFFFFu / sizeof(dir_entry))
        goto done;
    capacity = hard_disk.count + cd.count;
    if (capacity) {
        merged = entries_new(capacity, 0x1E0);
        if (!merged)
            goto done;
    }
    while (local < hard_disk.count || shared < cd.count) {
        BOOL take_local = shared == cd.count;
        int order = -1;
        if (local < hard_disk.count && shared < cd.count) {
            order = game_FUN_0053b6e0(hard_disk.entries[local].name, cd.entries[shared].name);
            take_local = order <= 0;
        }
        if (local == hard_disk.count)
            take_local = FALSE;
        if (take_local) {
            merged[count] = hard_disk.entries[local++];
            merged[count].attributes = merged[count].drive = DRIVE_FIXED;
            if (!order)
                shared++;
        } else {
            merged[count] = cd.entries[shared++];
            merged[count].attributes = merged[count].drive = DRIVE_CDROM;
        }
        count++;
    }
    base->entries = merged;
    base->count = count;
    ok = TRUE;
done:
    dir_list_destruct(&hard_disk);
    dir_list_destruct(&cd);
    return ok;
}
