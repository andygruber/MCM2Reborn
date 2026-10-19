/*
    OpenMCM2 dev build - the controls after the window loses and regains the focus.

    The game reads its keyboard, mouse and joysticks as DirectInput's buffered data: a press, a
    release, an axis's new place. While another window has the focus the devices are lost, and
    what happened meanwhile never arrives: a key or button let go out there stays held in the
    game, and a stick that went back to the middle stays pushed (alt-tabbing flipped the camera
    and locked the controls).

    So every device the game makes is watched: what is held is followed through its data, and
    each time the game acquires the device again (after it was lost, or the first time) the next
    data it reads begins with the device as it is now - a release for each button no longer held
    and, for a joystick, each axis and hat where it now is. Each read of the keyboard or a joystick
    after that also checks what it thinks held against the device itself, and lets go what is no
    longer down (not the mouse's: its state is its movement since the last, which the game reads).

    And the game gets its devices back only from the window's focus messages: its keyboard read
    acquires again on DIERR_INPUTLOST alone, and the game's own Unacquire leaves it at
    DIERR_NOTACQUIRED (scratchpad input/input.md, "Focus / alt-tab"). So a read that finds its
    device not acquired while the game's window is the one in front acquires it and reads again.

    Modern pads are read independently (pad.c, padsony.c). START pauses; B/Circle or BACK
    taps Escape in menus, closing an open dropdown first. UI focus and activation run in
    ui.c without a legacy joystick. Legacy joystick reads return neutral input before entering
    their driver, avoiding duplicate actions and the old buffered joystick path altogether.
*/

#include "dev.h"

#include <mmsystem.h>

#define DI_OK              0
#define DIERR_INPUTLOST    ((HRESULT) 0x8007001E)
#define DIERR_NOTACQUIRED  ((HRESULT) 0x8007000C)

#define DEVICES_MOST 8
#define BUTTONS_MOST 256                /* a keyboard's keys; a joystick's buttons from JOY_BUTTONS */
#define JOY_BUTTONS  0x30               /* DIJOYSTATE: six axes, two sliders, four hats, then buttons */

typedef struct object_data {            /* DIDEVICEOBJECTDATA */
    DWORD offset, data, time, sequence;
} object_data;

typedef struct data_format {            /* DIDATAFORMAT, its size of data alone */
    DWORD size, object_size, flags, data_size;
} data_format;

enum kind { OTHER, KEYBOARD, MOUSE, JOYSTICK };

static struct device {
    void* self;
    int   kind;
    DWORD state_size;                   /* GetDeviceState's, by the data format set */
    BOOL  resync;                       /* acquired again: the next data starts from the state */
    BOOL  xinput;                       /* a joystick that is an XInput pad */
    BYTE  held[BUTTONS_MOST];
    DWORD reads, items, resyncs, since; /* [trace] input=1: this second's */
    HRESULT last;
} g_devices[DEVICES_MOST];

typedef HRESULT (WINAPI* create_fn)(HINSTANCE, DWORD, REFIID, void**, void*);
typedef HRESULT (WINAPI* create_device_fn)(void*, REFGUID, void**, void*);
typedef HRESULT (WINAPI* create_device_ex_fn)(void*, REFGUID, REFIID, void**, void*);
typedef HRESULT (WINAPI* acquire_fn)(void*);
typedef HRESULT (WINAPI* state_fn)(void*, DWORD, void*);
typedef HRESULT (WINAPI* data_fn)(void*, DWORD, object_data*, DWORD*, DWORD);
typedef HRESULT (WINAPI* format_fn)(void*, const data_format*);
typedef HRESULT (WINAPI* info_fn)(void*, BYTE* instance);
typedef HRESULT (WINAPI* property_fn)(void*, const void* property, void* header);
typedef HWND    (WINAPI* foreground_fn)(void);
typedef DWORD   (WINAPI* window_process_fn)(HWND, DWORD*);
typedef DWORD   (WINAPI* process_id_fn)(void);

extern create_fn _imp__DirectInputCreateEx;     /* the game's own import slot */

static create_fn           g_create;
static create_device_fn    g_create_device;
static create_device_ex_fn g_create_device_ex;
static acquire_fn          g_acquire;
static state_fn            g_get_state;
static data_fn             g_get_data;
static format_fn           g_set_format;
static info_fn             g_get_info;
static property_fn         g_get_property;
static BOOL                g_start_was;

