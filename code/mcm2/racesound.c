/*
    OpenMCM2 - the race's engine sounds (RaceSound, racesnd.cpp), each frame: every rider in the
    race has a sound record (this + 0x98, 0x54 bytes each, this + 0x94 of them), sorted by how far
    its bike is from the listener, the nearest four given the four voices.

    The records have room for 11, the most the game allows. In the dev build a race may have more
    (riders.c caps the records' filling at 11, the player's first): then before each frame the
    records are given the player and the ten bikes nearest the listener, a record whose bike has
    gone further handed to one that came nearer, set up as the game sets a record up for its bike.
*/

#include "runtime.h"
#include "game.h"

int __fastcall game_orig_FUN_004e8720(BYTE* self, void* edx_unused, float dt);

#ifdef OPENMCM2_DEV
#include "../dev/dev.h"

#define RECORDS         11
#define RECORD_SIZE     0x54
#define BIKES_MOST      32

#define sound_race(s)       FIELD(s, 0x34, BYTE*)
#define sound_listener(s)   FIELD(s, 0x38, BYTE*)
#define sound_player(s)     FIELD(s, 0x2C, BYTE*)
#define sound_records(s)    FIELD(s, 0x94, int)
#define sound_record(s, i)  ((s) + 0x98 + (i) * RECORD_SIZE)

#define record_bike(r)      FIELD(r, 0x00, BYTE*)

#define bike_out(b)         FIELD(b, 0x4A0, int)

BYTE* __fastcall game_FUN_00421420(BYTE* race, void* edx_unused, int* cursor);      /* its riders, in turn */
float __fastcall game_FUN_004e91d0(BYTE* self, void* edx_unused, BYTE* from, BYTE* to);  /* flat distance, 500 at most */

/* The engine kind of a bike: 0 a two-stroke 125, 1 a 250, 2 a four-stroke (as the filling has it). */
static int engine_kind(BYTE* bike)
{
    if (FIELD(bike, 0x738, int) < 250)
        return 0;
    return FIELD(bike, 0x737, BYTE) ? 2 : 1;
}

/* A record set up for its new bike: as the filling sets it up (0x004E821D), and as the frame
   leaves it, so that nothing the bike was doing sounds as a change. */
static void record_take(BYTE* self, BYTE* record, BYTE* bike)
{
    int kind = engine_kind(bike);

    record_bike(record) = bike;
    FIELD(record, 0x0C, int) = kind;
    FIELD(record, 0x38, int) = FIELD(self, 0x840 + kind * 0x28, int);
    FIELD(record, 0x14, int) = FIELD(self, 0xC00 + kind * 0x28, int);
    FIELD(record, 0x18, int) = 0;
    FIELD(record, 0x10, int) = 1;
    FIELD(record, 0x20, int) = 0;
    FIELD(record, 0x1C, int) = 1;
    FIELD(record, 0x24, int) = FIELD(bike, 0x478, BYTE);
    FIELD(record, 0x2C, int) = FIELD(bike, 0x108, BYTE);
    FIELD(record, 0x28, int) = FIELD(record, 0x48, int);
}

/* The player and the bikes nearest the listener into the records, when there are more than they hold. */
static void records_nearest(BYTE* self)
{
    BYTE* bikes[BIKES_MOST];
    float away[BIKES_MOST];
    BYTE* wanted[RECORDS];
    BOOL  held[RECORDS] = { 0 };
    int count = 0, chosen = 0, cursor = 0, i, k;
    BYTE* bike;

    if (!sound_race(self) || !sound_listener(self) || sound_records(self) != RECORDS)
        return;
    while ((bike = game_FUN_00421420(sound_race(self), NULL, &cursor)) != NULL && count < BIKES_MOST) {
        bikes[count] = bike;
        away[count] = bike == sound_player(self) ? -1.0f
                   : bike_out(bike) ? 1000000.0f
                   : game_FUN_004e91d0(self, NULL, sound_listener(self) + 0x170, bike + 0x0C);
        count++;
    }
    if (count <= RECORDS)
        return;

    /* The nearest RECORDS, the player first (at -1). */
    while (chosen < RECORDS) {
        int nearest = -1;

        for (i = 0; i < count; i++)
            if (bikes[i] && (nearest < 0 || away[i] < away[nearest]))
                nearest = i;
        wanted[chosen++] = bikes[nearest];
        bikes[nearest] = NULL;
    }

    /* A record keeps its bike if it is still wanted; the others take the wanted bikes left over. */
    for (k = 0; k < RECORDS; k++)
        for (i = 0; i < RECORDS; i++)
            if (wanted[k] && record_bike(sound_record(self, i)) == wanted[k] && !held[i]) {
                held[i] = TRUE;
                wanted[k] = NULL;
                break;
            }
    for (k = 0, i = 0; k < RECORDS; k++) {
        if (!wanted[k])
            continue;
        while (held[i])
            i++;
        record_take(self, sound_record(self, i), wanted[k]);
        held[i] = TRUE;
    }
}
#endif

PORT(0x004E8720)
int __fastcall race_sound_frame(BYTE* self, void* edx_unused, float dt)
{
#ifdef OPENMCM2_DEV
    if (riders_most() > RECORDS)
        records_nearest(self);
#endif
    return game_orig_FUN_004e8720(self, edx_unused, dt);
}
