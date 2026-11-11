#ifndef UQM_PLAYER_LOCATION_H_
#define UQM_PLAYER_LOCATION_H_
#include "libs/compiler.h"
#include "libs/gfxlib.h"

/* Borrowed C projection of the active player's native travel state. Logical
 * map units differ from displayed star-map units; local flight uses system
 * units. Do not retain these pointers across a top-level mode transition/load. */
typedef struct { SDWORD x, y; } PLAYER_MAP_POSITION;
typedef struct {
    POINT position;
    UWORD facing; /* Saved facing + 1; zero means fresh system entry. */
    BYTE planet; /* Legacy one-based inner-system selector, zero = outer. */
    BYTE loadOrbit; /* Save/load marker, NOT the live orbit state. */
} PLAYER_FLIGHT_POSITION;
typedef struct {
    PLAYER_MAP_POSITION position;
    PLAYER_FLIGHT_POSITION flight;
} PLAYER_TRAVEL_POSITION;
#ifdef __cplusplus
extern "C" {
#endif
PLAYER_TRAVEL_POSITION *GetPlayerTravelPosition(void);
/* Explicit old-format save/load boundaries. Never called on every frame. */
void ExportPlayerTravelPosition(void);
void ImportPlayerTravelPosition(void);
void ResetPlayerTravelPosition(void);
#ifdef __cplusplus
}
#endif
#endif
