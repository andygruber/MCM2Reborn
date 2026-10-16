/*
    OpenMCM2 dev build - what MCM2:Reborn's race HUD shows (hud.h), read from the game each frame.

    The race under way is game_FUN_0045e300's (the BikeRace of the event running). Its riders come
    from its iterator (0x00421420), the local player first at +0x38; each rider is a KrustyBike
    holding its place, laps, times and the gaps' tracking, and its engine. The track's line is the
    track file's (SX and nationals) or the gates (Baja and enduro); quarry and tag have none.
*/

#include "hud.h"
#include "game.h"

#include <float.h>

BYTE* __fastcall game_FUN_0045e300(BYTE* events);                       /* the race under way */
BYTE* __fastcall game_FUN_00421420(BYTE* race, void* edx_unused, int* cursor);  /* its riders */

#define FT_PER_S_TO_MPH 0.68181825f             /* the original gauge's own factor */
#define race_kind       FIELD(game_main, 0x2D74, int)   /* 0 quarry, 1 Baja, 2 national, 3 SX, 4 tag, 5 enduro */
#define race_mode       FIELD(game_main, 0x2D70, int)
#define race_laps       FIELD(game_main, 0x2D90, int)
#define race_track      ((const char*) game_main + 0x2D70 + 0x36)
#define baja_event      FIELD(game_main, 0x560, BYTE*)
#define quarry_event    FIELD(game_main, 0x55C, BYTE*)
#define tag_event       FIELD(game_main, 0x568, BYTE*)
#define race_minutes    FIELD(game_main, 0x2EB0, float)     /* a timed event's length */

/* A rider (KrustyBike). */
#define rider_x(r)      FIELD(r, 0x0C, float)
#define rider_z(r)      FIELD(r, 0x14, float)
#define rider_speed(r)  FIELD(r, 0xBC, float)   /* feet a second */
#define rider_engine(r) FIELD(r, 0x480, BYTE*)
#define rider_left(r)   FIELD(r, 0x4A0, int)    /* gone from the race */
#define rider_name(r)   ((const char*) (r) + 0x5E0)
#define rider_last(r)   FIELD(r, 0x74C, float)  /* seconds */
#define rider_best(r)   FIELD(r, 0x750, float)
#define rider_clock(r)  FIELD(r, 0x754, float)
#define rider_place(r)  FIELD(r, 0x784, int)
#define rider_laps(r)   FIELD(r, 0x7A0, unsigned short)
#define rider_score(r)  FIELD(r, 0x768, float)

/* The race's tracker: a node a rider, in race order once ranked. */
#define race_trackers(w)    FIELD(w, 0xC4, BYTE*)
#define tracker_rider(n)    FIELD(n, 0x04, BYTE*)
#define tracker_to_go(n)    FIELD(n, 0x0C, float)   /* along the track to the line */
#define tracker_lap_fix(n)  FIELD(n, 0x18, int)     /* -1 short of the line */
#define tracker_next(n)     FIELD(n, 0x50, BYTE*)

static int milliseconds(float seconds)
{
    return seconds > 0 && seconds < FLT_MAX / 2 ? (int) (seconds * 1000) : 0;
}

static void copy_name(char* to, const char* from, int size)
{
    int i;

    for (i = 0; i < size - 1 && from[i]; i++)
        to[i] = from[i] >= 'a' && from[i] <= 'z' ? from[i] - 'a' + 'A' : from[i];
    to[i] = 0;
}

/* ---- The track's line, read once a race ---- */

static BYTE* g_track_race;              /* the race the line was read for */

static void part_begin(hud_view* v)
{
    if (v->parts < HUD_PARTS_MOST)
        v->part_start[v->parts++] = v->track_count;
}

static void track_add(hud_view* v, float x, float z)
{
    if (v->track_count < HUD_TRACK_MOST) {
        v->track[v->track_count][0] = x;
        v->track[v->track_count][1] = z;
        v->track_count++;
    }
}

/*
    SX and nationals: the track file's sections (race+0x48 points at the pointer to the first, as
    the radar reads them, 0x00521D70), each a loop of points 0x30 bytes long (its two edges' x, y
    and z at +0x0C and +0x18, the next point at +0x2C) and its children (count +0x10, the array of
    them +0x14). Each is a line of the map through the middle of its edges, thinned to its share.
*/
#define SECTIONS_MOST 64

