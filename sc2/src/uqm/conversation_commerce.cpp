// Merchant rules adapted from Paul Reiche/Fred Ford's scripts (GPL-2.0-or-later).
#include "conversation_commerce.hpp"
#include "inventory.hpp"
#include "hyper.h"
#include "units.h"
#include <cmath>
#include "game_state.hpp"
#include "sis_legacy.hpp"
#include "simulation_engine.hpp"
#include "build.h"
#include "planets/planets.h"
#include <algorithm>
#include <array>
#include <limits>
#include <string_view>
namespace uqm {
using dialogue::Json;
using dialogue::Result;
using dialogue::Integer;
namespace {
int module(std::string_view name) {
    static constexpr std::pair<std::string_view, int> modules[] = {
        {"BLASTER_WEAPON", BLASTER_WEAPON}, {"ANTIMISSILE_DEFENSE", ANTIMISSILE_DEFENSE},
        {"HIGHEFF_FUELSYS", HIGHEFF_FUELSYS}, {"TRACKING_SYSTEM", TRACKING_SYSTEM},
        {"CANNON_WEAPON", CANNON_WEAPON}, {"SHIVA_FURNACE", SHIVA_FURNACE}};
    for (const auto &[key, id] : modules) if (name == key) return id;
    throw std::invalid_argument("unknown technology module: " + std::string(name));
}
unsigned shield(std::string_view name) {
    static constexpr std::pair<std::string_view, unsigned> hazards[] = {
        {"BIOLOGICAL_DISASTER", BIOLOGICAL_DISASTER}, {"EARTHQUAKE_DISASTER", EARTHQUAKE_DISASTER},
        {"LIGHTNING_DISASTER", LIGHTNING_DISASTER}, {"LAVASPOT_DISASTER", LAVASPOT_DISASTER}};
    for (const auto &[key, id] : hazards) if (name == key) return 1u << id;
    throw std::invalid_argument("unknown lander hazard: " + std::string(name));
}
Result exchange(Inventory &inventory, const InventoryTransaction &request) {
    Result success; // Allocate the success payload before committing payment.
    const auto result = inventory.transact(request);
    switch (result.status) {
    case InventoryStatus::Completed: return success;
    case InventoryStatus::Insufficient: return {"no_credit", {{"asset", result.asset}}};
    case InventoryStatus::NoCapacity: return {"no_room", {{"asset", result.asset}}};
    case InventoryStatus::Unavailable: return {"unavailable", {}};
    }
    throw std::logic_error("unknown inventory result");
}
unsigned escortSlots(const GameSession &session, COUNT race) {
    return race < session.raceFleets().size() ? MAX_BUILT_SHIPS - session.player().fleet().size() : 0;
}
// Reserve actual native vessels before any payment. Failure/exception removes
// only this reservation and restores RNG, leaving earlier fleet entries intact.
class ReservedShips {
public:
    ReservedShips(GameSession &session, RandomStream &random, COUNT race, unsigned count)
        : fleet_(session.player().fleet()), source_(random), random_(random) {
        ids_.reserve(count);
        const auto &fleets = session.raceFleets();
        if (race >= fleets.size()) return;
        const auto &definition = fleets.at(race);
        try {
            for (unsigned i = 0; i < count; ++i) {
                const auto id = fleet_.create(race, definition, source_);
                if (!id) break;
                ids_.push_back(*id);
            }
        } catch (...) { rollback(); throw; }
    }
    ~ReservedShips() { if (!committed_) rollback(); }
    unsigned size() const noexcept { return ids_.size(); }
    void commit() noexcept { committed_ = true; }
private:
    void rollback() noexcept {
        for (const auto id : ids_) fleet_.erase(id);
        ids_.clear();
        source_ = random_;
    }
    Fleet &fleet_;
    RandomStream &source_;
    RandomStream random_;
    std::vector<ShipId> ids_;
    bool committed_ = false;
};
}
ConversationCommerce::ConversationCommerce(GameSession &session, RandomStream &random, Json definitions)
    : session_(session), random_(random), player_(session.player()), clock_(session.clock()), definitions_(std::move(definitions)) {
    const auto positive = [](const Json &value, Integer limit = 65535) {
        if (!value.is_number_integer() || value <= 0 || value > limit) throw std::invalid_argument("invalid merchant quantity/price");
    };
    if (definitions_.contains("druuge")) {
        const auto &book = definitions_.at("druuge");
        for (auto item : {"ship", "rosy_sphere", "artifact_2", "artifact_3", "fuel"}) positive(book.at(item).at("crew"));
        positive(book.at("fuel").at("amount"));
        for (const auto &offer : book.at("sales")) {
            Inventory::validateKey(offer.at("asset").get<std::string>());
            if (!offer.at("ships").is_number_integer() || offer.at("ships") < 0 || offer.at("ships") > MAX_BUILT_SHIPS)
                throw std::invalid_argument("invalid merchant ship quantity");
            offer.at("fill_fuel").get<bool>(); offer.at("speech").get<std::string>();
        }
    }
    if (!definitions_.contains("melnorme")) return;
    const auto &book = definitions_.at("melnorme");
    positive(book.at("fuel").at("unit_price"));
    positive(book.at("sales").at("biomass").at("credits_per_unit"));
    positive(book.at("sales").at("rainbows").at("credits_per_new_location"));
    positive(book.at("information").at("price"));
    for (const auto &entry : book.at("information").at("categories")) {
        const auto maximum = player_.story().maximumNamed(entry.at("cursor").get<std::string>());
        if (!entry.at("speech").is_array() || entry.at("speech").empty() || entry.at("speech").size() > maximum)
            throw std::invalid_argument("invalid information catalog");
    }
    positive(book.at("rescue").at("extra_fuel_units"), 100);
    positive(book.at("penance").at("minimum_value"));
    // Validate every grant before actions can debit an account. These operations
    // have nonallocating writes after validation; data is immutable for the visit.
    for (const auto &technology : definitions_.at("melnorme").at("technology")) {
        if (!technology.at("price").is_number_integer() || technology.at("price") <= 0 || technology.at("price") > 65535)
            throw std::invalid_argument("invalid technology price");
        const auto &grant = technology.at("grant");
        const auto kind = grant.at("kind").get<std::string>();
        if (kind == "module_unlock") {
            module(grant.at("module").get_ref<const std::string &>());
            positive(grant.at("build_cost_ru"), 255 * MODULE_COST_SCALE);
            if (grant.at("build_cost_ru") < MODULE_COST_SCALE) throw std::invalid_argument("module unlock must have a nonzero scaled cost");
        } else if (kind == "lander_shield") shield(grant.at("hazard").get_ref<const std::string &>());
        else if (kind == "state") {
            const auto name = grant.at("key").get<std::string>();
            if (name != "IMPROVED_LANDER_SPEED" && name != "IMPROVED_LANDER_CARGO" && name != "IMPROVED_LANDER_SHOT")
                throw std::invalid_argument("invalid technology state field");
            const auto &value = grant.at("value");
            if (!value.is_number_integer() || value <= 0 || value > player_.story().maximumNamed(name)) throw std::out_of_range("invalid technology value");
        } else throw std::invalid_argument("unknown technology grant");
    }
}
bool ConversationCommerce::hasTechnology(const Json &technology) const {
    const auto &grant = technology.at("grant");
    const auto kind = grant.at("kind").get<std::string>();
    if (kind == "module_unlock") return player_.gameData().prices.modules[module(grant.at("module").get_ref<const std::string &>())] != 0;
    if (kind == "lander_shield") return (player_.story().readNamed("LANDER_SHIELDS") & shield(grant.at("hazard").get_ref<const std::string &>())) != 0;
    return player_.story().readNamed(grant.at("key").get<std::string>()) != 0;
}
void ConversationCommerce::grantTechnology(const Json &technology) {
    const auto &grant = technology.at("grant");
    const auto &kind = grant.at("kind").get_ref<const std::string &>();
    if (kind == "module_unlock") player_.gameData().prices.modules[module(grant.at("module").get_ref<const std::string &>())] = grant.at("build_cost_ru").get<unsigned>() / MODULE_COST_SCALE;
    else if (kind == "lander_shield") player_.story().setNamed("LANDER_SHIELDS", player_.story().readNamed("LANDER_SHIELDS") | shield(grant.at("hazard").get_ref<const std::string &>()));
    else player_.story().setNamed(grant.at("key").get_ref<const std::string &>(), grant.at("value").get<unsigned>());
}
const Json *ConversationCommerce::nextTechnology() const {
    if (!definitions_.contains("melnorme")) return nullptr;
    for (const auto &technology : definitions_.at("melnorme").at("technology")) if (!hasTechnology(technology)) return &technology;
    return nullptr;
}
bool ConversationCommerce::marketAvailable(const std::string &market, const std::string &category) const {
    if (market != "melnorme") throw std::invalid_argument("unknown merchant");
    if (category == "technology") return nextTechnology() != nullptr;
    const auto &categories = definitions_.at(market).at("information").at("categories");
    if (category == "information") {
        for (const auto &entry : categories) if (player_.story().readNamed(entry.at("cursor").get<std::string>()) < entry.at("speech").size()) return true;
        return false;
    }
    const auto &entry = categories.at(category);
    return player_.story().readNamed(entry.at("cursor").get<std::string>()) < entry.at("speech").size();
}
Integer ConversationCommerce::unreportedRainbows() const {
    unsigned mask = player_.story().readNamed("RAINBOW_WORLD0") | (player_.story().readNamed("RAINBOW_WORLD1") << 8);
    unsigned count = 0;
    for (; mask; mask >>= 1) count += mask & 1;
    return std::max<Integer>(0, Integer(count) - player_.story().readNamed("MELNORME_RAINBOW_COUNT"));
}
Result ConversationCommerce::purchase(const std::string &market, const std::string &item, const Json &quantity) {
    auto &inventory = player_.inventory();
    auto &story = player_.story();
    if (item != "fuel" && (!quantity.is_number_integer() || quantity != 1)) throw std::invalid_argument("purchase requires one item");
    if (market == "druuge") {
        const auto &prices = definitions_.at(market);
        const Integer cost = prices.at(item).at("crew");
        if (inventory.quantity("crew") < cost) return {"no_credit", {}};
        InventoryTransaction request{{{"crew", cost}}, {}};
        if (item == "ship") {
            if (!escortSlots(session_, DRUUGE_SHIP)) return {"no_room", {}};
            ReservedShips reserved{session_, random_, DRUUGE_SHIP, 1};
            if (reserved.size() != 1) return {"no_room", {}};
            auto result = exchange(inventory, request);
            if (result.code == "ok") { reserved.commit(); slavery_ += cost; }
            return result;
        }
        if (item == "fuel") {
            const auto free = inventory.capacity("fuel_hundredths") - inventory.quantity("fuel_hundredths");
            const auto added = std::max<Integer>(0, std::min<Integer>(free, prices.at(item).at("amount").get<Integer>() * FUEL_TANK_SCALE));
            if (added) request.receive["fuel_hundredths"] = added;
        } else request.receive["items." + item] = 1;
        auto result = exchange(inventory, request);
        if (result.code == "ok") {
            slavery_ += cost;
            if (item == "rosy_sphere") story.setNamed("ROSY_SPHERE", 1);
        }
        return result;
    }
    if (market != "melnorme") throw std::invalid_argument("unknown merchant");
    const auto &book = definitions_.at(market);
    Integer cost = 0;
    Json speech = Json::array();
    const Json *technology = nullptr;
    const Json *information = nullptr;
    unsigned index = 0;
    InventoryTransaction request;
    if (item == "fuel") {
        if (quantity != "fill" && (!quantity.is_number_integer() || quantity <= 0 || quantity > 65535))
            throw std::invalid_argument("invalid fuel purchase quantity");
        const auto fine = inventory.quantity("fuel_hundredths"), capacity = inventory.capacity("fuel_hundredths");
        const auto count = quantity == "fill" ? (capacity - fine + FUEL_TANK_SCALE - 1) / FUEL_TANK_SCALE : quantity.get<Integer>();
        if (count <= 0 || count > 65535) return {"unavailable", {}};
        if (fine / FUEL_TANK_SCALE + count > capacity / FUEL_TANK_SCALE) return {"no_room", {}};
        cost = count * book.at("fuel").at("unit_price").get<Integer>();
        request.receive["fuel_hundredths"] = std::min<Integer>(count * FUEL_TANK_SCALE, capacity - fine);
    } else if (item == "next_technology") {
        technology = nextTechnology();
        if (!technology) return {"unavailable", {}};
        cost = technology->at("price"); speech = technology->at("sold");
    } else {
        information = &book.at("information").at("categories").at(item);
        index = story.readNamed(information->at("cursor").get<std::string>());
        if (index >= information->at("speech").size()) return {"unavailable", {}};
        cost = book.at("information").at("price"); speech = information->at("speech").at(index);
    }
    const auto balance = inventory.quantity("credits");
    if (balance < cost) return {"no_credit", {{"missing", cost - balance}}};
    Result result{"ok", {{"speech", std::move(speech)}}}; // Allocate before payment.
    request.give["credits"] = cost;
    const auto paid = exchange(inventory, request);
    if (paid.code != "ok") return paid;
    if (technology) grantTechnology(*technology);
    if (information) {
        // Legacy knowledge effects are explicit story policy, not inventory rules.
        if (item == "alien_races" && index == 13 && !story.readNamed("FOUND_PLUTO_SPATHI")) {
            story.setNamed("KNOW_SPATHI_PASSWORD", 1); story.setNamed("SPATHI_HOME_VISITS", 7);
        }
        if (item == "alien_races" && index == 14) {
            story.setNamed("KNOW_ABOUT_SHATTERED", std::max(2u, story.readNamed("KNOW_ABOUT_SHATTERED")));
            story.setNamed("KNOW_SYREEN_WORLD_SHATTERED", 1);
        }
        story.setNamed(information->at("cursor").get_ref<const std::string &>(), index + 1);
    }
    return result;
}
Result ConversationCommerce::sell(const std::string &market, const std::string &item, const Json &quantity) {
    if (market == "druuge") {
        if (quantity != "sphere" && quantity != "regular") throw std::invalid_argument("invalid barter policy");
        return sellDruuge(item, quantity == "sphere");
    }
    if (quantity != "all") throw std::invalid_argument("sale requires all");
    if (market != "melnorme") throw std::invalid_argument("unknown sale merchant");
    auto &inventory = player_.inventory();
    const auto &sales = definitions_.at(market).at("sales");
    const auto count = item == "biomass" ? inventory.quantity("biomass") : item == "rainbows" ? unreportedRainbows() : throw std::invalid_argument("unknown sale asset");
    if (!count) return {"unavailable", {}};
    const Integer price = item == "biomass" ? sales.at(item).at("credits_per_unit").get<Integer>() : sales.at(item).at("credits_per_new_location").get<Integer>();
    const auto amount = count * price;
    Json speech = Json::array({item == "biomass" ? "SOLD_LIFE_DATA1" : "SOLD_RAINBOW_LOCATIONS1",
        Json{{"number", count}}, item == "biomass" ? "SOLD_LIFE_DATA2" : "SOLD_RAINBOW_LOCATIONS2",
        Json{{"number", amount}}, item == "biomass" ? "SOLD_LIFE_DATA3" : "SOLD_RAINBOW_LOCATIONS3"});
    Result result{"ok", {{"speech", std::move(speech)}}};
    InventoryTransaction request{{}, {{"credits", amount}}};
    if (item == "biomass") request.give["biomass"] = count;
    auto paid = exchange(inventory, request);
    if (paid.code != "ok") return paid;
    if (item == "rainbows") player_.story().setNamed("MELNORME_RAINBOW_COUNT", player_.story().readNamed("MELNORME_RAINBOW_COUNT") + count);
    return result;
}
Result ConversationCommerce::report(const std::string &name, const Json &parameters) {
    if (!parameters.empty()) throw std::invalid_argument("unexpected commerce report parameters");
    if (name == "melnorme.technology_pitch") {
        const auto next = nextTechnology();
        return {"ok", {{"speech", next ? next->at("pitch") : Json::array({"NEW_TECH_ALL_GONE"})}}};
    }
    if (name == "druuge.slavery") {
        auto &story = player_.story();
        const unsigned previous = story.readNamed("CREW_SOLD_TO_DRUUGE0") | (story.readNamed("CREW_SOLD_TO_DRUUGE1") << 8);
        const unsigned total = std::min(65535u, previous + slavery_);
        auto &cost = player_.gameData().prices.crew;
        if (total > 250 && previous <= 250) cost += previous > 100 ? 15 : 22;
        else if (total > 100 && previous <= 100) cost += 7;
        story.setNamed("CREW_SOLD_TO_DRUUGE0", total & 255);
        story.setNamed("CREW_SOLD_TO_DRUUGE1", total >> 8);
        slavery_ = 0;
        return {};
    }
    throw std::invalid_argument("unknown commerce report: " + name);
}
}

