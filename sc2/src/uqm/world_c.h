#ifndef UQM_WORLD_C_H_
#define UQM_WORLD_C_H_
#include "libs/compiler.h"
struct solarsys_state;
struct star_desc;
struct planet_desc;
typedef struct UQM_SYSTEM_RUNTIME UQM_SYSTEM_RUNTIME;
#ifdef __cplusplus
extern "C" {
#endif
/* Production C exploration borrows a native entry-scope context. Prepare creates
 * its behavior before any hooks; Build attaches descriptors after generation.
 * Create/Destroy are paired owning adapters only for legacy debug tooling. */
void PrepareSystemWorlds(struct solarsys_state *system, const struct star_desc *star);
void CreateSystemWorlds(struct solarsys_state *system, const struct star_desc *star);
void BuildSystemWorlds(struct solarsys_state *system, const struct star_desc *star);
void BuildMoonWorlds(struct solarsys_state *system, const struct planet_desc *planet);
void DestroySystemWorlds(struct solarsys_state *system);
/* Prepared native system required. These are borrowed C views; no selected
 * descriptor pointers are stored. NULL selection clears focus, not the view. */
struct planet_desc *GetSystemSelectedDescriptor(struct solarsys_state *system);
void SelectSystemWorld(struct solarsys_state *system, const struct planet_desc *world);
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
