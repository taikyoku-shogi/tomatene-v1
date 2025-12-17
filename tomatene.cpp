#include <bits/stdc++.h>

#include <frozen/unordered_map.h>
#include <frozen/string.h>

#define ENGINE_NAME "Tomatene"
#define ENGINE_DESC "Experimental taikyoku shogi engine"
#define ENGINE_AUTHOR "s1050613"
#define ENGINE_VERSION "v0"

#define INITIAL_TSFEN "IC,WT,RR,W,FD,RME,T,BC,RH,FDM,ED,WDV,FDE,FK,RS,RIG,GLG,CP,K,GLG,LG,RS,FK,FDE,CDV,ED,FDM,RH,BC,T,LME,FD,W,RR,TS,IC/RVC,FEL,TD,FSW,FWO,RDM,FOD,MS,RP,RSR,SSP,GD,RTG,RBE,NS,GOG,SVG,DE,NK,SVG,SWR,BD,RBE,RTG,GD,SSP,RSR,RP,MS,FOD,RDM,FWO,FSW,TD,WE,RVC/GCH,SD,RUS,RW,AG,FLG,RIT,RDR,BO,WID,FP,RBI,OK,PCK,WD,FDR,COG,PHM,KM,COG,FDR,WD,PCK,OK,RBI,FP,WID,BO,LDR,LTG,FLG,AG,RW,RUS,SD,GCH/SVC,VB,CH,PIG,CG,PG,HG,OG,CST,SBO,SR,GOS,L,FWC,GS,FID,WDM,VG,GG,WDM,FID,GS,FWC,L,GOS,SR,SBO,CST,OG,HG,PG,CG,PIG,CH,VB,SVC/SC,CLE,AM,FCH,SW,FLC,MH,VT,S,LS,CLD,CPC,RC,RHS,FIO,GDR,GBI,DS,DV,GBI,GDR,FIO,RHS,RC,CPC,CLD,LS,S,VT,MH,FLC,SW,FCH,AM,CLE,SC/WC,WF,RHD,SM,PS,WO,FIL,FIE,FLD,PSR,FGO,SCR,BDG,WG,FG,PH,HM,LT,GT,C,KR,FG,WG,BDG,SCR,FGO,PSR,FLD,FIE,FIL,WO,PS,SM,LHD,WF,WC/TC,VW,SO,DO,FLH,FB,AB,EW,WIH,FC,OM,HC,NB,SB,FIS,FIW,TF,CM,PM,TF,FIW,FIS,EB,WB,HC,OM,FC,WIH,EW,AB,FB,FLH,DO,SO,VW,TC/EC,VSP,EBG,H,SWO,CMK,CSW,SWW,BM,BT,OC,SF,BBE,OR,SQM,CS,RD,FE,LH,RD,CS,SQM,OR,BBE,SF,OC,BT,BM,SWW,CSW,CMK,SWO,H,EBG,BDR,EC/CHS,SS,VS,WIG,RG,MG,FST,HS,WOG,OS,EG,BOS,SG,LPS,TG,BES,IG,GST,GM,IG,BES,TG,LPS,SG,BOS,EG,OS,WOG,HS,FST,MG,RG,WIG,VS,SS,CHS/RCH,SMK,VM,FLO,LBS,VP,VH,CAS,DH,DK,SWS,HHW,FLE,SPS,VL,FIT,CBS,RDG,LD,CBS,FIT,VL,SPS,FLE,HHW,SWS,DK,DH,CAS,VH,VP,LBS,FLO,VM,SMK,LC/P36/5,D,4,GB,3,D,6,D,3,GB,4,D,5/36/36/36/36/36/36/36/36/36/36/36/36/5,d,4,gb,3,d,6,d,3,gb,4,d,5/p36/lc,smk,vm,flo,lbs,vp,vh,cas,dh,dk,sws,hhw,fle,sps,vl,fit,cbs,ld,rdg,cbs,fit,vl,sps,fle,hhw,sws,dk,dh,cas,vh,vp,lbs,flo,vm,smk,rch/chs,ss,vs,wig,rg,mg,fst,hs,wog,os,eg,bos,sg,lps,tg,bes,ig,gm,gst,ig,bes,tg,lps,sg,bos,eg,os,wog,hs,fst,mg,rg,wig,vs,ss,chs/ec,bdr,ebg,h,swo,cmk,csw,sww,bm,bt,oc,sf,bbe,or,sqm,cs,rd,lh,fe,rd,cs,sqm,or,bbe,sf,oc,bt,bm,sww,csw,cmk,swo,h,ebg,vsp,ec/tc,vw,so,do,flh,fb,ab,ew,wih,fc,om,hc,wb,eb,fis,fiw,tf,pm,cm,tf,fiw,fis,sb,nb,hc,om,fc,wih,ew,ab,fb,flh,do,so,vw,tc/wc,wf,lhd,sm,ps,wo,fil,fie,fld,psr,fgo,scr,bdg,wg,fg,kr,c,gt,lt,hm,ph,fg,wg,bdg,scr,fgo,psr,fld,fie,fil,wo,ps,sm,rhd,wf,wc/sc,cle,am,fch,sw,flc,mh,vt,s,ls,cld,cpc,rc,rhs,fio,gdr,gbi,dv,ds,gbi,gdr,fio,rhs,rc,cpc,cld,ls,s,vt,mh,flc,sw,fch,am,cle,sc/svc,vb,ch,pig,cg,pg,hg,og,cst,sbo,sr,gos,l,fwc,gs,fid,wdm,gg,vg,wdm,fid,gs,fwc,l,gos,sr,sbo,cst,og,hg,pg,cg,pig,ch,vb,svc/gch,sd,rus,rw,ag,flg,ltg,ldr,bo,wid,fp,rbi,ok,pck,wd,fdr,cog,km,phm,cog,fdr,wd,pck,ok,rbi,fp,wid,bo,rdr,rit,flg,ag,rw,rus,sd,gch/rvc,we,td,fsw,fwo,rdm,fod,ms,rp,rsr,ssp,gd,rtg,rbe,bd,swr,svg,nk,de,svg,gog,ns,rbe,rtg,gd,ssp,rsr,rp,ms,fod,rdm,fwo,fsw,td,fel,rvc/ic,ts,rr,w,fd,lme,t,bc,rh,fdm,ed,cdv,fde,fk,rs,lg,glg,k,cp,glg,rig,rs,fk,fde,wdv,ed,fdm,rh,bc,t,rme,fd,w,rr,wt,ic 0"

