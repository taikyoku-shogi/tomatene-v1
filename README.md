# Tomatene v1
Tomatene v1 is a proof-of-concept taikyoku shogi engine conforming to the ATSA rules and the ATSI v0 protocol.

It can be played with on https://taikyoku-shogi.github.io/play by using [websocketd](https://github.com/joewalnes/websocketd).

It uses compiler macros and constexpr spam to implement the pieces whilst avoiding the need to hard-code all 301 piece types.

Features:
- Alpha-beta minimax search
- Iterative deepening
- Principal variation search
- Move ordering:
  - Transposition table
  - Killer heuristic
- Incremental move generation with attack maps
- Material evaluation
  - Individual piece evaluations calculated from movements
  - Pieces further forward and in the center are valued more
  - Bonus for being close to the (starting positions of the) royal families 