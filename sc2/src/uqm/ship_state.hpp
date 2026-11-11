#ifndef UQM_SHIP_STATE_HPP_
#define UQM_SHIP_STATE_HPP_
#include <array>
#include <cstdint>
#include <string>
namespace uqm {
// Copied values, never another mutable ship store. Fuel uses the existing game
// units (100 units per tank unit); crew excludes the captain, 65535 means dead.
struct ShipState {
    std::uint32_t resourceUnits, fuel;
    std::uint16_t crew, mineralMass, biologicalMass;
    std::array<std::uint16_t, 8> minerals;
    std::array<std::uint8_t, 16> modules;
    std::array<std::uint8_t, 11> drives;
    std::array<std::uint8_t, 8> jets;
    std::uint8_t landers;
    std::string shipName, commanderName, planetName;
};
}
#endif
