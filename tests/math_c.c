#include "libs/mathlib.h"
#include "uqm/units.h"

/* Validate value ownership and the shared math/angle C interface. */
int math_c_contract(void) {
    RandomContext context = {RandomDefaultSeed};
    RandomContext copy = context;
    const RandomContext *read_only = &copy;
    int angle = 0;
    int ok = square_root(81) == 9 && SINE(angle++, 100) == -100;
    return ok && angle == 1 && COSINE(16, 100) == 100
            && NORMALIZE_ANGLE(-1) == 63 && ARCTAN(0, 0) == FULL_CIRCLE
            && RandomContext_Random(&context) == 207482415u
            && RandomContext_GetSeed(read_only) == RandomDefaultSeed
            && RandomContext_Random(&copy) == 207482415u;
}
