#ifndef UQM_NAVIGATION_TRANSITION_HPP_
#define UQM_NAVIGATION_TRANSITION_HPP_
#include "star_map.hpp"
#include <optional>
namespace uqm {
enum class NavigationRealm { HyperSpace, QuasiSpace };
enum class NavigationDestination { SolarSystem, Encounter, HyperSpace, QuasiSpace };
// Value returned after navigation's engine and presentation scopes have ended.
// SolarSystem requires a catalogue ID; Encounter optionally identifies a story
// destination (Arilou). Roaming fleets use the session's explicit selection.
struct NavigationTransition {
    NavigationDestination destination;
    std::optional<StarId> star;
};
}
#endif