#define ESTIMATED_GAME_LENGTH = 900;

std::vector<std::string> splitString(std::string str, char delimiter) {
	std::vector<std::string> res;
	std::stringstream ss(str);
	std::string token;
	while(std::getline(ss, token, delimiter)) {
		res.push_back(token);
	}
	return res;
}
std::string textAfter(std::string str, std::string target) {
	size_t i = str.find(target);
	if(i == std::string::npos) {
		return "";
	}
	return str.substr(i + 1);
}
std::string getItem(std::vector<std::string> vec, int index) {
	if(index < 0 || index >= vec.size()) {
		return ""; // WHYYYY C++ DOESN'T HAVE THIS.....
	}
	return vec[index];
}
bool isNumber(std::string str) {
	for(char c : str) {
		if(!std::isdigit(c)) {
			return false;
		}
	}
	return true;
}
template <typename T>
T sign(T x) {
	return (x > 0) - (x < 0);
}

struct PieceSpecies {
	enum Type : uint16_t {
		None,
		#define Piece(code, move, promo) code,
			#include "pieces.inc"
		#undef Piece
		TotalCount
	};
};

struct PieceInfo {
	const char* movement;
	PieceSpecies::Type promotion;
};
inline constexpr PieceInfo PieceTable[] = {
	#define Piece(code, move, promo) { move, PieceSpecies::promo },
		#include "pieces.inc"
	#undef Piece
};

// std::unordered_map<std::string, uint16_t> pieceIds;

inline constexpr frozen::unordered_map<frozen::string, uint16_t, PieceSpecies::TotalCount - 1> PieceSpeciesToId = {
	#define Piece(code, move, promo) { #code, PieceSpecies::code },
		#include "pieces.inc"
	#undef Piece
};
struct PieceIdsWrapper {
	constexpr uint16_t operator[](std::string_view species) const {
		return PieceSpeciesToId.at(species);
	}
};
inline constexpr PieceIdsWrapper pieceIds;

// hacky wrapper around a uint16_t lol
struct Piece {
	uint16_t value;
	
	operator uint16_t() const {
		return value;
	}
	
	static inline Piece create(std::string species, bool canPromote, uint8_t owner) {
		uint16_t id = pieceIds[species];
		return create(id, canPromote, owner);
	}
	static inline Piece create(uint16_t id, bool canPromote, uint8_t owner) {
		// std::cout << "creating piece " << species << " = " << id << std::endl;
		return Piece { static_cast<uint16_t>((owner << 10) | (canPromote << 9) | id) };
	}
	
	inline uint8_t getOwner() const {
		return value >> 10;
	}
	inline uint16_t canPromote() const {
		return ((value >> 9) & 1) && PieceTable[getSpecies()].promotion != 0;
	}
	inline uint16_t getSpecies() const {
		return value & 0b111111111;
	}
	inline bool isRoyal() const {
		uint16_t pieceSpecies = getSpecies();
		return pieceSpecies == PieceSpecies::K || pieceSpecies == PieceSpecies::CP;
	}
};

