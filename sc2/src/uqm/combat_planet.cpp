#include "combat_planet.hpp"
#include "combat_entity_legacy.hpp"
#include "simulation_engine.hpp"
#include "races.h"
#include "ship.h"
#include "globdata.h"
#include "collide.h"
#include "units.h"
#include "libs/mathlib.h"
#include <utility>
extern "C" FRAME planet[];
namespace uqm::combat {
void Planet::postprocess(ELEMENT &self) { CalculateGravity(&self); }
void Planet::collide(ELEMENT &self, POINT &point, ELEMENT &other, POINT &otherPoint) {
    collision(&self, &point, &other, &otherPoint);
}
std::optional<std::reference_wrapper<ELEMENT>> spawnPlanet(SimulationEngine &engine, PlanetProperties properties) {
    SimulationBinding selected{engine};
    auto behavior = std::make_unique<Planet>(properties);
    const auto allocation = engine.allocateElement();
    if (!allocation) return std::nullopt;
    auto &element = allocation->get();
    element.playerNr = NEUTRAL_PLAYER_NUM;
    element.hit_points = 200;
    element.state_flags = APPEARING;
    element.life_span = NORMAL_LIFE + 1;
    SetPrimType(&DisplayArray[element.PrimIndex], STAMP_PRIM);
    element.current.image.farray = planet;
    element.current.image.frame = SetAbsFrameIndex(planet[0], properties.spriteIndex);
    detail::attachEntity(element, std::move(behavior),
        detail::EntityHooks::Postprocess | detail::EntityHooks::Collision);
    ZeroVelocityComponents(&element.velocity);
    do {
        element.current.location.x = WRAP_X(DISPLAY_ALIGN_X(engine.random()()));
        element.current.location.y = WRAP_Y(DISPLAY_ALIGN_Y(engine.random()()));
    } while (engine.gravity(element) || TimeSpaceMatterConflict(&element));
    element.mass_points = element.hit_points;
    engine.enqueue(element);
    return allocation;
}
}