static void section_line(hud_view* v, BYTE* section, int share)
{
    BYTE* head = FIELD(section, 0x08, BYTE*);
    BYTE* p;
    int count = 0, step, k = 0;

    if (!head || v->parts == HUD_PARTS_MOST)
        return;
    p = head;
    do {
        count++;
        p = FIELD(p, 0x2C, BYTE*);
    } while (p && p != head && count < 100000);
    step = count / (share > 2 ? share - 1 : 1) + 1;
    part_begin(v);
    p = head;
    do {
        if (k++ % step == 0)
            track_add(v, (FIELD(p, 0x0C, float) + FIELD(p, 0x18, float)) / 2,
                      (FIELD(p, 0x14, float) + FIELD(p, 0x20, float)) / 2);
        p = FIELD(p, 0x2C, BYTE*);
    } while (p && p != head && k < count);
    if (p == head)                      /* a loop: closed */
        track_add(v, (FIELD(head, 0x0C, float) + FIELD(head, 0x18, float)) / 2,
                  (FIELD(head, 0x14, float) + FIELD(head, 0x20, float)) / 2);
}

static void track_from_file(hud_view* v, BYTE* race)
{
    BYTE* found[SECTIONS_MOST];
    BYTE** first = FIELD(race, 0x48, BYTE**);
    int count = 0, i, k;

    if (!first || !*first)
        return;
    found[count++] = *first;
    for (i = 0; i < count; i++) {           /* every section once, through the children */
        int children = FIELD(found[i], 0x10, int);
        BYTE** child = FIELD(found[i], 0x14, BYTE**);

        for (k = 0; child && k < children && count < SECTIONS_MOST; k++) {
            int seen = 0, j;

            for (j = 0; j < count; j++)
                seen |= found[j] == child[k];
            if (!seen && child[k])
                found[count++] = child[k];
        }
    }
    for (i = 0; i < count; i++)
        section_line(v, found[i], HUD_TRACK_MOST / count);
}

/* Baja and enduro: the gates, in order (each 0x18 bytes from +0xD8, x y z first). */
static void track_from_gates(hud_view* v)
{
    BYTE* gates = baja_event;
    int count, i;

    if (!gates)
        return;
    part_begin(v);
    count = FIELD(gates, 0xAC, int);
    for (i = 0; i < count && i < HUD_TRACK_MOST; i++)
        track_add(v, FIELD(gates, 0xD8 + i * 0x18, float), FIELD(gates, 0xE0 + i * 0x18, float));
}

static void track_read(hud_view* v, BYTE* race)
{
    const char* name = race_track;
    const char* c;
    int length;

    v->track_count = 0;
    v->parts = 0;
    if (race_kind == 2 || race_kind == 3)
        track_from_file(v, race);
    else if (race_kind == 1 || race_kind == 5)
        track_from_gates(v);

    for (c = name; *c; c++)             /* the track file's name, without folder or extension */
        if (*c == '\\' || *c == '/')
            name = c + 1;
    copy_name(v->track_name, name, sizeof v->track_name);
    for (length = lstrlenA(v->track_name); length > 0; length--)
        if (v->track_name[length - 1] == '.') {
            v->track_name[length - 1] = 0;
            break;
        }
}

/* ---- The riders ---- */

static void riders_read(hud_view* v, BYTE* race, BYTE* me)
{
    BYTE* rider;
    int cursor = 0;

    v->rider_count = 0;
    v->riders = 0;
    while ((rider = game_FUN_00421420(race, NULL, &cursor)) != NULL) {
        hud_rider* r;

        if (rider_left(rider))
            continue;
        v->riders++;
        if (v->rider_count == HUD_RIDERS_MOST)
            continue;
        r = &v->rider[v->rider_count++];
        r->x = rider_x(rider);
        r->z = rider_z(rider);
        r->place = rider_place(rider);
        r->player = rider == me;
    }
}

