#include "system_behavior.hpp"
#include "system_builders.hpp"
#include "gendef.h"
namespace uqm {
std::unique_ptr<SystemBehavior> createSystemBehavior(BYTE special, StoryState &story) {
    switch (special) {
        case CHMMR_DEFINED: return createChmmrSystemBehavior(story);
        case SOL_DEFINED: return createSolSystemBehavior(story);
        case SHOFIXTI_DEFINED: return createShofixtiSystemBehavior(story);
        case START_COLONY_DEFINED: return createColonySystemBehavior(story);
        case SPATHI_DEFINED: return createSpathiSystemBehavior(story);
        case MELNORME0_DEFINED: return createMelnormeSystemBehavior(story);
        case MELNORME1_DEFINED: return createMelnormeSystemBehavior(story);
        case MELNORME2_DEFINED: return createMelnormeSystemBehavior(story);
        case MELNORME3_DEFINED: return createMelnormeSystemBehavior(story);
        case MELNORME4_DEFINED: return createMelnormeSystemBehavior(story);
        case MELNORME5_DEFINED: return createMelnormeSystemBehavior(story);
        case MELNORME6_DEFINED: return createMelnormeSystemBehavior(story);
        case MELNORME7_DEFINED: return createMelnormeSystemBehavior(story);
        case MELNORME8_DEFINED: return createMelnormeSystemBehavior(story);
        case TALKING_PET_DEFINED: return createTalkingPetSystemBehavior(story);
        case SYREEN_DEFINED: return createSyreenSystemBehavior(story);
        case MYCON_TRAP_DEFINED: return createTrapSystemBehavior(story);
        case BURVIXESE_DEFINED: return createBurvixeseSystemBehavior(story);
        case SLYLANDRO_DEFINED: return createSlylandroSystemBehavior(story);
        case DRUUGE_DEFINED: return createDruugeSystemBehavior(story);
        case BOMB_DEFINED: return createUtwigSystemBehavior(story);
        case UTWIG_DEFINED: return createUtwigSystemBehavior(story);
        case AQUA_HELIX_DEFINED: return createThraddashSystemBehavior(story);
        case THRADD_DEFINED: return createThraddashSystemBehavior(story);
        case SUN_DEVICE_DEFINED: return createMyconSystemBehavior(story);
        case MYCON_DEFINED: return createMyconSystemBehavior(story);
        case EGG_CASE0_DEFINED: return createMyconSystemBehavior(story);
        case EGG_CASE1_DEFINED: return createMyconSystemBehavior(story);
        case EGG_CASE2_DEFINED: return createMyconSystemBehavior(story);
        case ANDROSYNTH_DEFINED: return createAndrosynthSystemBehavior(story);
        case TAALO_PROTECTOR_DEFINED: return createOrzSystemBehavior(story);
        case ORZ_DEFINED: return createOrzSystemBehavior(story);
        case SHIP_VAULT_DEFINED: return createVaultSystemBehavior(story);
        case URQUAN_WRECK_DEFINED: return createWreckSystemBehavior(story);
        case MAIDENS_DEFINED: return createVuxSystemBehavior(story);
        case VUX_BEAST_DEFINED: return createVuxSystemBehavior(story);
        case VUX_DEFINED: return createVuxSystemBehavior(story);
        case SAMATRA_DEFINED: return createSaMatraSystemBehavior(story);
        case ZOQFOT_DEFINED: return createZoqFotPikSystemBehavior(story);
        case ZOQ_SCOUT_DEFINED: return createZoqFotPikScoutSystemBehavior(story);
        case YEHAT_DEFINED: return createYehatSystemBehavior(story);
        case PKUNK_DEFINED: return createPkunkSystemBehavior(story);
        case SUPOX_DEFINED: return createSupoxSystemBehavior(story);
        case RAINBOW_DEFINED: return createRainbowWorldSystemBehavior(story);
        case ILWRATH_DEFINED: return createIlwrathSystemBehavior(story);
        default: return std::make_unique<SystemBehavior>(story);
    }
}
}
