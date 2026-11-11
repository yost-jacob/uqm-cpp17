#ifndef UQM_SIS_STATE_HPP_
#define UQM_SIS_STATE_HPP_
#include "sis.h"
#include "ship_state.hpp"
#include <array>
#include <cstddef>
namespace uqm {
namespace detail { struct SISAccess; }
struct SISLoadout {
    std::array<BYTE, NUM_MODULE_SLOTS> modules{};
    std::array<BYTE, NUM_DRIVE_SLOTS> drives{};
    std::array<BYTE, NUM_JET_SLOTS> jets{};
};
// Persistent Precursor vessel. Fleet owns this optional value; it is neither
// an escort slot nor the temporary combat::Ship behavior. All quantities retain
// legacy units. Storage setters do not enforce outfitting/cost/capacity policy.
class SIS {
public:
    SIS() = default;
    SIS(const SIS &) = delete;
    SIS &operator=(const SIS &) = delete;
    DWORD fuel() const noexcept { return data_.FuelOnBoard; }
    COUNT crew() const noexcept { return data_.CrewEnlisted; }
    bool alive() const noexcept { return crew() != static_cast<COUNT>(~0u); }
    void setFuel(DWORD fuel) noexcept { data_.FuelOnBoard = fuel; }
    void setCrew(COUNT crew) noexcept { data_.CrewEnlisted = crew; }
    // Combat counts the captain; persistent crew does not. Zero survivors means
    // defeat (65535), while one survivor means a living captain with zero crew.
    void recordCombatCrew(COUNT includingCaptain) noexcept {
        data_.CrewEnlisted = includingCaptain ? includingCaptain - 1 : static_cast<COUNT>(~0u);
    }
    SISLoadout loadout() const noexcept; // Detached configuration for a combat visit.
    ShipState snapshot() const; // Detached UI/save-facing quantities and names.
    void reset() noexcept { data_ = {}; } // Only with no active combat/visit borrows.
private:
    friend struct detail::SISAccess;
    SIS_STATE data_{};
};
}
#endif
