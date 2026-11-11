#ifndef UQM_SYSTEM_BEHAVIOR_HPP_
#define UQM_SYSTEM_BEHAVIOR_HPP_
#include "planets/planets.h"
#include "comm.h"
#include <memory>
namespace uqm {
class StoryState;
class GameSession;
// One behavior per explored system. story state must outlive this object. Hook
// descriptors belong to the active C engine frame and are never retained.
// Return meanings/order match the original generation hooks; node info may be null for counts.
class SystemBehavior {
public:
    explicit SystemBehavior(StoryState &story) noexcept : story_(story) {}
    virtual ~SystemBehavior() = default;
    // Attach immutable catalogue context before hooks. It outlives exploration.
    void attachStar(const STAR_DESC &star);
    void attachSession(GameSession &session);
    SystemBehavior(const SystemBehavior &) = delete;
    SystemBehavior &operator=(const SystemBehavior &) = delete;
    // NPC lifecycle hooks run with the initialized system and encounter queues.
    virtual bool initNpcs(SOLARSYS_STATE &system);
    virtual bool reinitNpcs(SOLARSYS_STATE &system);
    virtual bool uninitNpcs(SOLARSYS_STATE &system);
    // Construction hooks build descriptors; SolarSystem/World constructors own the resulting layout.
    virtual bool generatePlanets(SOLARSYS_STATE &system);
    virtual bool generateMoons(SOLARSYS_STATE &system, PLANET_DESC &world);
    virtual bool generateName(const SOLARSYS_STATE &system, const PLANET_DESC &world);
    // Entering orbit may invoke story UI/encounters; story state is checked now.
    virtual bool generateOrbital(SOLARSYS_STATE &system, PLANET_DESC &world);
    // Keep legacy node-index/count conventions. info is nullable for count queries.
    virtual COUNT generateMinerals(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info);
    virtual COUNT generateEnergy(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info);
    virtual COUNT generateLife(const SOLARSYS_STATE &system, const PLANET_DESC &world, COUNT node, NODE_INFO *info);
    // Called for a surface pickup; preserve the legacy accepted/removal convention.
    virtual bool pickupMinerals(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node);
    virtual bool pickupEnergy(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node);
    virtual bool pickupLife(SOLARSYS_STATE &system, PLANET_DESC &world, COUNT node);
protected:
    const STAR_DESC &star() const;
    COUNT communicate(CONVERSATION conversation);
    StoryState &story_;
private:
    const STAR_DESC *star_ = nullptr;
    GameSession *session_ = nullptr;
};
// Catalogue special-system ID selects rules; story state is checked by hooks.
std::unique_ptr<SystemBehavior> createSystemBehavior(BYTE specialSystem, StoryState &story);
std::unique_ptr<SystemBehavior> createChmmrSystemBehavior(StoryState &story);
} // namespace uqm
#endif
