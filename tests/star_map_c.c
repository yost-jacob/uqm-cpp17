#include <string.h>
#include "uqm/starmap.h"

/* Read names and identities through the real C bridge; names are not GAME_STRINGs. */
int star_map_c_contract(void) {
    const STAR_DESC *star = GetStar(0);
    char name[8];
    char unicode[3];
    utf8StringCopy(unicode, sizeof unicode, "A\xc2\xb0");
    GetClusterName(star, name, sizeof name);
    GetClusterName(star, NULL, 0);
    /* Golden hash of the original 520-record, 4,160-byte compiled C table.
     * Sentinel reconstruction and packed enum conversions are adapter duties. */
    uint64_t hash = UINT64_C(14695981039346656037);
    COUNT i;
    for (i = 0; i < STAR_CATALOGUE_SIZE; ++i) {
        const STAR_DESC *s = GetStar(i);
        const unsigned bytes[] = {(unsigned)s->star_pt.x & 255, (unsigned)s->star_pt.x >> 8,
            (unsigned)s->star_pt.y & 255, (unsigned)s->star_pt.y >> 8,
            s->Type, s->Index, s->Prefix, s->Postfix};
        size_t b;
        for (b = 0; b < sizeof bytes / sizeof bytes[0]; ++b)
            hash = (hash ^ bytes[b]) * UINT64_C(1099511628211);
        if (GetStarIndex(s) != i)
            return 0;
    }
    /* Legacy cursor semantics remain solely at this C boundary. */
    const POINT anywhere = {0, 0};
    const STAR_DESC *last = NULL;
    COUNT count = 0;
    while ((last = FindStarInSpace(last, &anywhere, -1, -1, false))) {
        if (GetStarIndex(last) != count++)
            return 0;
    }
    if (count != NUM_SOLAR_SYSTEMS)
        return 0;
    count = 0;
    while ((last = FindStarInSpace(last, &anywhere, -1, -1, true))) {
        if (GetStarIndex(last) != QUASISPACE_FIRST + count++)
            return 0;
    }
    if (count != NUM_QUASISPACE_LOCATIONS
            || FindStarInSpace(star, &anywhere, -1, -1, true) != NULL)
        return 0;
    GetClusterName(GetStar(NUM_SOLAR_SYSTEMS), name, sizeof name);
    if (name[0] != 0 || GetStar(STAR_CATALOGUE_SIZE) != NULL)
        return 0;
    GetClusterName(star, name, sizeof name);
    return hash == UINT64_C(0xb815f9d5b77cef91)
            && strcmp(unicode, "A") == 0 && unicode[2] == 0 && GetStarIndex(star) == 0
            && strcmp(GetStarPrefix(star), "Zeta") == 0
            && strcmp(GetStarPostfix(star), "Serpentis") == 0
            && strcmp(name, "Zeta Se") == 0
            && strcmp(GetQuasiSpaceName(), "UNKNOWN") == 0
            && FindStarInSpace(NULL, &star->star_pt, 0, 0, false) == star;
}
