#include "gravity_field.hpp"
#include "units.h"
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
namespace uqm::combat {
GravityField::GravityField(int range, int acceleration) : range_(range), acceleration_(acceleration) {
    if (range < 0 || range > 32767 || acceleration < 0 || acceleration > 1023)
        throw std::out_of_range("invalid combat gravity field");
}
bool GravityField::contains(SIZE dx, SIZE dy) const noexcept {
    const std::int64_t x = WORLD_TO_DISPLAY(std::abs(int(dx)));
    const std::int64_t y = WORLD_TO_DISPLAY(std::abs(int(dy)));
    return range_ && x * x + y * y <= std::int64_t(range_) * range_;
}
void GravityField::apply(VELOCITY_DESC &velocity, SIZE dx, SIZE dy) const {
    const auto angle = ARCTAN(dx, dy);
    DeltaVelocityComponents(&velocity, COSINE(angle, WORLD_TO_VELOCITY(acceleration_)),
        SINE(angle, WORLD_TO_VELOCITY(acceleration_)));
}
}
