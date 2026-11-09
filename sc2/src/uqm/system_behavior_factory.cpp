#include "system_behavior.hpp"
#include "system_builders.hpp"
#include "gendef.h"
namespace uqm {
std::unique_ptr<SystemBehavior> createSystemBehavior(BYTE special, CampaignState &campaign) {
    switch (special) {
        case CHMMR_DEFINED: return createChmmrSystemBehavior(campaign);
        case SOL_DEFINED: return createSolSystemBehavior(campaign);
        case SHOFIXTI_DEFINED: return createShofixtiSystemBehavior(campaign);
        case START_COLONY_DEFINED: return createColonySystemBehavior(campaign);
        case SPATHI_DEFINED: return createSpathiSystemBehavior(campaign);
        case MELNORME0_DEFINED: return createMelnormeSystemBehavior(campaign);
        case MELNORME1_DEFINED: return createMelnormeSystemBehavior(campaign);
        case MELNORME2_DEFINED: return createMelnormeSystemBehavior(campaign);
        case MELNORME3_DEFINED: return createMelnormeSystemBehavior(campaign);
        case MELNORME4_DEFINED: return createMelnormeSystemBehavior(campaign);
        case MELNORME5_DEFINED: return createMelnormeSystemBehavior(campaign);
        case MELNORME6_DEFINED: return createMelnormeSystemBehavior(campaign);
        case MELNORME7_DEFINED: return createMelnormeSystemBehavior(campaign);
        case MELNORME8_DEFINED: return createMelnormeSystemBehavior(campaign);
        case TALKING_PET_DEFINED: return createTalkingPetSystemBehavior(campaign);
        case SYREEN_DEFINED: return createSyreenSystemBehavior(campaign);
        case MYCON_TRAP_DEFINED: return createTrapSystemBehavior(campaign);
        case BURVIXESE_DEFINED: return createBurvixeseSystemBehavior(campaign);
        case SLYLANDRO_DEFINED: return createSlylandroSystemBehavior(campaign);
        case DRUUGE_DEFINED: return createDruugeSystemBehavior(campaign);
        case BOMB_DEFINED: return createUtwigSystemBehavior(campaign);
        case UTWIG_DEFINED: return createUtwigSystemBehavior(campaign);
        case AQUA_HELIX_DEFINED: return createThraddashSystemBehavior(campaign);
        case THRADD_DEFINED: return createThraddashSystemBehavior(campaign);
        case SUN_DEVICE_DEFINED: return createMyconSystemBehavior(campaign);
        case MYCON_DEFINED: return createMyconSystemBehavior(campaign);
        case EGG_CASE0_DEFINED: return createMyconSystemBehavior(campaign);
        case EGG_CASE1_DEFINED: return createMyconSystemBehavior(campaign);
        case EGG_CASE2_DEFINED: return createMyconSystemBehavior(campaign);
        case ANDROSYNTH_DEFINED: return createAndrosynthSystemBehavior(campaign);
        case TAALO_PROTECTOR_DEFINED: return createOrzSystemBehavior(campaign);
        case ORZ_DEFINED: return createOrzSystemBehavior(campaign);
        case SHIP_VAULT_DEFINED: return createVaultSystemBehavior(campaign);
        case URQUAN_WRECK_DEFINED: return createWreckSystemBehavior(campaign);
        case MAIDENS_DEFINED: return createVuxSystemBehavior(campaign);
        case VUX_BEAST_DEFINED: return createVuxSystemBehavior(campaign);
        case VUX_DEFINED: return createVuxSystemBehavior(campaign);
        case SAMATRA_DEFINED: return createSaMatraSystemBehavior(campaign);
        case ZOQFOT_DEFINED: return createZoqFotPikSystemBehavior(campaign);
        case ZOQ_SCOUT_DEFINED: return createZoqFotPikScoutSystemBehavior(campaign);
        case YEHAT_DEFINED: return createYehatSystemBehavior(campaign);
        case PKUNK_DEFINED: return createPkunkSystemBehavior(campaign);
        case SUPOX_DEFINED: return createSupoxSystemBehavior(campaign);
        case RAINBOW_DEFINED: return createRainbowWorldSystemBehavior(campaign);
        case ILWRATH_DEFINED: return createIlwrathSystemBehavior(campaign);
        default: return std::make_unique<SystemBehavior>(campaign);
    }
}
}
