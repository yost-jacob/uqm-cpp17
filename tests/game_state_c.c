#include "uqm/globdata.h"
#include <string.h>

/* These exercise real C macros and whole-aggregate assignments used by legacy
 * code. No queue/resource handles are allocated by these fixtures. */
static GLOBDATA saved;
void game_state_c_reset(void) { memset(&GlobData, 0, sizeof(GlobData)); }
void *game_state_c_address(void) { return &GlobData; }
void game_state_c_seed(void)
{
    GLOBAL_SIS(FuelOnBoard) = 123456;
    GLOBAL_SIS(CrewEnlisted) = 42;
    GLOBAL_SIS(ResUnits) = 98765;
    GLOBAL_SIS(TotalElementMass) = 28;
    GLOBAL_SIS(TotalBioMass) = 9;
    GLOBAL_SIS(ElementAmounts[7]) = 28;
    GLOBAL_SIS(ModuleSlots[15]) = 12;
    GLOBAL_SIS(DriveSlots[10]) = 3;
    GLOBAL_SIS(JetSlots[7]) = 4;
    GLOBAL_SIS(NumLanders) = 2;
    memcpy(GLOBAL_SIS(ShipName), "Vindicator", 11);
    memcpy(GLOBAL_SIS(CommanderName), "Captain", 8);
    memcpy(GLOBAL_SIS(PlanetName), "Earth", 6);
    GLOBAL_SIS(log_x) = -12345;
    GLOBAL_SIS(log_y) = 67890;
    GLOBAL(ip_location.x) = -321;
    GLOBAL(ip_location.y) = 654;
    GLOBAL(ShipFacing) = 15;
    GLOBAL(ip_planet) = 3;
    GLOBAL(in_orbit) = 1;
    SET_GAME_STATE(STARBASE_AVAILABLE, 1);
    SET_GAME_STATE(SUN_DEVICE_ON_SHIP, 1);
    SET_GAME_STATE(CHMMR_BOMB_STATE, 2);
    SET_GAME_STATE(SHOFIXTI_VISITS, 5);
    SET_GAME_STATE(SHOFIXTI_STACK1, 2);
    SET_GAME_STATE(SHOFIXTI_STACK2, 6);
}
int game_state_c_observes_native_updates(void)
{
    return GLOBAL_SIS(FuelOnBoard) == 999 && GLOBAL_SIS(CrewEnlisted) == 65535
            && !GET_GAME_STATE(STARBASE_AVAILABLE) && GET_GAME_STATE(SUN_DEVICE_ON_SHIP)
            && GET_GAME_STATE(CHMMR_BOMB_STATE) == 3 && GET_GAME_STATE(SHOFIXTI_VISITS) == 7
            && GET_GAME_STATE(SHOFIXTI_STACK1) == 2 && GET_GAME_STATE(SHOFIXTI_STACK2) == 6;
}
void game_state_c_save(void) { saved = GlobData; }
void game_state_c_restore(void) { GlobData = saved; }
void game_state_c_unterminated_name(void)
{
    memset(GLOBAL_SIS(ShipName), 'X', sizeof(GLOBAL_SIS(ShipName)));
}
unsigned game_state_c_name_capacity(void) { return sizeof(GLOBAL_SIS(ShipName)); }
int game_state_c_counter_neighbors_intact(void)
{
    return GET_GAME_STATE(SHOFIXTI_STACK1) == 2 && GET_GAME_STATE(SHOFIXTI_STACK2) == 6;
}
/* Cover unaligned legacy 32-bit fields as well as the typed small counters. */
int game_state_c_packed_words(void)
{
    BYTE bits[8] = {0};
    BYTE copy[8] = {0};
    setGameState32(bits, 3, UINT32_C(0xfedcba98)
#ifdef STATE_DEBUG
            , "test word"
#endif
            );
    copyGameState(copy, 5, bits, 3, 34);
    return getGameState32(bits, 3) == UINT32_C(0xfedcba98)
            && getGameState32(copy, 5) == UINT32_C(0xfedcba98);
}
