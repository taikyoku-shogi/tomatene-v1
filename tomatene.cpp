#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <unordered_set>

#include <frozen/unordered_map.h>
#include <frozen/string.h>

#include "zobristHashes.h"
#include "transpositionTable.h"

#define ENGINE_NAME "Tomatene"
#define ENGINE_DESC "Experimental taikyoku shogi engine"
#define ENGINE_AUTHOR "s1050613"
#define ENGINE_VERSION "v0"

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

// Centipawns
typedef int32_t eval_t;

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
	
	constexpr Vec2 operator+(const Vec2 &other) const {
		return { static_cast<int8_t>(x + other.x), static_cast<int8_t>(y + other.y) };
	}constexpr Vec2 operator+(const int other) const {
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
struct Vec2Hash {
	size_t operator()(const Vec2& v) const noexcept {
		return std::hash<int>{}(v.x << 8 | v.y);
		// return std::hash<int>{}(v.x) ^ std::hash<int>{}(v.y);
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
			value += slide.range * (isRangeCapturingPiece(static_cast<PieceSpecies::Type>(pieceSpecies))? 50 : 5);
		}
		if(pieceSpecies == PieceSpecies::K || pieceSpecies == PieceSpecies::CP) {
			value += 100000;
		}
		values[pieceSpecies] = value;
	}
	for(int pieceSpecies = 1; pieceSpecies < 302; pieceSpecies++) {
		auto promotion = PieceTable[pieceSpecies].promotion;
		values[pieceSpecies] += values[promotion] / 10;
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
	
	eval_t pieceBaseEval = basePieceValues[piece.getSpecies()];
	eval_t horizontalCenterBonus = 18 - std::abs(x - 17.5f);
	eval_t verticalCenterBonus = std::min(y, static_cast<int8_t>(25));
	
	return pieceBaseEval + horizontalCenterBonus + verticalCenterBonus + horizontalCenterBonus * verticalCenterBonus / 2;
}

inline constexpr bool isPosWithinBounds(Vec2 pos) {
	return pos.x >= 0 && pos.x < 36 && pos.y >= 0 && pos.y < 36;
}

inline constexpr Vec2 movementDirToBoardDir(Vec2 movementDir, uint8_t pieceOwner) {
	return pieceOwner? Vec2{ static_cast<int8_t>(-movementDir.x), movementDir.y } : Vec2{ movementDir.x, static_cast<int8_t>(-movementDir.y) };
}

class GameState {
public:
	std::array<Piece, 1296> board{};
	uint8_t currentPlayer = 0;
	std::array<int8_t, 2> royalsLeft{};
	std::array<std::array<std::vector<uint32_t>, 1296>, 2> movesPerSquarePerPlayer;
	// this could be made unsigned, but we'll be converting it to signed all the time
	eval_t absEval = 0;
	int32_t hash = 0;
	
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
		// std::cout << std::to_string(x) << "," << std::to_string(y) << std::endl;
		return board[x + 36 * y];
	}
	void setSquare(int8_t x, int8_t y, Piece piece) {
		if(getSquare(x, y)) {
			// clearing before setting makes it a bit easier
			clearSquare(x, y);
		}
		// handle promotion here
		if(piece.canPromote() && piece.isInPromotionZone(y)) {
			PieceSpecies::Type promotedSpecies = PieceTable[piece.getSpecies()].promotion;
			if(promotedSpecies) {
				piece = Piece::create(promotedSpecies, 0, piece.getOwner());
			}
		}
		board[x + 36 * y] = piece;
		absEval += evalPiece(piece, x, y) * (piece.getOwner()? -1 : 1);
		if(piece.isRoyal()) {
			royalsLeft[piece.getOwner()]++;
		}
		hash ^= ZobristHashes::getHash(piece.getSpecies(), x, y);
	}
	void clearSquare(int8_t x, int8_t y) {
		Piece oldPiece = getSquare(x, y);
		if(oldPiece) {
			absEval -= evalPiece(oldPiece, x, y) * (oldPiece.getOwner()? -1 : 1);
			if(oldPiece.isRoyal()) {
				royalsLeft[oldPiece.getOwner()]--;
			}
		}
		board[x + 36 * y] = Piece{0};
		hash ^= ZobristHashes::getHash(oldPiece.getSpecies(), x, y);
		// movesPerSquarePerPlayer[x + 36 * y].clear();
	}
	
	void generateMoves() {
		size_t i = 0;
		for(int8_t y = 0; y < 36; y++) {
			for(int8_t x = 0; x < 36; x++) {
				movesPerSquarePerPlayer[0][i].clear();
				movesPerSquarePerPlayer[1][i].clear();
				Piece piece = getSquare(x, y);
				auto pieceOwner = piece.getOwner();
				bool pieceIsRangeCapturing = isRangeCapturingPiece(piece.getSpecies());
				auto pieceRank = piece.getRank();
				if(piece) {
					auto &movements = PieceTable[piece.getSpecies()].movements;
					std::unordered_set<Vec2, Vec2Hash> attackingSquares;
					std::unordered_set<Vec2, Vec2Hash> validMoveLocations;
					std::unordered_set<Vec2, Vec2Hash> rangeCapturingMoveLocations;
					for(const auto &slide : movements.slides) {
						bool slideIsRangeCapturing = pieceIsRangeCapturing && slide.range == 35;
						Vec2 target{ x, y };
						Vec2 slideDir = movementDirToBoardDir(slide.dir, pieceOwner);
						for(int8_t dist = 0; dist < slide.range; dist++) {
							target += slideDir;
							if(!isPosWithinBounds(target)) {
								break;
							}
							attackingSquares.insert(target);
							Piece attackingPiece = getSquare(target.x, target.y);
							bool isBlocked = attackingPiece && (slideIsRangeCapturing? attackingPiece.getRank() >= pieceRank : attackingPiece.getOwner() == pieceOwner);
							if(isBlocked) {
								break;
							}
							if(slideIsRangeCapturing) {
								rangeCapturingMoveLocations.insert(target);
							} else {
								validMoveLocations.insert(target);
								if(attackingPiece) {
									break;
								}
							}
						}
					}
					movesPerSquarePerPlayer[pieceOwner][i].reserve(validMoveLocations.size() + rangeCapturingMoveLocations.size());
					std::transform(validMoveLocations.begin(), validMoveLocations.end(), std::back_inserter(movesPerSquarePerPlayer[pieceOwner][i]), [x, y](const Vec2 &target) {
						return createMove(x, y, target.x, target.y);
					});
					std::transform(rangeCapturingMoveLocations.begin(), rangeCapturingMoveLocations.end(), std::back_inserter(movesPerSquarePerPlayer[pieceOwner][i]), [x, y](const Vec2 &target) {
						return createMove(x, y, target.x, target.y, true);
					});
				}
				i++;
			}
		}
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

inline bool inPromotionZone(uint8_t owner, int8_t y) {
	return owner? y > 24 : y < 11;
}
void makeMove(GameState &gameState, uint32_t move, bool regenerateMoves = true) {
	// move layout:
	// 6 bits: src x, 6 bits: src y
	// 6 bits: dst x, 6 bits: dst y
	// for range capturing pieces, 1 bit extra: capture all
	// for lion-like pieces, extra 1 bit: does middle step, extra 2 bits: middle step x offset, extra 2 bits: middle step y offset
	
	int8_t srcX = (move >> 18) & 0b111111;
	int8_t srcY = (move >> 12) & 0b111111;
	Piece piece = gameState.getSquare(srcX, srcY);
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
			gameState.clearSquare(srcX + middleStepX, middleY);
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
				gameState.clearSquare(x, y);
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
	
	gameState.clearSquare(srcX, srcY);
	gameState.setSquare(destX, destY, piece);
	gameState.currentPlayer = 1 - gameState.currentPlayer;
	if(regenerateMoves) {
		gameState.generateMoves();
	}
}

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
	int8_t srcX = (move >> 18) & 0b111111;
	int8_t srcY = (move >> 12) & 0b111111;
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
	return createMove(srcPos.x, srcPos.y, destPos.x, destPos.y, isRangeCapturingMove);
}

#define INF_SCORE 100000000

unsigned int nodesSearched = 0;

void unmakeMove(GameState &gameState, uint32_t move) {}
eval_t search(GameState &gameState, eval_t alpha, eval_t beta, uint8_t depth) {
	nodesSearched++;
	// checking royal pieces only needs to be done for the current player - no point checking the player who just moved
	if(gameState.royalsLeft[gameState.currentPlayer] == 0) {
		return -INF_SCORE;
	}
	if(depth == 0) {
		return gameState.eval();
	}
	std::vector<uint32_t> moves = gameState.getAllMovesForPlayer(gameState.currentPlayer);
	// std::cout << "log Found " << moves.size() << " moves" << std::endl;
	TranspositionTableEntry* ttEntry = transpositionTable.get(gameState.hash);
	if(ttEntry) {
		std::vector<uint32_t>::iterator it = std::find(moves.begin(), moves.end(), ttEntry->bestMove);
		if(it != moves.end()) {
			// std::rotate(moves.begin(), it, it + 1);
			// std::swap(*moves.begin(), *it);
			for(size_t i = 0; i < moves.size(); i++) {
				if(moves[i] == ttEntry->bestMove) {
					moves[i] = moves[0];
					moves[0] = ttEntry->bestMove;
					// std::cout << "log Found TT move " << std::to_string(ttEntry->bestMove) << std::endl;
					break;
				}
			}
		}
	}
	uint32_t bestMove = 0;
	for(uint32_t move : moves) {
		// std::cout << "log trying move " << stringifyMove(gameState, move) << std::endl;
		GameState newGameState = gameState;
		makeMove(newGameState, move, depth > 1);
		int32_t score = -search(newGameState, -beta, -alpha, depth - 1);
		// unmakeMove(gameState, move);
		if(score > alpha) {
			alpha = score;
			bestMove = move;
			if(alpha >= beta) {
				break;
			}
		}
	}
	transpositionTable.put(gameState.hash, bestMove);
	return alpha;
}

void makeBestMove(GameState &gameState) {
	// Piece piece = gameState.getSquare(18, 25);
	// uint8_t x, y;
	// while(true) {
	// 	std::cin >> x >> y;
	// 	std::cout << std::to_string(evalPiece(piece, x, y)) << std::endl;
	// }
	
	
	const float timeToMove = timeIncrement + startingTime / ESTIMATED_GAME_LENGTH;
	// std::cout << "log Time to move: " << std::to_string(timeToMove) << " seconds" << std::endl;
	using clock = std::chrono::steady_clock;
	auto startTime = clock::now();
	auto stopTime = startTime + std::chrono::duration<float>(timeToMove);
	
	eval_t eval;
	for(int depth = 1; depth <= 2; depth++) {
		std::cout << "log Searching to depth " << depth << "..." << std::endl;
		nodesSearched = 0;
		eval = search(gameState, -INF_SCORE, INF_SCORE, depth);
		std::cout << "log Score for us after depth " << depth << " and " << nodesSearched << " nodes: " << eval << std::endl;
		if(clock::now() >= stopTime) {
			break;
		}
	}
	
	TranspositionTableEntry* ttEntry = transpositionTable.get(gameState.hash);
	if(!ttEntry) {
		std::cout << "log Fatal error: No TT entry found for gameState hash " << gameState.hash << std::endl;
		throw std::runtime_error("No TT entry found!!!!!!");
	}
	
	uint32_t bestMove = ttEntry->bestMove;
	std::cout << "move " << stringifyMove(gameState, bestMove) << std::endl;
	makeMove(gameState, bestMove);
	std::cout << "eval " << gameState.absEval << std::endl;
	std::cout << "log Zobrist hash: " << gameState.hash << std::endl;
}

int main() {
	GameState gameState;
	
	std::string line;
	while(std::getline(std::cin, line)) {
		std::vector<std::string> arguments = splitString(line, ' ');
		if(!arguments.size()) continue;
		std::string command = getItem(arguments, 0);
		// std::cout << atsiInitialised << std::endl;
		if(atsiInitialised) {
			if(command == "identify") {
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
			} else if(command == "opmove") {
				// todo: parse move and call makeMove() or something...
				uint32_t move = parseMove(gameState, arguments);
				makeMove(gameState, move);
				// gameState.currentPlayer = 1 - gameState.currentPlayer;
				makeBestMove(gameState);
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
			}
		}
	}
	
	return 0;
}