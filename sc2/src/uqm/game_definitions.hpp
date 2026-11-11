#ifndef UQM_GAME_DEFINITIONS_HPP_
#define UQM_GAME_DEFINITIONS_HPP_
#include "star_map.hpp"
#include "ship_tuning.hpp"
#include <memory>
struct star_desc;
namespace uqm {
namespace detail { struct DefinitionsAccess; }
// Immutable after construction. Sessions own their catalogs; borrowed values
// expire with that session. Arcade definitions contain ships, not a starmap.
class GameDefinitions {
public:
    GameDefinitions(); // Empty only for standalone compatibility/setup.
    explicit GameDefinitions(std::istream &ships);
    GameDefinitions(std::istream &ships, std::istream &stars);
    ~GameDefinitions();
    GameDefinitions(const GameDefinitions &) = delete;
    GameDefinitions &operator=(const GameDefinitions &) = delete;
    const StarMap &stars() const; // Throws when this session has no starmap.
    const combat::ShipTuningCatalog &ships() const; // Throws before loading.
    bool hasStars() const noexcept;
    bool hasShips() const noexcept;
private:
    friend struct detail::DefinitionsAccess;
    struct Storage;
    std::unique_ptr<Storage> storage_;
};
}
#endif
