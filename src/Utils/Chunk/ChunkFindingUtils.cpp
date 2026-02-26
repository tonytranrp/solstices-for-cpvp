#include "ChunkFindingUtils.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <limits>
#include <string>
#include <tuple>
#include <utility>

#include <SDK/Minecraft/World/Block.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/World/Chunk/LevelChunk.hpp>
#include <SDK/Minecraft/World/Chunk/SubChunkBlockStorage.hpp>
#include <Utils/MemUtils.hpp>

namespace {
    template <typename T>
    [[nodiscard]] bool isNullOrSentinel(const T* ptr) noexcept
    {
        if (ptr == nullptr) {
            return true;
        }

        return reinterpret_cast<uintptr_t>(ptr) == std::numeric_limits<uintptr_t>::max();
    }

    [[nodiscard]] std::string normalizeBlockName(std::string blockName)
    {
        auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };

        const auto begin = std::find_if(blockName.begin(), blockName.end(), isNotSpace);
        const auto end = std::find_if(blockName.rbegin(), blockName.rend(), isNotSpace).base();
        if (begin >= end)
        {
            return {};
        }

        blockName = std::string(begin, end);
        std::ranges::transform(blockName, blockName.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (blockName.starts_with("minecraft:"))
        {
            blockName.erase(0, std::string("minecraft:").size());
        }

        if (blockName.starts_with("tile."))
        {
            blockName.erase(0, std::string("tile.").size());
        }

        return blockName;
    }
}

