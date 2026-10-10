# AggroChess implementation roadmap

This is the ordered execution list for the engine. Work proceeds on one item at
a time. When an item meets its exit criteria, mark it complete and move the
`NEXT` marker to the first unfinished item.

## Status legend

- ✅ **Complete** — implemented and verified.
- ▶️ **Active** — the single item labelled `NEXT` to implement next.
- ⏳ **Queued** — waiting for earlier dependencies.
- 🧪 **Experimental** — only retained if measurement shows a benefit.

## Ordered backlog

### Foundation and correctness

1. ✅ **CMake, CTest, and continuous integration**
   - C++20 CMake build, strict warnings, Debug and Release tests.
   - GCC/Clang GitHub Actions matrix and an ASan/UBSan job.
   - Truthful build and capability documentation.

2. ✅ **Attack detection and fully legal move generation**
   - Pawn, knight, king, bishop, rook, and queen attacks.
   - Check detection, pins through legality filtering, castling, en passant,
     and all promotions.
   - Standard perft coverage and differential legal-move testing.

### Search-ready position state

3. ✅ **Incremental Zobrist hashing**
   - Deterministic keys for every piece/square pair, side to move, castling
     rights, and en-passant file.
   - Keys update incrementally for normal moves, captures, promotions,
     en passant, castling, rights changes, and side changes.
   - Full recomputation, perft-tree, special-move, rejected-move, and
     deterministic randomized make/unmake tests verify exact restoration.

4. ✅ **Compact delta-based undo records**
   - Reduced `Undo` from 96 bytes to a guarded maximum of 24 bytes.
   - Piece movement is reversed directly for normal moves, castling, en
     passant, captures, and promotions; only irreversible deltas are stored.
   - Expensive position consistency validation remains enabled in Debug builds
     and is removed from the Release hot path.
   - Start-position perft depth 5 improved from a 721 ms median to 612 ms on
     the same local build and hardware (about 18% more nodes per second).

5. ✅ **Search-oriented position access**
   - Added incrementally maintained, constant-time king-square access.
   - Added a one-byte-per-square mailbox for constant-time piece type and color
     access; an interleaved depth-5 perft comparison measured about 3.9% more
     nodes per second despite the larger `Position` object.
   - Split representation invariants (`isInternallyConsistent`) from semantic
     chess validation (`isLegalPosition`), including king, pawn,
     promotion inventory, castling, en-passant, and check-state constraints.

6. ✅ **Move identity, notation, and parsing**
   - Added canonical equality plus explicit invalid and null move identities.
   - Kept hot-path moves at four bytes while providing a stable 16-bit raw
     encoding for future transposition-table storage.
   - Added UCI coordinate serialization and legal parsing for normal moves,
     captures, castling, en passant, and every promotion type.
   - External text is resolved against generated legal moves; malformed, null,
     incomplete-promotion, and position-illegal input is rejected.

7. ✅ **Game-state and draw adjudication**
   - Added checkmate, stalemate, fifty-move, threefold-repetition, and
     insufficient-material classification with checkmate precedence.
   - Added push/pop position-key history bounded by the reversible halfmove
     window and tested across reversible cycles and pawn-move boundaries.
   - En-passant hashing now affects repetition identity only when a legal
     en-passant capture exists, including pinned-pawn handling.
   - Precomputed pawn, knight, and king attack tables offset the additional
     identity checks; depth-5 perft measured about 19% faster than the previous
     roadmap state on the same local build and hardware.

8. ✅ **Static exchange evaluation (SEE)**
   - Added legal target-square exchange analysis with optimal capture/decline
     choices, dynamic x-ray discovery, and king-safety filtering.
   - Handles pinned attackers, king captures, en passant, quiet and capture
     promotions, underpromotion values, and competing recapture choices.
   - Verified against an independent legal-capture minimax oracle across 16,470
     randomly reached tactical moves; retained for diagnostics/ordering only.

9. ✅ **Transposition table**
   - Added resizable, power-of-two storage using four verified 16-byte entries
     per aligned 64-byte cluster, ready for a future UCI `Hash` option.
   - Stores depth, normalized score, upper/lower/exact bound, 16-bit move,
     generation, and full key with depth-and-age replacement.
   - Added ply-correct mate-score storage/probing plus clear, resize, generation,
     collision, replacement, and zero-key behavior.
   - Shared lifecycle locks and striped cluster locks pass concurrent
     store/probe/resize tests and a ThreadSanitizer run.

### First playable engine

10. ✅ **Classical tapered evaluation v1**
    - Added material, generated piece-square terms, mobility, isolated/doubled/
      connected/passed pawns, bishop pair, rook files/seventh rank, king shelter
      and ring pressure, space, and 24-step game-phase interpolation.
    - Exposes an explainable component breakdown plus symmetric white-relative
      and side-to-move scores, with colour/board symmetry and feature tests.
    - Added the first `AggroChess` executable: a truthful synchronous UCI
      preview that parses positions and chooses the best static one-ply move.

11. ▶️ **NEXT — Baseline alpha-beta search**
    - Fail-soft negamax, mate/stalemate/draw scores, mate-distance handling,
      node limits, and principal-variation reconstruction.
    - No selective pruning until the baseline is demonstrably correct.

