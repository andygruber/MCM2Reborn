/*
    OpenMCM2 - a race's standings (EventManager.cpp, RaceStatus.cpp): the riders ranked when the
    race ends, each put into the event's standings (events + 0x50, 0x50 bytes a rider, events + 0x4C
    of them), and the quarry's ranking while it runs.

    The event's standings hold 11 riders, the most the game allows. In the dev build a race may have
    more (dev.h, g_reborn.riders): then every rider's standing is also kept in a table of its own here
    (race_standing), which the results screens read, while the event's 11 get the first riders as
    they always did - the player among them - for those that read the event's own (the high scores,
    the pro circuit's pay). The sorts here have room for RIDERS_MOST.
*/

#include "runtime.h"
#include "menus.h"

#define RIDERS_MOST      32
#define EVENT_STANDINGS  11         /* the event's room */
#define STANDING_SIZE    0x50

#define race_kind        FIELD(game_main, 0x2D74, int)
#define race_mode        FIELD(game_main, 0x2D70, int)     /* 2 a series */
#define SERIES           2

#define bike_out(b)      FIELD(b, 0x4A0, int)
#define bike_place(b)    FIELD(b, 0x784, int)

BYTE* __fastcall game_FUN_0045e300(BYTE* events);                            /* the race under way */
BYTE* __fastcall game_FUN_00421420(BYTE* race, void* edx_unused, int* cursor); /* its riders, in turn */
void  __fastcall game_FUN_0045d670(BYTE* standing, void* edx_unused, BYTE* bike);  /* a rider's standing */
void  __stdcall  game_FUN_004600f0(BYTE* bike, int* points);                 /* a series place's points */
void  __cdecl    game_FUN_00539a96(void* base, size_t count, size_t size,
                                   int (__cdecl* compare)(const void*, const void*));   /* qsort */

/* The game's comparisons: by the first int, by the second, and the series' (16 bytes, by points). */
#define BY_FIRST      ((int (__cdecl*)(const void*, const void*)) 0x0045F8C0)
#define BY_SECOND     ((int (__cdecl*)(const void*, const void*)) 0x0045F8F0)
#define BY_POINTS     ((int (__cdecl*)(const void*, const void*)) 0x0045E3E0)

typedef struct { int key; BYTE* bike; } keyed;      /* by_first */
typedef struct { BYTE* bike; int key; } bike_keyed; /* by_second */

static BYTE g_standings[RIDERS_MOST][STANDING_SIZE];
static int  g_standings_count;

/* The race's standings in rank order of their making: from the event, or past 11 from here. */
BYTE* race_standing(int index)
{
    if (g_standings_count > EVENT_STANDINGS)
        return g_standings[index];
    return FIELD(game_main, 0x570, BYTE*) + 0x50 + index * STANDING_SIZE;
}

int race_standings_count(void)
{
    if (g_standings_count > EVENT_STANDINGS)
        return g_standings_count;
    return FIELD(FIELD(game_main, 0x570, BYTE*), 0x4C, int);
}

/* Standing `index`: the event's own while it has room, else this table's alone. */
static BYTE* standing_at(BYTE* events, int index)
{
    return index < EVENT_STANDINGS ? events + 0x50 + index * STANDING_SIZE : g_standings[index];
}

/* The riders still in, by `key` (the bike's int at that offset), their places set 1, 2, ... */
static void place_by(BYTE* race, int key_offset, BOOL key_first)
{
    keyed by_key[RIDERS_MOST];
    bike_keyed by_bike[RIDERS_MOST];
    int cursor = 0, n = 0, i;
    BYTE* bike;

    while ((bike = game_FUN_00421420(race, NULL, &cursor)) != NULL && n < RIDERS_MOST) {
        if (bike_out(bike))
            continue;
        by_key[n].key = by_bike[n].key = FIELD(bike, key_offset, int);
        by_key[n].bike = by_bike[n].bike = bike;
        n++;
    }
    if (key_first) {
        game_FUN_00539a96(by_key, n, sizeof by_key[0], BY_FIRST);
        for (i = 0; i < n; i++)
            bike_place(by_key[i].bike) = i + 1;
    } else {
        game_FUN_00539a96(by_bike, n, sizeof by_bike[0], BY_SECOND);
        for (i = 0; i < n; i++)
            bike_place(by_bike[i].bike) = i + 1;
    }
}

