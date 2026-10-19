/* Retail environment stems, by event. Variants in a Baja/Enduro environment
   inherit its origin; community MCM1 conversions remain custom tracks. */
#include "dev.h"

int g_track_source[6] = { 0 };            /* 0 all, 1 base game, 2 custom */

BOOL track_is_stock(int kind, const char* stem)
{
    static const char* const prefix[6] = { "quarry", "baja", "nat", "sx", "tag", NULL };
    static const int most[6] = { 5, 5, 12, 17, 3, 0 };
    static const char* const enduro[] = { "airfield", "farm", "openpit", "skilodge", "trailer" };
    const char* p;
    int i, n;

    if (kind < 0 || kind >= 6 || !stem)
        return FALSE;
    if (kind == 5) {
        for (i = 0; i < 5; i++)
            if (!lstrcmpiA(stem, enduro[i]))
                return TRUE;
        return FALSE;
    }
    p = prefix[kind];
    for (i = 0; p[i]; i++) {
        char c = stem[i];
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (!c || c != p[i]) return FALSE;
    }
    if (stem[i] < '0' || stem[i] > '9' || stem[i + 1] < '0' || stem[i + 1] > '9' || stem[i + 2])
        return FALSE;
    n = (stem[i] - '0') * 10 + stem[i + 1] - '0';
    return n >= 1 && n <= most[kind];
}
