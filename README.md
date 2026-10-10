# AggroChess

AggroChess is intended to become a strong, aggressively minded UCI chess
engine. Development is incremental: every engine component is introduced only
after its public behavior has focused unit tests and, where applicable, perft
coverage.

## Current milestone: first executable and classical evaluation

The board layer uses six piece-type bitboards and two color bitboards. The
square convention is fixed: `a1 = 0`, `b1 = 1`, through `h1 = 7`; `a2 = 8`;
and `h8 = 63`. A square mask is `1ULL << squareIndex(square)`.

`Position::fromFen` validates the syntax of all six FEN fields and rejects
malformed board layouts, side-to-move values, castling-right strings,
en-passant squares, and move counters. `Position::toFen` emits a canonical
six-field representation.

The attack layer supplies pawn, knight, king, bishop, rook, and queen attack
masks together with square-attack and check detection. Sliding attacks
currently use simple occupancy-aware ray traversal; faster lookup techniques
can be introduced later behind the same API after profiling.

`generateLegalMoves` covers quiet moves, captures, double pawn pushes,
promotions and promotion captures, en passant, and castling. Candidate moves
are applied and rejected if they leave the moving king in check. Castling also
requires the appropriate right and rook, an empty path, and safe source,
transit, and destination squares. Kings are never generated as capturable
pieces. En-passant legality includes discovered-check cases.

`makeMove` and `unmakeMove` maintain piece placement, castling rights,
en-passant state, FEN counters, and a deterministic 64-bit Zobrist key. The
key covers every piece/square pair, side to move, castling rights, and the
en-passant file, and is updated incrementally for all move types. Tests compare
incremental keys with full recomputation throughout perft trees and
randomized make/unmake sequences.

`Undo` is a compact delta record of at most 24 bytes rather than a complete
position snapshot. Unmake reverses piece movement directly and restores only
irreversible state. This covers captures, en passant, castling, and promotions
while reducing search-stack traffic.

Alongside the bitboards, `Position` maintains a one-byte-per-square mailbox and
cached king squares. `pieceTypeAt`, `colorAt`, and `kingSquare` therefore avoid
scanning piece bitboards in move application, check detection, and future
search code. `isInternallyConsistent` verifies that all redundant
representations agree, while `isLegalPosition` separately applies semantic
chess constraints to imported or diagnostic positions.

`Move` is a four-byte, directly accessible hot-path value with canonical
identity, explicit invalid and null states, and a stable 16-bit representation
for future transposition-table entries. `moveToUci` serializes legal move
identities, while `moveFromUci` resolves coordinate text against the current
legal move list instead of trusting externally supplied move flags.

`PositionHistory` records search/game keys with push/pop semantics. Game-state
classification covers checkmate, stalemate, the fifty-move rule, threefold
repetition, and insufficient material. Repetition scanning is bounded by the
halfmove clock. En-passant state contributes to a position key only when at
least one legal en-passant capture exists, so pinned or otherwise inert targets
do not create false position identities.

`staticExchangeEvaluation` evaluates material exchanges on a move's destination
square. It follows dynamic x-rays, excludes pinned or otherwise illegal
recaptures, lets either side decline a losing continuation, and handles en
passant and promotions. It is currently a tested diagnostic and future move-
ordering input, not a pruning rule.

`TranspositionTable` provides resizable, four-way clustered storage with full
key verification, depth, bound, score, best move, generation aging, and
ply-correct mate-score normalization. Its public resize hook is exposed by the
UCI `Hash` option. Shared lifecycle locking and striped cluster locking make
probe, store, clear, and resize operations safe for later parallel search.

The first classical evaluator uses middlegame/endgame material and piece-square
terms, mobility, pawn structure and passers, bishop pair, rook activity, king
shelter and ring pressure, and space. A 24-step phase value tapers every
component between middlegame and endgame. `evaluateDetailed` exposes each term
for tests and diagnostics; `evaluate` returns a score relative to the side to
move.

`makeMove` remains a low-level structural operation intended for moves produced
by the legal generator; it does not independently validate every
piece-movement and king-safety rule.

## Correctness coverage

The test suite includes focused attack-mask and move-rule tests plus standard
perft positions. It currently checks:

- the initial position through depth 4 (`197281` nodes);
- Kiwipete through depth 3 (`97862` nodes);
- standard perft positions 3–6 through depth 3;
- pins, checkmate, castling through check, all promotion types, and the
  en-passant discovered-check edge case;
- position restoration after perft.

## First executable

The `AggroChess` target is a synchronous UCI preview. It accepts `uci`,
`isready`, `ucinewgame`, `setoption name Hash value ...`, `position`, `go`,
`stop`, `ponderhit`, `eval`, and `quit`. It plays legal games by evaluating
every root move one ply deep, recognizes immediate mates and draws, and emits a
legal `bestmove`.

After building, run it on Linux/macOS with:

```sh
./build/AggroChess
```

With a multi-configuration Windows generator, the equivalent output is usually
`build\\Release\\AggroChess.exe`. GitHub Actions also builds, tests, and uploads
this file as the `AggroChess-windows-x64` workflow artifact.

This is deliberately not yet a competitive search engine: `go` limits and
`stop` are only protocol placeholders until iterative search and time
management arrive. The next implementation item is baseline alpha-beta search.
See the ordered, live [`ROADMAP.md`](ROADMAP.md) for the execution queue and
[`ENGINE_ANALYSIS.md`](ENGINE_ANALYSIS.md) for the broader design rationale.

## Build and test

A C++20 compiler and CMake 3.20 or newer are required. From the repository
root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

To run the test suite with AddressSanitizer and UndefinedBehaviorSanitizer
using GCC or Clang:

```sh
cmake -S . -B build-sanitize \
  -DCMAKE_BUILD_TYPE=Debug \
  -DAGGROCHESS_ENABLE_SANITIZERS=ON
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

Tests are enabled by default through CMake's `BUILD_TESTING` option. To build
the core library and engine executable without test targets, configure with
`-DBUILD_TESTING=OFF`.

GitHub Actions builds and tests Debug and Release configurations with GCC and
Clang. The GCC Debug job also runs both sanitizers.
