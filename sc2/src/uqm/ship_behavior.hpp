#ifndef UQM_SHIP_BEHAVIOR_HPP_
#define UQM_SHIP_BEHAVIOR_HPP_

#include "races.h"
#include "element.h"
#include <array>

namespace uqm::combat {
using WeaponSlots = std::array<HELEMENT, 6>;

// Production migration boundary. Each concrete ship owns its mutable descriptor;
// native execution/AI code dispatches directly; C element/asset code borrows it.
// Native hooks take references; projectile callbacks remain in the element engine.
class Ship {
public:
    explicit Ship(const RACE_DESC &definition) noexcept;
    virtual ~Ship() = default;
    Ship(const Ship &) = delete;
    Ship &operator=(const Ship &) = delete;

    RACE_DESC &legacyData() noexcept { return data_; }
    const RACE_DESC &legacyData() const noexcept { return data_; }
    // element is a live ship element belonging to this behavior. Hooks may mutate
    // it and the descriptor; finish() makes subsequent hooks inert.
    void preprocess(ELEMENT &element) noexcept;
    void postprocess(ELEMENT &element) noexcept;
    // element is the live ship or an AI prediction copy with a valid STARSHIP
    // association. Clears all slots, then returns the number of candidate slots
    // (at most weapons.size()); individual handles may be null. The caller must
    // enqueue real shots or free speculative shots through the element engine.
    COUNT initializeWeapon(ELEMENT &element, WeaponSlots &weapons) noexcept;
    // Explicit legacy completion before asset release; idempotent. Destructors
    // clean up owned values but never publish campaign results.
    void finish() noexcept;
    bool preprocessingEnabled() const noexcept { return preprocessingEnabled_; }
    void enablePreprocessing(bool enabled) noexcept { preprocessingEnabled_ = enabled; }
protected:
    virtual void beforeMotion(ELEMENT &) noexcept {}
    virtual void afterMotion(ELEMENT &) noexcept {}
    virtual COUNT createWeapons(ELEMENT &, WeaponSlots &) noexcept = 0;
    virtual void onFinish() noexcept {}
private:
    RACE_DESC data_;
    bool preprocessingEnabled_ = true;
    bool finished_ = false;
};

// Descriptor must belong to a live Ship (ship_owner is set by its constructor).
Ship &shipFor(RACE_DESC &descriptor) noexcept;
} // namespace uqm::combat
#endif
