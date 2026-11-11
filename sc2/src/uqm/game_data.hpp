#ifndef UQM_GAME_DATA_HPP_
#define UQM_GAME_DATA_HPP_
#include "game_data.h"
#include <array>
#include <stdexcept>
namespace uqm {
using GameData = UQM_GAME_DATA;
using PriceTables = UQM_PRICE_TABLES;
// Immutable base definitions. Campaign copies are saved independently; story
// callers mutate those copies, never this baseline. Ship prices are immutable.
inline constexpr auto ShipPrices = std::array<COUNT, YEHAT_REBEL_SHIP + 1>{
1600,  /* ARILOU_SHIP */
		3000,  /* CHMMR_SHIP */
		1100,  /* HUMAN_SHIP */
		2300,  /* ORZ_SHIP */
		2000,  /* PKUNK_SHIP */
		 500,  /* SHOFIXTI_SHIP */
		1800,  /* SPATHI_SHIP */
		1600,  /* SUPOX_SHIP */
		1000,  /* THRADDASH_SHIP */
		2200,  /* UTWIG_SHIP */
		1200,  /* VUX_SHIP */
		2300,  /* YEHAT_SHIP */
		3600,  /* MELNORME_SHIP */
		1700,  /* DRUUGE_SHIP */
		1000,  /* ILWRATH_SHIP */
		2100,  /* MYCON_SHIP */
		4400,  /* SLYLANDRO_SHIP */
		 700,  /* UMGAH_SHIP */
		3000,  /* URQUAN_SHIP */
		 600,  /* ZOQFOTPIK_SHIP */
		1300,  /* SYREEN_SHIP */
		3000,  /* BLACK_URQUAN_SHIP */
		2300,  /* YEHAT_REBEL_SHIP */
};
inline constexpr GameData defaultGameData() noexcept {
    GameData data{};
    data.prices.crew = 3;
    data.prices.fuel = 20;
    data.prices.modules[PLANET_LANDER] = 500 / MODULE_COST_SCALE;
    data.prices.modules[FUSION_THRUSTER] = 500 / MODULE_COST_SCALE;
    data.prices.modules[TURNING_JETS] = 500 / MODULE_COST_SCALE;
    data.prices.modules[CREW_POD] = 2000 / MODULE_COST_SCALE;
    data.prices.modules[STORAGE_BAY] = 750 / MODULE_COST_SCALE;
    data.prices.modules[FUEL_TANK] = 500 / MODULE_COST_SCALE;
    data.prices.modules[DYNAMO_UNIT] = 2000 / MODULE_COST_SCALE;
    data.prices.modules[GUN_WEAPON] = 2000 / MODULE_COST_SCALE;
    data.prices.minerals[COMMON] = 1;
    data.prices.minerals[CORROSIVE] = 2;
    data.prices.minerals[BASE_METAL] = 3;
    data.prices.minerals[NOBLE] = 4;
    data.prices.minerals[RARE_EARTH] = 5;
    data.prices.minerals[PRECIOUS] = 6;
    data.prices.minerals[RADIOACTIVE] = 8;
    data.prices.minerals[EXOTIC] = 25;
    return data;
}
// Reject non-catalog race IDs; callers need no native queue or ship instance.
inline COUNT shipPrice(std::size_t race) { return ShipPrices.at(race); }
}
#endif
