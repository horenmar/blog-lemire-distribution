#pragma once

// We use functions in different TU to block inlining and constant-folding.
// This would easily be defeated by LTO, but we assume release build w/o LTO

#include <stdint.h>
#include <string>

//template <typename T>
//constexpr T same(T n) { return n; }

uint32_t same(uint32_t);
uint64_t same(uint64_t);

// Distribution bounds based on the max number of equidistant floating point numbers in float/double
uint32_t float_bound(uint32_t);
uint64_t float_bound(uint64_t);
