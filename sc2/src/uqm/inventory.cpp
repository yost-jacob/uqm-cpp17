#include "inventory.hpp"
#include "game_state.hpp"
#include "sis_legacy.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <iterator>
#include <type_traits>
#include <stdexcept>
#include <vector>
namespace uqm {
static_assert(std::is_trivially_copy_assignable_v<SIS_STATE>, "transaction commit must not throw");
namespace {
struct Item { std::string_view name, field; };
constexpr Item items[] = {
    {"sun_device", "SUN_DEVICE_ON_SHIP"}, {"rosy_sphere", "ROSY_SPHERE_ON_SHIP"},
    {"artifact_2", "ARTIFACT_2_ON_SHIP"}, {"artifact_3", "ARTIFACT_3_ON_SHIP"},
    {"utwig_bomb", "UTWIG_BOMB_ON_SHIP"}, {"maidens", "MAIDENS_ON_SHIP"},
    {"talking_pet", "TALKING_PET_ON_SHIP"}, {"aqua_helix", "AQUA_HELIX_ON_SHIP"},
    {"clear_spindle", "CLEAR_SPINDLE_ON_SHIP"}, {"umgah_broadcaster", "UMGAH_BROADCASTERS_ON_SHIP"},
    {"taalo_protector", "TAALO_PROTECTOR_ON_SHIP"}, {"egg_case_0", "EGG_CASE0_ON_SHIP"},
    {"egg_case_1", "EGG_CASE1_ON_SHIP"}, {"egg_case_2", "EGG_CASE2_ON_SHIP"},
    {"syreen_shuttle", "SYREEN_SHUTTLE_ON_SHIP"}, {"vux_beast", "VUX_BEAST_ON_SHIP"},
    {"portal_spawner", "PORTAL_SPAWNER_ON_SHIP"}, {"portal_key", "PORTAL_KEY_ON_SHIP"},
    {"burvix_broadcaster", "BURV_BROADCASTERS_ON_SHIP"}, {"moonbase", "MOONBASE_ON_SHIP"}
};
constexpr std::string_view minerals[] = {"minerals.common", "minerals.corrosive", "minerals.base_metal", "minerals.noble",
                                        "minerals.rare_earth", "minerals.precious", "minerals.radioactive", "minerals.exotic"};
enum class Kind { Crew, Fuel, Ru, Credits, Biomass, Landers, Mineral, Item };
struct Key { Kind kind; std::size_t index = 0; };
Key key(std::string_view name) {
    if (name == "crew") return {Kind::Crew};
    if (name == "fuel") return {Kind::Fuel};
    if (name == "fuel_hundredths") return {Kind::Fuel, 1};
    if (name == "ru") return {Kind::Ru};
    if (name == "credits") return {Kind::Credits};
    if (name == "biomass") return {Kind::Biomass};
    if (name == "landers") return {Kind::Landers};
    for (std::size_t i = 0; i < std::size(minerals); ++i)
        if (name == minerals[i]) return {Kind::Mineral, i};
    for (std::size_t i = 0; i < std::size(items); ++i)
        if (name.substr(0, 6) == "items." && name.substr(6) == items[i].name) return {Kind::Item, i};
    throw std::invalid_argument("unknown inventory asset: " + std::string(name));
}
constexpr std::int64_t moduleCapacity(Kind kind, BYTE module) {
    if (kind == Kind::Crew && module == CREW_POD) return CREW_POD_CAPACITY;
    if (kind == Kind::Mineral && module == STORAGE_BAY) return STORAGE_BAY_CAPACITY;
    if (kind == Kind::Fuel && module == FUEL_TANK) return FUEL_TANK_CAPACITY;
    if (kind == Kind::Fuel && module == HIGHEFF_FUELSYS) return HEFUEL_TANK_CAPACITY;
    return 0;
}
std::int64_t capacityLimit(const SIS_STATE &sis, Key asset) {
    std::int64_t total = asset.kind == Kind::Fuel ? FUEL_RESERVE : 0;
    for (const auto module : sis.ModuleSlots) total += moduleCapacity(asset.kind, module);
    switch (asset.kind) {
    case Kind::Crew: case Kind::Fuel: case Kind::Mineral: return total;
    case Kind::Biomass: case Kind::Credits: return std::numeric_limits<COUNT>::max();
    case Kind::Ru: return std::numeric_limits<DWORD>::max();
    case Kind::Landers: return MAX_LANDERS;
    case Kind::Item: return 1;
    }
    throw std::logic_error("unhandled inventory capacity");
}
std::int64_t read(const SIS_STATE &sis, const std::array<unsigned, std::size(items)> &owned,
                  unsigned credits, Key asset) {
    switch (asset.kind) {
    case Kind::Crew: return sis.CrewEnlisted;
    case Kind::Fuel: return sis.FuelOnBoard; // Internal calculations preserve fractional tank units.
    case Kind::Ru: return sis.ResUnits;
    case Kind::Credits: return credits;
    case Kind::Biomass: return sis.TotalBioMass;
    case Kind::Landers: return sis.NumLanders;
    case Kind::Mineral: return sis.ElementAmounts[asset.index];
    case Kind::Item: return owned[asset.index];
    }
    throw std::logic_error("unhandled inventory asset");
}
}
Inventory::Inventory(StoryState &fields)
    : legacyFields_(fields), fleet_(MAX_BUILT_SHIPS, SISPresence::Present) {}
void Inventory::validateKey(std::string_view asset) { key(asset); }
bool Inventory::ownsLegacyField(std::string_view field) noexcept {
    if (field == "MELNORME_CREDIT0" || field == "MELNORME_CREDIT1" || field == "ULTRON_CONDITION") return true;
    return std::any_of(std::begin(items), std::end(items),
                       [field](const Item &item) { return item.field == field; });
}
std::int64_t Inventory::quantity(std::string_view name) const {
    const auto asset = key(name);
    if (asset.kind == Kind::Item) return legacyFields_.readNamed(items[asset.index].field);
    if (asset.kind == Kind::Credits)
        return legacyFields_.readNamed("MELNORME_CREDIT0") | (legacyFields_.readNamed("MELNORME_CREDIT1") << 8);
    const auto &sis = detail::SISAccess::record(fleet_.sis()->get());
    if (asset.kind == Kind::Crew && !fleet_.sis()->get().alive()) return 0;
    const auto amount = read(sis, {}, 0, asset);
    return asset.kind == Kind::Fuel && asset.index == 0 ? amount / FUEL_TANK_SCALE : amount;
}
std::int64_t Inventory::capacity(std::string_view name) const {
    const auto asset = key(name);
    const auto maximum = capacityLimit(detail::SISAccess::record(fleet_.sis()->get()), asset);
    return asset.kind == Kind::Fuel && asset.index == 0 ? maximum / FUEL_TANK_SCALE : maximum;
}
std::string_view Inventory::mineralKey(std::size_t category) {
    if (category >= std::size(minerals)) throw std::out_of_range("unknown mineral category");
    return minerals[category];
}
InventoryResult Inventory::transact(const InventoryTransaction &request) {
    struct Transfer { Key asset; std::int64_t amount; std::string name; };
    std::vector<Transfer> give, receive;
    auto prepare = [](const InventoryPackage &package, auto &out) {
        out.reserve(package.size());
        for (const auto &[name, amount] : package) {
            const auto asset = key(name);
            if (amount <= 0) throw std::invalid_argument("inventory quantities must be positive integers");
            if (asset.kind == Kind::Fuel && asset.index == 0 && amount > std::numeric_limits<std::int64_t>::max() / FUEL_TANK_SCALE)
                throw std::out_of_range("inventory fuel quantity overflow");
            out.push_back({asset, asset.kind == Kind::Fuel && asset.index == 0 ? amount * FUEL_TANK_SCALE : amount, name});
        }
    };
    prepare(request.give, give); prepare(request.receive, receive);
    if (give.empty() && receive.empty()) throw std::invalid_argument("empty inventory transaction");
    auto &live = detail::SISAccess::record(fleet_.sis()->get());
    if (!fleet_.sis()->get().alive()) return {InventoryStatus::Unavailable, "crew"};
    SIS_STATE staged = live;
    std::array<unsigned, std::size(items)> owned{};
    for (std::size_t i = 0; i < owned.size(); ++i) owned[i] = legacyFields_.readNamed(items[i].field);
    unsigned credits = quantity("credits");
    auto store = [&](Key asset, std::int64_t value) {
        switch (asset.kind) {
        case Kind::Crew: staged.CrewEnlisted = value; break;
        case Kind::Fuel: staged.FuelOnBoard = value; break;
        case Kind::Ru: staged.ResUnits = value; break;
        case Kind::Credits: credits = value; break;
        case Kind::Biomass: staged.TotalBioMass = value; break;
        case Kind::Landers: staged.NumLanders = value; break;
        case Kind::Mineral: staged.ElementAmounts[asset.index] = value; break;
        case Kind::Item: owned[asset.index] = value; break;
        }
    };
    for (const auto &entry : give) {
        const auto current = read(staged, owned, credits, entry.asset);
        if (entry.amount > current) return {InventoryStatus::Insufficient, entry.name};
        store(entry.asset, current - entry.amount);
    }
    for (const auto &entry : receive) {
        const auto current = read(staged, owned, credits, entry.asset);
        const auto maximum = capacityLimit(staged, entry.asset);
        if (current > maximum || entry.amount > maximum - current)
            return {InventoryStatus::NoCapacity, entry.name};
        store(entry.asset, current + entry.amount);
    }
    std::uint64_t mineralMass = 0;
    for (const auto amount : staged.ElementAmounts) mineralMass += amount;
    const auto cargoCapacity = static_cast<std::uint64_t>(capacityLimit(staged, {Kind::Mineral}));
    if (mineralMass > cargoCapacity || mineralMass > std::numeric_limits<COUNT>::max())
        return {InventoryStatus::NoCapacity, "minerals"};
    staged.TotalElementMass = mineralMass;
    // Allocate/validate every packed compatibility write before touching live SIS.
    std::vector<std::pair<std::string, unsigned>> changes;
    for (std::size_t i = 0; i < owned.size(); ++i)
        if (owned[i] != legacyFields_.readNamed(items[i].field)) changes.emplace_back(items[i].field, owned[i]);
    if (credits != quantity("credits")) {
        changes.emplace_back("MELNORME_CREDIT0", credits & 255);
        changes.emplace_back("MELNORME_CREDIT1", credits >> 8);
    }
    legacyFields_.setNamedBatch(changes);
    live = staged; // Trivial nonthrowing assignment; storage addresses stay stable.
    return {InventoryStatus::Completed, {}};
}

// Legacy UI wrappers read the selected SIS record, including arcade compatibility
// buffers. These are pure capacity queries; native callers use Inventory directly.
extern "C" COUNT GetModuleCrewCapacity(BYTE module) { return moduleCapacity(Kind::Crew, module); }
extern "C" COUNT GetModuleStorageCapacity(BYTE module) { return moduleCapacity(Kind::Mineral, module); }
extern "C" DWORD GetModuleFuelCapacity(BYTE module) { return moduleCapacity(Kind::Fuel, module); }
extern "C" COUNT GetCrewPodCapacity(void) { return capacityLimit(*GetPlayerSISState(), {Kind::Crew}); }
extern "C" COUNT GetStorageBayCapacity(void) { return capacityLimit(*GetPlayerSISState(), {Kind::Mineral}); }
extern "C" DWORD GetFuelTankCapacity(void) { return capacityLimit(*GetPlayerSISState(), {Kind::Fuel}); }
}
