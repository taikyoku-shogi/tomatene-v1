#pragma once

#include <cstdint>
#include <array>

// How many bits the transposition table keys should be.
constexpr inline uint32_t TRANSPOSITION_TABLE_BITS = 20;
constexpr inline uint32_t TRANSPOSITION_TABLE_SIZE = 1u << TRANSPOSITION_TABLE_BITS;
constexpr inline uint32_t HASH_MASK = (1 << TRANSPOSITION_TABLE_BITS) - 1;

struct TranspositionTableEntry {
	uint32_t hash;
	uint32_t bestMove;
};

class TranspositionTable {
private:
	std::array<TranspositionTableEntry, TRANSPOSITION_TABLE_SIZE>table;
public:
	TranspositionTableEntry* get(uint32_t hash) {
		uint32_t maskedHash = hash & HASH_MASK;
		TranspositionTableEntry& entry = table[maskedHash];
		if(entry.hash == hash) {
			return &entry;
		}
		return nullptr;
	}
	void put(uint32_t hash, uint32_t bestMove) {
		uint32_t maskedHash = hash & HASH_MASK;
		table[maskedHash] = { hash, bestMove };
	}
};