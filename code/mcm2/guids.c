/*
    OpenMCM2 - the DirectDraw and Direct3D interface IDs, defined here because dxguid.lib, where
    they normally come from, is not linked (see tools/link.py).
*/

#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION   0x0700
#define CINTERFACE

#include <windows.h>
#include <initguid.h>
#include <ddraw.h>
#include <d3d.h>

/* Declared by the SDK without DEFINE_GUID, so INITGUID does not define it. */
DEFINE_GUID(IID_IUnknown, 0x00000000, 0x0000, 0x0000, 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46);
