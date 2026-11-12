#include "exploration_legacy.hpp"
#include "system_runtime.hpp"
#include "game_state.hpp"
#include "globdata.h"
#include "gamestr.h"
#include "gendef.h"
#include "libs/strlib.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
namespace {
// C save/debug calls run synchronously on the session thread. This adapter
// exposes only save/name/limit operations, never the complete exploration.
thread_local uqm::ExplorationLegacyBinding *saveView = nullptr;
}
namespace uqm {
void prepareSystemSave(SolarSystemLocation &location, const SolarSystem &system,
        const SystemFlightState &flight, const STAMP &ship) {
    auto &output = location.flight;
    if (location.orbit) {
        const auto &world = location.orbit->world;
        output.planet = world.planet + 1;
        output.loadOrbit = world.moon ? *world.moon + 2 : 1;
    } else {
        output.facing = GetFrameIndex(ship.frame) + 1;
        output.loadOrbit = 0;
        output.planet = system.inInnerSystem() ? system.innerPlanet().id().planet + 1 : 0;
        if (system.inInnerSystem()) output.position = flight.outerAnchor;
    }
}
void saveSolarSystemLocation(SOLARSYS_STATE &system) {
    prepareSystemSave(systemLocation(system), *system.worlds->system, system.worlds->flight, GLOBAL(ShipStamp));
}
ExplorationLegacyBinding::ExplorationLegacyBinding(PlayerEntity &player,
        const std::unique_ptr<SolarSystem> &system, SystemFlightState &flight,
        const STAR_DESC &star, const STAMP &ship)
    : player_(player), system_(system), flight_(flight), star_(star), ship_(ship), previous_(saveView) {
    if (!system_) throw std::invalid_argument("save binding requires a constructed system");
    saveView = this;
}
ExplorationLegacyBinding::~ExplorationLegacyBinding() { saveView = previous_; }
bool ExplorationLegacyBinding::inOrbit() const {
    return std::get<SolarSystemLocation>(player_.location()).orbit.has_value();
}
void ExplorationLegacyBinding::prepareSave() {
    prepareSystemSave(std::get<SolarSystemLocation>(player_.location()), *system_, flight_, ship_);
}
void ExplorationLegacyBinding::refreshLimits() { flight_.configure(player_.sis().loadout()); }
void ExplorationLegacyBinding::copyName(UNICODE *buffer, COUNT capacity) const {
    if (!capacity) return;
    if (const auto named = namedPlanetaryBody(star_, *system_)) {
        utf8StringCopy(buffer, capacity, named->data());
        return;
    }
    utf8StringCopy(buffer, capacity, GLOBAL_SIS(PlanetName));
    if (!system_->inInnerSystem() || !system_->hasSelectedWorld()) return;
    const auto moon = system_->selectedWorld().id().moon;
    const auto length = std::strlen(buffer);
    if (moon && capacity >= length + 3) std::snprintf(buffer + length, capacity - length, "-%c", 'A' + *moon);
}
}
extern "C" {
void RefreshExplorationFlightLimits(void) { if (saveView) saveView->refreshLimits(); }
bool playerInSolarSystem(void) { return saveView != nullptr; }
bool playerInPlanetOrbit(void) { return saveView && saveView->inOrbit(); }
void SaveSolarSysLocation(void) {
    if (!saveView) std::abort(); // A save caller must first establish exploration context.
    saveView->prepareSave();
}
void GetPlanetOrMoonName(UNICODE *buffer, COUNT capacity) {
    if (saveView) saveView->copyName(buffer, capacity);
    else utf8StringCopy(buffer, capacity, GLOBAL_SIS(PlanetName));
}
}

std::optional<std::string_view>
uqm::namedPlanetaryBody(const STAR_DESC &star, const SolarSystem &model)
{
    const auto text = [](COUNT index) -> std::optional<std::string_view> {
        const auto *value = GAME_STRING(index);
        return value ? std::optional<std::string_view>{value} : std::nullopt;
    };
	if (!model.inInnerSystem() || !model.hasSelectedWorld())
		return std::nullopt; // Not inside an inner system, so no name


	if (star.Index == SOL_DEFINED)
	{	// Planets and moons in Sol
		int planet;
		int moon;

		planet = model.selectedWorld().id().planet;

		if (!model.selectedWorld().id().moon)
		{	// A planet
			return text(PLANET_NUMBER_BASE + planet);
		}

		// Moons
		moon = *model.selectedWorld().id().moon;
		switch (planet)
		{
			case 2: // Earth
				switch (moon)
				{
					case 0: // Starbase
						return text(STARBASE_STRING_BASE + 0);
					case 1: // Luna
						return text(PLANET_NUMBER_BASE + 9);
				}
				break;
			case 4: // Jupiter
				switch (moon)
				{
					case 0: // Io
						return text(PLANET_NUMBER_BASE + 10);
					case 1: // Europa
						return text(PLANET_NUMBER_BASE + 11);
					case 2: // Ganymede
						return text(PLANET_NUMBER_BASE + 12);
					case 3: // Callisto
						return text(PLANET_NUMBER_BASE + 13);
				}
				break;
			case 5: // Saturn
				if (moon == 0) // Titan
					return text(PLANET_NUMBER_BASE + 14);
				break;
			case 7: // Neptune
				if (moon == 0) // Triton
					return text(PLANET_NUMBER_BASE + 15);
				break;
		}
	}
	else if (star.Index == SPATHI_DEFINED)
	{
		if (model.selectedWorld().id().planet == 0 && !model.selectedWorld().id().moon)
		{
#ifdef NOTYET
			return "Spathiwa";
#endif // NOTYET
		}
	}
	else if (star.Index == SAMATRA_DEFINED)
	{
		if (model.selectedWorld().id().planet == 4 && model.selectedWorld().id().moon == 0)
		{	// Sa-Matra
			return text(PLANET_NUMBER_BASE + 32);
		}
	}

	return std::nullopt;
}
