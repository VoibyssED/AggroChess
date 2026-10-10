# AggroChess engine analysis and strength roadmap

The live, one-at-a-time implementation queue is maintained in
[`ROADMAP.md`](ROADMAP.md). This document retains the architectural analysis
and rationale behind that queue.

## Executive assessment

> Baseline note: this analysis describes commit `ed555d7`. Phases 0 and 1 have
> since begun: the repository now has CMake/CTest and CI, attack/check
> detection, fully legal move generation, standard perft coverage,
> incrementally maintained deterministic Zobrist keys, compact delta-based undo
> records, cached king squares, a profiled piece mailbox, separate internal
> consistency versus semantic legality validation, canonical move/UCI notation,
> repetition history, terminal/draw adjudication, legal static exchange
> evaluation, a concurrent clustered transposition table, a tapered classical
> evaluator, and a first synchronous one-ply UCI executable.

At commit `ed555d7`, AggroChess is a tested **position-state prototype**, not yet a chess engine. It can parse/emit FEN and apply explicitly described state transitions, but it has no attack generation, legal move generation, draw/checkmate detection, evaluation, search, time management, or UCI executable. It therefore has no meaningful playing strength yet.

The current bitboard convention is a reasonable foundation. The highest-value path is not to add “aggression bonuses” now; it is to build a correct, measurable engine core, then establish playing strength, then tune an aggressive style under an Elo constraint. A shallow engine rewarded for checks and sacrifices will be aggressive only in the sense of losing quickly.

## What is good already

- A conventional `a1 = bit 0` bitboard mapping.
- Piece-type and color bitboards with encapsulated mutation.
- Explicit move categories for castling, en passant, and promotion.
- FEN syntax checks and round-trip tests.
- Move/unmove tests cover all special move categories and restoration sequences.
- Invalid state transitions are checked before mutation.
- The code compiles cleanly with strict GCC warnings (`-Wall -Wextra -Wpedantic -Wconversion`), and both current test programs pass.

These are useful correctness choices for an early prototype.

## Immediate repository problems

### 1. The documented program does not exist

`README.md` claims there is a compilable UCI shell and gives CMake/CTest commands, but the tracked tree contains no `main.cpp`, UCI implementation, or `CMakeLists.txt`. The first engineering task should make the build, test, and feature documentation truthful. CI should prove the documented commands work.

### 2. Move application is unsuitable for a hot search loop

`Undo` stores eight complete bitboards and all position state (`src/board.h:106-121`). On this ABI, both `Position` and `Undo` are 80 bytes. Every node copies 64 bytes of bitboards in addition to other state (`src/board.cpp:283-292`), and unmake copies them all back (`src/board.cpp:334-342`).

For search, retain only irreversible/delta state: captured piece, castling rights, en-passant square, halfmove clock, and previous hash/evaluation accumulator. Reverse the piece movement directly. Keep expensive validation and snapshot-based checks available in debug/test builds, not in the release hot path.

### 3. `makeMove` is structural, not legal—and accepts many malformed transitions

This is acknowledged in the README, but it must be resolved before the API is used by UCI or search. Examples currently accepted include:

- a bishop/rook/knight moving in an impossible pattern;
- a pawn double-pushing from the wrong rank or through an occupied square;
- promotion on a non-promotion rank;
- en passant when the position's en-passant target does not match;
- castling with no castling right, through occupied squares, out of check, or through check;
- capturing the enemy king;
- any move that leaves the moving king in check.

The fast design is not to make release `makeMove` rediscover all geometry. Generate legal or pseudo-legal moves, apply them cheaply, and reject king-exposing moves. Provide a checked public/UCI parser and assertions in debug builds.

### 4. Position validity is only bitboard consistency

`Position::isValid()` (`src/board.cpp:158-180`) allows zero/multiple kings, pawns on ranks 1/8, impossible castling rights, and impossible en-passant state. Split the concept into:

- `isInternallyConsistent()` for cheap bitboard invariants;
- `isLegalPosition()` for semantic validation used by tests/debug/import boundaries.

FEN import should define whether it accepts syntactically valid study positions or only reachable/legal positions rather than implying full validation.

### 5. Representation lacks search state

A search-ready `Position` needs at least an incremental Zobrist key, repetition history, reliable king-square access, and usually a square-to-piece mailbox or piece lists. `pieceAt()` currently scans all six piece types (`src/board.cpp:184-189`). A compact mailbox makes captures, UCI parsing, SEE, and incremental evaluation cheaper and simpler while bitboards remain the primary move-generation representation.

