#include "sis_state.hpp"
#include <algorithm>
#include <iterator>
#include <type_traits>
#include <stdexcept>
namespace uqm {
namespace {
template<std::size_t N>
std::string name(const char (&text)[N]) {
    return {text, std::find(text, text + N, '\0')};
}
}
COUNT SIS::mineral(COUNT category) const {
    if (category >= NUM_ELEMENT_CATEGORIES) throw std::out_of_range("invalid mineral category");
    return data_.ElementAmounts[category];
}
SISLoadout SIS::loadout() const noexcept {
    SISLoadout result{};
    std::copy(std::begin(data_.ModuleSlots), std::end(data_.ModuleSlots), result.modules.begin());
    std::copy(std::begin(data_.DriveSlots), std::end(data_.DriveSlots), result.drives.begin());
    std::copy(std::begin(data_.JetSlots), std::end(data_.JetSlots), result.jets.begin());
    return result;
}
ShipState SIS::snapshot() const {
    const auto &sis = data_;
    ShipState result{};
    result.resourceUnits = sis.ResUnits;
    result.fuel = sis.FuelOnBoard;
    result.crew = sis.CrewEnlisted;
    result.mineralMass = sis.TotalElementMass;
    result.biologicalMass = sis.TotalBioMass;
    static_assert(std::extent_v<decltype(sis.ElementAmounts)> == std::tuple_size<decltype(result.minerals)>::value);
    static_assert(std::extent_v<decltype(sis.ModuleSlots)> == std::tuple_size<decltype(result.modules)>::value);
    static_assert(std::extent_v<decltype(sis.DriveSlots)> == std::tuple_size<decltype(result.drives)>::value);
    static_assert(std::extent_v<decltype(sis.JetSlots)> == std::tuple_size<decltype(result.jets)>::value);
    std::copy(std::begin(sis.ElementAmounts), std::end(sis.ElementAmounts), result.minerals.begin());
    std::copy(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), result.modules.begin());
    std::copy(std::begin(sis.DriveSlots), std::end(sis.DriveSlots), result.drives.begin());
    std::copy(std::begin(sis.JetSlots), std::end(sis.JetSlots), result.jets.begin());
    result.landers = sis.NumLanders;
    result.shipName = name(sis.ShipName);
    result.commanderName = name(sis.CommanderName);
    result.planetName = name(sis.PlanetName);
    return result;
}
}
