#ifndef LIBS_MATH_RANDOM_HPP_
#define LIBS_MATH_RANDOM_HPP_
#include "random.h"
#include <random>
#include <cstdint>

namespace uqm {
// Value-owned general gameplay stream. The cached state supports the old C
// save/restore API without exposing or serializing a library implementation.
// System generation deliberately retains RandomContext's separate recurrence.
class RandomStream {
public:
    explicit RandomStream(std::uint32_t seed = RandomDefaultSeed) noexcept { reseed(seed); }
    std::uint32_t operator()() noexcept { return state_ = static_cast<std::uint32_t>(engine_()); }
    std::uint32_t state() const noexcept { return state_; }
    // Returns the previous state. Any uint32 seed is accepted, using the
    // standard engine's modulus and zero-to-one normalization.
    std::uint32_t reseed(std::uint32_t seed) noexcept {
        const auto previous = state_;
        state_ = seed % std::minstd_rand0::modulus;
        if (!state_) state_ = 1;
        engine_.seed(state_);
        return previous;
    }
private:
    std::minstd_rand0 engine_;
    std::uint32_t state_ = RandomDefaultSeed;
};

// Synchronous C adapter binding; the required stream must outlive the scope.
// Nested bindings restore the parent without consuming any parent randomness.
class RandomBinding {
public:
    explicit RandomBinding(RandomStream &stream) noexcept;
    ~RandomBinding();
    RandomBinding(const RandomBinding &) = delete;
    RandomBinding &operator=(const RandomBinding &) = delete;
private:
    RandomStream *previous_;
};
}
#endif