#define DEVICE_TYPE         36          /* DIDEVICEINSTANCE: dwDevType, its type then its subtype */
#define TYPE_JOYSTICK       4           /* DIDEVTYPE_JOYSTICK */
#define SUBTYPE_TRADITIONAL 2
#define SUBTYPE_GAMEPAD     4
#define KEY_ESCAPE          0x01        /* DIK_ESCAPE */
#define PROPERTY_PATH       ((const void*) 12)  /* DIPROP_GUIDANDPATH */

typedef struct guid_and_path {          /* DIPROPGUIDANDPATH */
    DWORD size, header_size, object, how;
    GUID  class_guid;
    WCHAR path[MAX_PATH];
} guid_and_path;

/* A joystick that is an XInput pad: its device path names it so ("IG_"). */
static BOOL is_xinput(void* self)
{
    guid_and_path p = { sizeof p, 16, 0, 0 };
    int i;

    if (!g_get_property || g_get_property(self, PROPERTY_PATH, &p) < 0)
        return FALSE;
    for (i = 0; i + 2 < MAX_PATH && p.path[i]; i++)
        if ((p.path[i] == 'I' || p.path[i] == 'i') && (p.path[i + 1] == 'G' || p.path[i + 1] == 'g')
            && p.path[i + 2] == '_')
            return TRUE;
    return FALSE;
}

/* Legacy joystick bindings are retired. The modern reader owns controllers. */
static BOOL silenced(const struct device* d)
{
    return d->kind == JOYSTICK;
}

/* The window in front is one of the game's. */
static BOOL game_in_front(void)
{
    static foreground_fn foreground;
    static window_process_fn window_process;
    static process_id_fn process_id;
    DWORD owner = 0;
    HWND window;

    if (!foreground) {
        foreground = (foreground_fn) api("user32.dll", "GetForegroundWindow");
        window_process = (window_process_fn) api("user32.dll", "GetWindowThreadProcessId");
        process_id = (process_id_fn) api("kernel32.dll", "GetCurrentProcessId");
    }
    if (!foreground || !window_process || !process_id)
        return FALSE;
    window = foreground();
    return window && window_process(window, &owner) && owner == process_id();
}

BOOL input_in_foreground(void)
{
    return game_in_front();
}

static struct device* device_of(void* self, BOOL make)
{
    int i;

    for (i = 0; i < DEVICES_MOST; i++)
        if (g_devices[i].self == self)
            return &g_devices[i];
    if (!make)
        return NULL;
    for (i = 0; i < DEVICES_MOST; i++)
        if (!g_devices[i].self) {
            ZeroMemory(&g_devices[i], sizeof g_devices[i]);
            g_devices[i].self = self;
            return &g_devices[i];
        }
    return NULL;
}

BOOL input_is_xinput(void* device)
{
    struct device* d = device_of(device, FALSE);

    return d && d->xinput;
}

/* The button an offset is, as an index into held[], or -1 for an axis or hat. */
static int button_of(const struct device* d, DWORD offset)
{
    if (d->kind == KEYBOARD)
        return offset < BUTTONS_MOST ? (int) offset : -1;
    if (d->kind == MOUSE)
        return offset >= 12 && offset < 20 ? (int) offset : -1;     /* DIMOUSESTATE's buttons */
    if (d->kind == JOYSTICK)
        return offset >= JOY_BUTTONS && offset < JOY_BUTTONS + BUTTONS_MOST ? (int) (offset - JOY_BUTTONS) : -1;
    return -1;
}

/* The device as it is now, as data: written into `to` (room for `room`), how many. The buttons
   no longer down are let go; with `axes`, a joystick's axes and hats come too. */
static DWORD resync_data(struct device* d, object_data* to, DWORD room, BOOL axes)
{
    BYTE state[272];                    /* DIJOYSTATE2, the largest */
    DWORD n = 0, offset;
    int b;

    if (!d->state_size || d->state_size > sizeof state || g_get_state(d->self, d->state_size, state) < 0)
        return 0;
    for (b = 0; b < BUTTONS_MOST && n < room; b++) {
        DWORD at = d->kind == JOYSTICK ? JOY_BUTTONS + b : (DWORD) b;

        if (!d->held[b] || at >= d->state_size || (state[at] & 0x80))
            continue;
        to[n].offset = at;
        to[n].data = 0;
        to[n].time = timeGetTime();
        to[n].sequence = 0;
        n++;
        d->held[b] = 0;
    }
    if (axes && d->kind == JOYSTICK)    /* the six axes, the two sliders and the four hats */
        for (offset = 0; offset < JOY_BUTTONS && n < room; offset += 4) {
            to[n].offset = offset;
            to[n].data = *(DWORD*) (state + offset);
            to[n].time = timeGetTime();
            to[n].sequence = 0;
            n++;
        }
    return n;
}

