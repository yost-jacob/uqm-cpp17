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
    return strcmp(unicode, "A") == 0 && unicode[2] == 0 && GetStarIndex(star) == 0 && strcmp(GetStarPrefix(star), "Zeta") == 0
            && strcmp(GetStarPostfix(star), "Serpentis") == 0
            && strcmp(name, "Zeta Se") == 0
            && strcmp(GetQuasiSpaceName(), "UNKNOWN") == 0
            && FindStarInSpace(NULL, &star->star_pt, 0, 0, false) == star;
}
