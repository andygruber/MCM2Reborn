/*
    OpenMCM2 - the game's settings, kept in mcm2_profile.json beside it. See profile.h.
*/

#include "profile.h"
#include "runtime.h"

#include <intrin.h>

#define PROFILE_FILE "mcm2_profile.json"
#define GAME_KEY     "Software\\Microsoft\\Microsoft Games\\Motocross Madness 2"

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

        RegSetValueExA(key, name, 0, type, value, length);
    }
    while (expect(r, ','));

    return expect(r, '}');
}

static void load(void)
{
    HANDLE file = CreateFileA(PROFILE_FILE, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    char subkey[MAX_NAME], path[MAX_KEY_PATH];
    DWORD size, got;
    char* text;
    BYTE* value;
    reader r;
    int keys = 0;

    if (file == INVALID_HANDLE_VALUE)
    {
        log_line("profile: no %s yet - the game starts from its defaults", PROFILE_FILE);
        return;
    }

    size = GetFileSize(file, NULL);
    text = (char*) VirtualAlloc(NULL, size + MAX_VALUE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!text || !ReadFile(file, text, size, &got, NULL) || got != size)
    {
        CloseHandle(file);
        if (text)
            VirtualFree(text, 0, MEM_RELEASE);
        log_line("profile: could not read %s", PROFILE_FILE);
        return;
    }
    CloseHandle(file);

    value = (BYTE*) text + size;
    r.at = text;
    r.end = text + size;

    if (expect(&r, '{') && !expect(&r, '}'))
    {
        do
        {
            HKEY key;

            if (!read_string(&r, subkey, sizeof subkey) || !expect(&r, ':'))
                break;

            join_key(path, subkey);
            if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, path, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL))
                break;

            if (!load_key(&r, key, value))
            {
                RegCloseKey(key);
                break;
            }
            RegCloseKey(key);
            ++keys;
        }
        while (expect(&r, ','));
    }

    if (r.at < r.end && !expect(&r, '}'))
        log_line("profile: %s is malformed after %d keys", PROFILE_FILE, keys);
    else
        log_line("profile: loaded %d keys from %s", keys, PROFILE_FILE);

    VirtualFree(text, 0, MEM_RELEASE);
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

/* `subkey` and everything below it, each as its own entry. */
static void save_key(writer* w, const char* subkey, enum_value_fn enum_value, BYTE* value, int* entries)
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
        save_key(w, below, enum_value, value, entries);
    }
    RegCloseKey(key);
}

static void save(void)
{
    typedef BOOL (WINAPI *move_fn)(LPCSTR, LPCSTR, DWORD);
    enum_value_fn enum_value = (enum_value_fn) api("advapi32.dll", "RegEnumValueA");
    move_fn move = (move_fn) api("kernel32.dll", "MoveFileExA");
    writer w;
    BYTE* value;
    HANDLE file;
    DWORD written;
    int entries = 0;

    if (!enum_value || !move)
        return;

    w.capacity = 4 << 20;
    w.length = 0;
    w.text = (char*) VirtualAlloc(NULL, w.capacity + MAX_VALUE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!w.text)
        return;
    value = (BYTE*) w.text + w.capacity;

    put(&w, "{");
    save_key(&w, "", enum_value, value, &entries);
    put(&w, "\n}");

    /* Written beside it and then moved over it, so an interrupted save cannot lose the profile. */
    file = CreateFileA(PROFILE_FILE ".new", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
    if (entries && w.length < w.capacity && file != INVALID_HANDLE_VALUE &&
        WriteFile(file, w.text, w.length, &written, NULL) && written == w.length)
    {
        CloseHandle(file);
        if (move(PROFILE_FILE ".new", PROFILE_FILE, MOVEFILE_REPLACE_EXISTING))
            log_line("profile: saved %d keys to %s", entries, PROFILE_FILE);
    }
    else if (file != INVALID_HANDLE_VALUE)
    {
        CloseHandle(file);
        DeleteFileA(PROFILE_FILE ".new");
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

void profile_install(void)
{
    exit_fn mine = exit_process;

    load();

    g_exit = _imp__ExitProcess;
    if (!write_protected(&_imp__ExitProcess, &mine, sizeof mine))
        log_line("profile: could not take over ExitProcess - settings will not be saved");
}