namespace uqm {
namespace {
unsigned removableValue(const SIS_STATE &sis, const GameData &data) {
    unsigned worth = sis.NumLanders * data.prices.modules[PLANET_LANDER];
    for (auto part : sis.DriveSlots) if (part < EMPTY_SLOT) worth += data.prices.modules[FUSION_THRUSTER];
    for (auto part : sis.JetSlots) if (part < EMPTY_SLOT) worth += data.prices.modules[TURNING_JETS];
    unsigned required = (sis.CrewEnlisted + CREW_POD_CAPACITY - 1) / CREW_POD_CAPACITY;
    for (auto part : sis.ModuleSlots) {
        if (part >= BOMB_MODULE_0) continue;
        if (part == CREW_POD && required) { --required; continue; }
        worth += data.prices.modules[part];
    }
    return worth;
}
bool sameRescueState(const SIS_STATE &a, const SIS_STATE &b) {
    return a.NumLanders == b.NumLanders && a.CrewEnlisted == b.CrewEnlisted &&
        a.FuelOnBoard == b.FuelOnBoard && a.TotalElementMass == b.TotalElementMass &&
        std::equal(std::begin(a.DriveSlots), std::end(a.DriveSlots), std::begin(b.DriveSlots)) &&
        std::equal(std::begin(a.JetSlots), std::end(a.JetSlots), std::begin(b.JetSlots)) &&
        std::equal(std::begin(a.ModuleSlots), std::end(a.ModuleSlots), std::begin(b.ModuleSlots));
}
void applyEquipment(SIS_STATE &live, const SIS_STATE &equipment) noexcept {
    live.NumLanders = equipment.NumLanders;
    std::copy(std::begin(equipment.DriveSlots), std::end(equipment.DriveSlots), std::begin(live.DriveSlots));
    std::copy(std::begin(equipment.JetSlots), std::end(equipment.JetSlots), std::begin(live.JetSlots));
    std::copy(std::begin(equipment.ModuleSlots), std::end(equipment.ModuleSlots), std::begin(live.ModuleSlots));
}
}
Integer ConversationCommerce::salvageValue() const {
    return removableValue(detail::SISAccess::record(player_.sis()), player_.gameData());
}
Result ConversationCommerce::prepareRescue(const std::string &name) {
    if (name != "melnorme") throw std::invalid_argument("unknown rescue policy");
    // Repeated preparation returns the same quote, never rerolls or changes state.
    const auto &sis = detail::SISAccess::record(player_.sis());
    const auto &book = definitions_.at("melnorme").at("rescue");
    const auto position = player_.travel().position;
    const auto x = LOGX_TO_UNIVERSE(position.x), y = LOGY_TO_UNIVERSE(position.y);
    const auto dx = Integer(x) - SOL_X, dy = Integer(y) - SOL_Y;
    const auto required = static_cast<unsigned>(std::sqrt(double(dx * dx + dy * dy))) + book.at("extra_fuel_units").get<unsigned>() * FUEL_TANK_SCALE;
    const auto capacity = player_.inventory().capacity("fuel_hundredths");
    if (!rescue_) {
        RescueOffer offer{sis, sis, static_cast<unsigned>(std::min<Integer>(required, capacity)), false};
        RandomContext random{RandomDefaultSeed};
        RandomContext_SeedRandom(&random, y / 100 * 1013 + x / 100 + sis.NumLanders + sis.ModuleSlots[1] + sis.ModuleSlots[4] + sis.ModuleSlots[7] + sis.ModuleSlots[10]);
        unsigned crewPods = std::count(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), CREW_POD);
        unsigned bays = std::count(std::begin(sis.ModuleSlots), std::end(sis.ModuleSlots), STORAGE_BAY);
        const unsigned requiredPods = (sis.CrewEnlisted + CREW_POD_CAPACITY - 1) / CREW_POD_CAPACITY;
        const unsigned requiredBays = (sis.TotalElementMass + STORAGE_BAY_CAPACITY - 1) / STORAGE_BAY_CAPACITY;
        unsigned worth = 0;
        for (unsigned attempts = 0; worth < offer.fuel / FUEL_TANK_SCALE && attempts < 255; ++attempts) {
            const auto value = RandomContext_Random(&random);
            unsigned part = (value & 255) % (CREW_POD + 1);
            auto removeFirst = [](auto &slots, BYTE empty) {
                const auto found = std::find_if(std::begin(slots), std::end(slots), [](BYTE part) { return part < EMPTY_SLOT; });
                if (found == std::end(slots)) return false;
                *found = empty; return true;
            };
            if (part == PLANET_LANDER) { if (!offer.stripped.NumLanders) continue; --offer.stripped.NumLanders; }
            else if (part == FUSION_THRUSTER) { if (!removeFirst(offer.stripped.DriveSlots, EMPTY_SLOT)) continue; }
            else if (part == TURNING_JETS) { if (!removeFirst(offer.stripped.JetSlots, EMPTY_SLOT + 1)) continue; }
            else {
                const auto slot = ((value >> 8) & 255) % NUM_MODULE_SLOTS;
                part = offer.stripped.ModuleSlots[slot];
                if (part >= BOMB_MODULE_0 || part == FUEL_TANK || part == HIGHEFF_FUELSYS ||
                    (part == CREW_POD && crewPods <= requiredPods) || (part == STORAGE_BAY && bays <= requiredBays)) continue;
                if (part == CREW_POD) --crewPods;
                if (part == STORAGE_BAY) --bays;
                offer.stripped.ModuleSlots[slot] = EMPTY_SLOT + 2;
            }
            worth += player_.gameData().prices.modules[part];
        }
        rescue_ = offer;
    }
    if (rescue_->accepted) return {"unavailable", {}};
    Json speech = Json::array();
    const auto &offer = *rescue_;
    // Compare the offer's concrete changes, not whole-struct padding bytes.
    const bool free = sameRescueState(offer.original, offer.stripped);
    if (free) {
        Result result{"free", {{"speech", Json::array({"CHARITY"})}}};
        if (!sameRescueState(sis, offer.original)) return {"stale", {}};
        player_.sis().setFuel(std::min<Integer>(capacity, sis.FuelOnBoard + offer.fuel));
        player_.story().setNamed("MELNORME_RESCUE_REFUSED", 0);
        rescue_->accepted = true;
        return result;
    }
    speech.push_back("RESCUE_OFFER");
    speech.push_back(offer.fuel == capacity ? "RESCUE_TANKS" : "RESCUE_HOME");
    std::array<unsigned, BOMB_MODULE_0> removed{};
    removed[PLANET_LANDER] = offer.original.NumLanders - offer.stripped.NumLanders;
    const auto countRemoved = [&](const auto &before, const auto &after, unsigned kind) {
        for (unsigned i = 0; i < std::size(before); ++i) if (before[i] < EMPTY_SLOT && after[i] >= EMPTY_SLOT) ++removed[kind];
    };
    countRemoved(offer.original.DriveSlots, offer.stripped.DriveSlots, FUSION_THRUSTER);
    countRemoved(offer.original.JetSlots, offer.stripped.JetSlots, TURNING_JETS);
    for (unsigned i = 0; i < NUM_MODULE_SLOTS; ++i)
        if (offer.original.ModuleSlots[i] < BOMB_MODULE_0 && offer.stripped.ModuleSlots[i] >= EMPTY_SLOT) ++removed[offer.original.ModuleSlots[i]];
    static constexpr std::pair<unsigned, const char *> names[] = {{PLANET_LANDER,"LANDERS"},{FUSION_THRUSTER,"THRUSTERS"},{TURNING_JETS,"JETS"},{CREW_POD,"PODS"},{STORAGE_BAY,"BAYS"},{DYNAMO_UNIT,"DYNAMOS"},{SHIVA_FURNACE,"FURNACES"},{GUN_WEAPON,"GUNS"},{BLASTER_WEAPON,"BLASTERS"},{CANNON_WEAPON,"CANNONS"},{TRACKING_SYSTEM,"TRACKERS"},{ANTIMISSILE_DEFENSE,"DEFENSES"}};
    static constexpr const char *numbers[] = {"ONE","TWO","THREE","FOUR","FIVE","SIX","SEVEN","EIGHT","NINE","TEN","ELEVEN","TWELVE","THIRTEEN","FOURTEEN","FIFTEEN","SIXTEEN"};
    unsigned remaining = std::count_if(removed.begin(), removed.end(), [](unsigned n) { return n != 0; });
    const auto entries = remaining;
    for (const auto &[id, noun] : names) if (removed[id]) {
        if (remaining == 1 && entries > 1) speech.push_back("END_LIST_WITH_AND");
        if (removed[id] > std::size(numbers)) throw std::out_of_range("rescue quantity exceeds number-speech catalog");
        speech.push_back("ENUMERATE_" + std::string(numbers[removed[id] - 1])); speech.push_back(noun); --remaining;
    }
    return {"offer", {{"token", "melnorme.rescue"}, {"speech", std::move(speech)}}};
}
Result ConversationCommerce::acceptRescue(const std::string &token) {
    if (token != "melnorme.rescue" || !rescue_ || rescue_->accepted) return {"unavailable", {}};
    auto &live = detail::SISAccess::record(player_.sis());
    if (!sameRescueState(live, rescue_->original)) return {"stale", {}};
    applyEquipment(live, rescue_->stripped);
    live.FuelOnBoard = std::min<Integer>(player_.inventory().capacity("fuel_hundredths"), live.FuelOnBoard + rescue_->fuel);
    player_.story().setNamed("MELNORME_RESCUE_REFUSED", 0);
    rescue_->accepted = true;
    return {};
}
Result ConversationCommerce::stripShip(const std::string &policy) {
    if (policy != "melnorme.penance") throw std::invalid_argument("unknown strip policy");
    if (salvageValue() < definitions_.at("melnorme").at("penance").at("minimum_value").get<Integer>()) return {"not_worth_stripping", {}};
    auto &live = detail::SISAccess::record(player_.sis());
    auto required = (live.CrewEnlisted + CREW_POD_CAPACITY - 1) / CREW_POD_CAPACITY;
    live.NumLanders = 0;
    std::fill(std::begin(live.DriveSlots), std::end(live.DriveSlots), EMPTY_SLOT);
    std::fill(std::begin(live.JetSlots), std::end(live.JetSlots), EMPTY_SLOT + 1);
    for (auto &part : live.ModuleSlots) if (part < BOMB_MODULE_0) {
        if (part == CREW_POD && required) { --required; continue; }
        part = EMPTY_SLOT + 2;
    }
    live.FuelOnBoard = std::min<DWORD>(live.FuelOnBoard, FUEL_RESERVE);
    live.TotalBioMass = live.TotalElementMass = 0;
    std::fill(std::begin(live.ElementAmounts), std::end(live.ElementAmounts), 0);
    player_.story().setNamed("MELNORME_RESCUE_REFUSED", 0);
    return {};
}
}

