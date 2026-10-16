/*
    OpenMCM2 - the resource manager's lookups, ported from the game (ResourceManager.cpp in the
    original).

    The manager holds every entry of every archive the game has open (the menus' dat files, a
    track's env) in one array, and the original finds a name by comparing it with each in turn:
    once or twice for each of a menu's pictures, and once for each entry of an archive as it
    is added. With thousands of entries open that is about a millisecond a picture, and a
    second to list a kind's tracks, so the dev build looks names up in an index of the array
    (rebuilt whenever the array has changed) and gives the same answers.
*/

#include "game.h"

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"
#endif

/* The manager's fields, and an entry's. */
#define RES_COUNT       0x08        /* int: the entries in use */
#define RES_ENTRIES     0x14        /* BYTE*[] */
#define ENTRY_NAME      0x08        /* char* */
#define ENTRY_ARCHIVE   0x14        /* the stream it is read from, or NULL */
#define ENTRY_OFFSET    0x18
#define ENTRY_SIZE      0x1C
#define STREAM_OPEN     0x08        /* the file it has open */
#define STREAM_PARENT   0x1C        /* the entry it is read from, inside another archive */

int   __cdecl    game_FUN_0053b6e0(const char* a, const char* b);           /* _stricmp */
void* __fastcall game_FUN_004ee6f0(void* parent);                           /* that entry's file */
void  __fastcall game_FUN_004622b0(void* stream, void* edx_unused, DWORD offset, DWORD a, DWORD b);
void  __fastcall game_FUN_00461cd0(void* stream);                           /* close it */
void  __cdecl    game_FUN_004a46b0(void* block);                            /* the game's free */

#define entries_of(m)   FIELD(m, RES_ENTRIES, BYTE**)
#define count_of(m)     FIELD(m, RES_COUNT, int)

/* Whether an entry's stream has its file open: its own, or the archive entry it lies in. */
static BOOL stream_open(BYTE* stream)
{
    BYTE* parent = FIELD(stream, STREAM_PARENT, BYTE*);

    return (parent ? game_FUN_004ee6f0(parent) : FIELD(stream, STREAM_OPEN, void*)) != NULL;
}

#ifdef OPENMCM2_DEV
/*
    The index: each name's first entry, by a hash of the name in either case (as _stricmp
    compares). It belongs to one manager's array as it was when built, known by the array, its
    count and the sum of its entries' addresses; any of them changing builds it again.
*/
static struct {
    BYTE*  manager;
    BYTE** entries;
    int    count;
    DWORD  sum;
    int*   slots;           /* an entry's index + 1, or 0 */
    int    size;            /* a power of two, at least twice the count */
} res_index;

static DWORD name_hash(const char* name)
{
    DWORD hash = 2166136261u;

    for (; *name; name++) {
        char c = *name;

        hash = (hash ^ (BYTE) (c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c)) * 16777619u;
    }
    return hash;
}

static DWORD entries_sum(BYTE** entries, int count)
{
    DWORD sum = 0;
    int i;

    for (i = 0; i < count; i++)
        sum = sum * 31 + (DWORD) entries[i];
    return sum;
}

/* The index of `manager`'s array, built again when it is not up to date. */
static BOOL index_ready(BYTE* manager)
{
    BYTE** entries = entries_of(manager);
    int count = count_of(manager);
    DWORD sum = entries_sum(entries, count);
    int size, i;

    if (res_index.slots && res_index.manager == manager && res_index.entries == entries
        && res_index.count == count && res_index.sum == sum)
        return TRUE;
    for (size = 64; size < count * 2; size *= 2)
        ;
    if (size != res_index.size) {
        mem_free(res_index.slots);
        res_index.slots = mem_alloc(size * sizeof(int));
        res_index.size = res_index.slots ? size : 0;
        if (!res_index.slots)
            return FALSE;
    } else {
        ZeroMemory(res_index.slots, size * sizeof(int));
    }
    for (i = 0; i < count; i++) {
        DWORD slot = name_hash(FIELD(entries[i], ENTRY_NAME, char*)) & (size - 1);

        while (res_index.slots[slot]) {
            if (!game_FUN_0053b6e0(FIELD(entries[res_index.slots[slot] - 1], ENTRY_NAME, char*),
                                   FIELD(entries[i], ENTRY_NAME, char*)))
                break;                  /* a later entry of the same name: the first answers */
            slot = (slot + 1) & (size - 1);
        }
        if (!res_index.slots[slot])
            res_index.slots[slot] = i + 1;
    }
    res_index.manager = manager;
    res_index.entries = entries;
    res_index.count = count;
    res_index.sum = sum;
    return TRUE;
}

