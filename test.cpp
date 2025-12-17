#include <iostream>
#include <string_view>
#include <frozen/unordered_map.h>
#include <frozen/string.h>

// 1. The Piece Structure
struct Piece {
    // The Enum defined inside the struct for scoping
    enum Type : uint16_t {
        None = 0,
        #define Piece(code, move, promo) code,
            #include "pieces.inc"
        #undef Piece
        TOTAL_COUNT
    };

    uint16_t raw;
    
    // Implicit conversion for easy math
    operator uint16_t() const { return raw; }
};

// 2. The Frozen Lookup Table
// This table is built at compile-time and stored in the binary's read-only section.
namespace PieceRegistry {
    // static constexpr frozen::unordered_map<frozen::string, uint16_t, Piece::TOTAL_COUNT - 1> CodeToID = ({
    //     #define Piece(code, move, promo) { #code, Piece::code },
    //         #include "pieces.inc"
    //     #undef Piece
    // });

    // Helper function for safe lookups
    // Piece::Type fromString(std::string_view code) {
    //     auto it = CodeToID.find(code);
    //     if (it != CodeToID.end()) {
    //         return it->second;
    //     }
    //     return Piece::None;
    // }
}

constexpr frozen::unordered_map<frozen::string, uint16_t, Piece::TOTAL_COUNT - 1> CodeToID = {
	#define Piece(code, move, promo) { #code, Piece::code },
		#include "pieces.inc"
	#undef Piece
};
// 3. Example Usage
int main() {
// 	constexpr frozen::unordered_map<frozen::string, int, 2> olaf = {
//     {"19", 19},
//     {"31", 31},
// };
// constexpr auto val = olaf.at("19");
// std::cout << val;
	
    // std::string_view input = "K"; // Imagine this comes from a file or UI
    
    // Piece::Type id = PieceRegistry::fromString(input);

    // if (id == Piece::GG) {
    //     std::cout << "Successfully identified Great General (ID: " << id << ")" << std::endl;
    // }
	std::string pieceName;
	std::cin >> pieceName;
	std::cout << pieceName << ": " << Piece::FLG << " has ID: " << CodeToID.at(static_cast<std::string_view>(pieceName)) << std::endl;

    return 0;
}