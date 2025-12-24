#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <bit>

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

#include "zobristHashes.h"
#include "transpositionTable.h"

inline constexpr depth_t MAX_DEPTH = 4;
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
	int8_t range;
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
	PieceSpecies::Type promotion;
	Movements movements;
};
#define UNPAREN(...) __VA_ARGS__
PieceInfo PieceTable[] = {
	{ PieceSpecies::None, {} },
	#define Piece(code, promo, move) { PieceSpecies::promo, UNPAREN move },
		#include "pieces.inc"
	#undef Piece
};

inline std::array<eval_t, 302> calculateBasePieceValues() {
	std::array<eval_t, 302> values;
	values[0] = 0;
	for(int pieceSpecies = 1; pieceSpecies < 302; pieceSpecies++) {
		Movements &movements = PieceTable[pieceSpecies].movements;
		eval_t value = 95; // centipawns
		for(const auto &slide : movements.slides) {
			value += slide.range * (isRangeCapturingPiece(static_cast<PieceSpecies::Type>(pieceSpecies)) && slide.range == 35? 50 : 5);
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
bool inGame = false; // i sthis needed?
uint8_t player = 0;
float startingTime = 0;
float timeIncrement = 0;

constexpr inline uint32_t createMove(int8_t srcX, int8_t srcY, int8_t destX, int8_t destY, bool rangeCaptures = false, bool didLionStep = false, Vec2 lionStepPos = Vec2{ 0, 0 }) {
	// std::cout << "log Creating move: "<< std::to_string(srcX) << " " << std::to_string(srcY) << " " << std::to_string(destX) << " " << std::to_string(destY) << std::endl;
	return didLionStep << 28 | lionStepPos.x << 26 | lionStepPos.y << 24 | rangeCaptures << 24 | srcX << 18 | srcY << 12 | destX << 6 | destY;
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
	constexpr inline bool getRank() const {
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
	
	return pieceBaseEval + horizontalCenterBonus + verticalCenterBonus + horizontalCenterBonus * verticalCenterBonus / 2;
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
	constexpr void bitTransformForEach(const BoardPosBitset &other, F &&bitTransformFunction, J &&forEachFunction) {
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
	inline constexpr BoardPosBitset getReverseAttacks(const Vec2 src) {
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
	inline constexpr T operator[](size_t index) {
		if(index >= currentSize) {
			throw std::runtime_error("StaticVector error: Out of bounds!");
		}
		return data[index];
	}
};
struct UndoSquare {
	Vec2 pos;
	Piece oldPiece;
};

class GameState {
private:
	std::array<Piece, 1296> board{};
	BoardPosBitset occupancyBitset{};
	std::array<BoardPosBitset, 2> playerOccupancyBitsets{};
	BidirectionalAttackMap bidirectionalAttackMap;
	BoardPosBitset squaresNeedingMoveRecalculation;
	std::array<std::array<std::vector<uint32_t>, 1296>, 2> movesPerSquarePerPlayer;
	// Keeps track of all the squares changed during a move, so that it can quickly unmake the move.
	StaticVector<StaticVector<UndoSquare, 36>, MAX_DEPTH> undoStack;
	
	void setSquare(int8_t x, int8_t y, Piece piece, bool regenerateMoves = true, bool saveToUndoStack = false) {
		Piece oldPiece = getSquare(x, y);
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
			hash ^= ZobristHashes::getHash(oldPiece.getSpecies(), x, y);
		}
		// handle promotion here
		if(piece.canPromote() && piece.isInPromotionZone(y)) {
			PieceSpecies::Type promotedSpecies = PieceTable[piece.getSpecies()].promotion;
			if(promotedSpecies) {
				piece = Piece::create(promotedSpecies, 0, piece.getOwner());
			}
		}
		board[Vec2{ x, y }.toIndex()] = piece;
		absEval += evalPiece(piece, x, y) * (piece.getOwner()? -1 : 1);
		if(piece.isRoyal()) {
			royalsLeft[piece.getOwner()]++;
		}
		hash ^= ZobristHashes::getHash(piece.getSpecies(), x, y);
		occupancyBitset.insert(Vec2{ x, y });
		playerOccupancyBitsets[piece.getOwner()].insert(Vec2{ x, y });
		if(regenerateMoves) {
			squaresNeedingMoveRecalculation.insert(Vec2{ x, y });
			squaresNeedingMoveRecalculation |= bidirectionalAttackMap.getReverseAttacks(Vec2{ x, y });
		}
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
			hash ^= ZobristHashes::getHash(oldPiece.getSpecies(), x, y);
		}
	}
public:
	uint8_t currentPlayer = 0;
	std::array<int8_t, 2> royalsLeft{};
	// this could be made unsigned, but we'll be converting it to signed all the time
	eval_t absEval = 0;
	hash_t hash = 0;
	
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
	
	inline constexpr Piece getSquare(int8_t x, int8_t y) const {
		return board[Vec2{ x, y }.toIndex()];
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
		int8_t destX = (move >> 6) & 0b111111;
		int8_t destY = move & 0b111111;
		PieceSpecies::Type pieceSpecies = piece.getSpecies();
		uint8_t pieceOwner = piece.getOwner();
		bool middleStepShouldPromote = false;
		if(isLionLikePiece(pieceSpecies)) {
			// lion-like pieces
			int8_t doesMiddleStep = move >> 28;
			if(doesMiddleStep) {
				int8_t middleStepX = ((move >> 26) & 0b11) - 1;
				int8_t middleStepY = ((move >> 24) & 0b11) - 1;
				int8_t middleY = srcY + middleStepY;
				clearSquare(srcX + middleStepX, middleY, regenerateMoves, saveState);
				middleStepShouldPromote = inPromotionZone(pieceOwner, middleY);
			}
		} else if(isRangeCapturingPiece(pieceSpecies)) {
			// range capturing pieces
			bool captureAllFlag = move >> 24;
			if(captureAllFlag) {
				int8_t dirX = sign(destX - srcX);
				int8_t dirY = sign(destY - srcY);
				int8_t x = srcX + dirX;
				int8_t y = srcY + dirY;
				int i = 0;
				while(x != destX || y != destY) {
					clearSquare(x, y, regenerateMoves, saveState);
					x += dirX;
					y += dirY;
					if(i++ > 100) {
						std::cout << "log Error: Range capturing has failed! src: " << srcX << "," << srcY << "   dest: " << destX << "," << destY << "   dir: " << dirX << "," << dirY << std::endl; 
						throw std::runtime_error("Range capturing fail");
					}
				}
			}
		}
		
		bool shouldPromote = middleStepShouldPromote || inPromotionZone(pieceOwner, destY);
		if(shouldPromote && piece.canPromote()) {
			piece = Piece::create(PieceTable[piece.getSpecies()].promotion, 0, pieceOwner);
		}
		
		clearSquare(srcX, srcY, regenerateMoves, saveState);
		setSquare(destX, destY, piece, regenerateMoves, saveState);
		currentPlayer = 1 - currentPlayer;
		if(regenerateMoves) {
			generateMoves();
		}
	}
	void unmakeMove() {
		StaticVector<UndoSquare, 36> &undoSquares = undoStack.back();
		for(const UndoSquare &undoSquare : undoSquares) {
			if(undoSquare.oldPiece) {
				setSquare(undoSquare.pos.x, undoSquare.pos.y, undoSquare.oldPiece);
			} else {
				clearSquare(undoSquare.pos.x, undoSquare.pos.y);
			}
		}
		undoStack.pop_back();
		generateMoves();
		currentPlayer = 1 - currentPlayer;
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
					Vec2 target = srcVec;
					Vec2 slideDir = movementDirToBoardDir(slide.dir, pieceOwner);
					for(int8_t dist = 0; dist < slide.range; dist++) {
						target += slideDir;
						if(!isPosWithinBounds(target)) {
							break;
						}
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
					for(Vec2 target = srcVec + dir; isPosWithinBounds(target); target += dir) {
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
				std::transform(pieceSpecies.begin(), pieceSpecies.end(), pieceSpecies.begin(), [](auto c) {
					return capitaliseLetter(c);
				});
				for(int j = 0; j < count; j++) {
					gameState.setSquare(x++, y, Piece::create(pieceSpecies, true, isSecondPlayer));
				}
			}
			y++;
		}
		gameState.currentPlayer = stringViewToNum<uint8_t>(fields[1]) % 2;
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
	int8_t destX = (move >> 6) & 0b111111;
	int8_t destY = move & 0b111111;
	
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
	Vec2 lionStepPos;
	if(arguments.size() > 3) {
		// must be a lion-like piece
		didLionStep = true;
		Vec2 stepPos = parseBoardPos(arguments[3]);
		Vec2 stepDeltaPos = stepPos - srcPos;
		lionStepPos = stepDeltaPos + 1; // 1 is subtracted in makeMove() so the range [-1, 1] is mapped to [0, 2] to work better as a 2-bit int
	}
	return createMove(srcPos.x, srcPos.y, destPos.x, destPos.y, isRangeCapturingMove, didLionStep, lionStepPos);
}

#define INF_SCORE 100000000

unsigned int nodesSearched = 0;

eval_t search(GameState &gameState, eval_t alpha, eval_t beta, depth_t depth) {
	nodesSearched++;
	// checking royal pieces only needs to be done for the current player - no point checking the player who just moved
	if(gameState.royalsLeft[gameState.currentPlayer] == 0) {
		return -INF_SCORE;
	}
	if(depth == 0) {
		return gameState.eval();
	}
	TranspositionTableEntry* ttEntry = transpositionTable.get(gameState.hash);
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
	eval_t bestScore = -INF_SCORE;
	uint32_t bestMove = 0;
	bool foundPvNode = 0;
	for(uint32_t move : moves) {
		gameState.makeMove(move, depth > 1, true);
		eval_t score;
		if(!foundPvNode) {
			score = -search(gameState, -beta, -alpha, depth - 1);
		} else {
			score = -search(gameState, -alpha - 1, -alpha, depth - 1);
			if(score > alpha && score < beta) {
				score = -search(gameState, -beta, -alpha, depth - 1);
			}
		}
		gameState.unmakeMove();
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
	transpositionTable.put(gameState.hash, bestMove, depth, bestScore, nodeType);
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
	uint32_t nodesSearched = 0;
	std::vector<uint32_t> moves = gameState.getAllMovesForPlayer(gameState.currentPlayer);
	for(uint32_t move : moves) {
		gameState.makeMove(move, depth > 1, true);
		nodesSearched += perft(gameState, depth - 1);
		gameState.unmakeMove();
	}
	return nodesSearched;
}

uint32_t findBestMove(GameState &gameState, depth_t maxDepth, float timeToMove = std::numeric_limits<float>::infinity()) {
	assert(maxDepth <= MAX_DEPTH);
	using clock = std::chrono::steady_clock;
	auto startTime = clock::now();
	auto stopTime = startTime + std::chrono::duration<float>(timeToMove);
	
	eval_t eval;
	for(depth_t depth = 1; depth <= maxDepth; depth++) {
		nodesSearched = 0;
		eval = search(gameState, -INF_SCORE, INF_SCORE, depth);
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
void makeBestMove(GameState &gameState) {
	const float timeToMove = timeIncrement + startingTime / ESTIMATED_GAME_LENGTH;
	// std::cout << "log Time to move: " << std::to_string(timeToMove) << " seconds" << std::endl;
	
	uint32_t bestMove = findBestMove(gameState, MAX_DEPTH, timeToMove);
	std::cout << "move " << stringifyMove(gameState, bestMove) << std::endl;
	gameState.makeMove(bestMove);
	std::cout << "eval " << gameState.absEval << std::endl;
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
				makeBestMove(gameState);
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
			} else if(command == "win") {
				inGame = false;
			} else if(command == "loss") {
				inGame = false;
			} else if(command == "draw") {
				inGame = false;
			} else if(command == "setparam") {
				// parameters not yet implemented, this command should never be received
			} else if(command == "quit") {
				atsiInitialised = false;
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
					using clock = std::chrono::steady_clock;
					auto start = clock::now();
					uint32_t nodesSearched = perft(gameState, depth);
					auto end = clock::now();
					auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
					std::cout << "Depth " << std::to_string(depth) << ": Found " << nodesSearched << " nodes in " << elapsed << " (" << nodesSearched * 1000 / elapsed.count() << " n/s)" << std::endl;
				}
			} else if(command == "search") {
				depth_t depth = std::stoi(getItem(arguments, 1));
				using clock = std::chrono::steady_clock;
				auto start = clock::now();
				eval_t eval = 0;
				nodesSearched = 0;
				eval = search(gameState, -INF_SCORE, INF_SCORE, depth);
				auto end = clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
				std::cout << "Depth " << std::to_string(depth) << ": Found " << nodesSearched << " nodes; Eval = " << eval << " in " << elapsed << " (" << static_cast<uint64_t>(nodesSearched) * 1000 / elapsed.count() << " n/s)" << std::endl;
			} else if(command == "bestmove") {
				depth_t depth = std::stoi(getItem(arguments, 1));
				using clock = std::chrono::steady_clock;
				auto start = clock::now();
				nodesSearched = 0;
				uint32_t bestMove = findBestMove(gameState, depth);
				auto end = clock::now();
				auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
				gameState.makeMove(bestMove, true, true);
				std::cout << "Depth " << std::to_string(depth) << ": Best move = " << stringifyMove(gameState, bestMove) << "; Eval = " << gameState.absEval << "; found " << nodesSearched << " nodes in " << elapsed << " (" << static_cast<uint64_t>(nodesSearched) * 1000 / elapsed.count() << " n/s)" << std::endl;
				gameState.unmakeMove();
			}
		}
	}
	
	return 0;
}