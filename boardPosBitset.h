#pragma once
#include <array>
#include <cstdint>
#include <vector>

// A bitset for storing board positions.
class BoardPosBitset {
private:
	std::array<uint64_t, 21> words{};
public:
	constexpr uint64_t getWord(size_t wordI) const {
		return words[wordI];
	}
	constexpr void insert(const Vec2 pos) {
		size_t i = pos.toIndex();
		insert(i);
	}
	constexpr void insert(const size_t i) {
		words[i >> 6] |= 1ULL << (i & 63);
	}
	constexpr void erase(const Vec2 pos) {
		size_t i = pos.toIndex();
		erase(i);
	}
	constexpr void erase(const size_t i) {
		words[i >> 6] &= ~(1ULL << (i & 63));
	}
	constexpr bool contains(const Vec2 pos) const {
		size_t i = pos.toIndex();
		return contains(i);
	}
	constexpr bool contains(const size_t i) const {
		return words[i >> 6] & 1ULL << (i & 63);
	}
	constexpr void clear() {
		words.fill(0);
	}
	constexpr size_t size() const {
		size_t count = 0;
		for(uint64_t word : words) {
			count += std::popcount(word);
		}
		return count;
	}
	
	constexpr BoardPosBitset& operator|=(const BoardPosBitset &other) noexcept {
		for(size_t i = 0; i < 21; i++) {
			words[i] |= other.getWord(i);
		}
		return *this;
	}
	
	template <typename T, std::invocable<Vec2> F>
	requires std::convertible_to<std::invoke_result_t<F, Vec2>, T>
	constexpr void transformInto(std::vector<T> &dest, F &&transformFunction) const {
		dest.reserve(dest.size() + size());
		size_t wordIndex = 0;
		for(uint64_t word : words) {
			while(word) {
				auto bitOffset = std::countr_zero(word);
				size_t index = (wordIndex << 6) + bitOffset;
				dest.push_back(transformFunction(Vec2::fromIndex(index)));
				
				// remove the trailing bit
				word &= word - 1;
			}
			wordIndex++;
		}
	}
	template <std::invocable<uint64_t, uint64_t> F, std::invocable<size_t> J>
	requires std::convertible_to<std::invoke_result_t<F, uint64_t, uint64_t>, uint64_t>
	constexpr void bitTransformForEach(const BoardPosBitset &other, F &&bitTransformFunction, J &&forEachFunction) const {
		size_t wordIndex = 0;
		for(uint64_t word : words) {
			word = bitTransformFunction(word, other.getWord(wordIndex));
			while(word) {
				auto bitOffset = std::countr_zero(word);
				size_t index = (wordIndex << 6) + bitOffset;
				forEachFunction(index);
				
				// remove the trailing bit
				word &= word - 1;
			}
			wordIndex++;
		}
	}
	template <std::invocable<size_t> F>
	constexpr void forEach(F &&forEachFunction) const {
		size_t wordIndex = 0;
		for(uint64_t word : words) {
			while(word) {
				auto bitOffset = std::countr_zero(word);
				size_t index = (wordIndex << 6) + bitOffset;
				forEachFunction(index);
				
				// remove the trailing bit
				word &= word - 1;
			}
			wordIndex++;
		}
	}
	template <std::invocable<size_t> F>
	constexpr void forEachAndClear(F &&forEachFunction) {
		size_t wordIndex = 0;
		for(uint64_t &word : words) {
			while(word) {
				auto bitOffset = std::countr_zero(word);
				size_t index = (wordIndex << 6) + bitOffset;
				forEachFunction(index);
				
				// remove the trailing bit
				word &= word - 1;
			}
			wordIndex++;
		}
	}
};