#ifndef UQM_NAVIGATION_ENCOUNTERS_HPP_
#define UQM_NAVIGATION_ENCOUNTERS_HPP_
#include "entity_handle.hpp"
#include "encount.h"
#include "simulation_engine.hpp"
#include <unordered_map>
namespace uqm {
// Transient projection index, owned by one navigation visit. Campaign records
// never retain engine slots. Runtime IDs survive vector erasure/moves; erase
// before removing a record, and clear when ending the navigation scene.
class NavigationEncounterViews {
public:
    std::optional<std::reference_wrapper<ELEMENT>> find(const encounter &record, SimulationEngine &engine) const noexcept {
        const auto found = elements_.find(record.runtime_id);
        return found == elements_.end() ? std::nullopt : engine.resolve(found->second);
    }
    void insert(const encounter &record, combat::EntityHandle element) { elements_.insert_or_assign(record.runtime_id, element); }
    void erase(const encounter &record) noexcept { elements_.erase(record.runtime_id); }
    void clear() noexcept { elements_.clear(); }
private:
    std::unordered_map<std::uint64_t, combat::EntityHandle> elements_;
};
}
#endif