/* [trace] input=1: once a second, what each device's reads came to. */
static void trace(struct device* d, HRESULT result, DWORD items)
{
    static const char* kinds[] = { "other", "keyboard", "mouse", "joystick" };

    if (!dev_settings.trace_input)
        return;
    d->reads++;
    d->items += items;
    d->last = result;
    if (timeGetTime() - d->since < 1000)
        return;
    dev_trace("input", "%s %08X: %lu reads, %lu items, %lu resyncs, last %08lX", kinds[d->kind], (DWORD) d->self,
              d->reads, d->items, d->resyncs, (DWORD) d->last);
    d->reads = d->items = d->resyncs = 0;
    d->since = timeGetTime();
}

static HRESULT WINAPI acquire(void* self);

/* A read that found its device lost or not acquired, the game's window in front: acquired again. */
static BOOL acquired_again(struct device* d, HRESULT result)
{
    if (result != DIERR_INPUTLOST && result != DIERR_NOTACQUIRED)
        return FALSE;
    d->resync = TRUE;
    return game_in_front() && acquire(d->self) >= 0;
}

static HRESULT WINAPI get_state(void* self, DWORD size, void* state)
{
    struct device* d = device_of(self, FALSE);
    HRESULT result;

    if (d && silenced(d) && state && size >= JOY_BUTTONS) {
        LONG* axes = (LONG*) state;             /* the axes and sliders in the middle, no hat, nothing down */
        int k;

        ZeroMemory(state, size);
        for (k = 0; k < 8; k++)
            axes[k] = 32767;
        for (k = 8; k < 12; k++)
            axes[k] = -1;
        return DI_OK;
    }
    result = g_get_state(self, size, state);
    if (d && acquired_again(d, result))
        result = g_get_state(self, size, state);
    return result;
}

static HRESULT WINAPI get_data(void* self, DWORD object_size, object_data* data, DWORD* count, DWORD flags)
{
    struct device* d = device_of(self, FALSE);
    DWORD room = count ? *count : 0;
    HRESULT result;
    DWORD i;

    if (d && silenced(d) && count) {
        *count = 0;
        return DI_OK;
    }
    result = g_get_data(self, object_size, data, count, flags);
    if (!d)
        return result;
    if (acquired_again(d, result)) {
        if (count)
            *count = room;
        result = g_get_data(self, object_size, data, count, flags);
    }
    trace(d, result, result >= 0 && count ? *count : 0);
    if (result < 0)
        return result;
    if (result < 0 || !data || !count || object_size != sizeof(object_data) || (flags & 1))  /* DIGDD_PEEK */
        return result;
    if (silenced(d)) {
        *count = 0;
        return result;
    }
    for (i = 0; i < *count; i++) {
        int b = button_of(d, data[i].offset);

        if (b >= 0)
            d->held[b] = (data[i].data & 0x80) != 0;
    }
    if (room > *count && (d->resync || d->kind == KEYBOARD || d->kind == JOYSTICK)) {
        object_data now[BUTTONS_MOST + 12];
        DWORD n = resync_data(d, now, room - *count < BUTTONS_MOST + 12 ? room - *count : BUTTONS_MOST + 12,
                              d->resync);

        /* The state first, then what was read after it. */
        for (i = *count; i-- > 0;)
            data[i + n] = data[i];
        for (i = 0; i < n; i++)
            data[i] = now[i];
        *count += n;
        if (d->resync)
            d->resyncs++;
        d->resync = FALSE;
    }
    if (d->kind == KEYBOARD) {
        BOOL binding = pad_binding_active();
        BOOL start = pad_start_down();
        BOOL escape_changed = start != g_start_was;
        BOOL escape_room = room >= *count + 2;

        if (!binding && game_in_front() && escape_changed && start && escape_room) {
            for (i = 0; i < 2; i++) {
                data[*count].offset = KEY_ESCAPE;
                data[*count].data = i ? 0 : 0x80;
                data[*count].time = timeGetTime();
                data[*count].sequence = 0;
                (*count)++;
            }
        }
        if (escape_changed && (!start || escape_room))
            g_start_was = start;

    }
    return result;
}

static HRESULT WINAPI acquire(void* self)
{
    HRESULT result = g_acquire(self);
    struct device* d = device_of(self, FALSE);

    if (d && result == DI_OK)           /* newly acquired (S_FALSE: it was still) */
        d->resync = TRUE;
    return result;
}

