#ifndef UQM_SHIP_BEHAVIOR_C_H_
#define UQM_SHIP_BEHAVIOR_C_H_
#include "races.h"
#include "element.h"
#ifdef __cplusplus
extern "C" {
#endif
// All descriptor/element arguments must be non-null and remain live for the call.
void PreprocessShipBehavior(RACE_DESC *ship, ELEMENT *element);
void PostprocessShipBehavior(RACE_DESC *ship, ELEMENT *element);
// weapons points to six writable handles; unused slots are cleared.
COUNT InitializeShipWeapon(RACE_DESC *ship, ELEMENT *element, HELEMENT weapons[6]);
#ifdef __cplusplus
}
#endif
#endif