### 6. Tests are a useful start but not an engine correctness gate

The tests use raw `assert`, which disappears under `NDEBUG`; there is no build system, CTest registration, perft, CI, sanitizer job, fuzzing, benchmark, or differential test. Current move tests intentionally use geometrically illegal moves in places, which is acceptable for a low-level mutation test but must be clearly separated from legal move tests.

## Recommended architecture and implementation order

Each stage should leave a working, tested artifact. Do not start evaluation or search until perft is exact.

### Phase 0 — build and quality gate

1. Add CMake targets for the engine/library and each test executable.
2. Add `ctest`, debug assertions, GCC/Clang warnings, ASan/UBSan CI, and release builds.
3. Add a deterministic `bench` command and record compiler/CPU/NPS with benchmark results.
4. Correct the README; do not advertise components before they exist.

**Exit condition:** a clean checkout builds and all tests run using the documented commands.

### Phase 1 — attack and legal move generation

1. Precompute pawn, knight, and king attack tables.
2. Implement bishop/rook/queen sliding attacks. Start with correct ray attacks; benchmark magic bitboards or BMI2 PEXT later rather than prematurely committing to one platform path.
3. Add `attackersTo`, `isSquareAttacked`, checkers, pins, and king-square tracking.
4. Generate all promotions, castling, and en passant correctly. En passant requires special discovered-check handling.
5. Add UCI move encode/decode and a compact move representation (16 or 32 bits).
6. Add perft and divide tests for the initial position and a broad standard suite, including castling, en passant, pinned pieces, checks, and promotions.

**Exit condition:** exact known perft counts through practical depths, plus make/unmake returns identical FEN and hash at every traversed node.

### Phase 2 — search-ready state and rules

1. Add incremental Zobrist hashing for pieces, side, castling, and relevant en-passant files.
2. Replace full-snapshot undo with delta undo.
3. Implement checkmate/stalemate, threefold repetition, fifty-move rule, and insufficient material.
4. Add static exchange evaluation (SEE).
5. Add a fixed-size transposition table with key verification, depth, bound type, score, age, and best move.
6. Handle mate score conversion correctly when storing/probing at different plies.

**Exit condition:** hash agrees with full recomputation under randomized move/unmove sequences; rule outcomes pass targeted tests.

### Phase 3 — first real playing engine

1. Implement fail-soft negamax alpha-beta with iterative deepening.
2. Add quiescence search for tactical captures/promotions, using SEE/delta pruning carefully.
3. Add principal-variation search and aspiration windows.
4. Order moves with TT move, winning captures, promotions, killers, counter-moves, and history scores.
5. Implement UCI fully: `position startpos|fen ... moves`, `go` limits, `stop`, `ponderhit`, `setoption`, and thread-safe cancellation.
6. Add robust time management with soft/hard deadlines and move overhead.

**Exit condition:** it completes games legally under a UCI tournament runner and has a reproducible Elo baseline.

### Phase 4 — evaluation

Start with a tapered, side-to-move-relative classical evaluation because it is debuggable and gives a training baseline:

- material and piece-square tables;
- mobility and trapped pieces;
- pawn structure: isolated, doubled, backward, connected, passed, candidate passers;
- bishop pair, rook files/seventh rank, knight outposts;
- king shelter and endgame king activity;
- threats, space, and initiative;
- game-phase interpolation and drawishness/scaling.

Tune parameters from positions and self-play rather than intuition alone (Texel-style fitting is a practical starting point). Once the complete pipeline is stable, an incrementally updated NNUE evaluator trained on high-quality search labels is likely the largest evaluation-strength step. Keep a classical fallback and test incremental accumulator correctness after every special move.

### Phase 5 — selective search strength

Add one change at a time and require statistically credible self-play gains:

- null-move pruning with zugzwang safeguards;
- late-move reductions with history/improving adjustments;
- reverse futility and ordinary futility pruning;
- razoring and delta pruning;
- mate-distance pruning;
- internal iterative reductions;
- check, recapture, passed-pawn, and singular extensions where testing supports them;
- correction histories and improved capture/continuation histories.

The exact formulas matter more than the feature names. Every heuristic must be tuned and tested; combining many unmeasured heuristics makes regressions impossible to diagnose.

### Phase 6 — production strength

