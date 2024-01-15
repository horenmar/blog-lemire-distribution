#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/internal/catch_uniform_integer_distribution.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <random>
#include <limits>
#include <cstdint>

#include "pcg.hpp"
#include "distributions.hpp"
#include "inlining-blocker.hpp"

template <template <typename T> class Dist> struct dist {
	template <typename T> using type = Dist<T>;
};

TEMPLATE_TEST_CASE("both distributions return the same numbers", "[reproducibility]",
	lemire_algorithm_reuse<uint32_t>,
	lemire_algorithm_reuse<uint64_t>,
	lemire_algorithm_lazy_reuse<uint32_t>,
	lemire_algorithm_lazy_reuse<uint64_t>
) {
	static constexpr size_t iters = 100'000;
	using T = typename TestType::result_type;
	TestType reuse(0, 1'000'000);
	lemire_algorithm_no_reuse<T> noreuse(0, 1'000'000);

	// We use different URBGs to check that the stretching and narrowing
	// of the bits also happens in the same way.
	SECTION("Using PCG") {
		SimplePcg32 rng1, rng2;
		for (size_t i = 0; i < iters; ++i) {
			REQUIRE(reuse(rng1) == noreuse(rng2));
		}
	}
	SECTION("Using MT_64") {
		std::mt19937_64 rng1, rng2;
		for (size_t i = 0; i < iters; ++i) {
			REQUIRE(reuse(rng1) == noreuse(rng2));
		}
	}
}

TEMPLATE_TEST_CASE("std and Catch2 implement the same distribution", "[reproducibility]", dist<lemire_algorithm_reuse>, dist<lemire_algorithm_lazy_reuse>) {
	static constexpr size_t iters = 100'000;
	// Note: We only test cases where the output width of the generator
	//       is the same as the width of the distribution type. In other
	//       cases, the stretching/narrowing of the bits from the URBG
	//       can lead to different bits being used by the actual distribution.
	SECTION("32 -> 32") {
		SimplePcg32 rng1, rng2;
		std::uniform_int_distribution<uint32_t> stddist(0, 100'000);
		typename TestType::type<uint32_t> catchdist(0, 100'000);
		for (size_t i = 0; i < iters; ++i) {
			REQUIRE(stddist(rng1) == catchdist(rng2));
		}
	}
	SECTION("64 -> 64") {
		std::mt19937_64 rng1, rng2;
		std::uniform_int_distribution<uint64_t> stddist(0, 100'000);
		typename TestType::type<uint64_t> catchdist(0, 100'000);
		for (size_t i = 0; i < iters; ++i) {
			REQUIRE(stddist(rng1) == catchdist(rng2));
		}
	}
}

TEMPLATE_TEST_CASE("no-reuse bench", "[!benchmark]", uint32_t, uint64_t) {
	size_t iters = GENERATE(100'000, 1'000'000, 10'000'000);

	SimplePcg32 rng;
	SECTION("baseline") {
		BENCHMARK("plain generator, iters=" + std::to_string(iters)) {
			TestType sum = 0;
			for (size_t n = 0; n < iters; ++n) {
				sum += Catch::Detail::fillBitsFrom<TestType>(rng);
			}
		};
	}
	SECTION("noreuse") {
		BENCHMARK("noreuse, iters=" + std::to_string(iters)) {
			TestType sum = 0;
			for (size_t high_now = 0; high_now < iters; ++high_now) {
				lemire_algorithm_no_reuse<TestType> dist(0, high_now);
				sum += dist(rng);
			}
			return sum;
		};
	}
	SECTION("reuse") {
		BENCHMARK("reuse, iters=" + std::to_string(iters)) {
			TestType sum = 0;
			for (size_t high_now = 0; high_now < iters; ++high_now) {
				lemire_algorithm_reuse<TestType> dist(0, high_now);
				sum += dist(rng);
			}
			return sum;
		};
	}
	SECTION("lazy-reuse") {
		BENCHMARK("lazy-reuse, iters=" + std::to_string(iters)) {
			TestType sum = 0;
			for (size_t high_now = 0; high_now < iters; ++high_now) {
				lemire_algorithm_lazy_reuse<TestType> dist(0, high_now);
				sum += dist(rng);
			}
			return sum;
		};
	}
}

template <typename TestType>
static void RunBenchmarksWithPremadeDistributions(TestType right_bound, size_t iters) {
	static_assert(std::is_unsigned<TestType>::value, "");
	SimplePcg32 rng;
	SECTION("baseline") {
		BENCHMARK("plain generator, iters=" + std::to_string(iters)) {
			TestType sum = 0;
			for (size_t n = 0; n < iters; ++n) {
				sum += Catch::Detail::fillBitsFrom<TestType>(rng);
			}
		};
	}
	SECTION("noreuse") {
		BENCHMARK("noreuse, iters=" + std::to_string(iters)) {
			TestType sum = 0;
			lemire_algorithm_no_reuse<TestType> dist(0, same(right_bound));
			for (size_t n = 0; n < iters; ++n) {
				sum += dist(rng);
			}
			return sum;
		};
	}
	SECTION("reuse") {
		BENCHMARK("reuse, iters=" + std::to_string(iters)) {
			TestType sum = 0;
			lemire_algorithm_reuse<TestType> dist(0, same(right_bound));
			for (size_t n = 0; n < iters; ++n) {
				sum += dist(rng);
			}
			return sum;
		};
	}
	SECTION("lazy-reuse") {
		BENCHMARK("lazy-reuse, iters=" + std::to_string(iters)) {
			TestType sum = 0;
			lemire_algorithm_lazy_reuse<TestType> dist(0, same(right_bound));
			for (size_t n = 0; n < iters; ++n) {
				sum += dist(rng);
			}
			return sum;
		};
	}
}

// We could make all these into a single test case by putting the bounds
// into generator as well, but this way it is easier to run individual
// benchmarks.
TEMPLATE_TEST_CASE("stacked bench", "[!benchmark]", uint32_t, uint64_t) {
	size_t iters = GENERATE(100'000, 1'000'000, 10'000'000);
	static constexpr TestType high = std::numeric_limits<TestType>::max() - 1;
	RunBenchmarksWithPremadeDistributions(high, iters);
}

TEMPLATE_TEST_CASE("integer for float generator bench", "[!benchmark]", uint32_t, uint64_t) {
	size_t iters = GENERATE(1, 5, 10, 100, 500, 1000, 10'000);
	RunBenchmarksWithPremadeDistributions(float_bound(TestType{}), iters);
}

TEMPLATE_TEST_CASE("integer for resampling", "[!benchmark]", uint32_t, uint64_t) {
	size_t iters = GENERATE(100'000, 1'000'000, 10'000'000, 100'000'000, 1'000'000'000);
	RunBenchmarksWithPremadeDistributions(TestType(100), iters);
}

