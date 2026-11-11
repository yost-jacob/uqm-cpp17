#include "conversation_state.hpp"
#include "inventory.hpp"
#include "build.h"
#include "commglue.h"
#include "conversation_native.h"
#include "game_state.hpp"
#include "gameev.h"
#include "libs/mathlib.h"
#include "planets/elemdata.h"
#include "ship_tuning.hpp"
#include "sis_state.hpp"
#include <algorithm>
#include <limits>
namespace uqm {
CampaignDialogueState::CampaignDialogueState(PlayerEntity &player, CampaignClock &clock, std::string script,
                                             Callback turn, Report report, dialogue::Json context)
    : LegacyTurnState(std::move(script), std::move(turn)), player_(player), story_(player.story()), clock_(clock),
      report_(std::move(report)), context_(std::move(context)) {}
dialogue::Json CampaignDialogueState::read(const std::string &scope, const std::string &key) const {
    if (scope == "player")
        return context_.at("player").at(key);
    if (scope == "encounter")
        return context_.at(key);
    if (scope != "story")
        throw std::invalid_argument("unsupported live state scope: " + scope);
    const StoryState &story = story_;
    return static_cast<dialogue::Integer>(story.readNamed(key));
}
dialogue::Result CampaignDialogueState::set(const std::string &scope, const std::string &key,
                                            const dialogue::Json &value) {
    if (scope != "story")
        throw std::invalid_argument("unsupported live state scope: " + scope);
    if (value.is_boolean())
        story_.setNamed(key, value.get<bool>() ? 1u : 0u);
    else {
        if (!value.is_number_integer() || value < 0 || value > std::numeric_limits<unsigned>::max())
            throw std::out_of_range("invalid story field value: " + key);
        story_.setNamed(key, value.get<unsigned>());
    }
    return {};
}
dialogue::Integer CampaignDialogueState::storyMaximum(const std::string &name) const {
    if (Inventory::ownsLegacyField(name)) throw std::invalid_argument("inventory field requires transact: " + name);
    return story_.maximumNamed(name);
}
dialogue::Result CampaignDialogueState::setStoryBatch(
    const std::vector<std::pair<std::string, dialogue::Json>> &batch) {
    std::vector<std::pair<std::string, unsigned>> changes;
    changes.reserve(batch.size());
    for (const auto &[name, value] : batch) {
        const auto maximum = storyMaximum(name);
        if (!value.is_boolean() && (!value.is_number_integer() || value < 0 || value > maximum))
            throw std::out_of_range("invalid story value: " + name);
        changes.emplace_back(name, value.is_boolean() ? unsigned(value.get<bool>()) : value.get<unsigned>());
    }
    story_.setNamedBatch(changes);
    return {};
}
void CampaignDialogueState::validateInventoryKey(const std::string &name) const { Inventory::validateKey(name); }
dialogue::Result CampaignDialogueState::transact(const InventoryTransaction &request) {
    const auto result = player_.inventory().transact(request);
    switch (result.status) {
    case InventoryStatus::Completed:
        DeltaSISGauges(UNDEFINED_DELTA, UNDEFINED_DELTA, UNDEFINED_DELTA);
        return {};
    case InventoryStatus::Insufficient: return {"insufficient", {{"asset", result.asset}}};
    case InventoryStatus::NoCapacity: return {"no_capacity", {{"asset", result.asset}}};
    case InventoryStatus::Unavailable: return {"unavailable", {{"asset", result.asset}}};
    }
    throw std::logic_error("unknown inventory transaction result");
}
bool CampaignDialogueState::hasItem(const std::string &item) const {
    return player_.inventory().quantity(item.rfind("items.", 0) == 0 ? item : "items." + item) != 0;
}
dialogue::Integer CampaignDialogueState::balance(const std::string &account) const {
    if (account != "credits" && account != "ru") throw std::invalid_argument("unknown inventory account: " + account);
    return player_.inventory().quantity(account);
}
dialogue::Result CampaignDialogueState::setOutcome(const std::string &outcome) {
    if (outcome == "peace")
        setSegue(Segue_peace);
    else if (outcome == "hostile")
        setSegue(Segue_hostile);
    else if (outcome == "victory")
        setSegue(Segue_victory);
    else if (outcome == "defeat")
        setSegue(Segue_defeat);
    else
        throw std::invalid_argument("unknown encounter outcome: " + outcome);
    return {};
}
dialogue::Integer CampaignDialogueState::randomBelow(dialogue::Integer bound) {
    if (bound <= 0)
        throw std::out_of_range("invalid random bound");
    return TFB_Random() % bound;
}
dialogue::Result CampaignDialogueState::scheduleEvent(const std::string &event,
                                                      dialogue::Integer months,
                                                      dialogue::Integer days,
                                                      dialogue::Integer years) {
    BYTE selector;
    if (event == "ARILOU_UMGAH_CHECK")
        selector = ARILOU_UMGAH_CHECK;
    else if (event == "ADVANCE_ILWRATH_MISSION")
        selector = ADVANCE_ILWRATH_MISSION;
    else if (event == "ADVANCE_THRADD_MISSION")
        selector = ADVANCE_THRADD_MISSION;
    else if (event == "ZOQFOT_DISTRESS_EVENT")
        selector = ZOQFOT_DISTRESS_EVENT;
    else if (event == "SPATHI_SHIELD_EVENT")
        selector = SPATHI_SHIELD_EVENT;
    else if (event == "ADVANCE_UTWIG_SUPOX_MISSION")
        selector = ADVANCE_UTWIG_SUPOX_MISSION;
    else if (event == "SLYLANDRO_RAMP_DOWN")
        selector = SLYLANDRO_RAMP_DOWN;
    else if (event == "ADVANCE_PKUNK_MISSION")
        selector = ADVANCE_PKUNK_MISSION;
    else if (event == "YEHAT_REBEL_EVENT")
        selector = YEHAT_REBEL_EVENT;
    else if (event == "SHOFIXTI_RETURN_EVENT")
        selector = SHOFIXTI_RETURN_EVENT;
    else if (event == "ADVANCE_MYCON_MISSION")
        selector = ADVANCE_MYCON_MISSION;
    else
        throw std::invalid_argument("unregistered calendar event: " + event);
    for (auto value : {months, days, years})
        if (value < 0 || value > std::numeric_limits<COUNT>::max())
            throw std::out_of_range("calendar offset out of range");
    if (!clock_.schedule(CalendarOffset{static_cast<unsigned>(years), static_cast<unsigned>(months),
                                       static_cast<unsigned>(days)}, static_cast<CampaignEvent>(selector)))
        throw std::runtime_error("campaign event queue is full");
    return {};
}
dialogue::Result CampaignDialogueState::prepareReport(const std::string &report,
                                                      const dialogue::Json &parameters) {
    if (!parameters.is_object())
        throw std::invalid_argument("report parameters must be an object");
    return report_(report, parameters);
}

} // namespace uqm
namespace uqm {
namespace {
COUNT raceId(const std::string &name) {
    struct NamedRace {
        const char *name;
        COUNT id;
    };
    static constexpr NamedRace races[] = {{"arilou", ARILOU_SHIP},
                                          {"chmmr", CHMMR_SHIP},
                                          {"human", HUMAN_SHIP},
                                          {"orz", ORZ_SHIP},
                                          {"pkunk", PKUNK_SHIP},
                                          {"shofixti", SHOFIXTI_SHIP},
                                          {"spathi", SPATHI_SHIP},
                                          {"supox", SUPOX_SHIP},
                                          {"thradd", THRADDASH_SHIP},
                                          {"utwig", UTWIG_SHIP},
                                          {"vux", VUX_SHIP},
                                          {"yehat", YEHAT_SHIP},
                                          {"melnorme", MELNORME_SHIP},
                                          {"druuge", DRUUGE_SHIP},
                                          {"ilwrath", ILWRATH_SHIP},
                                          {"mycon", MYCON_SHIP},
                                          {"probe", SLYLANDRO_SHIP},
                                          {"umgah", UMGAH_SHIP},
                                          {"urquan", URQUAN_SHIP},
                                          {"zoqfot", ZOQFOTPIK_SHIP},
                                          {"syreen", SYREEN_SHIP},
                                          {"blackurq", BLACK_URQUAN_SHIP},
                                          {"yehat_rebel", YEHAT_REBEL_SHIP},
                                          {"lastbat", SAMATRA_SHIP}};
    for (const auto &race : races)
        if (name == race.name)
            return race.id;
    throw std::invalid_argument("unknown campaign race: " + name);
}
} // namespace
dialogue::Integer CampaignDialogueState::fuel() const {
    return player_.sis().fuel() / FUEL_TANK_SCALE;
}
dialogue::Integer CampaignDialogueState::fuelCapacity() const {
    return player_.inventory().capacity("fuel");
}
dialogue::Integer CampaignDialogueState::quantity(const std::string &item) const {
    return player_.inventory().quantity(item == "radioactive" ? "minerals.radioactive" : item);
}
dialogue::Integer CampaignDialogueState::availableSlots(const std::string &type) const {
    return EscortFeasibilityStudy(raceId(type));
}
dialogue::Integer CampaignDialogueState::shipCount(const std::string &type) const {
    return CountEscortShips(raceId(type));
}
bool CampaignDialogueState::hasShip(const std::string &type) const {
    return shipCount(type) != 0;
}
std::string CampaignDialogueState::relationship(const std::string &type) const {
    const auto state = CheckAlliance(raceId(type));
    return state == GOOD_GUY ? "allied" : state == DEAD_GUY ? "extinct" : "neutral";
}
dialogue::Result CampaignDialogueState::createShip(const std::string &type,
                                                   const std::string &captain,
                                                   dialogue::Integer crew) {
    if (type != "spathi" || captain != "fwiffo")
        throw std::invalid_argument("unregistered named captain: " + type + "." + captain);
    if (crew <= 0 || crew > combat::shipTuning(type).ship.maxCrew)
        throw std::out_of_range("named captain crew exceeds ship capacity");
    const auto count = AddEscortShips(SPATHI_SHIP, 1);
    if (count && SetEscortCrewComplement(SPATHI_SHIP, static_cast<COUNT>(crew),
                                         NAME_OFFSET + NUM_CAPTAINS_NAMES) < 0)
        throw std::runtime_error("could not assign recruited captain");
    return {"ok", {{"count", count}}};
}
dialogue::Result CampaignDialogueState::giveShips(const std::string &type,
                                                  dialogue::Integer count) {
    if (count < 0 || count > std::numeric_limits<SIZE>::max())
        throw std::out_of_range("invalid escort count");
    const auto race = raceId(type);
    return {"ok", {{"count", count ? AddEscortShips(race, count) : 0}}};
}
dialogue::Result CampaignDialogueState::removeShips(const std::string &type,
                                                    dialogue::Integer count) {
    if (count < 0 || count > std::numeric_limits<COUNT>::max())
        throw std::out_of_range("invalid escort removal count");
    const auto race = raceId(type);
    const auto available = CountEscortShips(race);
    const auto removed = std::min<dialogue::Integer>(count, available);
    if (count >= available)
        RemoveEscortShips(race);
    else
        RemoveSomeEscortShips(race, static_cast<COUNT>(count));
    return {"ok", {{"count", removed}}};
}
dialogue::Result CampaignDialogueState::setRelationship(const std::string &type,
                                                        const std::string &relation) {
    if (relation != "allied" && relation != "neutral")
        throw std::invalid_argument("invalid alliance relation");
    SetRaceAllied(raceId(type), relation == "allied" ? TRUE : FALSE);
    return {};
}
bool CampaignDialogueState::raceAvailable(const std::string &type) const {
    const auto fleet = GetStarShipFromIndex(&GLOBAL(avail_race_q), raceId(type));
    if (!fleet)
        return false;
    const auto *info = LockFleetInfo(&GLOBAL(avail_race_q), fleet);
    const bool available = info->actual_strength != 0 || info->allied_state != DEAD_GUY;
    UnlockFleetInfo(&GLOBAL(avail_race_q), fleet);
    return available;
}
dialogue::Result CampaignDialogueState::revealRace(const std::string &type) {
    // The native reveal returns a race index; Arilou's valid index is zero.
    const bool available = raceAvailable(type);
    StartSphereTracking(raceId(type));
    return {"ok", {{"available", available}}};
}
dialogue::Result CampaignDialogueState::configureShip(const std::string &operation,
                                                      const dialogue::Json &parameters) {
    if (operation != "chmmr_bomb_refit" || !parameters.is_object() || !parameters.empty())
        throw std::invalid_argument("unregistered ship configuration: " + operation);
    ApplyChmmrBombRefit(story_.readNamed("STARBASE_AVAILABLE") != 0);
    return {};
}
dialogue::Result CampaignDialogueState::adjustShip(const std::string &property,
                                                   dialogue::Integer delta) {
    if (property == "fuel" || property == "crew") {
        const auto scale = property == "fuel" ? FUEL_TANK_SCALE : 1;
        // SIZE is the signed native gauge argument; never narrow into its sentinel.
        const auto bound = std::numeric_limits<SIZE>::max() / scale;
        if (delta < -bound || delta > bound)
            throw std::out_of_range("gauge adjustment exceeds native range");
        DeltaSISGauges(property == "crew" ? static_cast<SIZE>(delta) : 0,
                       property == "fuel" ? static_cast<SIZE>(delta * scale) : 0, 0);
        return {};
    }
    if (property == "landers") {
        auto &sis = player_.sis();
        const dialogue::Integer value = sis.landers();
        if (delta < -value || delta > MAX_LANDERS - value)
            throw std::out_of_range("lander adjustment exceeds capacity");
        sis.setLanders(static_cast<BYTE>(value + delta));
        DrawLanders();
        return {};
    }
    if (property != "biomass")
        throw std::invalid_argument("unsupported ship adjustment: " + property);
    auto &sis = player_.sis();
    const dialogue::Integer value = sis.biomass();
    if (delta < -value || delta > std::numeric_limits<COUNT>::max() - value)
        throw std::out_of_range("biomass adjustment exceeds storage range");
    sis.setBiomass(static_cast<COUNT>(value + delta));
    return {};
}
} // namespace uqm
