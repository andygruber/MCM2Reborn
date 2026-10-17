/*
    OpenMCM2 dev build - more riders on the gate ([reborn] riders, RIDERS ON THE GATE on the REBORN
    page): up to 30 in a single race, where the game allowed 11 (the player and 10 computers).
    Series and pro circuit races stay at 11, as their standings, points and save files are made for
    11 (dlgprocs.c clamps them as the race loads).

    The game keeps several of its riders' tables in fixed room for 11 (scratchpad riders/riders.md
    has them all). Those made bigger here, at start-up, each found by its bytes and left alone if
    they are not the game's as known:
      - BikeRace::LoadBikes (0x0041A770) kept each rider's start slot in 27 ints of its stack, the
        28th over the path it builds for the bike's files: its seven uses now read a table of 32.
      - The radar (RadarOverlay) and the visual cue (VisualCue) each list 11 bikes, the 12th over
        the list's count: the lists move to the end of their objects, made bigger.
    The race's standings, the end-of-race sort and the quarry's ranking have room for 32 in
    standings.c.
*/

#include "dev.h"

#define RIDERS_MOST 32

static int g_start_slots[RIDERS_MOST];

/* A site: the bytes the game has there, and what goes in their place (an address of ours at `at`). */
typedef struct site {
    DWORD       address;
    BYTE        was[8];
    BYTE        now[8];
    int         length;
    int         at;             /* where in `now` the address of ours goes, or -1 */
    const void* target;
} site;

/* lea r32, [esp + disp32] -> mov r32, imm32 (+ 2 nops); mov r32, [esp + disp32] -> mov r32, [abs] */
static const site slot_sites[] = {
    { 0x0041A954, { 0x8D, 0x9C, 0x24, 0x8C, 0x01, 0x00, 0x00 }, { 0xBB, 0, 0, 0, 0, 0x90, 0x90 }, 7, 1, g_start_slots },
    { 0x0041A99F, { 0x8D, 0x94, 0x24, 0x8C, 0x01, 0x00, 0x00 }, { 0xBA, 0, 0, 0, 0, 0x90, 0x90 }, 7, 1, g_start_slots },
    { 0x0041AA8D, { 0x8B, 0x8C, 0x24, 0x8C, 0x01, 0x00, 0x00 }, { 0x8B, 0x0D, 0, 0, 0, 0, 0x90 }, 7, 2, g_start_slots },
    { 0x0041AE53, { 0x8B, 0x94, 0x9C, 0x8C, 0x01, 0x00, 0x00 }, { 0x8B, 0x14, 0x9D, 0, 0, 0, 0 }, 7, 3, g_start_slots },
    { 0x0041C245, { 0x8B, 0x84, 0x24, 0x8C, 0x01, 0x00, 0x00 }, { 0xA1, 0, 0, 0, 0, 0x90, 0x90 }, 7, 1, g_start_slots },
    { 0x0041C5DA, { 0x8B, 0x94, 0x24, 0x98, 0x01, 0x00, 0x00 }, { 0x8B, 0x15, 0, 0, 0, 0, 0x90 }, 7, 2, g_start_slots },
    { 0x0041C882, { 0x8D, 0x8C, 0x24, 0x90, 0x01, 0x00, 0x00 }, { 0xB9, 0, 0, 0, 0, 0x90, 0x90 }, 7, 1, &g_start_slots[1] },
};

/*
    The overlays' bike lists, each moved to the end of its object, made bigger to hold 32
    (scratchpad riders/gen_sites.py makes these from the game: each changes one dword).
*/
/* RadarOverlay (vtable 0x0055DB54): its 11 bikes at +0x134 to 32 at +0x208 */
static const site radar_sites[] = {
    { 0x004E518B, { 0x68, 0x08, 0x02, 0x00, 0x00 }, { 0x68, 0x88, 0x02, 0x00, 0x00 }, 5, -1, NULL },   /* push 0x208 */
    { 0x00520CF9, { 0x8D, 0xBB, 0x34, 0x01, 0x00, 0x00 }, { 0x8D, 0xBB, 0x08, 0x02, 0x00, 0x00 }, 6, -1, NULL },   /* lea edi, [ebx + 0x134] */
    { 0x00520CFF, { 0xB9, 0x0B, 0x00, 0x00, 0x00 }, { 0xB9, 0x20, 0x00, 0x00, 0x00 }, 5, -1, NULL },   /* mov ecx, 0xb */
    { 0x00521179, { 0x89, 0x84, 0x8E, 0x34, 0x01, 0x00, 0x00 }, { 0x89, 0x84, 0x8E, 0x08, 0x02, 0x00, 0x00 }, 7, -1, NULL },   /* mov dword ptr [esi + ecx*4 + 0x134], eax */
    { 0x00521541, { 0x8D, 0x95, 0x34, 0x01, 0x00, 0x00 }, { 0x8D, 0x95, 0x08, 0x02, 0x00, 0x00 }, 6, -1, NULL },   /* lea edx, [ebp + 0x134] */
    { 0x00521555, { 0x8B, 0x94, 0x85, 0x34, 0x01, 0x00, 0x00 }, { 0x8B, 0x94, 0x85, 0x08, 0x02, 0x00, 0x00 }, 7, -1, NULL },   /* mov edx, dword ptr [ebp + eax*4 + 0x134] */
    { 0x00521575, { 0x8B, 0x94, 0x85, 0x34, 0x01, 0x00, 0x00 }, { 0x8B, 0x94, 0x85, 0x08, 0x02, 0x00, 0x00 }, 7, -1, NULL },   /* mov edx, dword ptr [ebp + eax*4 + 0x134] */
    { 0x00521587, { 0x8B, 0x94, 0x85, 0x34, 0x01, 0x00, 0x00 }, { 0x8B, 0x94, 0x85, 0x08, 0x02, 0x00, 0x00 }, 7, -1, NULL },   /* mov edx, dword ptr [ebp + eax*4 + 0x134] */
    { 0x005215E3, { 0x8D, 0xBD, 0x34, 0x01, 0x00, 0x00 }, { 0x8D, 0xBD, 0x08, 0x02, 0x00, 0x00 }, 6, -1, NULL },   /* lea edi, [ebp + 0x134] */
    { 0x00521681, { 0x8B, 0x84, 0x95, 0x34, 0x01, 0x00, 0x00 }, { 0x8B, 0x84, 0x95, 0x08, 0x02, 0x00, 0x00 }, 7, -1, NULL },   /* mov eax, dword ptr [ebp + edx*4 + 0x134] */
};

