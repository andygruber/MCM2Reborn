/*
    OpenMCM2 - the game's settings, kept in mcm2_profile.json beside it, and its renderer's own card
    profile in openmcm2_drivers.json. See profile.h.
*/

#include "profile.h"
#include "runtime.h"

#include <intrin.h>

#define PROFILE_FILE "mcm2_profile.json"
#define DRIVERS_FILE "openmcm2_drivers.json"
#define DRIVERS_KEY  "DriverInfo"
#ifdef OPENMCM2_DEV
/*
    MCM2:Reborn keeps its settings under a key of its own, so a retail install's are never read (its
    card profiles crash this renderer, and this renderer's crash the original) and never written
    over. The game's constructor, FUN_00525EC0, copies its key's name to game+0x4B8 from the
    string at 0x57B75C, named at the two sites below; every registry call then reads it there.
*/
#define GAME_KEY     "Software\\MCM2Reborn\\Game"
#define RETAIL_KEY   0x0057B75Cu
static const DWORD k_key_sites[] = { 0x00525FD3, 0x00525FF5 };     /* mov edi, imm32; push imm32 */
#else
#define GAME_KEY     "Software\\Microsoft\\Microsoft Games\\Motocross Madness 2"
#endif

#define MAX_NAME  256
#define MAX_KEY_PATH 512
#define MAX_VALUE 65536

static const char k_hex[] = "0123456789abcdef";

/* ------------------------------------------------------------------ reading the file */

typedef struct reader
{
    const char* at;
    const char* end;
} reader;

static void skip_space(reader* r)
{
    while (r->at < r->end && (*r->at == ' ' || *r->at == '\t' || *r->at == '\r' || *r->at == '\n'))
        ++r->at;
}

static BOOL expect(reader* r, char c)
{
    skip_space(r);
    if (r->at < r->end && *r->at == c)
    {
        ++r->at;
        return TRUE;
    }
    return FALSE;
}

/* A JSON string without \u escapes, which the profile never needs: names are ASCII. */
static BOOL read_string(reader* r, char* out, int size)
{
    int n = 0;

    if (!expect(r, '"'))
        return FALSE;

    while (r->at < r->end && *r->at != '"')
    {
        char c = *r->at++;

        if (c == '\\' && r->at < r->end)
            c = *r->at++;

        if (n >= size - 1)
            return FALSE;
        out[n++] = c;
    }
    out[n] = 0;
    return expect(r, '"');
}

static BOOL read_number(reader* r, DWORD* out)
{
    DWORD v = 0;

    skip_space(r);
    if (r->at >= r->end || *r->at < '0' || *r->at > '9')
        return FALSE;

    while (r->at < r->end && *r->at >= '0' && *r->at <= '9')
        v = v * 10 + (DWORD) (*r->at++ - '0');

    *out = v;
    return TRUE;
}

static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* A value's "hex bytes" string, decoded into `out`. */
static BOOL read_hex(reader* r, BYTE* out, DWORD size, DWORD* length)
{
    DWORD n = 0;

    if (!expect(r, '"'))
        return FALSE;

    while (r->at + 1 < r->end && *r->at != '"')
    {
        int hi = hex_digit(r->at[0]), lo = hex_digit(r->at[1]);

        if (hi < 0 || lo < 0 || n >= size)
            return FALSE;
        out[n++] = (BYTE) (hi << 4 | lo);
        r->at += 2;
    }
    *length = n;
    return expect(r, '"');
}

static void join_key(char* out, const char* subkey)
{
    lstrcpyA(out, GAME_KEY);
    if (*subkey)
    {
        int n = lstrlenA(out);
        out[n] = '\\';
        lstrcpyA(out + n + 1, subkey);
    }
}

