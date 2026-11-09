#include "ship_factory.hpp"
#include "ship_builders.hpp"

namespace uqm::combat {
std::unique_ptr<Ship> createShip(SPECIES_ID species) {
    switch (species) {
        case ARILOU_ID: return makeArilouShip();
        case CHMMR_ID: return makeChmmrShip();
        case EARTHLING_ID: return makeEarthlingShip();
        case ORZ_ID: return makeOrzShip();
        case PKUNK_ID: return makePkunkShip();
        case SHOFIXTI_ID: return makeShofixtiShip();
        case SPATHI_ID: return makeSpathiShip();
        case SUPOX_ID: return makeSupoxShip();
        case THRADDASH_ID: return makeThraddashShip();
        case UTWIG_ID: return makeUtwigShip();
        case VUX_ID: return makeVuxShip();
        case YEHAT_ID: return makeYehatShip();
        case MELNORME_ID: return makeMelnormeShip();
        case DRUUGE_ID: return makeDruugeShip();
        case ILWRATH_ID: return makeIlwrathShip();
        case MYCON_ID: return makeMyconShip();
        case SLYLANDRO_ID: return makeSlylandroShip();
        case UMGAH_ID: return makeUmgahShip();
        case UR_QUAN_ID: return makeUrQuanShip();
        case ZOQFOTPIK_ID: return makeZoqFotPikShip();
        case SYREEN_ID: return makeSyreenShip();
        case KOHR_AH_ID: return makeKohrAhShip();
        case ANDROSYNTH_ID: return makeAndrosynthShip();
        case CHENJESU_ID: return makeChenjesuShip();
        case MMRNMHRM_ID: return makeMmrnmhrmShip();
        case SIS_SHIP_ID: return makeFlagship();
        case SA_MATRA_ID: return makeSaMatraShip();
        case UR_QUAN_PROBE_ID: return makeUrQuanProbeShip();
        default: return nullptr;
    }
}
}
