#include "ship_behavior.hpp"
#include "ship_behavior_c.h"
#include <algorithm>
#include <cassert>

namespace uqm::combat {
Ship::Ship(const RACE_DESC &definition) noexcept : data_(definition) {
    data_.ship_owner = this;
}
void Ship::preprocess(ELEMENT &element) noexcept {
    if (preprocessingEnabled_ && !finished_) beforeMotion(element);
}
void Ship::postprocess(ELEMENT &element) noexcept {
    if (!finished_) afterMotion(element);
}
COUNT Ship::initializeWeapon(ELEMENT &element, WeaponSlots &weapons) noexcept {
    weapons.fill(nullptr);
    if (finished_) return 0;
    const auto count = createWeapons(element, weapons);
    assert(count <= weapons.size());
    return count;
}
void Ship::finish() noexcept {
    if (finished_) return;
    finished_ = true;
    onFinish();
}
Ship &shipFor(RACE_DESC &descriptor) noexcept {
    assert(descriptor.ship_owner);
    auto &ship = *static_cast<Ship *>(descriptor.ship_owner);
    assert(&ship.legacyData() == &descriptor);
    return ship;
}
}
extern "C" void PreprocessShipBehavior(RACE_DESC *ship, ELEMENT *element) {
    assert(ship && element);
    uqm::combat::shipFor(*ship).preprocess(*element);
}
extern "C" void PostprocessShipBehavior(RACE_DESC *ship, ELEMENT *element) {
    assert(ship && element);
    uqm::combat::shipFor(*ship).postprocess(*element);
}
extern "C" COUNT InitializeShipWeapon(RACE_DESC *ship, ELEMENT *element, HELEMENT weapons[6]) {
    assert(ship && element && weapons);
    uqm::combat::WeaponSlots slots{};
    const auto count = uqm::combat::shipFor(*ship).initializeWeapon(*element, slots);
    std::copy(slots.begin(), slots.end(), weapons);
    return count;
}
