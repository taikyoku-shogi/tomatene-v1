#pragma once

#include <cstdint>
#include <array>

// How many bits the transposition table keys should be.
constexpr inline uint32_t TRANSPOSITION_TABLE_BITS = 22;
constexpr inline uint32_t TRANSPOSITION_TABLE_SIZE = 1u << TRANSPOSITION_TABLE_BITS;
constexpr inline hash_t HASH_MASK = (1 << TRANSPOSITION_TABLE_BITS) - 1;

enum class NodeType {
	EXACT,
	LOWER_BOUND,
	UPPER_BOUND
};
struct TranspositionTableEntry {
	hash_t hash;
	uint32_t bestMove;
	// depth needs to be initialised here to 0 so that all actual entries will have a higher depth thatn it
	depth_t depth = 0;
	eval_t eval;
	NodeType nodeType;
};

class TranspositionTable {
private:
	std::array<TranspositionTableEntry, TRANSPOSITION_TABLE_SIZE>table{};
public:
	TranspositionTableEntry* get(hash_t hash) {
		hash_t maskedHash = hash & HASH_MASK;
		TranspositionTableEntry& entry = table[maskedHash];
		if(entry.hash == hash) {
			return &entry;
		}
		return nullptr;
	}
	void put(hash_t hash, uint32_t bestMove, depth_t depth, eval_t eval, NodeType nodeType) {
		hash_t maskedHash = hash & HASH_MASK;
		if(depth >= table[maskedHash].depth) {
			table[maskedHash] = { hash, bestMove, depth, eval, nodeType };
		}
	}
};