namespace ChunkFindingUtils {

void IncrementalScanner::reset() noexcept
{
    mRequest = {};
    mChunkOrder.clear();
    mChunkOrderIndex = 0;
    mCurrentSubChunkIndex = 0;
    mTrackedIdLookup.clear();
    mTrackedIdMax = 0;
    mDiscoveredIdLookup.clear();
    mDiscoveredIdMax = 0;
    mRecentlyCompletedChunks.clear();
    mCompletedChunks.clear();
    mActive = false;
}

bool IncrementalScanner::hasValidRequest() const noexcept
{
    const bool hasTrackedFilter = mRequest.trackedBlockIds != nullptr && !mRequest.trackedBlockIds->empty();
    const bool shouldDiscoverNames = mRequest.discoveredBlockNamesById != nullptr;

    return mRequest.blockSource != nullptr
        && (hasTrackedFilter || shouldDiscoverNames)
        && mRequest.scanRadiusChunks > 0;
}

void IncrementalScanner::clampRequest() noexcept
{
    mRequest.scanRadiusChunks = std::max(1, mRequest.scanRadiusChunks);
    mRequest.maxSubChunksPerStep = std::max<std::size_t>(1, mRequest.maxSubChunksPerStep);
}

void IncrementalScanner::buildTrackedLookup()
{
    mTrackedIdLookup.clear();
    mTrackedIdMax = 0;

    if (mRequest.trackedBlockIds == nullptr || mRequest.trackedBlockIds->empty())
    {
        return;
    }

    for (const int blockId : *mRequest.trackedBlockIds)
    {
        if (blockId > mTrackedIdMax)
        {
            mTrackedIdMax = blockId;
        }
    }

    if (mTrackedIdMax <= 0)
    {
        mTrackedIdMax = 0;
        return;
    }

    mTrackedIdLookup.resize(static_cast<std::size_t>(mTrackedIdMax) + 1, static_cast<uint8_t>(0));
    for (const int blockId : *mRequest.trackedBlockIds)
    {
        if (blockId <= 0 || blockId > mTrackedIdMax)
        {
            continue;
        }

        mTrackedIdLookup[static_cast<std::size_t>(blockId)] = static_cast<uint8_t>(1);
    }
}

void IncrementalScanner::buildDiscoveredLookup()
{
    mDiscoveredIdLookup.clear();
    mDiscoveredIdMax = 0;

    if (mRequest.discoveredBlockNamesById == nullptr || mRequest.discoveredBlockNamesById->empty())
    {
        return;
    }

    for (const auto& [blockId, blockName] : *mRequest.discoveredBlockNamesById)
    {
        (void)blockName;
        if (blockId > mDiscoveredIdMax)
        {
            mDiscoveredIdMax = blockId;
        }
    }

    if (mDiscoveredIdMax <= 0)
    {
        mDiscoveredIdMax = 0;
        return;
    }

    mDiscoveredIdLookup.resize(static_cast<std::size_t>(mDiscoveredIdMax) + 1, static_cast<uint8_t>(0));
    for (const auto& [blockId, blockName] : *mRequest.discoveredBlockNamesById)
    {
        (void)blockName;
        if (blockId <= 0 || blockId > mDiscoveredIdMax)
        {
            continue;
        }

        mDiscoveredIdLookup[static_cast<std::size_t>(blockId)] = static_cast<uint8_t>(1);
    }
}

bool IncrementalScanner::isDiscoveredBlockId(const int blockId) const noexcept
{
    if (blockId <= 0 || blockId > mDiscoveredIdMax || mDiscoveredIdLookup.empty())
    {
        return false;
    }

    return mDiscoveredIdLookup[static_cast<std::size_t>(blockId)] != 0;
}

void IncrementalScanner::markDiscoveredBlockId(const int blockId)
{
    if (blockId <= 0)
    {
        return;
    }

    if (blockId > mDiscoveredIdMax)
    {
        const int oldMax = mDiscoveredIdMax;
        mDiscoveredIdMax = blockId;
        mDiscoveredIdLookup.resize(static_cast<std::size_t>(mDiscoveredIdMax) + 1, static_cast<uint8_t>(0));
        if (oldMax <= 0)
        {
            std::fill(mDiscoveredIdLookup.begin(), mDiscoveredIdLookup.end(), static_cast<uint8_t>(0));
        }
    }

    mDiscoveredIdLookup[static_cast<std::size_t>(blockId)] = static_cast<uint8_t>(1);
}

void IncrementalScanner::buildChunkOrder()
{
    struct Offset final {
        int dx = 0;
        int dz = 0;
    };

    const int radius = mRequest.scanRadiusChunks;
    const std::size_t sideLength = static_cast<std::size_t>(radius * 2 + 1);
    std::vector<Offset> offsets;
    offsets.reserve(sideLength * sideLength);

    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            offsets.push_back(Offset{.dx = dx, .dz = dz});
        }
    }

    std::ranges::sort(offsets, [](const Offset& a, const Offset& b) {
        const auto rank = [](const Offset& o) {
            const int ring = std::max(std::abs(o.dx), std::abs(o.dz));
            const int manhattan = std::abs(o.dx) + std::abs(o.dz);
            return std::tuple{ring, manhattan, o.dx, o.dz};
        };
        return rank(a) < rank(b);
    });

    mChunkOrder.clear();
    mChunkOrder.reserve(offsets.size());
    for (const auto& offset : offsets) {
        mChunkOrder.emplace_back(
            mRequest.centerChunk.x + offset.dx,
            mRequest.centerChunk.y + offset.dz
        );
    }

    mChunkOrderIndex = 0;
    mCurrentSubChunkIndex = 0;
}

void IncrementalScanner::begin(const ScanRequest& request)
{
    mRequest = request;
    clampRequest();

    if (!hasValidRequest()) {
        reset();
        return;
    }

    buildTrackedLookup();
    buildDiscoveredLookup();
    buildChunkOrder();
    mRecentlyCompletedChunks.clear();
    mActive = !mChunkOrder.empty();
}

std::vector<ChunkPos> IncrementalScanner::consumeRecentlyCompletedChunks()
{
    std::vector<ChunkPos> recent = std::move(mRecentlyCompletedChunks);
    mRecentlyCompletedChunks.clear();
    return recent;
}

void IncrementalScanner::advanceChunk() noexcept
{
    if (mChunkOrderIndex + 1 < mChunkOrder.size()) {
        ++mChunkOrderIndex;
        return;
    }

    mChunkOrderIndex = mChunkOrder.size();
    mActive = false;
}

