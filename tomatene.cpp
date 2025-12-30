#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <bit>
#include <cmath>

#include <frozen/unordered_map.h>
#include <frozen/string.h>

#define ENGINE_NAME "Tomatene"
#define ENGINE_DESC "Experimental taikyoku shogi engine"
#define ENGINE_AUTHOR "s1050613"
#define ENGINE_VERSION "v0"

// Centipawns
typedef int32_t eval_t;
typedef uint8_t depth_t;
typedef uint64_t hash_t;

#include "transpositionTable.h"

inline constexpr depth_t MAX_DEPTH = 3;
inline constexpr float ESTIMATED_GAME_LENGTH = 900;

constexpr std::vector<std::string> splitString(std::string str, char delimiter) {
	std::vector<std::string> res;
	std::stringstream ss(str);
	std::string token;
	while(std::getline(ss, token, delimiter)) {
		res.push_back(token);
	}
	return res;
}

// adapted from https://stackoverflow.com/a/58048821
constexpr std::vector<std::string_view> splitStringView(const std::string_view str, const char delimeter = ',') {
	std::vector<std::string_view> res;
	int left = 0;
	int right = -1;
	for(int i = 0; i < static_cast<int>(str.size()); i++) {
		if(str[i] == delimeter) {
			left = right;
			right = i;
			int index = left + 1;
			int length = right - index;
			
			std::string_view column(str.data() + index, length);
			res.push_back(column);
		}
	}
	const std::string_view finalColumn(str.data() + right + 1, str.size() - right - 1);
	res.push_back(finalColumn);
	return res;
}
template <typename T>
constexpr T stringViewToNum(std::string_view str) {
	T value = 0;
	std::from_chars(str.data(), str.data() + str.size(), value);
	return value;
}
std::string textAfter(std::string str, std::string target) {
	size_t i = str.find(target);
	if(i == std::string::npos) {
		return "";
	}
	return str.substr(i + 1);
}
std::string getItem(std::vector<std::string> vec, size_t index) {
	if(index >= vec.size()) {
		return ""; // WHYYYY C++ DOESN'T HAVE THIS.....
	}
	return vec[index];
}
constexpr inline bool isNumber(char c) {
	return c >= '0' && c <= '9';
}
constexpr bool isNumber(std::string_view str) {
	for(char c : str) {
		if(!isNumber(c)) {
			return false;
		}
	}
	return true;
}
constexpr inline bool isLetter(char c) {
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
constexpr inline bool isUppercaseLetter(char c) {
	return c >= 'A' && c <= 'Z';
}
constexpr inline char capitaliseLetter(char c) {
	if(isUppercaseLetter(c)) {
		return c;
	}
	return c - ('a' - 'A');
}
constexpr inline std::string capitaliseString(std::string s) {
	std::string res(s.size(), '\0');
	std::transform(s.begin(), s.end(), res.begin(), [](auto c) {
		return capitaliseLetter(c);
	});
	return res;
}
constexpr inline std::string lowercaseString(std::string s) {
	std::string res(s.size(), '\0');
	std::transform(s.begin(), s.end(), res.begin(), [](auto c) {
		return std::tolower(c);
	});
	return res;
}
template <typename T>
T sign(T x) {
	return (x > 0) - (x < 0);
}

struct PieceSpecies {
	enum Type : uint16_t {
		None,
		#define Piece(code, promo, move) code,
			#include "pieces.inc"
		#undef Piece
		TotalCount
	};
};

inline constexpr bool isLionLikePiece(PieceSpecies::Type pieceSpecies) {
	return pieceSpecies == PieceSpecies::L || pieceSpecies == PieceSpecies::FFI || pieceSpecies == PieceSpecies::BS || pieceSpecies == PieceSpecies::H;
}
inline constexpr bool isRangeCapturingPiece(PieceSpecies::Type pieceSpecies) {
	return pieceSpecies == PieceSpecies::GG || pieceSpecies == PieceSpecies::VG || pieceSpecies == PieceSpecies::FLG || pieceSpecies == PieceSpecies::AG || pieceSpecies == PieceSpecies::FID || pieceSpecies == PieceSpecies::FCR;
}

struct Vec2 {
	int8_t x;
	int8_t y;
	
	static constexpr Vec2 fromIndex(size_t index) {
		return Vec2{ static_cast<int8_t>(index % 36), static_cast<int8_t>(index / 36) };
	} 
	constexpr size_t toIndex() const {
		return x + 36 * y;
	}
	constexpr Vec2 operator+(const Vec2 &other) const {
		return { static_cast<int8_t>(x + other.x), static_cast<int8_t>(y + other.y) };
	}
	constexpr Vec2 operator+(const int other) const {
		return { static_cast<int8_t>(x + other), static_cast<int8_t>(y + other) };
	}
	constexpr Vec2 operator-(const Vec2 &other) const {
		return { static_cast<int8_t>(x - other.x), static_cast<int8_t>(y - other.y) };
	}
	constexpr Vec2& operator+=(const Vec2 &other) {
		x += other.x;
		y += other.y;
		return *this;
	}
	constexpr bool operator==(const Vec2 &other) const noexcept {
		return x == other.x && y == other.y;
	}
};
struct Slide {
	Vec2 dir;
	uint8_t range;
};
// these could be simplified
struct CompoundMovementStep1 {
	std::vector<Slide> slides;
	bool canContinueAfterCapture;
};
struct CompoundMovementStep2 {
	std::vector<Slide> slides;
};
struct Movements {
	std::vector<Slide> slides;
	std::vector<Vec2> tripleSlashedArrowDirs;
	std::vector<std::pair<CompoundMovementStep1, CompoundMovementStep2>> compoundMoves;
};

struct PieceInfo {
	std::string name;
	PieceSpecies::Type promotion;
	Movements movements;
};
#define UNPAREN(...) __VA_ARGS__
const std::array<PieceInfo, 302> PieceTable = { {
	{ "[None]", PieceSpecies::None, {} },
	#define Piece(code, promo, move) { #code, PieceSpecies::promo, UNPAREN move },
		#include "pieces.inc"
	#undef Piece
} };

inline std::array<eval_t, 302> calculateBasePieceValues() {
	constexpr eval_t basePieceValue = 95;
	constexpr eval_t slideFactor = 5;
	constexpr eval_t rangeCapturingFactor = 10;
	
	std::array<eval_t, 302> values;
	values[0] = 0;
	for(int pieceSpecies = 1; pieceSpecies < 302; pieceSpecies++) {
		const Movements &movements = PieceTable[pieceSpecies].movements;
		eval_t value = basePieceValue; // centipawns
		for(const auto &slide : movements.slides) {
			bool isRangeCapturingSlide = isRangeCapturingPiece(static_cast<PieceSpecies::Type>(pieceSpecies)) && slide.range == 35;
			value += slide.range * slideFactor * (isRangeCapturingSlide? rangeCapturingFactor : 1) * std::pow(slide.dir.x * slide.dir.x + slide.dir.y * slide.dir.y, 0.25);
		}
		if(pieceSpecies == PieceSpecies::K || pieceSpecies == PieceSpecies::CP) {
			value += 100000;
		}
		values[pieceSpecies] = value;
	}
	return values;
}
const std::array<eval_t, 302> basePieceValues = calculateBasePieceValues();

inline constexpr frozen::unordered_map<frozen::string, uint16_t, PieceSpecies::TotalCount - 1> PieceSpeciesToId = {
	#define Piece(code, promo, move) { #code, PieceSpecies::code },
		#include "pieces.inc"
	#undef Piece
};
struct PieceIdsWrapper {
	inline constexpr uint16_t operator[](std::string_view species) const {
		return PieceSpeciesToId.at(species);
	}
};
inline constexpr PieceIdsWrapper pieceIds;

TranspositionTable transpositionTable;

std::array<uint32_t, MAX_DEPTH + 1> killerMoves1{};
std::array<uint32_t, MAX_DEPTH + 1> killerMoves2{};

bool atsiInitialised = false;
uint8_t player = 0;
float startingTime = 0;
float timeIncrement = 0;

constexpr inline uint32_t createMove(int8_t srcX, int8_t srcY, int8_t destX, int8_t destY, bool rangeCaptures = false, bool didMiddleStep = false, Vec2 middleStepPos = Vec2{ 0, 0 }) {
	// std::cout << "log Creating move: "<< std::to_string(srcX) << " " << std::to_string(srcY) << " " << std::to_string(destX) << " " << std::to_string(destY) << std::endl;
	return didMiddleStep << 29 | middleStepPos.x << 27 | middleStepPos.y << 25 | rangeCaptures << 24 | srcX << 18 | srcY << 12 | destX << 6 | destY;
}

// hacky wrapper around a uint16_t lol
struct Piece {
	uint16_t value;
	
	constexpr operator uint16_t() const {
		return value;
	}
	
	static inline constexpr Piece create(std::string_view species, bool canPromote, uint8_t owner) {
		uint16_t id = pieceIds[species];
		return create(id, canPromote, owner);
	}
	static inline constexpr Piece create(uint16_t id, bool canPromote, uint8_t owner) {
		// std::cout << "creating piece " << species << " = " << id << std::endl;
		return Piece { static_cast<uint16_t>((owner << 10) | (canPromote << 9) | id) };
	}
	
	constexpr inline uint8_t getOwner() const {
		return value >> 10;
	}
	inline uint16_t canPromote() const {
		return ((value >> 9) & 1) && PieceTable[getSpecies()].promotion != 0;
	}
	constexpr inline PieceSpecies::Type getSpecies() const {
		return static_cast<PieceSpecies::Type>(value & 0b111111111);
	}
	constexpr inline bool isRoyal() const {
		uint16_t pieceSpecies = getSpecies();
		return pieceSpecies == PieceSpecies::K || pieceSpecies == PieceSpecies::CP;
	}
	constexpr inline uint8_t getRank() const {
		if(isRoyal()) return 4;
		uint16_t pieceSpecies = getSpecies();
		if(pieceSpecies == PieceSpecies::GG) return 3;
		if(pieceSpecies == PieceSpecies::VG) return 2;
		if(pieceSpecies == PieceSpecies::FLG) return 1;
		if(pieceSpecies == PieceSpecies::AG) return 1;
		if(pieceSpecies == PieceSpecies::FID) return 1;
		if(pieceSpecies == PieceSpecies::FCR) return 1;
		return 0;
	}
	constexpr inline bool isInPromotionZone(int8_t y) const {
		return getOwner()? y > 24 : y < 11;
	}
};

constexpr eval_t evalPiece(Piece piece, int8_t x, int8_t y) {
	if(!piece) return 0;
	uint8_t owner = piece.getOwner();
	// flip x and y to be relative to the piece. 0 means to the left of the piece or the back of the board behind the piece.
	if(owner) {
		x = 35 - x;
	} else {
		y = 35 - y;
	}
	
	uint16_t species = piece.getSpecies();
	eval_t pieceBaseEval = basePieceValues[species];
	if(piece.canPromote()) {
		pieceBaseEval += basePieceValues[PieceTable[species].promotion] / 10;
	}
	eval_t horizontalCenterBonus = 18 - std::abs(x - 17.5f);
	eval_t verticalCenterBonus = std::min(y, static_cast<int8_t>(25));
	
	eval_t manhattenDistanceFromRoyals = std::abs(x - 17.5f) + y;
	eval_t protectingRoyalsBonus = manhattenDistanceFromRoyals <= 4? 2000 : 0;
	
	eval_t manhattenDistanceFromOpponentRoyals = std::abs(x - 17.5f) + 35 - y;
	eval_t attackingOpponentRoyalsBonus = manhattenDistanceFromOpponentRoyals < 8? 2000 + (7 - manhattenDistanceFromOpponentRoyals) * 200 : 0;
	
	return pieceBaseEval + horizontalCenterBonus + verticalCenterBonus + horizontalCenterBonus * verticalCenterBonus / 2 + protectingRoyalsBonus + attackingOpponentRoyalsBonus;
}

inline constexpr bool isPosWithinBounds(Vec2 pos) {
	return pos.x >= 0 && pos.x < 36 && pos.y >= 0 && pos.y < 36;
}
inline bool inPromotionZone(uint8_t owner, int8_t y) {
	return owner? y > 24 : y < 11;
}
inline constexpr Vec2 movementDirToBoardDir(Vec2 movementDir, uint8_t pieceOwner) {
	return pieceOwner? Vec2{ static_cast<int8_t>(-movementDir.x), movementDir.y } : Vec2{ movementDir.x, static_cast<int8_t>(-movementDir.y) };
}
inline constexpr Vec2 getMoveSrcPos(uint32_t move) {
	return Vec2{ static_cast<int8_t>((move >> 18) & 0b111111), static_cast<int8_t>((move >> 12) & 0b111111) };
}
inline constexpr Vec2 getMoveDestPos(uint32_t move) {
	return Vec2{ static_cast<int8_t>((move >> 6) & 0b111111), static_cast<int8_t>(move & 0b111111) };
}

// this file relies on Piece. I'm too lazy to add proper header files.
#include "zobristHashes.h"
#include "boardPosBitset.h"

class BidirectionalAttackMap {
private:
	std::array<BoardPosBitset, 1296> attackMap{};
	std::array<BoardPosBitset, 1296> reverseAttackMap{};
	inline constexpr void insertReverse(const size_t srcI, const size_t targetI) {
		reverseAttackMap[targetI].insert(srcI);
	}
	inline constexpr void eraseReverse(const size_t srcI, const size_t targetI) {
		reverseAttackMap[targetI].erase(srcI);
	}
public:
	inline constexpr const BoardPosBitset &getReverseAttacks(const Vec2 src) const {
		return reverseAttackMap[src.toIndex()];
	}
	inline constexpr void setAttacks(const Vec2 src, const BoardPosBitset &attacks) {
		size_t srcI = src.toIndex();
		BoardPosBitset &currentAttacks = attackMap[srcI];
		
		// find the changed attacks and update them accordingly
		currentAttacks.bitTransformForEach(attacks, [](uint64_t oldW, uint64_t newW) {
			// doing a single xor operation rather than two & ~ operations is a bit faster
			return oldW ^ newW;
		}, [this, srcI, &attacks](size_t targetI) {
			if(attacks.contains(targetI)) {
				insertReverse(srcI, targetI);
			} else {
				eraseReverse(srcI, targetI);
			}
		});
		
		currentAttacks = attacks;
	}
};

template <typename T, size_t Capacity>
class StaticVector {
private:
	std::array<T, Capacity> data;
	size_t currentSize = 0;
public:
	inline constexpr void push_back(const T &value) {
		if(currentSize >= Capacity) {
			throw std::runtime_error("StaticVector error: Capacity exceeded!");
		}
		data[currentSize++] = value;
	}
	inline constexpr void pop_back() {
		if(!currentSize) {
			throw std::runtime_error("StaticVector error: Cannot pop, already empty!");
		}
		currentSize--;
	}
	inline constexpr size_t size() {
		return currentSize;
	}
	inline constexpr T& back() {
		if(!currentSize) {
			throw std::runtime_error("StaticVector error: Cannot call back(), empty!");
		}
		return data[currentSize - 1];
	}
	inline constexpr const T& back() const {
		if(!currentSize) {
			throw std::runtime_error("StaticVector error: Cannot call back(), empty!");
		}
		return data[currentSize - 1];
	}
	inline constexpr T* begin() {
		return data.data();
	}
	inline constexpr T* end() {
		return data.data() + currentSize;
	}
};
struct UndoSquare {
	Vec2 pos;
	Piece oldPiece;
};

class GameState {
private:
	std::array<Piece, 1296> board{};
	uint16_t moveCounter = 0;
	BoardPosBitset occupancyBitset{};
	std::array<BoardPosBitset, 2> playerOccupancyBitsets{};
	BidirectionalAttackMap bidirectionalAttackMap;
	// Squares that need to recalculate their attack map and their moves
	BoardPosBitset squaresNeedingMoveRecalculation;
	std::array<std::array<std::vector<uint32_t>, 1296>, 2> movesPerSquarePerPlayer;
	// Keeps track of all the squares changed during a move, so that it can quickly unmake the move.
	StaticVector<StaticVector<UndoSquare, 36>, MAX_DEPTH> undoStack;
	std::vector<hash_t> positionHashHistory;
	int positionHashHistorySize = 0;
	
	void setSquare(int8_t x, int8_t y, Piece piece, bool regenerateMoves = true, bool saveToUndoStack = false) {
		uint8_t pieceOwner = piece.getOwner();
		Piece oldPiece = getSquare(x, y);
		if(piece == oldPiece) return; // idk why
		if(saveToUndoStack) {
			UndoSquare undoSquare = UndoSquare{ Vec2{ x, y }, oldPiece };
			undoStack.back().push_back(undoSquare);
		}
		if(oldPiece) {
			uint8_t oldPieceOwner = oldPiece.getOwner();
			absEval -= evalPiece(oldPiece, x, y) * (oldPieceOwner? -1 : 1);
			if(oldPiece.isRoyal()) {
				royalsLeft[oldPieceOwner]--;
			}
			playerOccupancyBitsets[oldPieceOwner].erase(Vec2{ x, y });
			hash ^= ZobristHashes::getHash(oldPiece, x, y);
			if(regenerateMoves) {
				squaresNeedingMoveRecalculation.insert(Vec2{ x, y });
				bidirectionalAttackMap.getReverseAttacks(Vec2{ x, y }).forEach([&](size_t i) {
					Vec2 pos = Vec2::fromIndex(i);
					Piece attackingPiece = getSquare(pos);
					if(!attackingPiece) {
						// IDK why this happens sometimes
						return;
					}
					if(isRangeCapturingPiece(attackingPiece.getSpecies())) {
						uint8_t oldPieceRank = oldPiece.getRank();
						uint8_t newPieceRank = piece.getRank();
						uint8_t attackingPieceRank = attackingPiece.getRank();
						// if the rank makes a difference to the range capturing piece which is attacking this square, it needs to recalculate the attack map - it will be attacking more/less squares. if the relative rank doesn't change however, it doesn't need to do anything :)
						if((oldPieceRank >= attackingPieceRank) != (newPieceRank >= attackingPieceRank)) {
							squaresNeedingMoveRecalculation.insert(i);
						}
						return;
					}
					// NOTE: Everything above this line is correct. If I insert this squre into `squaresNeedingMoveRecalculation`, it will function exactly as without this optimization. Therefore all issues must be below this line but still inside this function.
					
					uint8_t oldPieceOwner = oldPiece.getOwner();
					uint8_t newPieceOwner = piece.getOwner();
					if(oldPieceOwner == newPieceOwner) {
						// If a range-capturing piece lands on a piece from the same team, moves for regular pieces (i.e. pieces which don't range-capture and hence don't consider rank) don't change at all.
						// std::cout << std::format("At ({}, {}), {} was replaced by {}, both owned by player {}. Technically: {}, {}", x, y, PieceTable[oldPiece.getSpecies()].name, PieceTable[piece.getSpecies()].name, oldPieceOwner + 1, std::to_string(oldPiece), std::to_string(piece)) << std::endl;
						return;
					}
					
					uint8_t attackingPieceOwner = attackingPiece.getOwner();
					uint32_t move = createMove(pos.x, pos.y, x, y);
					auto &moves = movesPerSquarePerPlayer[attackingPieceOwner][i];
					if(attackingPieceOwner == newPieceOwner) {
						// This implies attackingPieceOwner != oldPieceOwner, and hence there previously existed a valid move to this location. However since it is being replaced by a piece from the same team, it is no longer a valid move location and the move has to be removed.
						auto it = std::find(moves.begin(), moves.end(), move);
						if(it == moves.end()) {
							// for some reason
							return;
							// std::cout << std::format("Piece {} at ({}, {}) doesn't have move to remove, to ({}, {}). Previously was player {}'s {}, now is player {}'s {}", PieceTable[attackingPiece.getSpecies()].name, pos.x, pos.y, x, y, oldPiece.getOwner() + 1, PieceTable[oldPiece.getSpecies()].name, piece.getOwner() + 1, PieceTable[piece.getSpecies()].name) << std::endl;
						} else {
							moves.erase(it);
						}
					} else {
						// Here it means that there wasn't a valid move to this location, but now that an enemy piece is here, it can now move to this location. Hence a move must be added.
						if(std::find(moves.begin(), moves.end(), move) != moves.end()) {
							std::cout << "Already has move" << std::endl;
						} else {
							// moves.push_back(move);
							squaresNeedingMoveRecalculation.insert(i);
						}
					}
				});
			}
		} else if(regenerateMoves) {
			squaresNeedingMoveRecalculation.insert(Vec2{ x, y });
			squaresNeedingMoveRecalculation |= bidirectionalAttackMap.getReverseAttacks(Vec2{ x, y });
		}
		// handle promotion here
		if(piece.canPromote() && piece.isInPromotionZone(y)) {
			PieceSpecies::Type promotedSpecies = PieceTable[piece.getSpecies()].promotion;
			if(promotedSpecies) {
				piece = Piece::create(promotedSpecies, 0, pieceOwner);
			}
		}
		board[Vec2{ x, y }.toIndex()] = piece;
		absEval += evalPiece(piece, x, y) * (pieceOwner? -1 : 1);
		if(piece.isRoyal()) {
			royalsLeft[pieceOwner]++;
		}
		hash ^= ZobristHashes::getHash(piece, x, y);
		occupancyBitset.insert(Vec2{ x, y });
		playerOccupancyBitsets[pieceOwner].insert(Vec2{ x, y });
	}
	void clearSquare(int8_t x, int8_t y, bool regenerateMoves = true, bool saveToUndoStack = false) {
		Piece oldPiece = getSquare(x, y);
		if(oldPiece) {
			if(saveToUndoStack) {
				UndoSquare undoSquare = UndoSquare{ Vec2{ x, y }, oldPiece };
				undoStack.back().push_back(undoSquare);
			}
			uint8_t oldPieceOwner = oldPiece.getOwner();
			absEval -= evalPiece(oldPiece, x, y) * (oldPieceOwner? -1 : 1);
			if(oldPiece.isRoyal()) {
				royalsLeft[oldPieceOwner]--;
			}
			board[Vec2{ x, y }.toIndex()] = Piece{ 0 };
			occupancyBitset.erase(Vec2{ x, y });
			playerOccupancyBitsets[oldPieceOwner].erase(Vec2{ x, y });
			if(regenerateMoves) {
				squaresNeedingMoveRecalculation.insert(Vec2{ x, y });
				squaresNeedingMoveRecalculation |= bidirectionalAttackMap.getReverseAttacks(Vec2{ x, y });
			}
			hash ^= ZobristHashes::getHash(oldPiece, x, y);
		}
	}
public:
	uint8_t currentPlayer = 0;
	std::array<int8_t, 2> royalsLeft{};
	// this could be made unsigned, but we'll be converting it to signed all the time
	eval_t absEval = 0;
	hash_t hash = 0;
	uint16_t age = 0;
	
	GameState() {
		// random guess at an upper bound for how many moves will go between captures. regardless it won't affect memory much at all.
		positionHashHistory.reserve(50);
	}
	
	std::string toString() const {
		std::ostringstream oss;
		oss << "Player: " << static_cast<int>(currentPlayer) << "\nBoard: [";
		for(size_t i = 0; i < 1296; i++) {
			oss << board[i];
			if(i != 1295) oss << ", ";
		}
		oss << "]\nZobrist hash: " << hash;
		return oss.str();
	}
	eval_t eval() const {
		if(currentPlayer) {
			return absEval * -1;
		}
		return absEval;
	}
	// Checks if there is a draw (by repetition), by checking if the most current game state hash is repeated 3 times earlier.
	bool isDraw() const {
		// not 3 like in Western chess
		uint8_t repetitionsToDraw = 4;
		// casting to an int avoids unsigned integer underflow at the start of the for loop
		if(positionHashHistorySize < repetitionsToDraw * 2 - 1) {
			return false;
		}
		
		for(int i = positionHashHistorySize - 3; i >= 0; i -= 2) {
			if(positionHashHistory[i] == hash && !--repetitionsToDraw) {
				return true;
			}
		}
		return false;
	}
	
	inline constexpr Piece getSquare(int8_t x, int8_t y) const {
		return board[Vec2{ x, y }.toIndex()];
	}
	inline constexpr Piece getSquare(Vec2 pos) const {
		return board[pos.toIndex()];
	}
	inline constexpr bool playerHasPieceAtSquare(uint8_t player, Vec2 pos) const {
		return playerOccupancyBitsets[player].contains(pos);
	}
	void makeMove(uint32_t move, bool regenerateMoves = true, bool saveState = false) {
		if(saveState) {
			undoStack.push_back(StaticVector<UndoSquare, 36>{});
		}
		// move layout:
		// 6 bits: src x, 6 bits: src y
		// 6 bits: dst x, 6 bits: dst y
		// for range capturing pieces, 1 bit extra: capture all
		// for lion-like pieces, extra 1 bit: does middle step, extra 2 bits: middle step x offset, extra 2 bits: middle step y offset
		
		auto [srcX, srcY] = getMoveSrcPos(move);
		Piece piece = getSquare(srcX, srcY);
		auto [destX, destY] = getMoveDestPos(move);
		uint8_t pieceOwner = piece.getOwner();
		bool middleStepShouldPromote = false;
		bool ageShouldChange = false;
		if(move >> 24) {
			// range capturing move
			int8_t dirX = sign(destX - srcX);
			int8_t dirY = sign(destY - srcY);
			int8_t x = srcX + dirX;
			int8_t y = srcY + dirY;
			while(x != destX || y != destY) {
				if(!saveState && !ageShouldChange && occupancyBitset.contains(Vec2{ x, y })) {
					ageShouldChange = true;
				}
				clearSquare(x, y, regenerateMoves, saveState);
				x += dirX;
				y += dirY;
			}
		} else if(move >> 29) {
			// lion-like pieces. not yet implemented so this should never trigger.
			std::cout << "log Does middle step: " << std::to_string(move) << std::endl;
			int8_t middleStepX = ((move >> 27) & 0b11) - 1;
			int8_t middleStepY = ((move >> 25) & 0b11) - 1;
			int8_t middleX = srcX + middleStepX;
			int8_t middleY = srcY + middleStepY;
			clearSquare(middleX, middleY, regenerateMoves, saveState);
			if(!saveState && occupancyBitset.contains(Vec2{ middleX, middleY })) {
				ageShouldChange = true;
			}
			middleStepShouldPromote = inPromotionZone(pieceOwner, middleY);
		}
		
		bool shouldPromote = middleStepShouldPromote || inPromotionZone(pieceOwner, destY);
		if(shouldPromote && piece.canPromote()) {
			piece = Piece::create(PieceTable[piece.getSpecies()].promotion, 0, pieceOwner);
			if(!saveState) {
				ageShouldChange = true;
			}
		}
		
		// std::cout << std::format("Moving piece {} from ({}, {}) to ({}, {}); capturing {}", PieceTable[piece.getSpecies()].name, srcX, srcY, destX, destY, PieceTable[getSquare(destX, destY).getSpecies()].name) << std::endl;
		clearSquare(srcX, srcY, regenerateMoves, saveState);
		if(!saveState && !ageShouldChange && occupancyBitset.contains(Vec2{ destX, destY })) {
			ageShouldChange = true;
		}
		setSquare(destX, destY, piece, regenerateMoves, saveState);
		currentPlayer = 1 - currentPlayer;
		hash = ~hash;
		if(regenerateMoves) {
			generateMoves();
		}
		// captures are irreversible moves and hence the "age" of the game must be incremented
		if(ageShouldChange) {
			age++;
			positionHashHistory.clear();
			positionHashHistorySize = 0;
		}
		positionHashHistory.push_back(hash);
		positionHashHistorySize++;
		
		if(!saveState) {
			moveCounter++;
		}
	}
	void unmakeMove(bool regenerateMoves = true) {
		StaticVector<UndoSquare, 36> &undoSquares = undoStack.back();
		for(const UndoSquare &undoSquare : undoSquares) {
			if(undoSquare.oldPiece) {
				setSquare(undoSquare.pos.x, undoSquare.pos.y, undoSquare.oldPiece, regenerateMoves);
			} else {
				clearSquare(undoSquare.pos.x, undoSquare.pos.y, regenerateMoves);
			}
		}
		undoStack.pop_back();
		if(regenerateMoves) {
			generateMoves();
		}
		currentPlayer = 1 - currentPlayer;
		hash = ~hash;
		positionHashHistory.pop_back();
		positionHashHistorySize--;
	}
	
	void generateMoves() {
		squaresNeedingMoveRecalculation.forEachAndClear([this](size_t i) {
			movesPerSquarePerPlayer[0][i].clear();
			movesPerSquarePerPlayer[1][i].clear();
			Vec2 srcVec = Vec2::fromIndex(i);
			auto [x, y] = srcVec;
			Piece piece = getSquare(x, y);
			auto pieceOwner = piece.getOwner();
			bool pieceIsRangeCapturing = isRangeCapturingPiece(piece.getSpecies());
			auto pieceRank = piece.getRank();
			if(piece) {
				auto &movements = PieceTable[piece.getSpecies()].movements;
				BoardPosBitset attackingSquares;
				BoardPosBitset validMoveLocations;
				BoardPosBitset rangeCapturingMoveLocations;
				for(const auto &slide : movements.slides) {
					bool slideIsRangeCapturing = pieceIsRangeCapturing && slide.range == 35;
					Vec2 slideDir = movementDirToBoardDir(slide.dir, pieceOwner);
					uint8_t distToEdgeOfBoard = std::min(slideDir.x? slideDir.x > 0? (35 - x) / slideDir.x : -x / slideDir.x : 35, slideDir.y? slideDir.y > 0? (35 - y) / slideDir.y : -y / slideDir.y : 35);
					uint8_t maxDist = std::min(slide.range, distToEdgeOfBoard);
					Vec2 target = srcVec;
					for(uint8_t dist = 0; dist < maxDist; dist++) {
						target += slideDir;
						attackingSquares.insert(target);
						if(occupancyBitset.contains(target)) {
							bool isBlocked = slideIsRangeCapturing? getSquare(target.x, target.y).getRank() >= pieceRank : playerOccupancyBitsets[pieceOwner].contains(target);
							if(isBlocked) {
								break;
							}
							if(slideIsRangeCapturing) {
								rangeCapturingMoveLocations.insert(target);
							} else {
								validMoveLocations.insert(target);
								break;
							}
						} else {
							if(slideIsRangeCapturing) {
								rangeCapturingMoveLocations.insert(target);
							} else {
								validMoveLocations.insert(target);
							}
						}
					}
				}
				for(const Vec2 &tripleSlashedArrowDir : movements.tripleSlashedArrowDirs) {
					Vec2 dir = movementDirToBoardDir(tripleSlashedArrowDir, pieceOwner);
					uint8_t jumpsRemaining = 4; // it will be decremented first before being checked, so this will make it trigger on the fourth jump
					uint8_t distToEdgeOfBoard = std::min(dir.x? dir.x > 0? 35 - x : x : 35, dir.y? dir.y > 0? 35 - y : y : 35);
					Vec2 target = srcVec;
					for(uint8_t dist = 0; dist < distToEdgeOfBoard; dist++) {
						target += dir;
						attackingSquares.insert(target);
						if(occupancyBitset.contains(target)) {
							bool isBlocked = playerOccupancyBitsets[pieceOwner].contains(target);
							if(!isBlocked) {
								validMoveLocations.insert(target);
							}
							if(!--jumpsRemaining) {
								break;
							}
						} else {
							validMoveLocations.insert(target);
						}
					}
				}
				// TODO: implement compound moves!!
				// for(const auto &compoundMove : movements.compoundMoves) {
					
				// }
				bidirectionalAttackMap.setAttacks(srcVec, attackingSquares);
				rangeCapturingMoveLocations.transformInto(movesPerSquarePerPlayer[pieceOwner][i], [x, y](const Vec2 &target) {
					return createMove(x, y, target.x, target.y, true);
				});
				validMoveLocations.transformInto(movesPerSquarePerPlayer[pieceOwner][i], [x, y](const Vec2 &target) {
					return createMove(x, y, target.x, target.y);
				});
			}
		});
	}
	std::vector<uint32_t> getAllMovesForPlayer(uint8_t player) {
		size_t totalSpace = 0;
		for(const std::vector<uint32_t> &moves : movesPerSquarePerPlayer[player]) {
			totalSpace += moves.size();
		}
		// std::cout << "log Space for " << totalSpace << " moves"<<std::endl;
		
		std::vector<uint32_t> allMoves;
		allMoves.reserve(totalSpace);
		for(const std::vector<uint32_t> &moves : movesPerSquarePerPlayer[player]) {
			allMoves.insert(allMoves.end(), moves.begin(), moves.end());
		}
		
		return allMoves;
	}
	
	std::string toTsfen() {
		std::string tsfen = "";
		for(int y = 0; y < 36; y++) {
			for(int x = 0; x < 36; x++) {
				Piece piece = getSquare(x, y);
				if(piece) {
					std::string pieceName = PieceTable[piece.getSpecies()].name;
					tsfen += piece.getOwner()? pieceName : lowercaseString(pieceName);
				} else {
					tsfen += "1";
				}
				if(x < 35) {
					tsfen += ",";
				}
			}
			if(y < 35) {
				tsfen += "/";
			}
		}
		tsfen += " " + std::to_string(moveCounter);
		return tsfen;
	}
	void logTsfen() {
		std::cout << "log TSFEN: " << toTsfen() << std::endl;
	}
	
	static GameState fromTsfen(std::string_view tsfen) {
		auto fields = splitStringView(tsfen, ' ');
		GameState gameState;
		
		auto rows = splitStringView(fields[0], '/');
		int y = 0;
		for(auto row : rows) {
			int x = 0;
			auto cells = splitStringView(row, ',');
			for(auto cell : cells) {
				if(isNumber(cell)) {
					// empty spaces
					int emptySquares = stringViewToNum<uint8_t>(cell);
					x += emptySquares;
					continue;
				}
				std::string pieceSpecies;
				size_t i = 0;
				while(i < cell.size() && isLetter(cell[i])) {
					pieceSpecies += cell[i++];
				}
				int count = 1;
				if(i < cell.size()) {
					count = stringViewToNum<uint8_t>(cell.substr(i));
				}
				bool isSecondPlayer = isUppercaseLetter(pieceSpecies[0]);
				pieceSpecies = capitaliseString(pieceSpecies);
				for(int j = 0; j < count; j++) {
					gameState.setSquare(x++, y, Piece::create(pieceSpecies, true, isSecondPlayer));
				}
			}
			y++;
		}
		uint16_t moveCounter = stringViewToNum<uint16_t>(fields[1]);
		gameState.moveCounter = moveCounter;
		gameState.currentPlayer = moveCounter % 2;
		gameState.generateMoves();
		
		return gameState;
	}
};

inline constexpr std::string_view INITIAL_TSFEN = {
	#include "initialTsfen.inc"
};
inline GameState initialGameState = GameState::fromTsfen(INITIAL_TSFEN);

std::string stringifyBoardPos(int8_t x, int8_t y) {
	int8_t file = 36 - x;
	std::string rank(1, 'a' + (y % 26)); // I love c++
	if(y >= 26) {
		rank += rank;
	}
	return std::to_string(file) + rank;
}
Vec2 parseBoardPos(std::string boardPos) {
	int8_t file = isNumber(boardPos.at(1))? (boardPos.at(0) - '0') * 10 + (boardPos.at(1) - '0') : boardPos.at(0) - '0';
	int8_t x = 36 - file;
	std::string rank = boardPos.substr(1 + isNumber(boardPos.at(1)));
	int8_t y = rank.at(0) - 'a' + (rank.length() > 1) * 26;
	return { x, y };
}
std::string stringifyMove(GameState &gameState, uint32_t move) {
	auto [srcX, srcY] = getMoveSrcPos(move);
	auto [destX, destY] = getMoveDestPos(move);
	
	// std::cout << "log stringifyMove = " << std::to_string(srcX) << " " << std::to_string(srcY) << " " << std::to_string(destX) << " " << std::to_string(destY) << std::endl;
	std::string moveStr = stringifyBoardPos(srcX, srcY) + " " + stringifyBoardPos(destX, destY);
	
	Piece piece = gameState.getSquare(srcX, srcY);
	PieceSpecies::Type pieceSpecies = piece.getSpecies();
	if(isLionLikePiece(pieceSpecies)) {
		int8_t doesMiddleStep = move >> 28;
		if(doesMiddleStep) {
			int8_t middleStepX = ((move >> 26) & 0b11) - 1;
			int8_t middleX = srcX + middleStepX;
			int8_t middleStepY = ((move >> 24) & 0b11) - 1;
			int8_t middleY = srcY + middleStepY;
			moveStr += " " + stringifyBoardPos(middleX, middleY);
		}
	}
	
	return moveStr;
}
uint32_t parseMove(GameState &gameState, std::vector<std::string> arguments) {
	auto srcPos = parseBoardPos(arguments[1]);
	auto destPos = parseBoardPos(arguments[2]);
	Piece movingPiece = gameState.getSquare(srcPos.x, srcPos.y);
	bool isRangeCapturingMove = false;
	if(isRangeCapturingPiece(movingPiece.getSpecies())) {
		auto &slides = PieceTable[movingPiece.getSpecies()].movements.slides;
		Vec2 deltaPos = destPos - srcPos;
		Vec2 slideDir = Vec2{ sign(deltaPos.x), sign(deltaPos.y) };
		for(const auto &slide : slides) {
			if(slide.range == 35 && movementDirToBoardDir(slide.dir, movingPiece.getOwner()) == slideDir) {
				isRangeCapturingMove = true;
			}
		}
	}
	bool didLionStep = false;
	Vec2 lionStepPos{0, 0};
	// each "opmove" command will have at least 5 arguments: "opmove [src] [dest] [time] [optime]" so more than 5 means there's a middle step
	if(arguments.size() > 5) {
		// must be a lion-like piece
		didLionStep = true;
		std::cout << "log LION STEP HAHHAHAHAHA" << std::endl;
		Vec2 stepPos = parseBoardPos(arguments[3]);
		Vec2 stepDeltaPos = stepPos - srcPos;
		lionStepPos = stepDeltaPos + 1; // 1 is subtracted in makeMove() so the range [-1, 1] is mapped to [0, 2] to work better as a 2-bit int
	}
	return createMove(srcPos.x, srcPos.y, destPos.x, destPos.y, isRangeCapturingMove, didLionStep, lionStepPos);
}

#define MAX_SCORE 100000000

uint64_t totalNodesSearched = 0;
uint32_t nodesSearched = 0;

eval_t search(GameState &gameState, eval_t alpha, eval_t beta, depth_t depth) {
	nodesSearched++;
	// checking royal pieces only needs to be done for the current player - no point checking the player who just moved
	if(gameState.royalsLeft[gameState.currentPlayer] == 0) {
		return -MAX_SCORE;
	}
	if(depth == 0) {
		return gameState.eval();
	}
	if(gameState.isDraw()) {
		return 0;
	}
	TranspositionTableEntry* ttEntry = transpositionTable.get(gameState.hash);
	// this makes it look at more nodes somehow... and there are hash collisions still... so I don't really trust it
	
	// if(ttEntry && ttEntry->depth >= depth) {
	// 	if(ttEntry->nodeType == NodeType::EXACT || (ttEntry->nodeType == NodeType::LOWER_BOUND && ttEntry->eval >= beta) || (ttEntry->nodeType == NodeType::UPPER_BOUND && ttEntry->eval <= alpha)) {
	// 		return ttEntry->eval;
	// 	}
	// }
	std::vector<uint32_t> moves = gameState.getAllMovesForPlayer(gameState.currentPlayer);
	// std::cout << "log Found " << moves.size() << " moves" << std::endl;
	bool hasTtMove = ttEntry;
	uint32_t ttMove = hasTtMove? ttEntry->bestMove : 0;
	for(size_t i = 0; i < moves.size(); i++) {
		if(moves[i] == ttMove) {
			moves[i] = moves[0];
			moves[0] = ttMove;
		} else if(moves[i] == killerMoves1[depth]) {
			moves[i] = moves[hasTtMove];
			moves[hasTtMove] = killerMoves1[depth];
		} else if(moves[i] == killerMoves2[depth]) {
			moves[i] = moves[hasTtMove + 1];
			moves[hasTtMove + 1] = killerMoves2[depth];
		}
	}
	
	eval_t originalAlpha = alpha;
	// Must have -1 at the end so even if all moves lead to loss (and hence the best is -MAX_SCORE), it will store a move rather than nothing at all.
	eval_t bestScore = -MAX_SCORE - 1;
	uint32_t bestMove = 0;
	bool foundPvNode = 0;
	bool regenerateMoves = depth > 1;
	for(uint32_t move : moves) {
		gameState.makeMove(move, regenerateMoves, true);
		eval_t score;
		if(!foundPvNode) {
			score = -search(gameState, -beta, -alpha, depth - 1);
		} else {
			score = -search(gameState, -alpha - 1, -alpha, depth - 1);
			if(score > alpha && score < beta) {
				score = -search(gameState, -beta, -alpha, depth - 1);
			}
		}
		gameState.unmakeMove(regenerateMoves);
		if(score > bestScore) {
			bestScore = score;
			bestMove = move;
			if(bestScore > alpha) {
				alpha = bestScore;
				if(alpha >= beta) {
					if(bestMove != killerMoves1[depth]) {
						killerMoves2[depth] = killerMoves1[depth];
						killerMoves1[depth] = bestMove;
					}
					break;
				}
				foundPvNode = true;
			}
		}
	}
	NodeType nodeType = bestScore <= originalAlpha? NodeType::UPPER_BOUND : bestScore >= beta? NodeType::LOWER_BOUND : NodeType::EXACT;
	transpositionTable.put(gameState.hash, bestMove, depth, gameState.age, bestScore, nodeType);
	return bestScore;
}
uint32_t perft(GameState &gameState, depth_t depth) {
	// checking royal pieces only needs to be done for the current player - no point checking the player who just moved
	if(gameState.royalsLeft[gameState.currentPlayer] == 0) {
		return 1;
	}
	if(depth == 0) {
		return 1;
	}
	if(gameState.isDraw()) {
		return 1;
	}
	uint32_t nodesSearched = 0;
	std::vector<uint32_t> moves = gameState.getAllMovesForPlayer(gameState.currentPlayer);
	bool regenerateMoves = depth > 1;
	for(uint32_t move : moves) {
		gameState.makeMove(move, regenerateMoves, true);
		nodesSearched += perft(gameState, depth - 1);
		gameState.unmakeMove(regenerateMoves);
	}
	return nodesSearched;
}
// WARNING: This will corrupt the transposition table!! Testing purposes only!
uint32_t perftTt(GameState &gameState, depth_t depth) {
	TranspositionTableEntry *ttEntry = transpositionTable.get(gameState.hash);
	if(ttEntry && ttEntry->depth == 19) return ttEntry->bestMove;
	
	if(gameState.royalsLeft[gameState.currentPlayer] == 0) {
		transpositionTable.put(gameState.hash, 1, 19, 0, 0, NodeType::EXACT);
		return 1;
	}
	if(depth == 0) {
		transpositionTable.put(gameState.hash, 1, 19, 0, 0, NodeType::EXACT);
		return 1;
	}
	if(gameState.isDraw()) {
		transpositionTable.put(gameState.hash, 1, 19, 0, 0, NodeType::EXACT);
		return 1;
	}
	uint32_t nodesSearched = 0;
	std::vector<uint32_t> moves = gameState.getAllMovesForPlayer(gameState.currentPlayer);
	bool regenerateMoves = depth > 1;
	for(uint32_t move : moves) {
		gameState.makeMove(move, regenerateMoves, true);
		nodesSearched += perftTt(gameState, depth - 1);
		gameState.unmakeMove(regenerateMoves);
	}
	transpositionTable.put(gameState.hash, nodesSearched, 19, 0, 0, NodeType::EXACT);
	return nodesSearched;
}

uint32_t findBestMove(GameState &gameState, depth_t maxDepth, float timeToMove = std::numeric_limits<float>::infinity()) {
	assert(maxDepth <= MAX_DEPTH);
	using clock = std::chrono::steady_clock;
	auto startTime = clock::now();
	auto stopTime = startTime + std::chrono::duration<float>(timeToMove);
	
	eval_t eval;
	for(depth_t depth = 1; depth <= maxDepth; depth++) {
		totalNodesSearched += nodesSearched;
		nodesSearched = 0;
		eval = search(gameState, -MAX_SCORE, MAX_SCORE, depth);
		std::cout << "log Score for us after depth " << std::to_string(depth) << " and " << nodesSearched << " nodes: " << eval << std::endl;
		if(clock::now() >= stopTime) {
			break;
		}
	}
	
	TranspositionTableEntry* ttEntry = transpositionTable.get(gameState.hash);
	if(!ttEntry) {
		std::cout << "log Fatal error: No TT entry found for gameState hash " << gameState.hash << std::endl;
		throw std::runtime_error("No TT entry found!!!!!!");
	}
	return ttEntry->bestMove;
}
void outputTtSize() {
	std::cout << "Transposition table size: " << transpositionTable.size << " / " << TRANSPOSITION_TABLE_SIZE << std::endl;
}
void makeBestMove(GameState &gameState) {
	const float timeToMove = timeIncrement + startingTime / ESTIMATED_GAME_LENGTH;
	// std::cout << "log Time to move: " << std::to_string(timeToMove) << " seconds" << std::endl;
	
	uint32_t bestMove = findBestMove(gameState, MAX_DEPTH, timeToMove);
	std::cout << "move " << stringifyMove(gameState, bestMove) << std::endl;
	gameState.makeMove(bestMove);
	std::cout << "eval " << gameState.absEval << std::endl;
	std::cout << "log ";
	outputTtSize();
}
struct FunctionTiming {
	uint32_t nodesSearched;
	std::chrono::milliseconds duration;
	std::string nodesPerSecond;
};
template <std::invocable F>
requires std::convertible_to<std::invoke_result_t<F>, uint32_t>
inline FunctionTiming timeFunction(F func) {
	using clock = std::chrono::steady_clock;
	auto start = clock::now();
	uint32_t nodesSearched = func();
	auto end = clock::now();
	auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
	return { nodesSearched, std::chrono::duration_cast<std::chrono::milliseconds>(elapsed), std::format("{} n/s", nodesSearched / (static_cast<float>(elapsed.count()) / 1000000.0f)) };
}

int main() {
	GameState gameState = initialGameState;
	
	std::string line;
	while(std::getline(std::cin, line)) {
		std::vector<std::string> arguments = splitString(line, ' ');
		if(!arguments.size()) continue;
		std::string command = getItem(arguments, 0);
		// std::cout << atsiInitialised << std::endl;
		if(atsiInitialised) {
			if(command == "opmove") {
				uint32_t opponentMove = parseMove(gameState, arguments);
				gameState.makeMove(opponentMove);
				if(gameState.royalsLeft[0] && gameState.royalsLeft[1]) {
					makeBestMove(gameState);
				}
			} else if(command == "identify") {
				std::cout << "info \"" << ENGINE_NAME << "\" \"" << ENGINE_DESC << "\" \"" << ENGINE_AUTHOR << "\" \"" << ENGINE_VERSION << "\"" << std::endl;
			} else if(command == "time") {
				startingTime = std::stof(getItem(arguments, 1));
				if(arguments.size() >= 3) {
					timeIncrement = std::stof(getItem(arguments, 2));
				} else {
					timeIncrement = 0;
				}
			} else if(command == "player") {
				player = std::stoi(getItem(arguments, 1));
			} else if(command == "startgame") {
				std::string initialPos = textAfter(line, " ");
				using clock = std::chrono::steady_clock;
				auto start = clock::now();
				if(initialPos == "initial") {
					// std::cout << "using initial tsfen!" << std::endl;
					gameState = initialGameState;
				} else {
					// std::cout << "got tsfen: '" << initialPos << "'" << std::endl;
					gameState = GameState::fromTsfen(initialPos);
					// std::cout << "parsed tsfen!" << std::endl;
				}
				auto end = clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
				// std::cout << "Elapsed: " << elapsed.count() << " µs\n";
				// std::cout << gameState.toString() << std::endl;
				if(player == 0) {
					makeBestMove(gameState);
				}
			} else if(command == "win" || command == "win" || command == "draw") {
				gameState.logTsfen();
				std::cout << std::format("log Found {} nodes in total", totalNodesSearched) << std::endl;
			} else if(command == "setparam") {
				// parameters not yet implemented, this command should never be received
			} else if(command == "quit") {
				atsiInitialised = false;
			} else if(command == "tsfen") {
				gameState.logTsfen();
			}
		} else {
			if(command == "atsiinit") {
				std::string version = getItem(arguments, 1);
				if(version == "v0") {
					std::cout << "atsiok" << std::endl;
					atsiInitialised = true;
				} else {
					std::cerr << "Unknown ATSI version: " << version << std::endl;
				}
			} else if(command == "perft") {
				depth_t maxDepth = std::stoi(getItem(arguments, 1));
				if(maxDepth > MAX_DEPTH) {
					std::cout << "Depth is greater than MAX_DEPTH = " << MAX_DEPTH << "; will only go to depth " << MAX_DEPTH << std::endl;
					maxDepth = MAX_DEPTH;
				}
				for(depth_t depth = 1; depth <= maxDepth; depth++) {
					FunctionTiming timing = timeFunction([&]() {
						return perft(gameState, depth);
					});
					std::cout << std::format("Depth {}: Found {} nodes in {} ({})", depth, timing.nodesSearched, timing.duration, timing.nodesPerSecond) << std::endl;
				}
			} else if(command == "perfttt") {
				depth_t depth = std::stoi(getItem(arguments, 1));
				if(depth > MAX_DEPTH) {
					std::cout << "Depth is greater than MAX_DEPTH = " << MAX_DEPTH << "; will only go to depth " << MAX_DEPTH << std::endl;
					depth = MAX_DEPTH;
				}
				FunctionTiming timing = timeFunction([&]() {
					return perftTt(gameState, depth);
				});
				std::cout << std::format("Depth {}: Found {} nodes in {} ({})", depth, timing.nodesSearched, timing.duration, timing.nodesPerSecond) << std::endl;
			} else if(command == "search") {
				depth_t depth = std::stoi(getItem(arguments, 1));
				eval_t eval;
				nodesSearched = 0;
				FunctionTiming timing = timeFunction([&]() {
					eval = search(gameState, -MAX_SCORE, MAX_SCORE, depth);
					return nodesSearched;
				});
				std::cout << std::format("Depth {}: Found {} nodes; Eval = {} in {} ({})", depth, nodesSearched, eval, timing.duration, timing.nodesPerSecond) << std::endl;
			} else if(command == "bestmove") {
				depth_t depth = std::stoi(getItem(arguments, 1));
				nodesSearched = 0;
				uint32_t bestMove = 0;
				FunctionTiming timing = timeFunction([&]() {
					bestMove = findBestMove(gameState, depth);
					return nodesSearched;
				});
				gameState.makeMove(bestMove, true, true);
				std::cout << std::format("Depth {}: Best move = {}; Current eval = {}; found {} nodes in {} ({})", depth, stringifyMove(gameState, bestMove), gameState.absEval, nodesSearched, timing.duration, timing.nodesPerSecond) << std::endl;
				gameState.unmakeMove();
			} else if(command == "ttsize") {
				outputTtSize();
			} else if(command == "pieces") {
				for(size_t i = 0; i < PieceTable.size(); i++) {
					std::cout << std::format("{}: {}; value: {}; promotion: {}", i, PieceTable[i].name, basePieceValues[i], PieceTable[PieceTable[i].promotion].name) << std::endl;
				}
			} else if(command == "values") {
				for(size_t i = 0; i < PieceTable.size(); i++) {
					std::cout << PieceTable[i].name << "," << basePieceValues[i] << std::endl;
				}
			}
		}
	}
	
	return 0;
}