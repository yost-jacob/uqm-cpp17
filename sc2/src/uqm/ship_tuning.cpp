#include "ship_tuning.hpp"
#include <array>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace uqm::combat {
namespace {
using Json = nlohmann::json;
struct Schema { const char *id; std::array<const char *, 2> profiles; };
// These identify supported behavior code, not balance values. A new behavior
// must explicitly declare its required projectile profiles here.
constexpr Schema Schemas[] = {
    {"androsyn", {"missile"}}, {"arilou", {}}, {"blackurq", {"missile", "gas"}},
    {"chenjesu", {"missile", "fragment"}}, {"chmmr", {}}, {"druuge", {"missile"}},
    {"human", {"missile"}}, {"ilwrath", {"missile"}}, {"lastbat", {"comet", "sentinel"}},
    {"melnorme", {"cmissile", "charge"}}, {"mmrnmhrm", {"missile"}}, {"mycon", {"plasma"}},
    {"orz", {"missile"}}, {"pkunk", {"missile"}}, {"probe", {}},
    {"shofixti", {"missile"}}, {"sis_ship", {"blaster"}}, {"slylandr", {}},
    {"spathi", {"missile", "discriminator"}}, {"supox", {"missile"}},
    {"syreen", {"missile"}}, {"thradd", {"missile", "napalm"}}, {"umgah", {"cone"}},
    {"urquan", {"missile"}}, {"utwig", {"missile"}}, {"vux", {"limpet"}},
    {"yehat", {"missile"}}, {"zoqfot", {"missile", "tongue"}}
};
// Check exact key sets, so a typo never silently leaves a field ineffective.
void fields(const Json &object, std::initializer_list<const char *> names) {
    if (!object.is_object() || object.size() != names.size())
        throw std::runtime_error("wrong field set");
    for (const auto *name : names)
        if (!object.contains(name)) throw std::runtime_error(std::string("missing field: ") + name);
}
int integer(const Json &object, const char *name, int minimum, int maximum) {
    const auto &value = object.at(name);
    if (!value.is_number_integer())
        throw std::runtime_error(std::string(name) + " must be an integer");
    if (value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(maximum))
        throw std::runtime_error(std::string(name) + " is out of range");
    const auto number = value.get<std::int64_t>();
    if (number < minimum || number > maximum)
        throw std::runtime_error(std::string(name) + " is out of range");
    return static_cast<int>(number);
}
ShipStats stats(const Json &value) {
    fields(value, {"max_crew", "max_energy", "max_thrust", "thrust_increment", "energy_regeneration",
        "energy_wait", "thrust_wait", "turn_wait", "ship_mass", "weapon_energy_cost", "weapon_wait",
        "special_energy_cost", "special_wait", "melee_cost", "projectiles"});
    // Crew/motion arithmetic also passes through legacy signed 16-bit fields.
    ShipStats result{};
    result.maxCrew = integer(value, "max_crew", 1, 32767);
    result.maxThrust = integer(value, "max_thrust", 0, 32767);
    result.thrustIncrement = integer(value, "thrust_increment", 0, 32767);
    result.maxEnergy = integer(value, "max_energy", 1, 255);
    // Keep the native value signed; the descriptor adapter encodes its byte.
    result.energyRegeneration = static_cast<std::int8_t>(integer(value, "energy_regeneration", -128, 127));
    result.energyWait = integer(value, "energy_wait", 0, 255);
    result.thrustWait = integer(value, "thrust_wait", 0, 255);
    result.turnWait = integer(value, "turn_wait", 0, 255);
    result.shipMass = integer(value, "ship_mass", 0, 255);
    result.weaponEnergyCost = integer(value, "weapon_energy_cost", 0, 255);
    result.weaponWait = integer(value, "weapon_wait", 0, 255);
    result.specialEnergyCost = integer(value, "special_energy_cost", 0, 255);
    result.specialWait = integer(value, "special_wait", 0, 255);
    result.meleeCost = integer(value, "melee_cost", 0, 255);
    return result;
}
ProjectileStats projectileStats(const Json &value) {
    fields(value, {"speed", "hit_points", "damage", "lifetime", "flight"});
    const auto &flight = value.at("flight");
    fields(flight, {"steering_period_ticks", "initial_delay_ticks", "acceleration_per_tick",
        "maximum_speed", "directional_sprite", "snap_to_target"});
    for (const auto *key : {"directional_sprite", "snap_to_target"})
        if (!flight.at(key).is_boolean()) throw std::runtime_error(std::string(key) + " must be boolean");
    FlightTuning behavior{
        static_cast<std::uint16_t>(integer(flight, "steering_period_ticks", 0, 32767)),
        static_cast<std::uint16_t>(integer(flight, "initial_delay_ticks", 0, 32767)),
        static_cast<std::int16_t>(integer(flight, "acceleration_per_tick", 0, 1023)),
        static_cast<std::int16_t>(integer(flight, "maximum_speed", 0, 1023)),
        flight.at("directional_sprite").get<bool>(), flight.at("snap_to_target").get<bool>()};
    if (behavior.maxSpeed < integer(value, "speed", 0, 1023))
        throw std::runtime_error("maximum_speed must cover launch speed");
    return {static_cast<std::int16_t>(integer(value, "speed", 0, 32767)),
        static_cast<std::int16_t>(integer(value, "hit_points", 1, 255)),
        // The element engine stores damage in its byte-sized mass field.
        static_cast<std::int16_t>(integer(value, "damage", 0, 255)),
        static_cast<std::uint16_t>(integer(value, "lifetime", 1, 32767)), behavior};
}
}
ShipTuningCatalog::ShipTuningCatalog(std::istream &input) {
    const auto root = Json::parse(input);
    fields(root, {"schema_version", "ships"});
    integer(root, "schema_version", 2, 2);
    const auto &records = root.at("ships");
    if (!records.is_object() || records.size() != std::size(Schemas))
        throw std::runtime_error("ships must contain the complete supported roster");
    for (const auto &schema : Schemas) {
        try {
            const auto &record = records.at(schema.id);
            ShipTuning tuning{stats(record), {}};
            const auto &profiles = record.at("projectiles");
            std::size_t expected = 0;
            for (const auto *name : schema.profiles) if (name) ++expected;
            if (!profiles.is_object() || profiles.size() != expected)
                throw std::runtime_error("wrong projectile profile set");
            for (const auto *name : schema.profiles) {
                if (!name) continue;
                try { tuning.projectiles.emplace(name, projectileStats(profiles.at(name))); }
                catch (const std::exception &error) {
                    throw std::runtime_error(std::string("projectiles.") + name + ": " + error.what());
                }
            }
            if (std::string_view(schema.id) == "sis_ship") {
                const auto &blaster = tuning.projectile("blaster");
                if (blaster.damage < 1 || blaster.damage > 255 / 3 || blaster.hitPoints > 255 / 3)
                    throw std::runtime_error("blaster damage/hit_points must support three module tiers");
            }
            if (std::string_view(schema.id) == "melnorme") {
                const auto &charge = tuning.projectile("charge");
                if (charge.damage < 1 || charge.damage > 255 / 8 || charge.hitPoints != charge.damage)
                    throw std::runtime_error("charge must have equal damage/hit_points and support four tiers");
            }
            if (std::string_view(schema.id) == "mycon") {
                const auto &plasma = tuning.projectile("plasma");
                if (plasma.damage < 1 || plasma.hitPoints != plasma.damage)
                    throw std::runtime_error("plasma damage/hit_points must be equal and positive");
            }
            ships_.emplace(schema.id, std::move(tuning));
        } catch (const std::exception &error) {
            throw std::runtime_error(std::string(schema.id) + ": " + error.what());
        }
    }
}
std::uint16_t weaponRangeHint(std::int64_t distance) noexcept {
    return static_cast<std::uint16_t>(std::clamp<std::int64_t>(distance, 0,
        std::numeric_limits<std::uint16_t>::max()));
}
const ShipTuning &ShipTuningCatalog::at(std::string_view id) const {
    const auto found = ships_.find(id);
    if (found == ships_.end()) throw std::out_of_range("unknown ship tuning ID");
    return found->second;
}
const ProjectileStats &ShipTuning::projectile(std::string_view name) const {
    const auto found = projectiles.find(name);
    if (found == projectiles.end()) throw std::out_of_range("unknown projectile tuning profile");
    return found->second;
}
}