- Lazy-SMP multi-threaded search and per-thread heuristics.
- Hash/thread options, NUMA-conscious allocation where relevant.
- Syzygy probing for exact endgames.
- Optional, transparent opening book only after core strength exists.
- Profile-guided optimization and platform-specific sliding attack paths selected at runtime/build time.
- Automated release binaries, reproducible benchmark, versioned network/weights, and tournament regression tests.

## Making aggression a real style rather than a weakness

### Define the objective

Use a lexicographic or constrained objective:

1. maximize playing strength/WDL;
2. among moves whose expected score is within a configurable margin, prefer the move with greater attacking potential.

Expose this as `UCI_LimitStrength`-independent `Aggression` (for example 0–100), with a neutral default suitable for rating tests. Never add a flat bonus for sacrifices, checks, or moving toward the king: those are easy to exploit and produce unsound play.

### Aggression features worth measuring

Build a separate, explainable **attack potential** score from features such as:

- weighted attacks into the enemy king ring, counting both attacker type and number of distinct attackers;
- safe checks, contact checks, mating-net constraints, and escape-square control;
- queen/rook alignment with the king and open/semi-open files toward it;
- pawn storms, especially opposite-side castling, with own-king risk included;
- space near the enemy king, piece tropism, and durable outposts;
- forcing threats, overloaded defenders, and favorable SEE sequences;
- initiative/tempo and the opponent's shortage of useful defensive moves;
- whether queens should remain on the board given the actual king-safety differential.

All features must be differential: an attack that opens the engine's own king should not score as aggression. Taper king-hunt terms sharply when queens/attacking material disappear.

### Search behavior for tactical courage

The engine must *see* compensation before it can sacrifice soundly:

- do not aggressively reduce checks, strong threats, advanced passed pawns, or moves entering a verified tactical sequence;
- search a carefully limited set of safe/forcing checks in quiescence;
- use SEE as an ordering/pruning tool, not an absolute ban on sacrifices;
- allow tactical moves with negative immediate SEE when deeper search demonstrates king attack or material recovery;
- preserve multiple root candidates with MultiPV during style selection;
- avoid pruning solely because a move temporarily loses material when it creates checks or severe threats.

These exceptions must remain narrow; disabling reductions for every nominally “aggressive” move will lose depth and Elo.

### Training an aggressive NNUE

A strong long-term approach is to train primarily on WDL/evaluation labels, then use either a small auxiliary attack-potential head or a style-aware root selector. Candidate training/self-play positions can be enriched for unbalanced material, opposite-side castling, exposed kings, and initiative. Do not distort the main value labels to declare objectively losing attacks good.

An aggressive opening repertoire can increase the frequency of suitable positions, but it is presentation/style, not a substitute for engine strength. Prefer objectively sound, unbalanced lines over dubious traps.

## Measurement plan

“Most aggressive” needs metrics as well as anecdotes. Maintain two test tracks:

### Strength gate

- Fixed hardware, time control, opening suite, colors reversed.
- Compare every search/evaluation change to the current baseline.
- Use SPRT or confidence intervals; do not merge based on a short winning streak.
- Include short-TC tests for iteration speed and longer-TC confirmation for tactical soundness.

### Style dashboard

At equal or bounded Elo loss, track:

- decisive-game rate and draw rate;
- average king-ring pressure and number of attackers;
- frequency of sound material investments (verified against deep neutral analysis);
- opposite-side castling/pawn-storm frequency;
- time spent with initiative and forcing moves available;
- queen-trade avoidance only when the attack score justifies it;
- tactical puzzle performance, especially sacrificial/mating suites;
- blunder rate and score loss of chosen attacks.

A useful release rule is: no “aggressive” change ships unless it either improves Elo, or improves a declared style metric while staying inside a predeclared Elo-loss budget.

## Highest-priority backlog

1. Add a truthful build/test/CI setup.
2. Implement attacks, legal move generation, UCI move parsing, and perft.
3. Add incremental hash, compact undo, repetition/rule handling, and TT.
4. Deliver a complete iterative-deepening alpha-beta UCI engine with time controls.
5. Establish benchmark and self-play/SPRT infrastructure.
6. Build/tune classical evaluation, then selective search.
7. Add an aggression score and root tie-break only after a neutral strength baseline exists.
8. Move to NNUE, SMP, and tablebases after correctness and measurement infrastructure are mature.

The key strategic recommendation is to treat **strength as the foundation of aggression**. Deep tactical search, accurate king-safety evaluation, and reliable compensation assessment create spectacular but sound attacks; arbitrary sacrifice bonuses create a personality that strong opponents punish.
