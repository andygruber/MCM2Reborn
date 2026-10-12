/*
    OpenMCM2 - the menus' GUI objects, ported from the game (GUIManager.cpp in the original).

    Each function here replaces the game's at the address in its PORT line, and was checked against
    the original's instructions. Field names are what the code does with them; the original names
    are not known.
*/

#include "runtime.h"

/* Two GUI objects can be paired, each pointing at the other. */
#define PARTNER    0x2C     /* the object this one is paired with */
#define PAIRED_BY  0xC0     /* on that object: the one that paired with it */

/*
    Pair `self` with `other`, or unpair `self` with NULL. TRUE when the two end up paired, and also
    when they already were; FALSE, changing nothing, when `other` is already paired with another.
*/
PORT(0x004890B0)
BOOL __fastcall gui_pair(BYTE* self, void* edx_unused, BYTE* other)
{
    BYTE* owner = other ? *(BYTE**) (other + PAIRED_BY) : NULL;

    (void) edx_unused;
    if (owner)
        return owner == self;

    *(BYTE**) (self + PARTNER) = other;
    if (other)
        *(BYTE**) (other + PAIRED_BY) = self;
    return TRUE;
}
