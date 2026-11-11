#ifndef UQM_INVENTORY_HPP_
#define UQM_INVENTORY_HPP_
#include "fleet.hpp"
#include "inventory_transaction.hpp"
#include <string_view>
namespace uqm {
class StoryState;
// Owns the player's fleet (and its optional flagship). Resource values retain
// their authoritative SIS/save storage; legacy item/credit bits are private
// compatibility storage, not narrative effects of an inventory transaction.
// Access is on the gameplay thread; the inventory invokes no callbacks.
class Inventory {
public:
    explicit Inventory(StoryState &legacyFields);
    Fleet &fleet() noexcept { return fleet_; }
    const Fleet &fleet() const noexcept { return fleet_; }
    // Read-only, repeatable; unknown names throw. Fuel uses displayed units.
    std::int64_t quantity(std::string_view asset) const;
    std::int64_t capacity(std::string_view asset) const;
    static void validateKey(std::string_view asset);
    static bool ownsLegacyField(std::string_view field) noexcept;
    // Validate all names/counts before inspecting balances. Exact fulfillment:
    // validate consideration, stage removals/additions, then commit everything.
    // Failure changes nothing, consumes no RNG, and performs no presentation.
    InventoryResult transact(const InventoryTransaction &request);
private:
    StoryState &legacyFields_;
    Fleet fleet_;
};
}
#endif
