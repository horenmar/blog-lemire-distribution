#pragma once

#include <catch2/internal/catch_random_integer_helpers.hpp>
#include <catch2/internal/catch_uniform_integer_distribution.hpp>

#include <cassert>

template <typename IntegerType>
class lemire_algorithm_no_reuse {
    static_assert(std::is_integral<IntegerType>::value, "...");

    using UnsignedIntegerType = Catch::Detail::make_unsigned_t<IntegerType>;

    IntegerType m_a, m_b;

    UnsignedIntegerType computeDistance(IntegerType a, IntegerType b) const {
        return transposeTo(b) - transposeTo(a) + 1;
    }

    static UnsignedIntegerType computeRejectionThreshold(UnsignedIntegerType ab_distance) {
        return (~ab_distance + 1) % ab_distance;
    }

    static UnsignedIntegerType transposeTo(IntegerType in) {
        return Catch::Detail::transposeToNaturalOrder<IntegerType>(
            static_cast<UnsignedIntegerType>(in));
    }
    static IntegerType transposeBack(UnsignedIntegerType in) {
        return static_cast<IntegerType>(
            Catch::Detail::transposeToNaturalOrder<IntegerType>(in));
    }

public:
    using result_type = IntegerType;

    lemire_algorithm_no_reuse(IntegerType a, IntegerType b) :
        m_a(a), m_b(b) {
        assert(a <= b);
    }

    template <typename Generator>
    result_type operator()(Generator& g) {
        auto ab_distance = computeDistance(m_a, m_b);
        // All possible values of result_type are valid.
        if (ab_distance == 0) {
            return transposeBack(Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g));
        }

        auto random_number = Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g);
        auto emul = Catch::Detail::extendedMult(random_number, ab_distance);
        // Unlike Lemire's algorithm we skip the ab_distance check, since
        // we precomputed the rejection threshold, which is always tighter.
        if (emul.lower < ab_distance) {
            auto rejection_threshold = computeRejectionThreshold(ab_distance);
            while (emul.lower < rejection_threshold) {
                random_number = Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g);
                emul = Catch::Detail::extendedMult(random_number, ab_distance);
            }
        }

        return transposeBack(m_a + emul.upper);
    }
};


// Implementation yoinked directly from Catch2's uniform_integer_distribution
template <typename IntegerType>
class lemire_algorithm_reuse {
    static_assert(std::is_integral<IntegerType>::value, "...");

    using UnsignedIntegerType = Catch::Detail::make_unsigned_t<IntegerType>;


    UnsignedIntegerType m_a;
    UnsignedIntegerType m_ab_distance;
    UnsignedIntegerType m_rejection_threshold;

    UnsignedIntegerType computeDistance(IntegerType a, IntegerType b) const {
        return transposeTo(b) - transposeTo(a) + 1;
    }

    static UnsignedIntegerType computeRejectionThreshold(UnsignedIntegerType ab_distance) {
        if (ab_distance == 0) { return 0; }
        return (~ab_distance + 1) % ab_distance;
    }

    static UnsignedIntegerType transposeTo(IntegerType in) {
        return Catch::Detail::transposeToNaturalOrder<IntegerType>(
            static_cast<UnsignedIntegerType>(in));
    }
    static IntegerType transposeBack(UnsignedIntegerType in) {
        return static_cast<IntegerType>(
            Catch::Detail::transposeToNaturalOrder<IntegerType>(in));
    }

public:
    using result_type = IntegerType;

    lemire_algorithm_reuse(IntegerType a, IntegerType b) :
        m_a(transposeTo(a)),
        m_ab_distance(computeDistance(a, b)),
        m_rejection_threshold(computeRejectionThreshold(m_ab_distance)) {
        assert(a <= b);
    }

    template <typename Generator>
    result_type operator()(Generator& g) {
        // All possible values of result_type are valid.
        if (m_ab_distance == 0) {
            return transposeBack(Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g));
        }

        auto random_number = Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g);
        auto emul = Catch::Detail::extendedMult(random_number, m_ab_distance);
        // Unlike Lemire's algorithm we skip the ab_distance check, since
        // we precomputed the rejection threshold, which is always tighter.
        while (emul.lower < m_rejection_threshold) {
            random_number = Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g);
            emul = Catch::Detail::extendedMult(random_number, m_ab_distance);
        }

        return transposeBack(m_a + emul.upper);
    }
};

// modified variant of lemire_algorithm_reuse
template <typename IntegerType>
class lemire_algorithm_lazy_reuse {
    static_assert(std::is_integral<IntegerType>::value, "...");

    using UnsignedIntegerType = Catch::Detail::make_unsigned_t<IntegerType>;


    UnsignedIntegerType m_a;
    UnsignedIntegerType m_ab_distance;
    UnsignedIntegerType m_rejection_threshold; // must be <m_ab_distance, so -1 is a valid "none" option
    static constexpr UnsignedIntegerType NONE = static_cast<UnsignedIntegerType>(-1);

    UnsignedIntegerType computeDistance(IntegerType a, IntegerType b) const {
        return transposeTo(b) - transposeTo(a) + 1;
    }

    static UnsignedIntegerType computeRejectionThreshold(UnsignedIntegerType ab_distance) {
        if (ab_distance == 0) { return 0; }
        return (~ab_distance + 1) % ab_distance;
    }

    static UnsignedIntegerType transposeTo(IntegerType in) {
        return Catch::Detail::transposeToNaturalOrder<IntegerType>(
            static_cast<UnsignedIntegerType>(in));
    }
    static IntegerType transposeBack(UnsignedIntegerType in) {
        return static_cast<IntegerType>(
            Catch::Detail::transposeToNaturalOrder<IntegerType>(in));
    }

public:
    using result_type = IntegerType;

    lemire_algorithm_lazy_reuse(IntegerType a, IntegerType b) :
        m_a(transposeTo(a)),
        m_ab_distance(computeDistance(a, b)),
        m_rejection_threshold(NONE) {
        assert(a <= b);
    }

    template <typename Generator>
    result_type operator()(Generator& g) {
        // All possible values of result_type are valid.
        if (m_ab_distance == 0) {
            return transposeBack(Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g));
        }

        auto random_number = Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g);
        auto emul = Catch::Detail::extendedMult(random_number, m_ab_distance);
        if (emul.lower < m_ab_distance) {
            if (m_rejection_threshold == NONE) {
                m_rejection_threshold = computeRejectionThreshold(m_ab_distance);
            }
            while (emul.lower < m_rejection_threshold) {
                random_number = Catch::Detail::fillBitsFrom<UnsignedIntegerType>(g);
                emul = Catch::Detail::extendedMult(random_number, m_ab_distance);
            }
        }

        return transposeBack(m_a + emul.upper);
    }
};
