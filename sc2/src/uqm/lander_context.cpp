#include "lander_runtime.hpp"
#include "inventory.hpp"
#include "simulation_engine_legacy.hpp"
#include "resinst.h"
#include "nameref.h"
#include <algorithm>
#include <stdexcept>
namespace uqm {
LanderAssets::LanderAssets(AssetCache &cache) {
    static constexpr RESOURCE resources[] = {LANDER_MASK_PMAP_ANIM, QUAKE_MASK_PMAP_ANIM,
        LIGHTNING_MASK_ANIM, LAVA_MASK_PMAP_ANIM, LANDER_SHIELD_MASK_ANIM,
        LANDER_LAUNCH_MASK_PMAP_ANIM, LANDER_RETURN_MASK_PMAP_ANIM};
    for (std::size_t i = 0; i < graphics_.size(); ++i) {
        graphics_[i] = std::static_pointer_cast<const GraphicAsset>(cache.find({AssetKind::Graphic, resources[i]}));
        frames[i] = graphics_[i]->frame();
    }
    sounds_ = std::static_pointer_cast<const SoundAsset>(cache.find({AssetKind::Sound, LANDER_SOUNDS}));
}
LanderVisit::LanderVisit(GameSession &owner, SOLARSYS_STATE &projection, SimulationEngine &engine, POINT landing)
    : surface_(std::get<SolarSystemLocation>(owner.player().location()), landing),
      session(owner), system(projection), story(owner.player().story()), simulation(engine),
      assets(owner.assets()), curLanderLoc(surface_.position()),
      campaign_(detail::encounterLegacy(owner.player())) {}
COUNT LanderVisit::freeCargo() const {
    const auto space = session.player().inventory().capacity(Inventory::mineralKey(0))
        - session.player().sis().mineralMass();
    return static_cast<COUNT>(std::max<std::int64_t>(0, space));
}
bool LanderVisit::embarkCrew() {
    if (finished_) throw std::logic_error("lander expedition already completed");
    if (crew_left >= int(LanderCrewColumns * LanderCrewRows)) return false;
    const auto result = session.player().inventory().transact({{{"crew", 1}}, {}});
    if (result.status == InventoryStatus::Insufficient) return false;
    if (result.status != InventoryStatus::Completed) throw std::logic_error("could not board lander crew");
    ++crew_left;
    return true;
}
LanderOutcome LanderVisit::finish(bool shutdown) {
    if (finished_) throw std::logic_error("lander expedition already completed");
    if (shutdown || session.menuExit() || session.dispatcher().exitRequest()
            || (activity() & (CHECK_ABORT | CHECK_LOAD))) {
        finished_ = true;
        return LanderOutcome::Cancelled;
    }
    InventoryTransaction transfer;
    const bool lost = crew_left == 0;
    if (lost) transfer.give["landers"] = 1;
    else {
        transfer.receive["crew"] = crew_left;
        if (BiologicalLevel) transfer.receive["biomass"] = BiologicalLevel;
        for (std::size_t i = 0; i < NUM_ELEMENT_CATEGORIES; ++i)
            if (ElementAmounts[i]) transfer.receive[std::string(Inventory::mineralKey(i))] = ElementAmounts[i];
    }
    const auto result = session.player().inventory().transact(transfer);
    if (result.status != InventoryStatus::Completed)
        throw std::logic_error("could not settle lander expedition: " + result.asset);
    finished_ = true;
    return lost ? LanderOutcome::Lost : LanderOutcome::Returned;
}
void removeLanderEffects(SimulationEngine &engine) noexcept {
    auto &queue = detail::SimulationAccess::queue(engine);
    for (auto handle = GetHeadLink(&queue); handle;) {
        auto &element = *static_cast<ELEMENT *>(handle);
        const auto next = _GetSuccLink(&element);
        if (element.state_flags & FINITE_LIFE) {
            RemoveQueue(&queue, handle);
            engine.releaseElement(element);
        }
        handle = next;
    }
}
}