void IncrementalScanner::scanSubChunk(
    SubChunkBlockStorage* reader,
    const int chunkBaseX,
    const int chunkBaseZ,
    const int subChunkBaseY,
    std::vector<FoundBlock>& outFoundBlocks
)
{
    if (isNullOrSentinel(reader) || !hasValidRequest()) {
        return;
    }

    const bool subChunkOk = TryCallWrapper([&]() {
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    const uint16_t elementId = static_cast<uint16_t>((x * 0x10 + z) * 0x10 + y);
                    Block* block = reader->getElement(elementId);
                    if (isNullOrSentinel(block)) {
                        continue;
                    }

                    BlockLegacy* legacy = block->mLegacy;
                    if (isNullOrSentinel(legacy)) {
                        continue;
                    }

                    const int blockId = legacy->getBlockId();
                    if (blockId <= 0) {
                        continue;
                    }

                    if (mRequest.discoveredBlockNamesById != nullptr && !isDiscoveredBlockId(blockId))
                    {
                        const std::string normalizedName = normalizeBlockName(legacy->mName);
                        if (!normalizedName.empty())
                        {
                            (*mRequest.discoveredBlockNamesById)[blockId] = normalizedName;
                            markDiscoveredBlockId(blockId);
                        }
                    }

                    if (blockId > mTrackedIdMax || mTrackedIdLookup.empty()) {
                        continue;
                    }

                    if (mTrackedIdLookup[static_cast<std::size_t>(blockId)] == 0) {
                        continue;
                    }

                    outFoundBlocks.push_back(FoundBlock{
                        .position = BlockPos(chunkBaseX + x, subChunkBaseY + y, chunkBaseZ + z),
                        .blockId = blockId,
                    });
                }
            }
        }
    });

    (void)subChunkOk;
}

bool IncrementalScanner::step(std::vector<FoundBlock>& outFoundBlocks)
{
    if (!mActive) {
        return true;
    }

    if (!hasValidRequest()) {
        reset();
        return true;
    }

    std::size_t processedSubChunks = 0;
    const std::size_t budget = mRequest.maxSubChunksPerStep;

    while (mActive && processedSubChunks < budget) {
        bool chunkComplete = true;
        bool chunkWasScanned = false;
        ChunkPos currentChunkPos{};

    const bool chunkOk = TryCallWrapper([&]() {
            if (mChunkOrderIndex >= mChunkOrder.size()) {
                chunkComplete = true;
                return;
            }

            const ChunkPos chunkPos = mChunkOrder[mChunkOrderIndex];
            currentChunkPos = chunkPos;
            LevelChunk* chunk = mRequest.blockSource->getChunk(chunkPos);
            if (isNullOrSentinel(chunk) || chunk->isLoading) {
                return;
            }

            std::vector<SubChunk>* subChunks = chunk->getSubChunks();
            if (isNullOrSentinel(subChunks) || subChunks->empty()) {
                return;
            }
            chunkWasScanned = true;

            const int baseX = chunkPos.x * 16;
            const int baseZ = chunkPos.y * 16;

            while (mCurrentSubChunkIndex < subChunks->size() && processedSubChunks < budget) {
                const auto& subChunk = (*subChunks)[mCurrentSubChunkIndex++];
                scanSubChunk(
                    subChunk.blockReadPtr,
                    baseX,
                    baseZ,
                    static_cast<int>(subChunk.subchunkIndex) * 16,
                    outFoundBlocks
                );

                ++processedSubChunks;
            }

            if (mCurrentSubChunkIndex < subChunks->size()) {
                chunkComplete = false;
            }
        });

        if (!chunkOk) {
            chunkComplete = true;
        }

        if (chunkComplete) {
            if (chunkWasScanned)
            {
                if (mCompletedChunks.insert(currentChunkPos).second)
                {
                    mRecentlyCompletedChunks.push_back(currentChunkPos);
                }
            }

            mCurrentSubChunkIndex = 0;
            advanceChunk();
        }
    }

    return !mActive;
}

void ScanRadiusCalibrator::reset() noexcept
{
    mRequest = {};
    mProbeDistance = 0;
    mLastValidDistance = 1;
    mResolvedRadiusChunks = 1;
    mActive = false;
}

