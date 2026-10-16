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
    and, for a joystick, each axis and hat where it now is.
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
    BYTE  held[BUTTONS_MOST];
} g_devices[DEVICES_MOST];

typedef HRESULT (WINAPI* create_fn)(HINSTANCE, DWORD, REFIID, void**, void*);
typedef HRESULT (WINAPI* create_device_fn)(void*, REFGUID, void**, void*);
typedef HRESULT (WINAPI* create_device_ex_fn)(void*, REFGUID, REFIID, void**, void*);
typedef HRESULT (WINAPI* acquire_fn)(void*);
typedef HRESULT (WINAPI* state_fn)(void*, DWORD, void*);
typedef HRESULT (WINAPI* data_fn)(void*, DWORD, object_data*, DWORD*, DWORD);
typedef HRESULT (WINAPI* format_fn)(void*, const data_format*);

extern create_fn _imp__DirectInputCreateEx;     /* the game's own import slot */

static create_fn           g_create;
static create_device_fn    g_create_device;
static create_device_ex_fn g_create_device_ex;
static acquire_fn          g_acquire;
static state_fn            g_get_state;
static data_fn             g_get_data;
static format_fn           g_set_format;

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

/* The device as it is now, as data: written into `to` (room for `room`), how many. */
static DWORD resync_data(struct device* d, object_data* to, DWORD room)
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
    if (d->kind == JOYSTICK)            /* the six axes, the two sliders and the four hats */
        for (offset = 0; offset < JOY_BUTTONS && n < room; offset += 4) {
            to[n].offset = offset;
            to[n].data = *(DWORD*) (state + offset);
            to[n].time = timeGetTime();
            to[n].sequence = 0;
            n++;
        }
    return n;
}

static HRESULT WINAPI get_data(void* self, DWORD object_size, object_data* data, DWORD* count, DWORD flags)
{
    struct device* d = device_of(self, FALSE);
    DWORD room = count ? *count : 0;
    HRESULT result = g_get_data(self, object_size, data, count, flags);
    DWORD i;

    if (!d)
        return result;
    if (result == DIERR_INPUTLOST || result == DIERR_NOTACQUIRED) {
        d->resync = TRUE;
        return result;
    }
    if (result < 0 || !data || !count || object_size != sizeof(object_data) || (flags & 1))  /* DIGDD_PEEK */
        return result;
    for (i = 0; i < *count; i++) {
        int b = button_of(d, data[i].offset);

        if (b >= 0)
            d->held[b] = (data[i].data & 0x80) != 0;
    }
    if (d->resync && room > *count) {
        object_data now[BUTTONS_MOST + 12];
        DWORD n = resync_data(d, now, room - *count < BUTTONS_MOST + 12 ? room - *count : BUTTONS_MOST + 12);

        /* The state first, then what was read after it. */
        for (i = *count; i-- > 0;)
            data[i + n] = data[i];
        for (i = 0; i < n; i++)
            data[i] = now[i];
        *count += n;
        d->resync = FALSE;
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
    }
    return g_set_format(self, format);
}

/* The devices' vtable (DirectInput's one for all of them): its Acquire, GetDeviceData and
   SetDataFormat ours, once. */
static void device_watch(void* device)
{
    void** vtable = *(void***) device;
    void* mine[3] = { (void*) acquire, (void*) get_data, (void*) set_format };

    if (g_get_data)
        return;
    g_acquire = (acquire_fn) vtable[7];
    g_get_state = (state_fn) vtable[9];
    g_get_data = (data_fn) vtable[10];
    g_set_format = (format_fn) vtable[11];
    if (!write_protected(&vtable[7], &mine[0], sizeof mine[0])
        || !write_protected(&vtable[10], &mine[1], sizeof mine[1])
        || !write_protected(&vtable[11], &mine[2], sizeof mine[2]))
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

void inputfix_install(void)
{
    create_fn mine = create;

    g_create = _imp__DirectInputCreateEx;
    if (!write_protected(&_imp__DirectInputCreateEx, &mine, sizeof mine))
        log_line("input: could not take over DirectInputCreateEx");
}
