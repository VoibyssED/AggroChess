#include "transposition_table.h"

#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>

namespace aggrochess {
namespace {

constexpr std::size_t bytesPerMegabyte = 1024U * 1024U;

std::int16_t checkedInt16(int value, const char* name) {
    if (value < std::numeric_limits<std::int16_t>::min() ||
        value > std::numeric_limits<std::int16_t>::max()) {
        throw std::out_of_range(std::string(name) + " does not fit in a TT entry");
    }
    return static_cast<std::int16_t>(value);
}

}  // namespace

int scoreToTranspositionTable(int score, int ply) noexcept {
    if (score >= MateScoreThreshold) return score + ply;
    if (score <= -MateScoreThreshold) return score - ply;
    return score;
}

int scoreFromTranspositionTable(int score, int ply) noexcept {
    if (score >= MateScoreThreshold) return score - ply;
    if (score <= -MateScoreThreshold) return score + ply;
    return score;
}

TranspositionTable::TranspositionTable(std::size_t megabytes) {
    resize(megabytes);
}

std::size_t TranspositionTable::clusterCountForBytes(std::size_t bytes) noexcept {
    const std::size_t requested = std::max<std::size_t>(1, bytes / sizeof(Cluster));
    std::size_t powerOfTwo = 1;
    while (powerOfTwo <= requested / 2U) powerOfTwo *= 2U;
    return powerOfTwo;
}

void TranspositionTable::resize(std::size_t megabytes) {
    if (megabytes > std::numeric_limits<std::size_t>::max() / bytesPerMegabyte) {
        throw std::overflow_error("transposition-table size is too large");
    }
    const std::size_t clusters =
        clusterCountForBytes(megabytes * bytesPerMegabyte);
    std::unique_lock lock(tableMutex_);
    clusters_.assign(clusters, Cluster{});
    generation_.store(0, std::memory_order_relaxed);
}

void TranspositionTable::clear() {
    std::unique_lock lock(tableMutex_);
    std::fill(clusters_.begin(), clusters_.end(), Cluster{});
    generation_.store(0, std::memory_order_relaxed);
}

void TranspositionTable::newSearch() noexcept {
    generation_.fetch_add(1, std::memory_order_relaxed);
}

std::size_t TranspositionTable::index(ZobristKey key) const noexcept {
    return static_cast<std::size_t>(key) & (clusters_.size() - 1U);
}

void TranspositionTable::store(ZobristKey key, int depth, int score,
                               Bound bound, Move bestMove, int ply) {
    if (bound == Bound::None) {
        throw std::invalid_argument("cannot store a TT entry with no bound");
    }
    const std::int16_t packedDepth = checkedInt16(depth, "depth");
    const std::int16_t packedScore = checkedInt16(
        scoreToTranspositionTable(score, ply), "score");
    const std::uint8_t currentGeneration = generation();

    std::shared_lock tableLock(tableMutex_);
    const std::size_t clusterIndex = index(key);
    std::unique_lock stripeLock(stripes_[clusterIndex % LockStripeCount]);
    Cluster& cluster = clusters_[clusterIndex];

    PackedEntry* replacement = nullptr;
    int replacementQuality = std::numeric_limits<int>::max();
    for (PackedEntry& entry : cluster.entries) {
        if (entry.bound == Bound::None) {
            replacement = &entry;
            break;
        }
        if (entry.key == key) {
            if (bound != Bound::Exact && depth < entry.depth &&
                entry.generation == currentGeneration) {
                return;
            }
            replacement = &entry;
            break;
        }

        const std::uint8_t age = static_cast<std::uint8_t>(
            currentGeneration - entry.generation);
        const int quality = static_cast<int>(entry.depth) -
                            static_cast<int>(age) * 8;
        if (quality < replacementQuality) {
            replacementQuality = quality;
            replacement = &entry;
        }
    }

    *replacement = PackedEntry{key, packedScore, packedDepth, bestMove.raw(),
                               currentGeneration, bound};
}

std::optional<TranspositionEntry> TranspositionTable::probe(
    ZobristKey key, int ply) const {
    std::shared_lock tableLock(tableMutex_);
    const std::size_t clusterIndex = index(key);
    std::shared_lock stripeLock(stripes_[clusterIndex % LockStripeCount]);
    const Cluster& cluster = clusters_[clusterIndex];
    for (const PackedEntry& entry : cluster.entries) {
        if (entry.bound == Bound::None || entry.key != key) continue;
        return TranspositionEntry{
            entry.key,
            scoreFromTranspositionTable(entry.score, ply),
            entry.depth,
            Move::fromRaw(entry.move),
            entry.bound,
            entry.generation
        };
    }
    return std::nullopt;
}

std::size_t TranspositionTable::clusterCount() const {
    std::shared_lock lock(tableMutex_);
    return clusters_.size();
}

std::size_t TranspositionTable::entryCount() const {
    return clusterCount() * EntriesPerCluster;
}

std::size_t TranspositionTable::allocatedBytes() const {
    return clusterCount() * sizeof(Cluster);
}

}  // namespace aggrochess
