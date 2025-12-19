// Made by ChatGPT

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

struct uint128_t {
    uint64_t hi;
    uint64_t lo;

    constexpr uint128_t operator^(const uint128_t& other) const {
        return {hi ^ other.hi, lo ^ other.lo};
    }

    constexpr int hamming_distance(const uint128_t& other) const {
        return std::popcount(hi ^ other.hi) + std::popcount(lo ^ other.lo);
    }
};

// xoroshiro128+ PRNG (better quality than xorshift)
struct xoroshiro128plus {
    uint64_t s[2];

    // rotate left
    static constexpr uint64_t rotl(const uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }

    uint64_t next64() {
        uint64_t s0 = s[0];
        uint64_t s1 = s[1];
        uint64_t result = s0 + s1;

        s1 ^= s0;
        s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14); // a, b
        s[1] = rotl(s1, 36);                   // c

        return result;
    }

    uint128_t next128() {
        uint64_t hi = next64();
        uint64_t lo = next64();
        return {hi, lo};
    }
};

int main() {
    constexpr int num_pieces = 301;
    constexpr int num_ranks = 36;
    constexpr int num_files = 36;
    constexpr int total_keys = num_pieces + num_ranks + num_files;

    xoroshiro128plus rng{{0x123456789BCDEF0ULL, 0x0FEDC987654321ULL}};

    std::vector<uint128_t> best_set(total_keys);
    int best_min_distance = -1;
    double best_avg_distance = -1.0;

    while (true) {
        // Generate candidate set
        std::vector<uint128_t> candidate(total_keys);
        for (int i = 0; i < total_keys; ++i) {
            candidate[i] = rng.next128();
        }

        // Compute pairwise distances
        int min_dist = 128;
        uint64_t sum_dist = 0;
        uint64_t pair_count = 0;
        bool early_exit = false;

        for (int i = 0; i < total_keys; ++i) {
            for (int j = i + 1; j < total_keys; ++j) {
                int dist = candidate[i].hamming_distance(candidate[j]);
                if (dist < min_dist) min_dist = dist;
                sum_dist += dist;
                ++pair_count;

                if (min_dist < best_min_distance) {
                    early_exit = true;
                    break;
                }
            }
            if (early_exit) break;
        }

        if (early_exit) continue;

        double avg_dist = static_cast<double>(sum_dist) / pair_count;

        // Keep if better
        if (min_dist > best_min_distance || (min_dist == best_min_distance && avg_dist > best_avg_distance)) {
            best_min_distance = min_dist;
            best_avg_distance = avg_dist;
            best_set = candidate;

            std::ofstream out("zobrist_keys.hpp");
            out << "#pragma once\n#include <array>\n#include <cstdint>\n\n";
            out << "struct uint128_t { uint64_t hi; uint64_t lo; };\n\n";
            out << "constexpr std::array<uint128_t, " << total_keys << "> ZOBRIST_KEYS = {\n";
            for (auto& k : best_set) {
                out << "    {" << k.hi << "ULL, " << k.lo << "ULL},\n";
            }
            out << "};\n";
            out.close();

            std::cout << "New best set: min dist = " << best_min_distance 
                      << ", avg dist = " << best_avg_distance << " saved.\n";
        }
    }
}
