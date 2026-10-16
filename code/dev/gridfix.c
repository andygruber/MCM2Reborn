/*
    OpenMCM2 dev build - a crash mended in the terrain's grid (GridNode, vtable slot 1, 0x00484CC0),
    which finds the cells a segment crosses for physics sweeps, sight lines and shadows.

    Each piece of the segment picks its cell by rounding its middle, and only a 16 was ever
    clamped (to 15). A piece sitting on a cell's edge just below zero rounds to -1, and the cell
    before the table was read: a stray pointer, and the crash at 0x0048547F. Both places now keep
    the cell inside its grid: a node's 16 x 16 children from 0 to 15, a leaf's 17 x 17 points from
    0 up (their top was never clamped and is left so).
*/

#include "dev.h"

#define NODE_CLAMP      0x00485443  /* cmp ecx,16; jne; mov ecx,15; mov [ebp-14h],ecx; the same for eax */
#define NODE_RESUME     0x00485460  /* mov eax,[rows + eax*4] */
#define LEAF_LOAD       0x00485609  /* mov eax,[ebp-0Ch]; mov ecx,[ebp-14h] */
#define LEAF_RESUME     0x0048560F  /* xor ebx,ebx */

static const BYTE node_was[] = { 0x83, 0xF9, 0x10, 0x75, 0x08 };
static const BYTE leaf_was[] = { 0x8B, 0x45, 0xF4, 0x8B, 0x4D, 0xEC };

/* At NODE_CLAMP ecx holds the column, [ebp-0Ch] the row; both go on in ecx and eax. */
static __declspec(naked) void node_cell(void)
{
    __asm {
        test ecx, ecx
        jge column_low
        xor ecx, ecx
    column_low:
        cmp ecx, 15
        jle column_high
        mov ecx, 15
    column_high:
        mov dword ptr [ebp - 14h], ecx
        mov eax, dword ptr [ebp - 0Ch]
        test eax, eax
        jge row_low
        xor eax, eax
    row_low:
        cmp eax, 15
        jle row_high
        mov eax, 15
    row_high:
        mov dword ptr [ebp - 0Ch], eax
        push NODE_RESUME
        ret
    }
}

static __declspec(naked) void leaf_cell(void)
{
    __asm {
        mov eax, dword ptr [ebp - 0Ch]
        mov ecx, dword ptr [ebp - 14h]
        test eax, eax
        jge row_low
        xor eax, eax
        mov dword ptr [ebp - 0Ch], eax
    row_low:
        test ecx, ecx
        jge column_low
        xor ecx, ecx
        mov dword ptr [ebp - 14h], ecx
    column_low:
        push LEAF_RESUME
        ret
    }
}

/* A jump to `to` written over `at`, if the bytes there are still the game's; the rest nops. */
static BOOL jump_over(DWORD at, const BYTE* was, int size, void (*to)(void))
{
    BYTE code[8];
    int i;

    for (i = 0; i < size; i++)
        if (((const BYTE*) at)[i] != was[i])
            return FALSE;
    code[0] = 0xE9;
    *(DWORD*) (code + 1) = (DWORD) to - (at + 5);
    for (i = 5; i < size; i++)
        code[i] = 0x90;
    return write_protected((void*) at, code, size);
}

void gridfix_install(void)
{
    BOOL node = jump_over(NODE_CLAMP, node_was, sizeof node_was, node_cell);
    BOOL leaf = jump_over(LEAF_LOAD, leaf_was, sizeof leaf_was, leaf_cell);

    log_line("dev build: terrain grid cells kept in their grids: node %d, leaf %d", node, leaf);
}
