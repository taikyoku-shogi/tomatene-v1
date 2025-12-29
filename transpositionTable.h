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
	uint32_t bestMove = 0;
	// depth needs to be initialised here to 0 so that all actual entries will have a higher depth thatn it
	depth_t depth = 0;
	// The age of an entry indicates how many irreversible moves have been made since the start of the game. E.g. when a capture is made, the age will be increased. This means stale entries can be identified better.
	uint16_t age;
	eval_t eval;
	NodeType nodeType;
};

class TranspositionTable {
private:
	std::array<TranspositionTableEntry, TRANSPOSITION_TABLE_SIZE>table{};
public:
	size_t size = 0;
	TranspositionTableEntry* get(hash_t hash) {
		hash_t maskedHash = hash & HASH_MASK;
		TranspositionTableEntry& entry = table[maskedHash];
		if(entry.hash == hash) {
			return &entry;
		}
		return nullptr;
	}
	void put(hash_t hash, uint32_t bestMove, depth_t depth, uint16_t age, eval_t eval, NodeType nodeType) {
		hash_t maskedHash = hash & HASH_MASK;
		if(table[maskedHash].bestMove == 0) { // this will detect new entries - a best move will never be 0
			size++;
		}
		if(depth >= table[maskedHash].depth || age != table[maskedHash].age) {
			table[maskedHash] = { hash, bestMove, depth, age, eval, nodeType };
		}
	}
};