# Tomatene
Tomatene is a proof-of-concept taikyoku shogi engine build for the ATSA rules and the ATSI v0 protocol.

It uses compiler macros and constexpr spam to implement the pieces whilst avoiding the need to hard-code all 301 piece types.

Features:
- Alpha-beta minimax search
- Iterative deepening
- Principal variation search
- Move ordering:
  - Transposition table
  - Killer heuristic
- Incremental move generation
- Material evaluation