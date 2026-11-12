#ifndef UQM_WORLD_C_H_
#define UQM_WORLD_C_H_
#include "libs/compiler.h"
struct solarsys_state;
typedef struct UQM_SYSTEM_RUNTIME UQM_SYSTEM_RUNTIME;
#ifdef __cplusplus
extern "C" {
#endif
/* Slots are zero-based; moon == -1 selects the planet. Unknown IDs are fatal
 * adapter errors. Configure only after the relevant descriptors are generated. */
void ConfigurePersonalEnergyBehavior(struct solarsys_state *system,
        COUNT planet, int moon, COUNT sites, COUNT reports);
void ConfigureArtifactEnergyBehavior(struct solarsys_state *system, COUNT planet, int moon);
typedef struct ENERGY_INTERACTION {
    BOOLEAN accepted, removeNode, hasReport;
    COUNT report;
} ENERGY_INTERACTION;
ENERGY_INTERACTION InteractWorldEnergy(struct solarsys_state *system,
        COUNT planet, int moon, COUNT site, BOOLEAN physicallyRetrieved);
#ifdef __cplusplus
}
#endif
#endif
