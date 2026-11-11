#ifndef UQM_SYSTEM_BUILDERS_HPP_
#define UQM_SYSTEM_BUILDERS_HPP_
#include "system_behavior.hpp"
namespace uqm {
// Internal concrete builders; callers use createSystemBehavior.
std::unique_ptr<SystemBehavior> createAndrosynthSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createBurvixeseSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createColonySystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createDruugeSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createIlwrathSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createMelnormeSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createMyconSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createOrzSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createTalkingPetSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createPkunkSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createRainbowWorldSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createSaMatraSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createShofixtiSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createSlylandroSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createSolSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createSpathiSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createSupoxSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createSyreenSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createThraddashSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createTrapSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createUtwigSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createVaultSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createVuxSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createWreckSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createYehatSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createZoqFotPikScoutSystemBehavior(StoryState &story);
std::unique_ptr<SystemBehavior> createZoqFotPikSystemBehavior(StoryState &story);
}
#endif
