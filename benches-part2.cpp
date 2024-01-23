#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include <catch2/internal/catch_random_integer_helpers.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "emul.hpp"
#include "pcg.hpp"
#include "distributions-lemire.hpp"
#include "distributions-others.hpp"
#include "inlining-blocker.hpp"

#include <random>

TEST_CASE("Verify emul results") {
	std::random_device rd;
	for (int i = 0; i < 1'000'000; ++i) {
		auto num1 = Catch::Detail::fillBitsFrom<uint64_t>(rd);
		auto num2 = Catch::Detail::fillBitsFrom<uint64_t>(rd);
		CAPTURE(num1, num2);
		auto result_naive = ext_mul_naive(num1, num2);
		auto result_optimized = ext_mul_optimized(num1, num2);
		auto result_intrinsic = ext_mul_intrinsic(num1, num2);
		REQUIRE(result_naive == result_optimized);
		REQUIRE(result_optimized == result_intrinsic);
	}
}


namespace {
	struct NaiveMult {
		static ext_mul_result Mult(uint64_t a, uint64_t b) {
			return ext_mul_naive(a, b);
		}
	};
	struct OptimizedMult {
		static ext_mul_result Mult(uint64_t a, uint64_t b) {
			return ext_mul_optimized(a, b);
		}
	};
	struct IntrinsicMult {
		static ext_mul_result Mult(uint64_t a, uint64_t b) {
			return ext_mul_intrinsic(a, b);
		}
	};
	static std::vector<uint64_t> generate_random_data(size_t size) {
		std::vector<uint64_t> data; data.reserve(size);
		std::random_device rd;
		for (size_t i = 0; i < size; ++i) {
			data.push_back(Catch::Detail::fillBitsFrom<uint64_t>(rd));
		}
		return data;
	}
}

TEST_CASE( "Mod benchmark", "[!benchmark]" ) {
    auto size = GENERATE( as<size_t>{}, 10'000, 100'000, 1'000'000 );
    auto N = GENERATE( as<uint64_t>{},
                       100,
                       std::numeric_limits<uint32_t>::max() - 1,
                       std::numeric_limits<uint32_t>::max(),
                       uint64_t( std::numeric_limits<uint32_t>::max() ) + 1,
                       uint64_t( 1 ) << 36,
                       uint64_t( 1 ) << 40,
                       uint64_t( 1 ) << 44,
                       uint64_t( 1 ) << 48,
                       uint64_t( 1 ) << 52,
                       uint64_t( 1 ) << 56,
                       uint64_t( 1 ) << 60,
                       std::numeric_limits<uint64_t>::max() / 2 + 1,
                       12298110947468241578,
                       std::numeric_limits<uint64_t>::max() - 1 );
    BENCHMARK_ADVANCED( "iters=" + std::to_string( size ) + ", bounds=" + std::to_string( N ) )( Catch::Benchmark::Chronometer meter ) {
        auto data = generate_random_data( size );
        meter.measure( [&]( int ) {
            uint64_t sum = 0;
            for ( auto in : data ) {
                sum += in % N;
            }
            return sum;
        } );
    };
}

TEMPLATE_TEST_CASE("Emul benchmarks", "[!benchmark]", NaiveMult, OptimizedMult, IntrinsicMult) {
	auto size = GENERATE(as<size_t>{}, 10'000, 100'000, 1'000'000);
	BENCHMARK_ADVANCED("iters=" + std::to_string(size))(Catch::Benchmark::Chronometer meter) {
		auto data1 = generate_random_data(size);
		auto data2 = generate_random_data(size);
		meter.measure([&](int) {
			uint64_t sum = 0;
			for (size_t sz = 0; sz < size; ++sz) {
				auto [high, low] = TestType::Mult(data1[sz], data2[sz]);
				sum += high;
				sum += low;
			}
			return sum;
		});
	};
}


TEMPLATE_TEST_CASE("Distribution tests", "[distributions]",
	OpenBSD_plain,
	OpenBSD_reuse,
	java_plain,
	java_reuse,
	lemire_plain_templated_mult<NaiveMult>,
	lemire_plain_templated_mult<OptimizedMult>,
	lemire_plain_templated_mult<IntrinsicMult>,
	lemire_reuse_templated_mult<NaiveMult>,
	lemire_reuse_templated_mult<OptimizedMult>,
	lemire_reuse_templated_mult<IntrinsicMult>) {
	const size_t tests = 10'000;
	SimplePcg32 pcg(std::random_device{}());
	SECTION("Some bounds") {
		uint64_t low = 7;
		uint64_t high = 22;
		TestType dist(low, high);
		for (size_t t = 0; t < tests; ++t) {
			auto result = dist(pcg);
			REQUIRE(result >= low);
			REQUIRE(result <= high);
		}
	}
	SECTION("Unitary bound") {
		uint64_t low = 42;
		uint64_t high = low;
		TestType dist(low, high);
		for (size_t t = 0; t < tests; ++t) {
			auto result = dist(pcg);
			REQUIRE(result >= low);
			REQUIRE(result <= high);
		}
	}
}

TEMPLATE_TEST_CASE("Benchmark with other distributions", "[!benchmark]",
	OpenBSD_plain,
	java_plain,
	OpenBSD_reuse,
	java_reuse,
	lemire_reuse_templated_mult<NaiveMult>,
	lemire_reuse_templated_mult<OptimizedMult>,
	lemire_reuse_templated_mult<IntrinsicMult>,
	lemire_plain_templated_mult<NaiveMult>,
	lemire_plain_templated_mult<OptimizedMult>,
	lemire_plain_templated_mult<IntrinsicMult>) {

	auto bounds = GENERATE(as<uint64_t>{},
		100,
		std::numeric_limits<uint32_t>::max() - 1,
		std::numeric_limits<uint32_t>::max(),
		uint64_t(std::numeric_limits<uint32_t>::max()) + 1,
		uint64_t(1) << 36,
		uint64_t(1) << 40,
		uint64_t(1) << 44,
		uint64_t(1) << 48,
		uint64_t(1) << 52,
		uint64_t(1) << 56,
		uint64_t(1) << 60,
		std::numeric_limits<uint64_t>::max() / 2 + 1,
		12298110947468241578,
		std::numeric_limits<uint64_t>::max() - 1);
	auto iters = GENERATE(as<size_t>{}, 100'000, 1'000'000, 10'000'000);

	SimplePcg32 rng;
	BENCHMARK("bounds=" + std::to_string(bounds) + ", iters=" + std::to_string(iters)) {
		uint64_t sum = 0;
		TestType dist(0, same(bounds));
		for (size_t n = 0; n < iters; ++n) {
			sum += dist(rng);
		}
		return sum;
	};
}

TEMPLATE_TEST_CASE("Benchmark without distribution reuse", "[!benchmark]",
	OpenBSD_plain,
	java_plain,
	lemire_plain_templated_mult<NaiveMult>,
	lemire_plain_templated_mult<OptimizedMult>,
	lemire_plain_templated_mult<IntrinsicMult>) {

	size_t iters = GENERATE(100'000, 1'000'000, 10'000'000);

	SimplePcg32 rng;
	BENCHMARK("iters=" + std::to_string(iters)) {
		uint64_t sum = 0;
		for (size_t high_now = 0; high_now < iters; ++high_now) {
			TestType dist(0, high_now);
			sum += dist(rng);
		}
		return sum;
	};
}
