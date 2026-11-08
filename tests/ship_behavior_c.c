#include "uqm/ship_behavior_c.h"
// Genuine C callers see native overrides and the same mutable descriptor.
COUNT exercise_ship_behavior_from_c(RACE_DESC *ship, ELEMENT *element, HELEMENT weapons[6])
{
    PreprocessShipBehavior(ship, element);
    PostprocessShipBehavior(ship, element);
    return InitializeShipWeapon(ship, element, weapons);
}
