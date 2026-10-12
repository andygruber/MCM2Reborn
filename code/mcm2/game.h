/*
    OpenMCM2 - the game's own types, data and functions, as far as ported code uses them.

    Layouts are what the game's code does with them; the original field names are not known.
    Unknown stretches are padding named after their offset, and every known field's offset is
    checked at compile time.
*/

#pragma once

#include <intrin.h>
#include <stddef.h>
#include "runtime.h"

#define LAYOUT(type, field, offset) \
    _Static_assert(offsetof(type, field) == (offset), #type "." #field " is not at " #offset)

/* The game's tracked new and delete. The file and line are for its leak report. */
void* __cdecl game_FUN_004a4600(size_t size, const char* file, int line);
void  __cdecl game_FUN_004a46b0(void* p);
#define game_new    game_FUN_004a4600
#define game_delete game_FUN_004a46b0

/* The game's main object, which 0x005743AC points at; its layout is mostly unknown. */
#define game_main (*(BYTE* const*) 0x005743AC)

/* The number of input devices the game found. */
static __inline int input_count(void)
{
    return *(int*) (*(BYTE**) (game_main + 0x14) + 4);
}

/* GameObject: the base of everything in the game's object tree, 44 bytes. */
typedef struct game_object {
    void** vtable;
    BYTE   unknown_04[0x28];
} game_object;

_Static_assert(sizeof(game_object) == 44, "GameObject is 44 bytes");

/* Slot 2 of a GameObject's vtable: unlink it from the tree and destroy it. */
static __inline void object_destroy(game_object* object)
{
    ((void (__fastcall**)(game_object*)) object->vtable)[2](object);
}

/* ContainerList (ContainerList.h): a growable array of pointers. */
typedef struct container_list {
    int    count;
    void** items;
    int    grow_by;
    int    capacity;
} container_list;

#define CONTAINER_LIST_H ((const char*) 0x0056C6E4)    /* the game's "...\ContainerList.h" */

/*
    ContainerList's add, as the game inlines it. When full, it grows to count + grow_by; if that
    is no bigger, or the allocation fails, the item is silently not added.
*/
static __inline void list_add(container_list* list, void* item)
{
    if (list->count >= list->capacity) {
        int capacity = list->count + list->grow_by;
        void** items;

        if (capacity <= list->capacity)
            return;
        items = game_new(capacity * sizeof *items, CONTAINER_LIST_H, 0x47);
        if (!items)
            return;
        __movsd((unsigned long*) items, (unsigned long*) list->items, list->capacity);
        game_delete(list->items);
        list->items = items;
        list->capacity = capacity;
    }
    list->items[list->count++] = item;
}
