#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <SDK/Minecraft/World/Chunk/ChunkSource.hpp>
#include <Utils/Structs.hpp>

class BlockSource;
class SubChunkBlockStorage;

namespace ChunkFindingUtils {

struct FoundBlock final {
    BlockPos position = BlockPos(0, 0, 0);
    int blockId = 0;
};

struct ScanRequest final {
    BlockSource* blockSource = nullptr;
    ChunkPos centerChunk = ChunkPos(0, 0);
    int scanRadiusChunks = 0;
    const std::unordered_set<int>* trackedBlockIds = nullptr;
    std::unordered_map<int, std::string>* discoveredBlockNamesById = nullptr;
    std::size_t maxSubChunksPerStep = 24;
};

struct CalibrationRequest final {
    BlockSource* blockSource = nullptr;
    ChunkPos originChunk = ChunkPos(0, 0);
    ChunkPos directionStep = ChunkPos(1, 0);
    int maxProbeDistanceChunks = 128;
    int probesPerStep = 4;
};

struct CalibrationStepResult final {
    bool active = false;
    bool finished = false;
    bool succeeded = false;
    int probeDistance = 0;
    int lastValidDistance = 1;
    int resolvedRadiusChunks = 1;
    float progress = 0.f;
};

class IncrementalScanner final {
public:
    void reset() noexcept;
    void begin(const ScanRequest& request);

    // Returns true once the scan is fully complete for the current request.
    bool step(std::vector<FoundBlock>& outFoundBlocks);
    [[nodiscard]] std::vector<ChunkPos> consumeRecentlyCompletedChunks();
    [[nodiscard]] const std::unordered_set<ChunkPos>& completedChunks() const noexcept { return mCompletedChunks; }

    [[nodiscard]] bool isActive() const noexcept { return mActive; }
    [[nodiscard]] const ScanRequest& request() const noexcept { return mRequest; }
    [[nodiscard]] std::size_t chunkCount() const noexcept { return mChunkOrder.size(); }
    [[nodiscard]] std::size_t chunkIndex() const noexcept { return mChunkOrderIndex; }

private:
    [[nodiscard]] bool hasValidRequest() const noexcept;
    void clampRequest() noexcept;
    void buildTrackedLookup();
    void buildDiscoveredLookup();
    [[nodiscard]] bool isDiscoveredBlockId(int blockId) const noexcept;
    void markDiscoveredBlockId(int blockId);
    void buildChunkOrder();
    void advanceChunk() noexcept;
    void scanSubChunk(
        SubChunkBlockStorage* reader,
        int chunkBaseX,
        int chunkBaseZ,
        int subChunkBaseY,
        std::vector<FoundBlock>& outFoundBlocks
    );

    ScanRequest mRequest{};
    std::vector<ChunkPos> mChunkOrder{};
    std::size_t mChunkOrderIndex = 0;
    std::size_t mCurrentSubChunkIndex = 0;
    std::vector<uint8_t> mTrackedIdLookup{};
    int mTrackedIdMax = 0;
    std::vector<uint8_t> mDiscoveredIdLookup{};
    int mDiscoveredIdMax = 0;
    std::vector<ChunkPos> mRecentlyCompletedChunks{};
    std::unordered_set<ChunkPos> mCompletedChunks{};
    bool mActive = false;
};

class ScanRadiusCalibrator final {
public:
    void reset() noexcept;
    bool begin(const CalibrationRequest& request);
    CalibrationStepResult step();

    [[nodiscard]] bool isActive() const noexcept { return mActive; }
    [[nodiscard]] int resolvedRadiusChunks() const noexcept { return mResolvedRadiusChunks; }
    [[nodiscard]] const CalibrationRequest& request() const noexcept { return mRequest; }

private:
    [[nodiscard]] bool hasValidRequest() const noexcept;
    void clampRequest() noexcept;
    [[nodiscard]] bool isChunkValid(const ChunkPos& chunkPos) const;

    CalibrationRequest mRequest{};
    int mProbeDistance = 0;
    int mLastValidDistance = 1;
    int mResolvedRadiusChunks = 1;
    bool mActive = false;
};

} // namespace ChunkFindingUtils