/* Each rider's standing, in the order the race has them; points in a series (or pro circuit). */
static int standings_fill(BYTE* events, BYTE* race, BOOL points)
{
    int cursor = 0, n = 0;
    BYTE* bike;

    while ((bike = game_FUN_00421420(race, NULL, &cursor)) != NULL && n < RIDERS_MOST) {
        BYTE* entry = standing_at(events, n);

        game_FUN_0045d670(entry, NULL, bike);
        if (points && !bike_out(bike))
            game_FUN_004600f0(bike, &FIELD(entry, 0x28, int));
        n++;
    }
    return n;
}

/* The race ended: its riders placed and put into the standings (series: placed by points). */
PORT(0x0045F990)
void __fastcall race_standings_make(BYTE* self)
{
    BYTE* race = game_FUN_0045e300(self);
    int n = 0, i;

    if (race_mode == SERIES)
        self[0x48]++;
    switch (race_kind) {
    case 0:
        place_by(race, 0x768, TRUE);
        n = standings_fill(self, race, race_mode == SERIES);
        break;
    case 1:
    case 2:
    case 3:
    case 5:
        if (FIELD(game_main, 0x18, int) > 1)
            place_by(race, 0x754, FALSE);
        n = standings_fill(self, race, race_mode == SERIES || FIELD(game_main, 0x3440, int));
        break;
    case 4:
        place_by(race, FIELD(game_main, 0x2EB8, int) ? 0x768 : 0x764, TRUE);
        n = standings_fill(self, race, race_mode == SERIES);
        break;
    }
    FIELD(self, 0x4C, int) = n < EVENT_STANDINGS ? n : EVENT_STANDINGS;
    g_standings_count = n;
    for (i = 0; i < n && i < EVENT_STANDINGS; i++)
        __movsb(g_standings[i], self + 0x50 + i * STANDING_SIZE, STANDING_SIZE);

    if (race_mode == SERIES) {
        struct { int points, unused[2]; BYTE* bike; } ranked[RIDERS_MOST];
        int cursor = 0, ranked_count = 0, k;
        BYTE* bike;

        /* The riders in, by the points their standings hold, the standings taken in turn. */
        while ((bike = game_FUN_00421420(race, NULL, &cursor)) != NULL && ranked_count < RIDERS_MOST) {
            if (bike_out(bike))
                continue;
            ranked[ranked_count].points = FIELD(standing_at(self, ranked_count), 0x28, int);
            ranked[ranked_count].bike = bike;
            ranked_count++;
        }
        game_FUN_00539a96(ranked, ranked_count, sizeof ranked[0], BY_POINTS);
        cursor = 0;
        while ((bike = game_FUN_00421420(race, NULL, &cursor)) != NULL) {
            for (k = 0; k < ranked_count; k++)
                if (ranked[k].bike == bike) {
                    bike_place(bike) = k + 1;
                    break;
                }
            for (k = 0; k < ranked_count; k++) {
                BYTE* entry = standing_at(self, k);

                if (FIELD(entry, 0, int) == FIELD(bike, 0x11BC, int)
                    && (int) entry[0x30] == (int) (signed char) bike[0x11C0]) {
                    FIELD(entry, 4, int) = bike_place(bike);
                    break;
                }
            }
        }
    }
}

/* The quarry's riders ranked while it runs, by their score; at the time limit all are done. */
PORT(0x004EB060)
int __cdecl quarry_ranking(int over)
{
    BYTE* status = FIELD(game_main, 0x55C, BYTE*);
    BYTE* race;
    keyed ranked[RIDERS_MOST];
    int cursor = 0, n = 0, i;
    BYTE* bike;

    if (race_kind != 0)
        return 1;
    race = FIELD(status, 0x34, BYTE*);
    while ((bike = game_FUN_00421420(race, NULL, &cursor)) != NULL && n < RIDERS_MOST) {
        BYTE* object = bike + FIELD(FIELD(bike, 4, BYTE*), 4, int);

        if (object[0x29] & 1) {
            ranked[n].key = FIELD(bike, 0x768, int);
            ranked[n].bike = bike;
            n++;
        }
    }
    game_FUN_00539a96(ranked, n, sizeof ranked[0], BY_FIRST);
    for (i = 0; i < n; i++)
        bike_place(ranked[i].bike) = i + 1;
    if (!over && n > 0
        && (double) FIELD(game_main, 0x2EB0, float) * 60.0 - 1.0
               <= (double) FIELD(status, 0x70, float) * 60.0 + FIELD(status, 0x74, float))
        for (i = 0; i < n; i++)
            ranked[i].bike[0x7A4] = 1;
    return 1;
}