int32_t evalPiece(Piece piece, uint8_t x, uint8_t y) {
	if(!piece) return 0;
	return 1; // placeholder for actual piece evaluation function
}

class GameState {
public:
	std::array<Piece, 1296> board{};
	uint8_t currentPlayer;
	std::array<int8_t, 2> royalsLeft{};
	std::vector<uint32_t> moves;
	
	Piece getSquare(uint8_t x, uint8_t y) const {
		return board[x + 36 * y];
	}
	void setSquare(uint8_t x, uint8_t y, Piece piece) {
		if(getSquare(x, y)) {
			// clearing before setting makes it a bit easier
			clearSquare(x, y);
		}
		board[x + 36 * y] = piece;
		absEval += evalPiece(piece, x, y) * (piece.getOwner()? -1 : 1);
		if(piece.isRoyal()) {
			royalsLeft[piece.getOwner()]++;
		}
	}
	void clearSquare(uint8_t x, uint8_t y) {
		Piece oldPiece = getSquare(x, y);
		if(oldPiece) {
			absEval -= evalPiece(oldPiece, x, y) * (oldPiece.getOwner()? -1 : 1);
			if(oldPiece.isRoyal()) {
				royalsLeft[oldPiece.getOwner()]--;
			}
		}
		board[x + 36 * y] = Piece{0};
	}
	std::string toString() const {
		std::ostringstream oss;
		oss << "Player: " << static_cast<int>(currentPlayer) << "\nBoard: [";
		for(size_t i = 0; i < 1296; i++) {
			oss << board[i];
			if(i != 1295) oss << ", ";
		}
		oss << "]";
		return oss.str();
	}
	int32_t eval() const {
		if(currentPlayer) {
			return absEval * -1;
		}
		return absEval;
	}
	
	static GameState fromTsfen(std::string tsfen) {
		auto fields = splitString(tsfen, ' ');
		GameState gameState;
		
		auto rows = splitString(fields[0], '/');
		int y = 0;
		for(auto row : rows) {
			int x = 0;
			auto cells = splitString(row, ',');
			for(auto cell : cells) {
				if(isNumber(cell)) {
					// empty spaces
					int emptySquares = stoi(cell);
					x += emptySquares;
					// for(int i = 0; i < emptySquares; i++) {
					// 	gameState.clearSquare(x++, y);
					// }
					continue;
				}
				std::string pieceSpecies;
				size_t i = 0;
				while(i < cell.size() && std::isalpha(cell[i])) {
					pieceSpecies += cell[i++];
				}
				int count = 1;
				if(i < cell.size()) {
					count = std::stoi(cell.substr(i));
				}
				bool isSecondPlayer = std::isupper(pieceSpecies[0]);
				std::transform(pieceSpecies.begin(), pieceSpecies.end(), pieceSpecies.begin(), [](auto c) {
					return std::toupper(c);
				});
				for(int j = 0; j < count; j++) {
					gameState.setSquare(x++, y, Piece::create(pieceSpecies, true, isSecondPlayer));
				}
			}
			y++;
		}
		gameState.currentPlayer = stoi(fields[1]) % 2;
		
		return gameState;
	}
private:
	int32_t absEval = 0;
};

