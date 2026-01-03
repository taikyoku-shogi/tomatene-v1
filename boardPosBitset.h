#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <bit>
#include <immintrin.h>

// A bitset for storing board positions.
class BoardPosBitset {
private:
	alignas(64) std::array<uint64_t, 21> words{};
public:
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
		uint64_t* thisWords = words.data();
		const uint64_t* otherWords = other.words.data();
		#ifdef __AVX512F__
			// Two AVX-512 registers + 1 AVX register + 1 regular register
			for(int i = 0; i < 2; i++) {
				uint64_t* thisWordAdr = thisWords + i * 8;
				__m512i thisWord = _mm512_load_si512(thisWordAdr);
				const uint64_t* otherWordAdr = otherWords + i * 8;
				__m512i otherWord = _mm512_load_si512(otherWordAdr);
				__m512i res = _mm512_or_si512(thisWord, otherWord);
				_mm512_store_si512(thisWordAdr, res);
			}
			
			__m256i* thisWordAdr = reinterpret_cast<__m256i*>(thisWords + 16);
			__m256i thisWord = _mm256_load_si256(thisWordAdr);
			const __m256i* otherWordAdr = reinterpret_cast<const __m256i*>(otherWords + 16);
			__m256i otherWord = _mm256_load_si256(otherWordAdr);
			__m256i res = _mm256_or_si256(thisWord, otherWord);
			_mm256_store_si256(thisWordAdr, res);
			
			words[20] |= other.words[20];
		#elif defined(__AVX2__)
			// 5 AVX registers + 1 regular register
			for(int i = 0; i < 5; i++) {
				__m256i* thisWordAdr = reinterpret_cast<__m256i*>(thisWords + i * 4);
				__m256i thisWord = _mm256_load_si256(thisWordAdr);
				const __m256i* otherWordAdr = reinterpret_cast<const __m256i*>(otherWords + i * 4);
				__m256i otherWord = _mm256_load_si256(otherWordAdr);
				__m256i res = _mm256_or_si256(thisWord, otherWord);
				_mm256_store_si256(thisWordAdr, res);
			}
			
			words[20] |= other.words[20];
		#else
			for(size_t i = 0; i < 21; i++) {
				words[i] |= other.words[i];
			}
		#endif
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
			word = bitTransformFunction(word, other.words[wordIndex]);
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