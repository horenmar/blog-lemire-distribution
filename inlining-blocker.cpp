#include "inlining-blocker.hpp"

uint32_t same(uint32_t n) { return n; }
uint64_t same(uint64_t n) { return n; }

uint32_t float_bound(uint32_t) {
	return 33554430;
}
uint64_t float_bound(uint64_t) {
	return 18014398509481982;
}