inline bool inPromotionZone(uint8_t owner, uint8_t y) {
	return owner? y > 24 : y < 11;
}
void makeMove(GameState &gameState, uint32_t move) {
	// move layout:
	// 6 bits: src x, 6 bits: src y
	// 6 bits: dst x, 6 bits: dst y
	// for capricorn, 1 bit extra: promotion
	// for range capturing pieces, 1 bit extra: capture all
	// for lion-like pieces, extra 1 bit: does middle step, extra 2 bits: middle step x offset, extra 2 bits: middle step y offset
	// for free eagle, 1 bit extra: captured all in the way, 1 bit more extra: capturing 3 or 4
	
	uint8_t srcX = (move >> 18) & 0b111111;
	uint8_t srcY = (move >> 12) & 0b111111;
	Piece piece = gameState.getSquare(srcX, srcY);
	uint8_t destX = (move >> 6) & 0b111111;
	uint8_t destY = move & 0b111111;
	uint16_t pieceSpecies = piece.getSpecies();
	uint8_t pieceOwner = piece.getOwner();
	bool middleStepShouldPromote = false;
	if(pieceSpecies == PieceSpecies::C) {
		middleStepShouldPromote = move >> 24;
	} else if(pieceSpecies == PieceSpecies::L || pieceSpecies == PieceSpecies::FFI || pieceSpecies == PieceSpecies::BS || pieceSpecies == PieceSpecies::H) {
		// lion-like pieces
		int8_t doesMiddleStep = move >> 28;
		if(doesMiddleStep) {
			int8_t middleStepX = ((move >> 26) & 0b11) - 1;
			int8_t middleStepY = ((move >> 24) & 0b11) - 1;
			uint8_t middleY = srcY + middleStepY;
			gameState.clearSquare(srcX + middleStepX, middleY);
			middleStepShouldPromote = inPromotionZone(pieceOwner, middleY);
		}
	} else if(pieceSpecies == PieceSpecies::FE) {
		// free eagle
		bool captureAllFlag = move >> 24;
		if(captureAllFlag) {
			int8_t dirX = sign(destX - srcX);
			int8_t dirY = sign(destY - srcY);
			bool shouldMove4 = move >> 25;
			uint8_t steps = 3 + shouldMove4;
			uint8_t x = srcX + dirX;
			uint8_t y = srcY + dirY;
			for(uint8_t i = 0; i < steps; i++) {
				gameState.clearSquare(x, y);
				x += dirX;
				y += dirY;
			}
		}
	} else if(pieceSpecies == PieceSpecies::GG || pieceSpecies == PieceSpecies::VG || pieceSpecies == PieceSpecies::FLG || pieceSpecies == PieceSpecies::AG || pieceSpecies == PieceSpecies::FID || pieceSpecies == PieceSpecies::FCR) {
		// range capturing pieces
		bool captureAllFlag = move >> 24;
		if(captureAllFlag) {
			int8_t dirX = sign(destX - srcX);
			int8_t dirY = sign(destY - srcY);
			uint8_t x = srcX + dirX;
			uint8_t y = srcY + dirY;
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
}

#define INF_SCORE 1000000

void unmakeMove(GameState &gameState, uint32_t move) {}
int32_t search(GameState &gameState, int32_t alpha, int32_t beta, uint8_t depth) {
	// checking royal pieces only needs to be done for the current player - no point checking the player who just moved
	if(gameState.royalsLeft[gameState.currentPlayer] == 0) {
		return -INF_SCORE;
	}
	if(depth == 0) {
		return gameState.eval();
	}
	std::vector<uint32_t> moves = gameState.moves;
	for(auto move : moves) {
		makeMove(gameState, move);
		int32_t score = -search(gameState, -beta, -alpha, depth - 1);
		unmakeMove(gameState, move);
		if(score >= beta) {
			// fail-soft
			return score;
		}
		if(score > alpha) {
			alpha = score;
		}
	}
	return alpha;
}
uint32_t bestMove(GameState &gameState, uint8_t depth) {
	int alpha = -INF_SCORE;
	uint32_t bestMove = 0;
	std::vector<uint32_t> moves = gameState.moves;
	for(auto move : moves) {
		makeMove(gameState, move);
		int32_t score = -search(gameState, -INF_SCORE, -alpha, depth - 1);
		unmakeMove(gameState, move);
		if(score > alpha) {
			alpha = score;
			bestMove = move;
		}
	}
	return bestMove;
}

int main() {
	bool atsiInitialised = true;
	bool inGame = false;
	uint8_t player = 0;
	float startingTime = 0;
	float timeIncrement = 0;
	
	int moveCounter = 0;
	
	GameState gameState;
	
	std::string line;
	while(std::getline(std::cin, line)) {
		std::vector<std::string> arguments = splitString(line, ' ');
		if(!arguments.size()) continue;
		std::string command = getItem(arguments, 0);
		std::cout << atsiInitialised << std::endl;
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
			} else if(command == "side") {
				player = std::stoi(getItem(arguments, 1));
			} else if(command == "startgame") {
				std::string initialPos = textAfter(line, " ");
				if(initialPos == "initial") {
					initialPos = INITIAL_TSFEN;
				}
				std::cout << "got tsfen: '" << initialPos << "'" << std::endl;
				gameState = GameState::fromTsfen(initialPos);
				std::cout << "parsed tsfen!" << std::endl;
				std::cout << gameState.toString() << std::endl;
				moveCounter = 0;
			} else if(command == "win") {
				inGame = false;
			} else if(command == "loss") {
				inGame = false;
			} else if(command == "draw") {
				inGame = false;
			} else if(command == "opmove") {
				moveCounter++;
				
			} else if(command == "setparam") {
				// parameters not yet implemented, this command should never be received
			} else if(command == "quit") {
				atsiInitialised = false;
			}
		} else {
			if(command == "atsiinit") {
				std::string version = getItem(arguments, 1);
				std::cout << "'" << version << "'" << std::endl;
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