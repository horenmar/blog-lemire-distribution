#pragma once

#include <cstdint>

class java_plain {
private:
	std::uint64_t m_a, m_b;

	template <typename Generator>
	uint64_t drawNumber(Generator& g) {
		return Catch::Detail::fillBitsFrom<uint64_t>(g);
	}

public:
	using result_type = std::uint64_t;

	java_plain(std::uint64_t a, std::uint64_t b) :m_a(a), m_b(b) {}

	template <typename Generator>
	result_type operator()(Generator& g) {
		const auto distance = m_b - m_a + 1;
		if (distance == 0) {
			return drawNumber(g);
		}
		auto x = drawNumber(g);
		auto r = x % distance;
		while (x - r > (-distance)) {
			x = drawNumber(g);
			r = x % distance;
		}
		return m_a + r;
	}
};

class java_reuse {
private:
	std::uint64_t m_a, m_distance;

	template <typename Generator>
	uint64_t drawNumber(Generator& g) {
		return Catch::Detail::fillBitsFrom<uint64_t>(g);
	}

public:
	using result_type = std::uint64_t;

	java_reuse(std::uint64_t a, std::uint64_t b) :m_a(a), m_distance(b - m_a + 1) {}

	template <typename Generator>
	result_type operator()(Generator& g) {
		if (m_distance == 0) {
			return drawNumber(g);
		}
		auto x = drawNumber(g);
		auto r = x % m_distance;
		while (x - r > (-m_distance)) {
			x = drawNumber(g);
			r = x % m_distance;
		}
		return m_a + r;
	}
};

class OpenBSD_plain {
private:
	std::uint64_t m_a, m_b;

	template <typename Generator>
	uint64_t drawNumber(Generator& g) {
		return Catch::Detail::fillBitsFrom<uint64_t>(g);
	}

public:
	using result_type = std::uint64_t;

	OpenBSD_plain(std::uint64_t a, std::uint64_t b) :m_a(a), m_b(b) {}

	template <typename Generator>
	result_type operator()(Generator& g) {
		const auto distance = m_b - m_a + 1;
		if (distance == 0) {
			return drawNumber(g);
		}
		const auto threshold = (-distance) % distance;

		// do while?
		auto x = drawNumber(g);
		while (x < threshold) {
			x = drawNumber(g);
		}

		return m_a + (x % distance);
	}
};

class OpenBSD_reuse {
private:
	std::uint64_t m_a, m_distance, m_threshold;

	template <typename Generator>
	uint64_t drawNumber(Generator& g) {
		return Catch::Detail::fillBitsFrom<uint64_t>(g);
	}

public:
	using result_type = std::uint64_t;

	OpenBSD_reuse(std::uint64_t a, std::uint64_t b) :m_a(a), m_distance(b - m_a + 1), m_threshold((-m_distance) % m_distance) {}

	template <typename Generator>
	result_type operator()(Generator& g) {
		if (m_distance == 0) {
			return drawNumber(g);
		}
		// do while?
		auto x = drawNumber(g);
		while (x < m_threshold) {
			x = drawNumber(g);
		}

		return m_a + (x % m_distance);
	}
};
