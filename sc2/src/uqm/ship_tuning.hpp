#ifndef UQM_SHIP_TUNING_HPP_
#define UQM_SHIP_TUNING_HPP_

#include <cstdint>
#include <istream>
#include <map>
#include <string>
#include <string_view>

namespace uqm::combat {
// Plain copied values, sized to the legacy fields they populate. Current crew
// and energy belong to an instance, not this immutable configuration.
struct ShipStats {
    std::uint16_t maxCrew, maxThrust, thrustIncrement;
    std::uint8_t maxEnergy, energyWait, thrustWait, turnWait;
    std::int8_t energyRegeneration;
    std::uint8_t shipMass, weaponEnergyCost, weaponWait, specialEnergyCost, specialWait, meleeCost;
};
struct FlightTuning {
    std::uint16_t steeringPeriod = 0, initialDelay = 0;
    std::int16_t acceleration = 0, maxSpeed = 0;
    bool directionalSprite = false, snapToTarget = false;
};
struct ProjectileStats {
    std::int16_t speed, hitPoints, damage;
    std::uint16_t lifetime;
    FlightTuning flight{};
};
struct ShipTuning {
    ShipStats ship;
    std::map<std::string, ProjectileStats, std::less<>> projectiles;
    // Names are structural identities, e.g. "missile" or "fragment". Reject a
    // missing profile; references remain valid for the catalog's lifetime.
    const ProjectileStats &projectile(std::string_view name) const;
};
class ShipTuningCatalog {
public:
    // Parse/validate the complete roster atomically. Reject missing/unknown IDs,
    // fields, nonintegral values and narrowing. No compiled fallback balances.
    explicit ShipTuningCatalog(std::istream &input);
    const ShipTuning &at(std::string_view id) const;
    std::size_t size() const noexcept { return ships_.size(); }
private:
    std::map<std::string, ShipTuning, std::less<>> ships_;
};
// Process-lifetime catalog, loaded through content/addon I/O before ship creation.
// No hot reload: native definitions cache references to immutable tuning.
const ShipTuning &shipTuning(std::string_view id);
// Bound an AI range hint to the legacy unsigned field. Physical projectile
// lifetime/speed remain unchanged; large modded ranges do not wrap the hint.
std::uint16_t weaponRangeHint(std::int64_t distance) noexcept;
}
#endif
