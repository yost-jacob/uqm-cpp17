#ifndef UQM_SHIP_TUNING_C_H_
#define UQM_SHIP_TUNING_C_H_
#include "libs/compiler.h"
#ifdef __cplusplus
extern "C" {
#endif
// Call after content/addon setup, before any ship resource creation. Loads once;
// failure logs its path/reason and leaves no partial catalog installed.
BOOLEAN LoadShipTuning(void);
#ifdef __cplusplus
}
#endif
#endif
