#ifndef UQM_SHIP_BEHAVIOR_HPP_
#define UQM_SHIP_BEHAVIOR_HPP_

#include "races.h"
#include "element.h"
#include <array>
#include <memory>

namespace uqm::combat {
using WeaponSlots = std::array<HELEMENT, 6>;

// Production migration boundary. Each concrete ship owns its mutable descriptor;
// the existing C element/asset/AI code borrows it through the buffer functions.
// Native hooks take references; projectile callbacks remain in the element engine.
class Ship {
public:
    explicit Ship(const RACE_DESC &definition) noexcept;
    virtual ~Ship() = default;
    Ship(const Ship &) = delete;
    Ship &operator=(const Ship &) = delete;

    RACE_DESC &legacyData() noexcept { return data_; }
    const RACE_DESC &legacyData() const noexcept { return data_; }
    void preprocess(ELEMENT &element) noexcept;
    void postprocess(ELEMENT &element) noexcept;
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

// Descriptor must belong to a live Ship (CodeRef is set by its constructor).
Ship &shipFor(RACE_DESC &descriptor) noexcept;
// Transfers a newly constructed owner to the legacy resource system. Destruction
// occurs only through DestroyShipBehavior, never HFree or descriptor copying.
RACE_DESC *releaseToLegacy(std::unique_ptr<Ship> ship) noexcept;
// C factory catch blocks use this diagnostic and return null; exceptions cannot
// escape through the resource loader's C call stack.
void logShipCreationFailure(const char *shipName) noexcept;
} // namespace uqm::combat
#endif