/* One key's object: "name": [type, "hex"], ... */
static BOOL load_key(reader* r, HKEY key, BYTE* value)
{
    char name[MAX_NAME];
    DWORD type, length;

    if (!expect(r, '{'))
        return FALSE;
    if (expect(r, '}'))
        return TRUE;

    do
    {
        if (!read_string(r, name, sizeof name) || !expect(r, ':') || !expect(r, '[') ||
            !read_number(r, &type) || !expect(r, ',') || !read_hex(r, value, MAX_VALUE, &length) ||
            !expect(r, ']'))
            return FALSE;

        if (key)
            RegSetValueExA(key, name, 0, type, value, length);
    }
    while (expect(r, ','));

    return expect(r, '}');
}

/* DriverInfo and the keys under it: the card profile the game makes of each display card. */
static BOOL is_drivers(const char* subkey)
{
    int n = lstrlenA(DRIVERS_KEY);
    char head[16];

    lstrcpynA(head, subkey, n + 1);
    return !lstrcmpiA(head, DRIVERS_KEY) && (subkey[n] == 0 || subkey[n] == '\\');
}

/*
    The original game's card profile, as PROFILE_FILE had it: the text of its entries, kept to be
    written back unchanged. OpenMCM2's renderer is profiled as a card of its own; with the two in
    one place each run of either game overwrote the other's, and the original game, given
    OpenMCM2's, crashed in its DirectDraw as it started.
*/
static char* g_theirs;
static DWORD g_theirs_length;

static void keep_theirs(const char* from, const char* to)
{
    DWORD n = (DWORD) (to - from);

    if (!g_theirs)
        return;
    if (g_theirs_length)
        g_theirs[g_theirs_length++] = ',';
    __movsb((unsigned char*) g_theirs + g_theirs_length, (const unsigned char*) from, n);
    g_theirs_length += n;
}

/*
    The keys in `text` into the registry; with `set_aside`, the card profile's are kept in
    g_theirs instead. Returns how many keys were read, or -1 if the text is malformed.
*/
static int load_text(const char* text, DWORD size, BOOL set_aside, BYTE* value)
{
    char subkey[MAX_NAME], path[MAX_KEY_PATH];
    reader r;
    int keys = 0;

    r.at = text;
    r.end = text + size;
    if (!expect(&r, '{') || expect(&r, '}'))
        return 0;
    do
    {
        const char* entry;
        HKEY key = NULL;
        BOOL aside;

        skip_space(&r);
        entry = r.at;
        if (!read_string(&r, subkey, sizeof subkey) || !expect(&r, ':'))
            return -1;
        aside = set_aside && is_drivers(subkey);
        join_key(path, subkey);
        if (!aside && RegCreateKeyExA(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL))
            return -1;
        if (!load_key(&r, key, value))
        {
            if (key)
                RegCloseKey(key);
            return -1;
        }
        if (key)
            RegCloseKey(key);
        if (aside)
            keep_theirs(entry, r.at);
        ++keys;
    }
    while (expect(&r, ','));
    return expect(&r, '}') ? keys : -1;
}

