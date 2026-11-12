#ifndef UQM_NAVIGATION_RULES_HPP_
#define UQM_NAVIGATION_RULES_HPP_
#include "races.h"
#include "velocity.h"
#include <array>
namespace uqm {
constexpr SIZE navigationSpeed(SIZE units) noexcept { return units * VELOCITY_SCALE / 2; }
struct EncounterSize { COUNT minimum = 0, maximum = 0; };
// Immutable baseline definitions. Story adjustments belong to the caller; these
// tables are not saved. Session exposes its constexpr instance to native modes.
struct NavigationRules {
    std::array<COUNT, NUM_AVAILABLE_RACES> spawnPercent{{
		20,  /* ARILOU_SHIP */
		 0,  /* CHMMR_SHIP */
		 0,  /* HUMAN_SHIP */
		20,  /* ORZ_SHIP */
		40,  /* PKUNK_SHIP */
		 0,  /* SHOFIXTI_SHIP */
		20,  /* SPATHI_SHIP */
		40,  /* SUPOX_SHIP */
		60,  /* THRADDASH_SHIP */
		40,  /* UTWIG_SHIP */
		40,  /* VUX_SHIP */
		60,  /* YEHAT_SHIP */
		 0,  /* MELNORME_SHIP */
		30,  /* DRUUGE_SHIP */
		60,  /* ILWRATH_SHIP */
		40,  /* MYCON_SHIP */
		 2,  /* SLYLANDRO_SHIP */
		30,  /* UMGAH_SHIP */
		70,  /* URQUAN_SHIP */
		 0,  /* ZOQFOTPIK_SHIP */
		 0,  /* SYREEN_SHIP */
		70,  /* BLACK_URQUAN_SHIP */
		60,  /* YEHAT_REBEL_SHIP */
		 0,  /* URQUAN_DRONE_SHIP */
    }};
    std::array<EncounterSize, YEHAT_REBEL_SHIP + 1> encounterSize{{
		EncounterSize{1, 5},  /* ARILOU_SHIP */
		EncounterSize{},                 /* CHMMR_SHIP */
		EncounterSize{},                 /* HUMAN_SHIP */
		EncounterSize{1, 5},  /* ORZ_SHIP */
		EncounterSize{1, 5},  /* PKUNK_SHIP */
		EncounterSize{},                 /* SHOFIXTI_SHIP */
		EncounterSize{1, 5},  /* SPATHI_SHIP */
		EncounterSize{1, 5},  /* SUPOX_SHIP */
		EncounterSize{1, 5},  /* THRADDASH_SHIP */
		EncounterSize{1, 5},  /* UTWIG_SHIP */
		EncounterSize{1, 5},  /* VUX_SHIP */
		EncounterSize{1, 5},  /* YEHAT_SHIP */
		EncounterSize{1, 1},  /* MELNORME_SHIP */
		EncounterSize{1, 5},  /* DRUUGE_SHIP */
		EncounterSize{1, 5},  /* ILWRATH_SHIP */
		EncounterSize{1, 5},  /* MYCON_SHIP */
		EncounterSize{1, 1},  /* SLYLANDRO_SHIP */
		EncounterSize{1, 5},  /* UMGAH_SHIP */
		EncounterSize{1, 5},  /* URQUAN_SHIP */
		EncounterSize{1, 5},  /* ZOQFOTPIK_SHIP */
		EncounterSize{},                 /* SYREEN_SHIP */
		EncounterSize{1, 5},  /* BLACK_URQUAN_SHIP */
		EncounterSize{1, 5},  /* YEHAT_REBEL_SHIP */
    }};
    std::array<SIZE, YEHAT_REBEL_SHIP + 1> pursuitSpeed{{
		navigationSpeed (40),  /* ARILOU_SHIP */
		navigationSpeed (27),  /* CHMMR_SHIP */
		navigationSpeed (24),  /* HUMAN_SHIP */
		navigationSpeed (40),  /* ORZ_SHIP */
		navigationSpeed (40),  /* PKUNK_SHIP */
		navigationSpeed (35),  /* SHOFIXTI_SHIP */
		navigationSpeed (48),  /* SPATHI_SHIP */
		navigationSpeed (40),  /* SUPOX_SHIP */
		navigationSpeed (50),  /* THRADDASH_SHIP */
		navigationSpeed (30),  /* UTWIG_SHIP */
		navigationSpeed (21),  /* VUX_SHIP */
		navigationSpeed (30),  /* YEHAT_SHIP */
		navigationSpeed (40),  /* MELNORME_SHIP */
		navigationSpeed (20),  /* DRUUGE_SHIP */
		navigationSpeed (25),  /* ILWRATH_SHIP */
		navigationSpeed (27),  /* MYCON_SHIP */
		navigationSpeed (60),  /* SLYLANDRO_SHIP */
		navigationSpeed (18),  /* UMGAH_SHIP */
		navigationSpeed (30),  /* URQUAN_SHIP */
		navigationSpeed (40),  /* ZOQFOTPIK_SHIP */
		navigationSpeed (36),  /* SYREEN_SHIP */
		navigationSpeed (30),  /* BLACK_URQUAN_SHIP */
		navigationSpeed (30),  /* YEHAT_REBEL_SHIP */
    }};
};
}
#endif
