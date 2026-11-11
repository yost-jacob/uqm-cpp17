#ifndef UQM_INVENTORY_TRANSACTION_HPP_
#define UQM_INVENTORY_TRANSACTION_HPP_
#include <cstdint>
#include <map>
#include <string>
namespace uqm {
// Positive whole quantities keyed by the inventory catalog. Packages own their
// values; transactions always describe player-gives first, player-receives second.
using InventoryPackage = std::map<std::string, std::int64_t, std::less<>>;
struct InventoryTransaction { InventoryPackage give, receive; };
enum class InventoryStatus { Completed, Insufficient, NoCapacity, Unavailable };
struct InventoryResult {
    InventoryStatus status;
    std::string asset; // Empty on success; otherwise identifies the failed asset.
};
}
#endif
