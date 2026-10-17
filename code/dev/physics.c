/*
    OpenMCM2 dev build - the bike's physics as the game has them, watched: [trace] physics=1
    records each physics step (called from the bike's steering, bikesteer.c), so changes to how a
    bike rides can be measured against the stock game first.
*/

#include "dev.h"
#include "game.h"

#include <mmsystem.h>

#define bike_bailed(b)  FIELD(b, 0x444, int)
#define bike_dt(b)      FIELD(b, 0x13C, float)
#define bike_vel(b)     ((float*) ((b) + 0x64))
#define bike_forward(b) ((float*) ((b) + 0x88))
#define bike_front(b)   FIELD(b, 0x5F0, BYTE*)
#define bike_rear(b)    FIELD(b, 0x5F4, BYTE*)
#define tyre_down(t)    FIELD(t, 0x260, int)

/*
    [trace] physics=1: each physics step of each bike, physics-record.csv in the game's folder,
    to see why a bike locks up or skips across a slope. Per tyre: down (+0x260), its depth to the
    ground (+0x150), whether it was handed to the rigid collision (+0x15C), its shock's squeeze
    (0 to 1) and whether it bottomed. Angles in thousandths of a radian, the rest in hundredths.
*/
#define tyre_rigid(t)       FIELD(t, 0x15C, int)
#define tyre_shock(t, f)    FIELD(t, (f) ? 0x2B0 : 0x2AC, BYTE*)

static HANDLE g_record = INVALID_HANDLE_VALUE;

static int hundredths(float x)
{
    return (int) (x * 100);
}

static int tyre_fields(char* line, BYTE* tyre, BOOL front)
{
    BYTE* shock = tyre ? tyre_shock(tyre, front) : NULL;

    if (!tyre)
        return wsprintfA(line, ",0,0,0,0,0");
    return wsprintfA(line, ",%d,%d,%d,%d,%d", tyre_down(tyre) != 0, hundredths(FIELD(tyre, 0x150, float)),
                     tyre_rigid(tyre) != 0, shock ? hundredths(FIELD(shock, 0x70, float)) : -1,
                     shock ? FIELD(shock, 0x50, int) != 0 : -1);
}

void physics_record(BYTE* bike)
{
    char line[512];
    const float* v = bike_vel(bike);
    const float* f = bike_forward(bike);
    DWORD done;
    int n;

    if (!dev_settings.trace_physics)
        return;
    if (g_record == INVALID_HANDLE_VALUE) {
        static const char header[] =
            "ms,bike,dt,speed,steer,turn,lean,bailed,wheelie,"
            "front down,front depth,front rigid,front squeeze,front bottomed,"
            "rear down,rear depth,rear rigid,rear squeeze,rear bottomed,vel x,vel y,vel z,fwd x,fwd y,fwd z\r\n";

        g_record = CreateFileA("physics-record.csv", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                               FILE_ATTRIBUTE_NORMAL, NULL);
        if (g_record == INVALID_HANDLE_VALUE)
            return;
        WriteFile(g_record, header, sizeof header - 1, &done, NULL);
    }
    n = wsprintfA(line, "%lu,%04X,%d,%d,%d,%d,%d,%d,%d", timeGetTime(), (unsigned) ((UINT_PTR) bike & 0xFFFF),
                  hundredths(bike_dt(bike) * 1000), hundredths(FIELD(bike, 0x434, float)),
                  (int) (FIELD(FIELD(bike, 0x47C, BYTE*), 4, float) * 1000), (int) (FIELD(bike, 0x4B8, float) * 1000),
                  (int) (FIELD(bike, 0x4AC, float) * 1000), bike_bailed(bike) != 0, FIELD(bike, 0x700, int) != 0);
    n += tyre_fields(line + n, bike_front(bike), TRUE);
    n += tyre_fields(line + n, bike_rear(bike), FALSE);
    n += wsprintfA(line + n, ",%d,%d,%d,%d,%d,%d\r\n", hundredths(v[0]), hundredths(v[1]), hundredths(v[2]),
                   hundredths(f[0]), hundredths(f[1]), hundredths(f[2]));
    WriteFile(g_record, line, n, &done, NULL);
}
