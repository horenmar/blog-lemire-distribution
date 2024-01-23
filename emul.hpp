#pragma once

#include <cstdint>

struct ext_mul_result {
    std::uint64_t upper;
    std::uint64_t lower;
    friend bool operator==( ext_mul_result const& lhs, ext_mul_result const& rhs ) {
        return lhs.upper == rhs.upper && lhs.lower == rhs.lower;
    }
};

// Returns 128 bit result of multiplying lhs and rhs
inline ext_mul_result ext_mul_naive( std::uint64_t lhs, std::uint64_t rhs ) {
    // We use the simple long multiplication approach for
    // correctness, we can use platform specific builtins
    // for performance later.

    // Split the lhs and rhs into two 32bit "digits", so that we can
    // do 64 bit arithmetic to handle carry bits.
    //            32b    32b    32b    32b
    //     lhs                  L1     L2
    //   * rhs                  R1     R2
    //            ------------------------
    //                       |  R2 * L2  |
    //                 |  R2 * L1  |
    //                 |  R1 * L2  |
    //           |  R1 * L1  |
    //           -------------------------
    //           |  a  |  b  |  c  |  d  |

#define CarryBits( x ) ( x >> 32 )
#define Digits( x ) ( x & 0xFF'FF'FF'FF )

    auto r2l2 = Digits( rhs ) * Digits( lhs );
    auto r2l1 = Digits( rhs ) * CarryBits( lhs );
    auto r1l2 = CarryBits( rhs ) * Digits( lhs );
    auto r1l1 = CarryBits( rhs ) * CarryBits( lhs );

    // Sum to columns first
    auto d = Digits( r2l2 );
    auto c = CarryBits( r2l2 ) + Digits( r2l1 ) + Digits( r1l2 );
    auto b = CarryBits( r2l1 ) + CarryBits( r1l2 ) + Digits( r1l1 );
    auto a = CarryBits( r1l1 );

    // Propagate carries between columns
    c += CarryBits( d );
    b += CarryBits( c );
    a += CarryBits( b );

    // Remove the used carries
    c = Digits( c );
    b = Digits( b );
    a = Digits( a );

    return {
        a << 32 | b, // upper 64 bits
        c << 32 | d  // lower 64 bits
    };
}

inline ext_mul_result ext_mul_optimized( std::uint64_t lhs, std::uint64_t rhs ) {
    std::uint64_t lhs_low = Digits( lhs );
    std::uint64_t rhs_low = Digits( rhs );
    std::uint64_t low_low = ( lhs_low * rhs_low );
    std::uint64_t high_high = CarryBits( lhs ) * CarryBits( rhs );

    // We add in carry bits from low-low already
    std::uint64_t high_low = ( CarryBits( lhs ) * rhs_low ) + CarryBits( low_low );
    // Note that we can add only low bits from high_low, to avoid overflow with large inputs
    std::uint64_t low_high = ( lhs_low * CarryBits( rhs ) ) + Digits( high_low );

    return { high_high + CarryBits( high_low ) + CarryBits( low_high ), ( low_high << 32 ) | Digits( low_low ) };
}

#undef CarryBits
#undef Digits

#if defined( __SIZEOF_INT128__ )
#    define USE_UINT128
#elif defined( _MSC_VER ) && ( defined( _WIN64 ) || defined( _M_ARM64 ) )
#    include <intrin.h>
#    pragma intrinsic( _umul128 )
#    define USE_MSVC_UMUL
#endif

inline ext_mul_result ext_mul_intrinsic( std::uint64_t lhs, uint64_t rhs ) {
#if defined( USE_UINT128 )
    auto result = __uint128_t( lhs ) * __uint128_t( rhs );
    return { static_cast<uint64_t>( result >> 64 ), static_cast<uint64_t>( result ) };
#elif defined( USE_MSVC_UMUL )
    std::uint64_t high;
    std::uint64_t low = _umul128( lhs, rhs, &high );
    return { high, low };
#else
    // We still want the code to compile
    throw std::runtime_error( "no intrisic available" );
#endif
}
