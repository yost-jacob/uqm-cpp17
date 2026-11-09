#ifndef UQM_SYSTEM_BUILDERS_HPP_
#define UQM_SYSTEM_BUILDERS_HPP_
#include "system_behavior.hpp"
namespace uqm {
// Internal concrete builders; callers use createSystemBehavior.
std::unique_ptr<SystemBehavior> createAndrosynthSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createBurvixeseSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createColonySystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createDruugeSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createIlwrathSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createMelnormeSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createMyconSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createOrzSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createTalkingPetSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createPkunkSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createRainbowWorldSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createSaMatraSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createShofixtiSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createSlylandroSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createSolSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createSpathiSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createSupoxSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createSyreenSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createThraddashSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createTrapSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createUtwigSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createVaultSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createVuxSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createWreckSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createYehatSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createZoqFotPikScoutSystemBehavior(CampaignState &campaign);
std::unique_ptr<SystemBehavior> createZoqFotPikSystemBehavior(CampaignState &campaign);
}
#endif