bool ScanRadiusCalibrator::hasValidRequest() const noexcept
{
    return mRequest.blockSource != nullptr
        && mRequest.maxProbeDistanceChunks > 0
        && mRequest.probesPerStep > 0;
}

void ScanRadiusCalibrator::clampRequest() noexcept
{
    mRequest.maxProbeDistanceChunks = std::max(1, mRequest.maxProbeDistanceChunks);
    mRequest.probesPerStep = std::max(1, mRequest.probesPerStep);

    if (mRequest.directionStep.x == 0 && mRequest.directionStep.y == 0)
    {
        mRequest.directionStep = ChunkPos(0, 1);
    }
}

bool ScanRadiusCalibrator::isChunkValid(const ChunkPos& chunkPos) const
{
    if (isNullOrSentinel(mRequest.blockSource))
    {
        return false;
    }

    LevelChunk* chunk = mRequest.blockSource->getChunk(chunkPos);
    if (isNullOrSentinel(chunk) || chunk->isLoading)
    {
        return false;
    }

    std::vector<SubChunk>* subChunks = chunk->getSubChunks();
    if (isNullOrSentinel(subChunks) || subChunks->empty())
    {
        return false;
    }

    for (const auto& subChunk : *subChunks)
    {
        if (!isNullOrSentinel(subChunk.blockReadPtr))
        {
            return true;
        }
    }

    return false;
}

bool ScanRadiusCalibrator::begin(const CalibrationRequest& request)
{
    mRequest = request;
    clampRequest();
    if (!hasValidRequest())
    {
        reset();
        return false;
    }

    mProbeDistance = 0;
    mLastValidDistance = 1;
    mResolvedRadiusChunks = 1;
    mActive = true;
    return true;
}

CalibrationStepResult ScanRadiusCalibrator::step()
{
    CalibrationStepResult result{};
    result.active = mActive;
    result.probeDistance = mProbeDistance;
    result.lastValidDistance = mLastValidDistance;
    result.resolvedRadiusChunks = mResolvedRadiusChunks;
    result.progress = static_cast<float>(mProbeDistance) / static_cast<float>(std::max(1, mRequest.maxProbeDistanceChunks));

    if (!mActive)
    {
        result.finished = true;
        result.succeeded = true;
        result.progress = 1.f;
        return result;
    }

    if (!hasValidRequest())
    {
        mActive = false;
        result.active = false;
        result.finished = true;
        result.succeeded = false;
        result.resolvedRadiusChunks = mResolvedRadiusChunks;
        return result;
    }

    bool finished = false;
    for (int sampleIndex = 0; sampleIndex < mRequest.probesPerStep && !finished; ++sampleIndex)
    {
        const int probeDistance = mProbeDistance + 1;
        if (probeDistance > mRequest.maxProbeDistanceChunks)
        {
            mResolvedRadiusChunks = std::max(1, mLastValidDistance);
            finished = true;
            break;
        }

        const ChunkPos probeChunk(
            mRequest.originChunk.x + mRequest.directionStep.x * probeDistance,
            mRequest.originChunk.y + mRequest.directionStep.y * probeDistance);

        mProbeDistance = probeDistance;
        if (isChunkValid(probeChunk))
        {
            mLastValidDistance = probeDistance;
        }
        else
        {
            mResolvedRadiusChunks = std::max(1, mLastValidDistance);
            finished = true;
        }
    }

    if (finished)
    {
        mActive = false;
    }

    result.active = mActive;
    result.finished = finished;
    result.succeeded = true;
    result.probeDistance = mProbeDistance;
    result.lastValidDistance = mLastValidDistance;
    result.resolvedRadiusChunks = std::max(1, mResolvedRadiusChunks);
    result.progress = std::clamp(
        static_cast<float>(mProbeDistance) / static_cast<float>(std::max(1, mRequest.maxProbeDistanceChunks)),
        0.f,
        1.f);

    if (finished)
    {
        result.progress = 1.f;
    }

    return result;
}

} // namespace ChunkFindingUtils
