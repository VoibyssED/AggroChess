#pragma once

#include "board.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <vector>

namespace aggrochess {

enum class Bound : std::uint8_t { None, Upper, Lower, Exact };

inline constexpr int MateScore = 32000;
inline constexpr int MateScoreThreshold = 31000;

[[nodiscard]] int scoreToTranspositionTable(int score, int ply) noexcept;
[[nodiscard]] int scoreFromTranspositionTable(int score, int ply) noexcept;

struct TranspositionEntry {
    ZobristKey key = 0;
    int score = 0;
    int depth = 0;
    Move bestMove{};
    Bound bound = Bound::None;
    std::uint8_t generation = 0;
};

class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t megabytes = 16);

    void resize(std::size_t megabytes);
    void clear();
    void newSearch() noexcept;

    void store(ZobristKey key, int depth, int score, Bound bound,
               Move bestMove, int ply);
    [[nodiscard]] std::optional<TranspositionEntry> probe(
        ZobristKey key, int ply) const;

    [[nodiscard]] std::size_t clusterCount() const;
    [[nodiscard]] std::size_t entryCount() const;
    [[nodiscard]] std::size_t allocatedBytes() const;
    [[nodiscard]] std::uint8_t generation() const noexcept {
        return generation_.load(std::memory_order_relaxed);
    }

private:
    struct PackedEntry {
        ZobristKey key = 0;
        std::int16_t score = 0;
        std::int16_t depth = 0;
        std::uint16_t move = 0;
        std::uint8_t generation = 0;
        Bound bound = Bound::None;
    };
    static_assert(sizeof(PackedEntry) == 16,
                  "transposition entries must remain cache compact");

    static constexpr std::size_t EntriesPerCluster = 4;
    struct alignas(64) Cluster {
        std::array<PackedEntry, EntriesPerCluster> entries{};
    };
    static_assert(sizeof(Cluster) == 64,
                  "a transposition cluster should occupy one cache line");

    static constexpr std::size_t LockStripeCount = 256;

    [[nodiscard]] std::size_t index(ZobristKey key) const noexcept;
    [[nodiscard]] static std::size_t clusterCountForBytes(
        std::size_t bytes) noexcept;

    mutable std::shared_mutex tableMutex_;
    mutable std::array<std::shared_mutex, LockStripeCount> stripes_{};
    std::vector<Cluster> clusters_;
    std::atomic<std::uint8_t> generation_{0};
};

}  // namespace aggrochess