/* A file's text, with MAX_VALUE bytes of room after it; NULL if there is none. */
static char* read_file(const char* name, DWORD* size)
{
    HANDLE file = CreateFileA(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    DWORD got;
    char* text;

    if (file == INVALID_HANDLE_VALUE)
        return NULL;
    *size = GetFileSize(file, NULL);
    text = (char*) VirtualAlloc(NULL, *size + MAX_VALUE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (text && (!ReadFile(file, text, *size, &got, NULL) || got != *size))
    {
        VirtualFree(text, 0, MEM_RELEASE);
        text = NULL;
    }
    CloseHandle(file);
    return text;
}

static void load(void)
{
    DWORD size;
    char* text = read_file(PROFILE_FILE, &size);
    int keys;

    if (!text)
    {
        log_line("profile: no %s yet - the game starts from its defaults", PROFILE_FILE);
        return;
    }
    g_theirs = (char*) VirtualAlloc(NULL, size + 1, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    keys = load_text(text, size, TRUE, (BYTE*) text + size);
    if (keys < 0)
        log_line("profile: %s is malformed", PROFILE_FILE);
    else
        log_line("profile: loaded %d keys from %s", keys, PROFILE_FILE);
    VirtualFree(text, 0, MEM_RELEASE);

    text = read_file(DRIVERS_FILE, &size);
    if (text)
    {
        keys = load_text(text, size, FALSE, (BYTE*) text + size);
        log_line("profile: the renderer's card profile, %d keys from %s", keys, DRIVERS_FILE);
        VirtualFree(text, 0, MEM_RELEASE);
    }
}

/* ------------------------------------------------------------------ writing it back */

typedef LONG (WINAPI *enum_value_fn)(HKEY, DWORD, LPSTR, LPDWORD, LPDWORD, LPDWORD, LPBYTE, LPDWORD);

typedef struct writer
{
    char* text;
    DWORD length;
    DWORD capacity;
} writer;

static void put(writer* w, const char* s)
{
    while (*s && w->length < w->capacity)
        w->text[w->length++] = *s++;
}

/* A JSON string; only the backslash in key paths and quotes need escaping. */
static void put_string(writer* w, const char* s)
{
    char c[2] = { 0, 0 };

    put(w, "\"");
    for (; *s; ++s)
    {
        if (*s == '\\' || *s == '"')
            put(w, "\\");
        c[0] = *s;
        put(w, c);
    }
    put(w, "\"");
}

static void lower(char* s)
{
    for (; *s; ++s)
    {
        if (*s >= 'A' && *s <= 'Z')
            *s += 'a' - 'A';
    }
}

/* One key's values, as the launcher writes them: lower-case names, json.dump(indent=1). */
static void save_values(writer* w, HKEY key, enum_value_fn enum_value, BYTE* value)
{
    char name[MAX_NAME], number[16];
    DWORD i, name_size, type, length, b;
    int first = 1;

    put(w, "{");
    for (i = 0;; ++i)
    {
        name_size = sizeof name;
        length = MAX_VALUE;
        if (enum_value(key, i, name, &name_size, NULL, &type, value, &length))
            break;

        lower(name);
        put(w, first ? "\n  " : ",\n  ");
        first = 0;
        put_string(w, name);
        wsprintfA(number, ": [\n   %u,\n   \"", type);
        put(w, number);
        for (b = 0; b < length; ++b)
        {
            char pair[3] = { k_hex[value[b] >> 4], k_hex[value[b] & 15], 0 };
            put(w, pair);
        }
        put(w, "\"\n  ]");
    }
    put(w, first ? "}" : "\n }");
}

/* `subkey` and everything below it, each as its own entry; with `no_drivers`, not the card profile. */
static void save_key(writer* w, const char* subkey, enum_value_fn enum_value, BYTE* value, int* entries,
                     BOOL no_drivers)
{
    char path[MAX_KEY_PATH], child[MAX_NAME], below[MAX_KEY_PATH];
    HKEY key;
    DWORD i;

    join_key(path, subkey);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, path, 0, KEY_READ, &key))
        return;

    put(w, *entries ? ",\n " : "\n ");
    put_string(w, subkey);
    put(w, ": ");
    save_values(w, key, enum_value, value);
    ++*entries;

    for (i = 0; !RegEnumKeyA(key, i, child, sizeof child); ++i)
    {
        lstrcpyA(below, subkey);
        if (*subkey)
            lstrcpyA(below + lstrlenA(below), "\\");
        lstrcpyA(below + lstrlenA(below), child);
        if (!no_drivers || !is_drivers(below))
            save_key(w, below, enum_value, value, entries, no_drivers);
    }
    RegCloseKey(key);
}

/* `w` written to `name`: beside it and then moved over it, so an interrupted save cannot lose it. */
static void write_file(const char* name, const writer* w, int entries)
{
    typedef BOOL (WINAPI *move_fn)(LPCSTR, LPCSTR, DWORD);
    move_fn move = (move_fn) api("kernel32.dll", "MoveFileExA");
    char temporary[MAX_PATH];
    HANDLE file;
    DWORD written;

    wsprintfA(temporary, "%s.new", name);
    file = CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (move && entries && w->length < w->capacity && file != INVALID_HANDLE_VALUE &&
        WriteFile(file, w->text, w->length, &written, NULL) && written == w->length)
    {
        CloseHandle(file);
        if (move(temporary, name, MOVEFILE_REPLACE_EXISTING))
            log_line("profile: saved %d keys to %s", entries, name);
    }
    else if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
        DeleteFileA(temporary);
    }
}

