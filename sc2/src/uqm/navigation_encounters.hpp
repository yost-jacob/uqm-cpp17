#ifndef UQM_NAVIGATION_ENCOUNTERS_HPP_
#define UQM_NAVIGATION_ENCOUNTERS_HPP_
#include "entity_handle.hpp"
#include "simulation_engine.hpp"
#include <unordered_map>
struct encounter;
namespace uqm {
// Transient projection index, owned by one navigation visit. Campaign records
// never retain engine slots. Record addresses stay stable in the campaign pool;
// erase before freeing a record, and clear when ending the navigation scene.
class NavigationEncounterViews {
public:
    std::optional<std::reference_wrapper<ELEMENT>> find(const encounter &record, SimulationEngine &engine) const noexcept {
        const auto found = elements_.find(&record);
        return found == elements_.end() ? std::nullopt : engine.resolve(found->second);
    }
    void insert(const encounter &record, combat::EntityHandle element) { elements_.insert_or_assign(&record, element); }
    void erase(const encounter &record) noexcept { elements_.erase(&record); }
    void clear() noexcept { elements_.clear(); }
private:
    std::unordered_map<const encounter *, combat::EntityHandle> elements_;
};
}
#endif