static HRESULT WINAPI set_format(void* self, const data_format* format)
{
    struct device* d = device_of(self, TRUE);

    if (d && format) {
        d->state_size = format->data_size;
        d->kind = format->data_size == 256 ? KEYBOARD
                : format->data_size == 16 || format->data_size == 20 ? MOUSE
                : format->data_size == 80 || format->data_size == 272 ? JOYSTICK : OTHER;
        ZeroMemory(d->held, sizeof d->held);
        d->xinput = d->kind == JOYSTICK && is_xinput(self);
        if (d->xinput)
            log_line("input: joystick %08X is an XInput pad", (DWORD) self);
    }
    return g_set_format(self, format);
}

static HRESULT WINAPI get_info(void* self, BYTE* instance)
{
    HRESULT result = g_get_info(self, instance);

    if (result >= 0 && instance && g_pad.analog_di && instance[DEVICE_TYPE] == TYPE_JOYSTICK
        && instance[DEVICE_TYPE + 1] == SUBTYPE_GAMEPAD) {
        instance[DEVICE_TYPE + 1] = SUBTYPE_TRADITIONAL;
        log_line("input: a game pad steers by its axes, as a joystick");
    }
    return result;
}

/* The devices' vtable (DirectInput's one for all of them): its Acquire, GetDeviceState,
   GetDeviceData, SetDataFormat and GetDeviceInfo ours, once. */
static void device_watch(void* device)
{
    void** vtable = *(void***) device;
    void* mine[5] = { (void*) acquire, (void*) get_data, (void*) set_format, (void*) get_state,
                      (void*) get_info };

    if (g_get_data)
        return;
    g_acquire = (acquire_fn) vtable[7];
    g_get_state = (state_fn) vtable[9];
    g_get_data = (data_fn) vtable[10];
    g_set_format = (format_fn) vtable[11];
    g_get_info = (info_fn) vtable[15];
    g_get_property = (property_fn) vtable[5];
    if (!write_protected(&vtable[7], &mine[0], sizeof mine[0])
        || !write_protected(&vtable[10], &mine[1], sizeof mine[1])
        || !write_protected(&vtable[11], &mine[2], sizeof mine[2])
        || !write_protected(&vtable[9], &mine[3], sizeof mine[3])
        || !write_protected(&vtable[15], &mine[4], sizeof mine[4]))
        log_line("input: could not watch DirectInput's devices");
}

static HRESULT WINAPI create_device(void* self, REFGUID guid, void** out, void* outer)
{
    HRESULT result = g_create_device(self, guid, out, outer);

    if (result >= 0 && out && *out)
        device_watch(*out);
    return result;
}

static HRESULT WINAPI create_device_ex(void* self, REFGUID guid, REFIID iid, void** out, void* outer)
{
    HRESULT result = g_create_device_ex(self, guid, iid, out, outer);

    if (result >= 0 && out && *out)
        device_watch(*out);
    return result;
}

/* DirectInput's own object: its CreateDevice and CreateDeviceEx ours, once. */
static HRESULT WINAPI create(HINSTANCE instance, DWORD version, REFIID iid, void** out, void* outer)
{
    HRESULT result = g_create(instance, version, iid, out, outer);

    if (result >= 0 && out && *out && !g_create_device) {
        void** vtable = *(void***) *out;
        void* mine[2] = { (void*) create_device, (void*) create_device_ex };

        g_create_device = (create_device_fn) vtable[3];
        g_create_device_ex = (create_device_ex_fn) vtable[9];
        if (!write_protected(&vtable[3], &mine[0], sizeof mine[0])
            || !write_protected(&vtable[9], &mine[1], sizeof mine[1]))
            log_line("input: could not watch DirectInput");
    }
    return result;
}

/* Modern controllers never create the legacy force-feedback joystick. Old saved
   effects must not enter BikeRace's three-call legacy effect block. */
static BOOL disable_legacy_race_feedback(BYTE* branch)
{
    const BYTE jump = 0xEB;
    if ((branch[0] != 0x74 && branch[0] != jump) || branch[1] != 0x41)
        return FALSE;
    return branch[0] == jump || write_protected(branch, &jump, sizeof jump);
}

void inputfix_install(void)
{
    create_fn mine = create;

    if (!disable_legacy_race_feedback((BYTE*)0x0041A456))
        log_line("input: could not disable legacy race force feedback");
    g_create = _imp__DirectInputCreateEx;
    if (!write_protected(&_imp__DirectInputCreateEx, &mine, sizeof mine))
        log_line("input: could not take over DirectInputCreateEx");
}
