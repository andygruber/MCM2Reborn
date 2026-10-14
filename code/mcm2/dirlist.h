/*
    OpenMCM2 - the drive and folder lists (dirlist.c): the computer's drives with their labels,
    a folder's names for a pattern (DirectoryList), and two folders' merged (CombinedDirectoryList),
    the hard disk's and the CD's, as the track screens read them.
*/

#pragma once

#include "game.h"

typedef struct drive_entry {
    char root[4];               /* "X:\" */
    UINT type;                  /* GetDriveTypeA's */
    char label[MAX_PATH];       /* the volume's */
} drive_entry;

typedef struct drive_list {
    drive_entry* drives;
    int          count;
} drive_list;

LAYOUT(drive_entry, label, 0x8);
_Static_assert(sizeof(drive_entry) == 0x10C, "a drive is 268 bytes");
_Static_assert(sizeof(drive_list) == 8, "the drive list is 8 bytes");

typedef struct dir_entry {
    char  name[MAX_PATH];
    DWORD attributes;           /* FindFirstFileA's */
    UINT  drive;                /* DRIVE_FIXED; the merge means it to tell the CD's apart */
} dir_entry;

typedef struct dir_list {
    void**     vtable;          /* slot 0 the destructor, slot 1 (offset 4) the scan */
    int        count;
    int        cursor;          /* the name the next and current ones go from */
    char       pattern[MAX_PATH];
    char       folder[MAX_PATH];
    dir_entry* entries;
    BOOL       files_too;       /* FALSE: folders only */
} dir_list;

LAYOUT(dir_list, count, 0x4);
LAYOUT(dir_list, cursor, 0x8);
LAYOUT(dir_list, pattern, 0xC);
LAYOUT(dir_list, folder, 0x110);
LAYOUT(dir_list, entries, 0x214);
LAYOUT(dir_list, files_too, 0x218);
_Static_assert(sizeof(dir_entry) == 0x10C, "an entry is 268 bytes");
_Static_assert(sizeof(dir_list) == 0x21C, "a DirectoryList is 540 bytes");

typedef struct merged_list {
    dir_list base;
    char     hard_disk[MAX_PATH];   /* the folder under the hard disk's game root */
    char     cd[MAX_PATH];          /* and under the CD's */
} merged_list;

LAYOUT(merged_list, hard_disk, 0x21C);
LAYOUT(merged_list, cd, 0x320);
_Static_assert(sizeof(merged_list) == 0x424, "a CombinedDirectoryList is 1060 bytes");

BOOL __fastcall drives_scan_entry(drive_list* self);
BOOL __fastcall drives_scan(drive_list* self);
void __fastcall drives_destruct(drive_list* self);
BOOL __fastcall drives_find(drive_list* self, void* edx_unused, const char* label, int* index,
                            UINT type);
BOOL __fastcall drives_root(drive_list* self, void* edx_unused, int index, char* root);
dir_list* __fastcall dir_list_construct(dir_list* self);
dir_list* __fastcall dir_list_delete(dir_list* self, void* edx_unused, DWORD flags);
void __fastcall dir_list_set_folder(dir_list* self, void* edx_unused, const char* folder);
void __fastcall dir_list_set_pattern(dir_list* self, void* edx_unused, const char* pattern,
                                     BOOL files_too);
BOOL __fastcall dir_list_name(dir_list* self, void* edx_unused, int index, char* name);
BOOL __fastcall dir_list_title(dir_list* self, void* edx_unused, int index, char* title);
int __fastcall dir_list_count(dir_list* self);
BOOL __fastcall dir_list_next_name(dir_list* self, void* edx_unused, char* name);
BOOL __fastcall dir_list_current_name(dir_list* self, void* edx_unused, char* name);
BOOL __fastcall dir_list_next_title(dir_list* self, void* edx_unused, char* title);
BOOL __fastcall dir_list_current_title(dir_list* self, void* edx_unused, char* title);
BOOL __fastcall dir_list_wanted(dir_list* self, void* edx_unused, const char* name,
                                DWORD attributes);
BOOL __fastcall dir_list_scan(dir_list* self);
BOOL __fastcall dir_list_has(dir_list* self, void* edx_unused, const char* name);
BOOL __fastcall dir_list_remove_folder(dir_list* self, void* edx_unused, const char* folder);
int __cdecl dir_entries_by_name(const void* a, const void* b);
void __fastcall dir_list_sort(dir_list* self);
void __fastcall dir_list_destruct(dir_list* self);
merged_list* __fastcall merged_list_construct(merged_list* self);
void __fastcall merged_list_destruct(merged_list* self);
merged_list* __fastcall merged_list_delete(merged_list* self, void* edx_unused, DWORD flags);
void __fastcall merged_list_set_folders(merged_list* self, void* edx_unused, const char* hard_disk,
                                        const char* cd);
BOOL __fastcall merged_list_scan(merged_list* self);
