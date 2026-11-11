#include "game_definitions_legacy.hpp"
#include "game_state.hpp"
#include "options.h"
#include "libs/reslib.h"
#include <sstream>
#include <stdexcept>
#include <vector>
namespace uqm {
struct GameDefinitions::Storage {
    std::unique_ptr<const StarMap> stars;
    std::unique_ptr<const combat::ShipTuningCatalog> ships;
    std::vector<STAR_DESC> descriptors;
};
GameDefinitions::GameDefinitions() : storage_(std::make_unique<Storage>()) {}
GameDefinitions::GameDefinitions(std::istream &ships) : GameDefinitions() {
    detail::DefinitionsAccess::loadShips(*this, ships);
}
GameDefinitions::GameDefinitions(std::istream &ships, std::istream &stars) : GameDefinitions(ships) {
    detail::DefinitionsAccess::loadStars(*this, stars);
}
GameDefinitions::~GameDefinitions() = default;
bool GameDefinitions::hasStars() const noexcept { return bool(storage_->stars); }
bool GameDefinitions::hasShips() const noexcept { return bool(storage_->ships); }
const StarMap &GameDefinitions::stars() const {
    if (!storage_->stars) throw std::logic_error("session has no star catalog");
    return *storage_->stars;
}
const combat::ShipTuningCatalog &GameDefinitions::ships() const {
    if (!storage_->ships) throw std::logic_error("session ship tuning has not been loaded");
    return *storage_->ships;
}
namespace {
void loadFile(GameDefinitions &data, const char *path,
              void (*load)(GameDefinitions &, std::istream &)) {
    try {
        std::istringstream input(detail::readDefinitionFile(path));
        load(data, input);
    } catch (const std::exception &error) {
        throw std::runtime_error(std::string(path) + ": " + error.what());
    }
}
}
void Session::loadDefinitions(bool includeStars) {
    // Build a complete candidate before publishing it to this new session.
    auto candidate = std::make_unique<GameDefinitions>();
    loadFile(*candidate, "base/ships.json", detail::DefinitionsAccess::loadShips);
    if (includeStars)
        loadFile(*candidate, "base/starmap.json", detail::DefinitionsAccess::loadStars);
    definitions_ = std::move(candidate);
}
namespace detail {
std::string readDefinitionFile(const char *path) {
    using Stream = std::unique_ptr<uio_Stream, decltype(&res_CloseResFile)>;
    const Stream file(res_OpenResFile(contentDir, path, "rb"), res_CloseResFile);
    if (!file) throw std::runtime_error(std::string("cannot open ") + path);
    std::string bytes(LengthResFile(file.get()), '\0');
    if (ReadResFile(bytes.data(), 1, bytes.size(), file.get()) != bytes.size())
        throw std::runtime_error(std::string("cannot read complete ") + path);
    return bytes;
}
GameDefinitions &activeDefinitions() {
    if (auto *runtime = boundRuntime()) {
        if (!runtime->definitions) throw std::logic_error("active session has no game definitions");
        return *runtime->definitions;
    }
    return DefinitionsAccess::storage(gameSession());
}
void DefinitionsAccess::loadStars(GameDefinitions &data, std::istream &input) {
    if (data.hasStars()) throw std::logic_error("cannot replace a live star catalog");
    auto stars = std::make_unique<const StarMap>(input);
    std::vector<STAR_DESC> descriptors(STAR_CATALOGUE_SIZE);
    for (const auto &star : stars->stars()) {
        const auto &p = star.properties();
        auto &out = descriptors.at(star.id().value);
        out.star_pt = {static_cast<COORD>(star.position().x), static_cast<COORD>(star.position().y)};
        out.Type = MAKE_STAR(static_cast<int>(p.type), static_cast<int>(p.color), p.owner);
        out.Index = p.specialSystem;
        out.Prefix = p.prefixOrder;
        out.Postfix = p.postfixId;
    }
    for (auto index : {int(NUM_SOLAR_SYSTEMS), STAR_CATALOGUE_SIZE - 1})
        descriptors[index].star_pt = {MAX_X_UNIVERSE * 2, MAX_Y_UNIVERSE * 2};
    for (std::size_t i = 0; i < descriptors.size(); ++i) descriptors[i].catalogue_index = i;
    data.storage_->descriptors = std::move(descriptors);
    data.storage_->stars = std::move(stars);
}
void DefinitionsAccess::loadShips(GameDefinitions &data, std::istream &input) {
    if (data.hasShips()) throw std::logic_error("cannot replace live ship tuning");
    data.storage_->ships = std::make_unique<const combat::ShipTuningCatalog>(input);
}
void DefinitionsAccess::clearStars(GameDefinitions &data) {
    data.storage_->descriptors.clear();
    data.storage_->stars.reset();
}
STAR_DESC *DefinitionsAccess::descriptor(GameDefinitions &data, COUNT index) {
    data.stars(); // Unloaded and invalid-index lookups have distinct contracts.
    return index < data.storage_->descriptors.size() ? &data.storage_->descriptors[index] : nullptr;
}
COUNT DefinitionsAccess::index(const GameDefinitions &data, const STAR_DESC *star) {
    if (!star || star->catalogue_index >= data.storage_->descriptors.size()
        || &data.storage_->descriptors[star->catalogue_index] != star)
        throw std::invalid_argument("star descriptor does not belong to this session");
    return star->catalogue_index;
}
GameDefinitions &DefinitionsAccess::storage(Session &session) { return *session.definitions_; }
}
}

namespace uqm::combat {
const ShipTuning &shipTuning(std::string_view id) {
    return detail::activeDefinitions().ships().at(id);
}
}