/*
    The riders just ahead and behind, from the tracker's race order, and how far in time: on the
    same lap, the distance between them at their speed; a lap or more apart, the time between
    their crossings of the line.
*/
static void gap_to(hud_gap* gap, BYTE* me_node, BYTE* other_node, BOOL ahead)
{
    BYTE* me = tracker_rider(me_node);
    BYTE* other = tracker_rider(other_node);
    int my_lap = rider_laps(me) + tracker_lap_fix(me_node);
    int their_lap = rider_laps(other) + tracker_lap_fix(other_node);

    gap->known = TRUE;
    copy_name(gap->name, rider_name(other), sizeof gap->name);
    if (my_lap == their_lap) {
        float speed = (rider_speed(me) + rider_speed(other)) / 2;
        float apart = tracker_to_go(ahead ? me_node : other_node) - tracker_to_go(ahead ? other_node : me_node);

        gap->gap_ms = (int) (apart / (speed > 15 ? speed : 15) * 1000);
    } else {
        int lap = (my_lap < their_lap ? my_lap : their_lap) % 100;
        const float* mine = FIELD(me, 0x77C, const float*);
        const float* theirs = FIELD(other, 0x77C, const float*);

        gap->gap_ms = mine && theirs ? milliseconds(ahead ? mine[lap] - theirs[lap] : theirs[lap] - mine[lap]) : 0;
    }
    if (gap->gap_ms < 0)
        gap->gap_ms = -gap->gap_ms;
}

static void gaps_read(hud_view* v, BYTE* race, BYTE* me)
{
    BYTE* before = NULL;
    BYTE* node;

    v->ahead.known = v->behind.known = FALSE;
    if (race_kind != 2 && race_kind != 3)
        return;
    for (node = race_trackers(race); node; before = node, node = tracker_next(node)) {
        if (tracker_rider(node) != me)
            continue;
        if (before)
            gap_to(&v->ahead, node, before, TRUE);
        if (tracker_next(node))
            gap_to(&v->behind, node, tracker_next(node), FALSE);
        return;
    }
}

/* ---- All of it ---- */

BOOL hud_read(hud_view* v)
{
    BYTE* race;
    BYTE* me;
    BYTE* engine;

    if (!game_main)
        return FALSE;
    race = game_FUN_0045e300(FIELD(game_main, 0x570, BYTE*));
    me = race ? FIELD(race, 0x38, BYTE*) : NULL;
    if (!me) {
        g_track_race = NULL;            /* the next race's line is read anew, at any address */
        return FALSE;
    }

    v->show_race = FIELD(game_main, 0xC28, int) != 0;       /* the game's own switches */
    v->show_map = FIELD(game_main, 0xC30, int) != 0;
    v->show_speed = FIELD(game_main, 0xC24, int) != 0;

    v->timed = race_kind == 0 || race_kind == 4;
    v->place = rider_place(me);
    v->laps = race_mode == 0 || race_mode == 4 ? 0 : race_laps;
    v->lap = rider_laps(me) + 1;
    if (v->laps && v->lap > v->laps)
        v->lap = v->laps;
    v->race_ms = milliseconds(rider_clock(me));
    if (v->timed) {                     /* the event's clock, minutes and seconds gone */
        BYTE* clock = race_kind == 0 ? quarry_event : tag_event;

        v->race_ms = clock ? milliseconds(race_minutes * 60 - FIELD(clock, 0x70, float) * 60
                                          - FIELD(clock, 0x74, float)) : 0;
    }
    v->last_ms = milliseconds(rider_last(me));
    v->best_ms = milliseconds(rider_best(me));
    v->score = (int) rider_score(me);

    v->speed_mph = rider_speed(me) * FT_PER_S_TO_MPH;
    engine = rider_engine(me);
    v->revs = engine ? FIELD(engine, 0x00, float) : -1;
    v->gear = engine ? FIELD(engine, 0x10, BYTE) + 1 : 0;

    if (race != g_track_race) {         /* read again each frame until the track has loaded */
        track_read(v, race);
        if (v->track_count >= 2 || race_kind == 0 || race_kind == 4)
            g_track_race = race;
    }
    riders_read(v, race, me);
    gaps_read(v, race, me);
    return TRUE;
}