/*
    The settings back to PROFILE_FILE with the original game's card profile as it was, and the
    renderer's to DRIVERS_FILE; then the original's put back in the registry, for the original game
    started on its own.
*/
static void save(void)
{
    typedef LONG (WINAPI *delete_tree_fn)(HKEY, LPCSTR);
    enum_value_fn enum_value = (enum_value_fn) api("advapi32.dll", "RegEnumValueA");
    delete_tree_fn delete_tree = (delete_tree_fn) api("advapi32.dll", "RegDeleteTreeA");
    char path[MAX_KEY_PATH];
    writer w;
    BYTE* value;
    int entries = 0;

    if (!enum_value)
        return;

    w.capacity = 4 << 20;
    w.text = (char*) VirtualAlloc(NULL, w.capacity + MAX_VALUE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!w.text)
        return;
    value = (BYTE*) w.text + w.capacity;

    w.length = 0;
    put(&w, "{");
    save_key(&w, "", enum_value, value, &entries, TRUE);
    if (entries && g_theirs_length && w.length + g_theirs_length + 4 < w.capacity)
    {
        put(&w, ",");
        __movsb((unsigned char*) w.text + w.length, (const unsigned char*) g_theirs, g_theirs_length);
        w.length += g_theirs_length;
    }
    put(&w, "\n}");
    write_file(PROFILE_FILE, &w, entries);

    entries = 0;
    w.length = 0;
    put(&w, "{");
    save_key(&w, DRIVERS_KEY, enum_value, value, &entries, FALSE);
    put(&w, "\n}");
    write_file(DRIVERS_FILE, &w, entries);

    join_key(path, DRIVERS_KEY);
    if (delete_tree)
        delete_tree(HKEY_LOCAL_MACHINE, path);
    if (g_theirs_length && g_theirs_length + 2 < w.capacity)
    {
        w.text[0] = '{';
        __movsb((unsigned char*) w.text + 1, (const unsigned char*) g_theirs, g_theirs_length);
        w.text[g_theirs_length + 1] = '}';
        load_text(w.text, g_theirs_length + 2, FALSE, value);
    }

    VirtualFree(w.text, 0, MEM_RELEASE);
}

/* ------------------------------------------------------------------ installing */

typedef void (WINAPI *exit_fn)(UINT);

extern exit_fn _imp__ExitProcess;       /* the game's own import slot */
static exit_fn g_exit;

static void WINAPI exit_process(UINT code)
{
    static volatile long saving;

    if (!_InterlockedExchange(&saving, 1))
        save();
    g_exit(code);
}

#ifdef OPENMCM2_DEV
static void key_redirect(void)
{
    static const char key[] = GAME_KEY;
    const char* mine = key;
    int i;

    for (i = 0; i < (int) (sizeof k_key_sites / sizeof *k_key_sites); i++) {
        DWORD* site = (DWORD*) k_key_sites[i];

        if (*site != RETAIL_KEY || !write_protected(site, &mine, sizeof mine))
            log_line("profile: the game's key at %08X is not as expected - left as it is", k_key_sites[i]);
    }
}
#endif

void profile_install(void)
{
    exit_fn mine = exit_process;

#ifdef OPENMCM2_DEV
    key_redirect();
#endif
    load();

    g_exit = _imp__ExitProcess;
    if (!write_protected(&_imp__ExitProcess, &mine, sizeof mine))
        log_line("profile: could not take over ExitProcess - settings will not be saved");
}
