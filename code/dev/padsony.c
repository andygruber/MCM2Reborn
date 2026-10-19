/* Native DualShock 4 / DualSense input, independent of the game's startup
   joystick list. DirectInput supplies the same Sony layout over USB/Bluetooth.
   Convert it to the modern pad's shared controls; no legacy game bindings. */
#define DIRECTINPUT_VERSION 0x0800
#define COBJMACROS
#include "padstate.h"
#include "game.h"
#include <dinput.h>
#include <mmsystem.h>
#define DIDFT_OPTIONAL 0x80000000u

typedef HRESULT (WINAPI* create8_fn)(HINSTANCE, DWORD, REFIID, void**, IUnknown*);
static const GUID iid_input8 = { 0xBF798030, 0x483A, 0x4DA2, {0xAA,0x99,0x5D,0x64,0xED,0x36,0x97,0x00} };
static const GUID axis_guids[7] = {
    {0xA36D02E0,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0,0}},
    {0xA36D02E1,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0,0}},
    {0xA36D02E2,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0,0}},
    {0xA36D02F4,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0,0}},
    {0xA36D02F5,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0,0}},
    {0xA36D02E3,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0,0}},
    {0xA36D02F2,0xC9F3,0x11CF,{0xBF,0xC7,0x44,0x45,0x53,0x54,0,0}},
};
static IDirectInput8A* g_input;
static IDirectInputDevice8A* g_device;
static BOOL g_tried;
static DWORD g_scanned;

static BOOL CALLBACK find_sony(const DIDEVICEINSTANCEA* instance, void* unused)
{
    DIOBJECTDATAFORMAT objects[39];
    DIDATAFORMAT format = { sizeof format, sizeof objects[0], DIDF_ABSAXIS, sizeof(DIJOYSTATE), 39, objects };
    IDirectInputDevice8A* device;
    DIPROPRANGE range = { { sizeof range, sizeof range.diph, 0, DIPH_BYOFFSET }, -32767, 32767 };
    static const DWORD offsets[7] = { 0, 4, 8, 12, 16, 20, 32 };
    int i;
    HWND window = FIELD(game_main, 0x31C, HWND);
    (void) unused;

    if ((instance->guidProduct.Data1 & 0xFFFF) != 0x054C || !window)
        return DIENUM_CONTINUE;
    if (FAILED(IDirectInput8_CreateDevice(g_input, &instance->guidInstance, &device, NULL)))
        return DIENUM_CONTINUE;
    ZeroMemory(objects, sizeof objects);
    for (i = 0; i < 7; i++) {
        objects[i].pguid = &axis_guids[i];
        objects[i].dwOfs = offsets[i];
        objects[i].dwType = (i == 6 ? DIDFT_POV : DIDFT_ABSAXIS) | DIDFT_ANYINSTANCE | DIDFT_OPTIONAL;
    }
    for (i = 0; i < 32; i++) {
        objects[7 + i].dwOfs = 48 + i;
        objects[7 + i].dwType = DIDFT_BUTTON | DIDFT_MAKEINSTANCE(i) | DIDFT_OPTIONAL;
    }
    if (FAILED(IDirectInputDevice8_SetDataFormat(device, &format))
        || FAILED(IDirectInputDevice8_SetCooperativeLevel(device, window, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND))) {
        IDirectInputDevice8_Release(device);
        return DIENUM_CONTINUE;
    }
    for (i = 0; i < 6; i++) {
        range.diph.dwObj = offsets[i];
        IDirectInputDevice8_SetProperty(device, (REFGUID) 4, &range.diph); /* DIPROP_RANGE */
    }
    IDirectInputDevice8_Acquire(device);
    g_device = device;
    log_line("pad: native PlayStation controller connected: %s", instance->tszProductName);
    return DIENUM_STOP;
}

BOOL sony_pad_read(xinput_state* state, BOOL scan)
{
    DWORD now = timeGetTime();
    DIJOYSTATE joy;
    HRESULT result;

    if (!g_tried) {
        create8_fn create = (create8_fn) api("dinput8.dll", "DirectInput8Create");
        g_tried = TRUE;
        if (create)
            create(GetModuleHandleA(NULL), 0x0800, &iid_input8, (void**) &g_input, NULL);
    }
    if (!g_input)
        return FALSE;
    if (!g_device && scan && (!g_scanned || now - g_scanned >= 1000)) {
        g_scanned = now;
        IDirectInput8_EnumDevices(g_input, DI8DEVCLASS_GAMECTRL, find_sony, NULL, DIEDFL_ATTACHEDONLY);
    }
    if (!g_device)
        return FALSE;
    ZeroMemory(&joy, sizeof joy);
    joy.lRx = joy.lRy = -32767;
    joy.rgdwPOV[0] = (DWORD) -1;
    IDirectInputDevice8_Poll(g_device);
    result = IDirectInputDevice8_GetDeviceState(g_device, sizeof joy, &joy);
    if (FAILED(result)) {
        IDirectInputDevice8_Acquire(g_device);
        IDirectInputDevice8_Poll(g_device);
        result = IDirectInputDevice8_GetDeviceState(g_device, sizeof joy, &joy);
    }
    if (FAILED(result)) {
        IDirectInputDevice8_Unacquire(g_device);
        IDirectInputDevice8_Release(g_device);
        g_device = NULL;
        log_line("pad: native PlayStation controller disconnected");
        return FALSE;
    }
    sony_pad_convert(&joy, state);
    return TRUE;
}

void sony_pad_convert(const void* raw, xinput_state* state)
{
    const DIJOYSTATE* joy = raw;
    static const WORD buttons[12] = { 0x4000,0x1000,0x2000,0x8000,0x100,0x200,0,0,0x20,0x10,0x40,0x80 };
    int i;
    ZeroMemory(state, sizeof *state);
    state->left_x = (SHORT) joy->lX;
    state->left_y = (SHORT) -joy->lY;
    state->right_x = (SHORT) joy->lZ;
    state->right_y = (SHORT) -joy->lRz;
    state->left_trigger = (BYTE) ((joy->lRx + 32767) * 255 / 65534);
    state->right_trigger = (BYTE) ((joy->lRy + 32767) * 255 / 65534);
    for (i = 0; i < 12; i++)
        if (joy->rgbButtons[i] & 0x80)
            state->buttons |= buttons[i];
    if (joy->rgbButtons[6] & 0x80) state->left_trigger = 255;
    if (joy->rgbButtons[7] & 0x80) state->right_trigger = 255;
    if (LOWORD(joy->rgdwPOV[0]) != 0xFFFF) {
        DWORD hat = joy->rgdwPOV[0];
        if (hat >= 31500 || hat <= 4500) state->buttons |= 1;
        if (hat >= 4500 && hat <= 13500) state->buttons |= 8;
        if (hat >= 13500 && hat <= 22500) state->buttons |= 2;
        if (hat >= 22500 && hat <= 31500) state->buttons |= 4;
    }
}