/* The first entry named `name`'s index, or -1. */
static int index_find(const char* name)
{
    DWORD slot = name_hash(name) & (res_index.size - 1);

    for (; res_index.slots[slot]; slot = (slot + 1) & (res_index.size - 1)) {
        int i = res_index.slots[slot] - 1;

        if (!game_FUN_0053b6e0(FIELD(res_index.entries[i], ENTRY_NAME, char*), name))
            return i;
    }
    return -1;
}
#endif

/* The first entry named `name` (case aside), or -1. */
static int entry_find(BYTE* self, const char* name)
{
    int i;

#ifdef OPENMCM2_DEV
    if (index_ready(self))
        return index_find(name);
#endif
    for (i = 0; i < count_of(self); i++)
        if (!game_FUN_0053b6e0(FIELD(entries_of(self)[i], ENTRY_NAME, char*), name))
            return i;
    return -1;
}

/*
    The entry named `name`, or NULL. Its stream is moved to it, unless `unopened` is FALSE and
    the stream has no file open.
*/
PORT(0x004EE0F0)
BYTE* __fastcall resources_find(BYTE* self, void* edx_unused, const char* name, BOOL unopened)
{
    int i = entry_find(self, name);
    BYTE* entry;
    BYTE* stream;

    (void) edx_unused;
    if (i < 0)
        return NULL;
    entry = entries_of(self)[i];
    stream = FIELD(entry, ENTRY_ARCHIVE, BYTE*);
    if (stream && (unopened || stream_open(stream)))
        game_FUN_004622b0(stream, NULL, FIELD(entry, ENTRY_OFFSET, DWORD), 0, 0);
    return entry;
}

/*
    An entry being added: when one of its name is there and its stream has no file open, that
    one takes the new entry's place and stream (the old stream closed when no entry uses it
    now), and TRUE says the new one is not wanted. FALSE adds it.
*/
PORT(0x004EDCD0)
BOOL __fastcall resources_merge(BYTE* self, void* edx_unused, BYTE* added)
{
    int i = entry_find(self, FIELD(added, ENTRY_NAME, char*));
    BYTE* entry;
    BYTE* old;
    int k;

    (void) edx_unused;
    if (i < 0)
        return FALSE;
    entry = entries_of(self)[i];
    if (FIELD(entry, ENTRY_ARCHIVE, BYTE*) && stream_open(FIELD(entry, ENTRY_ARCHIVE, BYTE*)))
        return FALSE;
    FIELD(entry, ENTRY_OFFSET, DWORD) = FIELD(added, ENTRY_OFFSET, DWORD);
    FIELD(entry, ENTRY_SIZE, DWORD) = FIELD(added, ENTRY_SIZE, DWORD);
    old = FIELD(entry, ENTRY_ARCHIVE, BYTE*);
    FIELD(entry, ENTRY_ARCHIVE, BYTE*) = FIELD(added, ENTRY_ARCHIVE, BYTE*);
    if (old == FIELD(added, ENTRY_ARCHIVE, BYTE*))
        return TRUE;
    for (k = 0; k < count_of(self); k++)
        if (FIELD(entries_of(self)[k], ENTRY_ARCHIVE, BYTE*) == old)
            return TRUE;
    if (old) {
        game_FUN_00461cd0(old);
        game_FUN_004a46b0(old);
    }
    return TRUE;
}