namespace uqm {
Result ConversationCommerce::sellDruuge(const std::string &item, bool sphere) {
    auto &inventory = player_.inventory();
    auto &story = player_.story();
    const auto &offer = definitions_.at("druuge").at("sales").at(item);
    std::string key = offer.at("asset");
    if (item == "fragments") {
        if (story.readNamed("FRAGMENTS_BOUGHT") >= 2) return {"unavailable", {}};
        for (unsigned i = 0; i < 3; ++i) {
            const auto candidate = "items.egg_case_" + std::to_string(i);
            if (inventory.quantity(candidate)) { key = candidate; break; }
        }
    }
    if (!inventory.quantity(key)) return {"unavailable", {}};
    if (sphere && story.readNamed("ROSY_SPHERE")) return {"unavailable", {}};
    InventoryTransaction request{{{key, 1}}, {}};
    if (sphere) {
        if (!inventory.quantity("items.rosy_sphere")) request.receive["items.rosy_sphere"] = 1;
        Result result{"ok", {{"speech", Json::array()}}};
        const auto paid = exchange(inventory, request);
        if (paid.code != "ok") return paid;
        story.setNamed("ROSY_SPHERE", 1);
        if (item == "fragments") story.setNamed("FRAGMENTS_BOUGHT", story.readNamed("FRAGMENTS_BOUGHT") + 1);
        return result;
    }
    const unsigned wanted = offer.at("ships");
    const unsigned slots = escortSlots(session_, DRUUGE_SHIP);
    ReservedShips reserved{session_, random_, DRUUGE_SHIP, std::min(wanted, slots)};
    if (reserved.size() != std::min(wanted, slots)) return {"no_room", {}};
    Json speech = Json::array({offer.at("speech"), "YOU_GET"});
    if (wanted) speech.push_back(slots >= wanted ? "DEAL_FOR_STATED_SHIPS" : slots == 0 ? "DEAL_FOR_NO_SHIPS" : "DEAL_FOR_LESS_SHIPS");
    if (offer.at("fill_fuel").get<bool>()) {
        if (wanted) speech.push_back("YOU_ALSO_GET");
        const auto fine = std::max<Integer>(0, inventory.capacity("fuel_hundredths") - inventory.quantity("fuel_hundredths"));
        if (fine) request.receive["fuel_hundredths"] = fine;
        const auto displayed = (fine + FUEL_TANK_SCALE / 2) / FUEL_TANK_SCALE;
        speech.push_back("FUEL0"); speech.push_back({{"number", displayed}}); speech.push_back("FUEL1");
        speech.push_back(displayed >= 250 ? "HIDEOUS_DEAL" : displayed >= 100 ? "BAD_DEAL" : displayed >= 50 ? "FAIR_DEAL" : displayed >= 10 ? "GOOD_DEAL" : "FINE_DEAL");
    }
    Result result{"ok", {{"speech", std::move(speech)}}};
    const auto paid = exchange(inventory, request);
    if (paid.code != "ok") return paid;
    reserved.commit();
    if (item == "fragments") story.setNamed("FRAGMENTS_BOUGHT", story.readNamed("FRAGMENTS_BOUGHT") + 1);
    return result;
}
}
