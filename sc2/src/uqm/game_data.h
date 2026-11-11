#ifndef UQM_GAME_DATA_H_
#define UQM_GAME_DATA_H_
#include "sis.h"
#include "races.h"

/* Session-owned economic tables. C-compatible storage keeps the existing save
 * adapters explicit; C++ callers obtain references from GameSession/PlayerEntity.
 * Module prices retain MODULE_COST_SCALE units; fuel is priced per displayed
 * unit. A zero module price still means unavailable under the legacy policy.
 * Only story/purchase code changes these tables; rendering must only read them.
 */
typedef struct UQM_PRICE_TABLES {
    BYTE crew;
    BYTE fuel;
    BYTE modules[NUM_MODULES];
    BYTE minerals[NUM_ELEMENT_CATEGORIES];
} UQM_PRICE_TABLES;

typedef struct UQM_GAME_DATA {
    UQM_PRICE_TABLES prices;
} UQM_GAME_DATA;

#ifdef __cplusplus
extern "C" {
#endif
/* Borrow the active session's tables. Never retain this pointer across a
 * session transition. Explicit C++ owners use gameData() instead. */
UQM_GAME_DATA *GetGameData(void);
/* Assign new-campaign prices to a non-null, live destination. This does not
 * change balances or story flags. Never call while loading saved prices. */
void ResetGameData(UQM_GAME_DATA *data);
/* Shared immutable ship price table, indexed by ARILOU_SHIP..YEHAT_REBEL_SHIP.
 * Invalid indices are programming errors and abort at this C boundary. */
COUNT GetShipPrice(COUNT race);
#ifdef __cplusplus
}
#endif
#endif
