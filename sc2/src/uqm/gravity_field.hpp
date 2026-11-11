#ifndef UQM_GRAVITY_FIELD_HPP_
#define UQM_GRAVITY_FIELD_HPP_
#include "element.h"
namespace uqm::combat {
// UQM's constant acceleration inside a circular field, not inverse-square
// gravity. Range uses display units; acceleration uses world units per tick.
class GravityField {
public:
    explicit GravityField(int range = GRAVITY_THRESHOLD, int acceleration = 1);
    int range() const noexcept { return range_; }
    int acceleration() const noexcept { return acceleration_; }
    // dx/dy are wrapped world-coordinate deltas pointing toward the source.
    bool contains(SIZE dx, SIZE dy) const noexcept;
    void apply(VELOCITY_DESC &velocity, SIZE dx, SIZE dy) const;
private:
    int range_, acceleration_;
};
}
#endif
