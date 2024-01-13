#pragma once

// Yoinked straight from Catch2's PCG, but all functions are visible
#include <cstdint>

inline uint32_t rotate_right(uint32_t val, uint32_t count) {
    const uint32_t mask = 31;
    count &= mask;
    return (val >> count) | (val << (-count & mask));
}

class SimplePcg32 {
    using state_type = std::uint64_t;
public:
    using result_type = std::uint32_t;
    static constexpr result_type(min)() {
        return 0;
    }
    static constexpr result_type(max)() {
        return static_cast<result_type>(-1);
    }

    // Provide some default initial state for the default constructor
    SimplePcg32() :SimplePcg32(0xed743cc4U) {}

    explicit SimplePcg32(result_type seed_) {
        seed(seed_);
    }

    void seed(result_type seed_) {
        m_state = 0;
        (*this)();
        m_state += seed_;
        (*this)();
    }

    result_type operator()() {
        // prepare the output value
        const uint32_t xorshifted = static_cast<uint32_t>(((m_state >> 18u) ^ m_state) >> 27u);
        const auto output = rotate_right(xorshifted, m_state >> 59u);

        // advance state
        m_state = m_state * 6364136223846793005ULL + s_inc;

        return output;
    }

private:
    // In theory we also need operator<< and operator>>, operator==, etc
    // In practice we do not use them, so we will skip them for now

    std::uint64_t m_state;
    static const std::uint64_t s_inc = (0x13ed0cc53f939476ULL << 1ULL) | 1ULL;
};