/* VisualCue (vtable 0x005593D8): its 11 bikes at +0xCC to 32 at +0xFC */
static const site cue_sites[] = {
    { 0x004E4EA8, { 0x68, 0xFC, 0x00, 0x00, 0x00 }, { 0x68, 0x7C, 0x01, 0x00, 0x00 }, 5, -1, NULL },   /* push 0xfc */
    { 0x0048BD10, { 0x8D, 0xBE, 0xCC, 0x00, 0x00, 0x00 }, { 0x8D, 0xBE, 0xFC, 0x00, 0x00, 0x00 }, 6, -1, NULL },   /* lea edi, [esi + 0xcc] */
    { 0x0048BD16, { 0xB9, 0x0B, 0x00, 0x00, 0x00 }, { 0xB9, 0x20, 0x00, 0x00, 0x00 }, 5, -1, NULL },   /* mov ecx, 0xb */
    { 0x0048BEF9, { 0x89, 0x84, 0x8E, 0xCC, 0x00, 0x00, 0x00 }, { 0x89, 0x84, 0x8E, 0xFC, 0x00, 0x00, 0x00 }, 7, -1, NULL },   /* mov dword ptr [esi + ecx*4 + 0xcc], eax */
    { 0x0048BF51, { 0x8D, 0x8E, 0xCC, 0x00, 0x00, 0x00 }, { 0x8D, 0x8E, 0xFC, 0x00, 0x00, 0x00 }, 6, -1, NULL },   /* lea ecx, [esi + 0xcc] */
    { 0x0048C804, { 0x8B, 0x84, 0x86, 0xCC, 0x00, 0x00, 0x00 }, { 0x8B, 0x84, 0x86, 0xFC, 0x00, 0x00, 0x00 }, 7, -1, NULL },   /* mov eax, dword ptr [esi + eax*4 + 0xcc] */
};

#define SITES(list) list, (int) (sizeof list / sizeof list[0])

static BOOL sites_are_the_games(const site* sites, int count)
{
    int i, k;

    for (i = 0; i < count; i++)
        for (k = 0; k < sites[i].length; k++)
            if (((const BYTE*) sites[i].address)[k] != sites[i].was[k])
                return FALSE;
    return TRUE;
}

static void sites_write(const site* sites, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        BYTE now[8];
        DWORD target = (DWORD) sites[i].target;

        __movsb(now, sites[i].now, sites[i].length);
        if (sites[i].at >= 0)
            __movsb(now + sites[i].at, (const BYTE*) &target, 4);
        write_protected((void*) sites[i].address, now, sites[i].length);
    }
}

/* Each table made bigger only if all its bytes are the game's as known. */
static void enlarge(const char* what, const site* sites, int count)
{
    BOOL known = sites_are_the_games(sites, count);

    if (known)
        sites_write(sites, count);
    log_line("dev build: %s %s", what, known ? "with room for 32" : "left as it is (not the game's as known)");
}

void riders_install(void)
{
    log_line("dev build: riders on the gate %d", riders_most());
    enlarge("the riders' start slots", SITES(slot_sites));
    enlarge("the radar's riders", SITES(radar_sites));
    enlarge("the visual cue's riders", SITES(cue_sites));
}

/* The most riders a single race may have: the player and the computers. */
int riders_most(void)
{
    return g_reborn.riders > 11 && g_reborn.riders <= 30 ? g_reborn.riders : 11;
}