12. ⏳ **Quiescence search**
    - Tactical captures and promotions with check-aware terminal handling.
    - SEE ordering and carefully tested delta pruning.
    - Tests for horizon-effect positions and bounded search behavior.

13. ⏳ **Iterative deepening, PVS, and aspiration windows**
    - Stable completed-iteration result and principal variation.
    - Safe fallback when an iteration is interrupted.
    - Aspiration widening on fail-low/fail-high.

14. ⏳ **Move ordering**
    - TT move, winning captures, promotions, killers, history,
      counter-moves, and continuation history.
    - Measure node-count reduction at fixed depth as well as elapsed time.

15. ⏳ **Complete UCI executable**
    - `uci`, `isready`, `ucinewgame`, `setoption`, `position`, `go`, `stop`,
      `ponderhit`, and `quit`.
    - Legal application of `position ... moves` and protocol-safe output.
    - Automated protocol and full-game smoke tests.

16. ⏳ **Time management and cancellation**
    - Move-time, clock/increment, moves-to-go, depth, node, mate, infinite,
      and ponder limits.
    - Soft and hard deadlines, configurable move overhead, and thread-safe
      stop handling.
    - Never lose on time in tournament smoke tests.

### Measurement and strength development

17. ⏳ **Deterministic benchmark and regression infrastructure**
    - Versioned position suite reporting nodes, time, NPS, depth, score, and
      benchmark signature.
    - Perft performance baseline and search benchmark in CI where stable.

18. ⏳ **Automated self-play and statistical testing**
    - Fixed opening suites with reversed colours and reproducible settings.
    - SPRT or confidence-interval-based acceptance instead of short-match
      impressions.
    - Track Elo, crashes, illegal moves, time losses, and draw rate.

19. 🧪 **Selective-search heuristics**
    - Introduce individually: null-move pruning, late-move reductions, reverse
      futility, futility, razoring, mate-distance pruning, internal iterative
      reductions, and narrowly justified extensions.
    - Keep only changes that pass correctness tests and statistical self-play.

20. 🧪 **Evaluation tuning**
    - Fit classical parameters from labelled positions and self-play.
    - Tune evaluation and search separately where possible.
    - Require held-out validation and Elo confirmation.

21. 🧪 **NNUE evaluation**
    - Reproducible data generation, training, network versioning, and loading.
    - Incremental accumulator updates for every special move.
    - Classical fallback and accumulator-vs-refresh correctness tests.

22. ⏳ **Parallel search**
    - Lazy-SMP worker model, per-thread search state, shared TT, and clean stop
      behavior.
    - `Threads` UCI option and scaling benchmarks.

23. ⏳ **Syzygy tablebases**
    - Configurable path and probe limits.
    - Correct WDL/DTZ handling, fifty-move interaction, and root selection.

24. 🧪 **Low-level performance optimization**
    - Profile before changing algorithms.
    - Consider precomputed leaper tables, magic bitboards or BMI2 PEXT,
      fixed-capacity move lists, prefetching, PGO, and platform dispatch.
    - Require benchmark improvement without perft or Elo regression.

### Aggressive identity

25. ⏳ **Aggression metrics and tactical test suite**
    - Measure king-ring pressure, number of attackers, forcing moves, pawn
      storms, initiative, sound material investments, decisive-game rate, and
      objective move loss.
    - Build a curated suite of attacking, defensive, and sacrificial positions.

26. 🧪 **Attack-potential evaluation**
    - Differential king-ring attacks, safe/contact checks, escape-square
      control, open lines toward the king, pawn storms, tropism, space, and
      defender overload.
    - Include own-king danger and game-phase scaling so aggression remains
      sound rather than becoming a sacrifice bonus.

27. 🧪 **Tactically aware search reductions and extensions**
    - Reduce forcing checks, severe threats, and critical passed-pawn moves less
      aggressively where testing supports it.
    - Permit negative-SEE sacrifices to be searched when forcing continuations
      justify them; do not exempt all checks or sacrifices from pruning.

28. 🧪 **Configurable root aggression policy**
    - Add an `Aggression` UCI option independent of strength limiting.
    - Maximize WDL first; among root moves inside a tested score margin, prefer
      greater attack potential.
    - Publish and enforce an Elo-loss budget for non-neutral settings.

29. 🧪 **Aggressive but sound opening repertoire**
    - Prefer objectively viable, unbalanced openings and opposite-side-castling
      structures.
    - Validate choices with deep neutral analysis and engine matches.
    - Keep the book optional and separate from intrinsic engine ratings.

30. 🧪 **Style-aware NNUE or auxiliary attack head**
    - Preserve objective WDL as the primary value target.
    - Use attack potential as an auxiliary signal or root tie-break, not as a
      replacement for correctness.
    - Retain only if it improves style metrics within the declared Elo budget.

## Ongoing rules for every item

- Add focused tests before or with behavior changes.
- Keep perft exact and run sanitizers for state/move changes.
- Record a benchmark before and after performance-sensitive work.
- Do not combine unrelated strength heuristics in one experiment.
- Update this file, the README, and exposed UCI documentation when an item is
  completed.
- There must be exactly one `NEXT` item at a time.
