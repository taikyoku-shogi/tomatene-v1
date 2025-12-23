// Made by ChatGPT

#include <array>
#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

// xoroshiro128+ PRNG
struct xoroshiro128plus {
    uint64_t s[2];

    static constexpr uint64_t rotl(uint64_t x, int k) {
        return (x << k) | (x >> (64 - k));
    }

    uint64_t next64() {
        uint64_t s0 = s[0];
        uint64_t s1 = s[1];
        uint64_t result = s0 + s1;

        s1 ^= s0;
        s[0] = rotl(s0, 55) ^ s1 ^ (s1 << 14);
        s[1] = rotl(s1, 36);

        return result;
    }
};

int main() {
    constexpr int num_pieces = 301;
    constexpr int num_ranks  = 36;
    constexpr int num_files  = 36;
    constexpr int total_keys = num_pieces + num_ranks + num_files;

    xoroshiro128plus rng{{0x12345123467F0ULL, 0x0FE65BBBBB4321ULL}};

    std::vector<uint64_t> best_set(total_keys);
    int best_min_distance = -1;
    double best_avg_distance = -1.0;

    while (true) {
        // Generate candidate set
        std::vector<uint64_t> candidate(total_keys);
        for (int i = 0; i < total_keys; ++i) {
            candidate[i] = rng.next64();
        }

        // Compute pairwise Hamming distances
        int min_dist = 32;
        uint64_t sum_dist = 0;
        uint64_t pair_count = 0;
        bool early_exit = false;

        for (int i = 0; i < total_keys; ++i) {
            for (int j = i + 1; j < total_keys; ++j) {
                int dist = std::popcount(candidate[i] ^ candidate[j]);

                if (dist < min_dist)
                    min_dist = dist;

                sum_dist += dist;
                ++pair_count;

                if (min_dist < best_min_distance) {
                    early_exit = true;
                    break;
                }
            }
            if (early_exit) break;
        }

        if (early_exit)
            continue;

        double avg_dist = static_cast<double>(sum_dist) / pair_count;

        // Keep if better
        if (min_dist > best_min_distance ||
            (min_dist == best_min_distance && avg_dist > best_avg_distance)) {

            best_min_distance = min_dist;
            best_avg_distance = avg_dist;
            best_set = candidate;

            std::ofstream out("zobrist_keys_64.hpp");
            out << "#pragma once\n#include <array>\n#include <cstdint>\n\n";
            out << "constexpr std::array<uint64_t, " << total_keys
                << "> ZOBRIST_KEYS = {\n";

            for (uint64_t k : best_set) {
                out << k << ",\n";
            }

            out << "};\n";
            out.close();

            std::cout << "New best set: min dist = "
                      << best_min_distance
                      << ", avg dist = "
                      << best_avg_distance
                      << " saved.\n";
        }
    }
}
