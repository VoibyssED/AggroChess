# AggroChess

AggroChess is intended to become a strong, aggressively minded UCI chess
engine. Development is incremental: every engine component is introduced only
after its public behavior has focused unit tests and, where applicable, perft
coverage.

## Current milestone: bitboard board representation

The board layer uses six piece-type bitboards and two color bitboards. The
square convention is fixed: `a1 = 0`, `b1 = 1`, through `h1 = 7`; `a2 = 8`;
and `h8 = 63`. A square mask is `1ULL << squareIndex(square)`.

`Position::fromFen` validates all six FEN fields and rejects malformed board
layouts, side-to-move values, castling rights, en-passant squares, and move
counters. `Position::toFen` emits the canonical six-field representation.
This layer intentionally does not perform move generation, attack detection,
or legality checking.

The mutation layer now provides `Move`, `Undo`, `makeMove`, and `unmakeMove`.
`Undo` stores the complete pre-move bitboards and state fields rather than
trying to infer them during restoration. Move kinds explicitly distinguish
quiet moves, captures, pawn double pushes, en-passant, castling, promotions,
and promotion captures. Structural transitions update castling rights,
en-passant state, and FEN counters; legality remains a later milestone.
The low-level piece-placement helpers are private to `Position`; external code
must use `makeMove` and `unmakeMove` so board invariants cannot be bypassed.
Structural validation is completed before mutation, so rejected moves leave
the original position unchanged.

## UCI shell

The project currently provides a compilable UCI protocol shell. It supports:

- `uci` / `uciok`
- `isready` / `readyok`
- `ucinewgame`
- `position` (recorded but not parsed yet)
- `go` (returns the temporary `bestmove 0000`)
- `stop`
- `quit`

`bestmove 0000` is intentional: legal move generation and board state are not
implemented yet, so the shell must not claim a move is legal.

## Build and test

From the repository root:

```sh
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Move generation, make/unmake, hashing, evaluation, and search will be added in
separate increments rather than as untested placeholders.
