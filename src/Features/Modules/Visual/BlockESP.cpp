#include "BlockESP.hpp"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <limits>

#include <Features/Events/BlockChangedEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Events/ProgressOverlayEvent.hpp>
#include <Features/FeatureManager.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerActionPacket.hpp>
#include <SDK/Minecraft/World/Block.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/World/Chunk/LevelChunk.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>
#include <Utils/MiscUtils/RenderUtils.hpp>
#include <Utils/MemUtils.hpp>
#include <Utils/StringUtils.hpp>
#include <spdlog/spdlog.h>

namespace {
    constexpr int REDSTONE_ORE = 73;
    constexpr int REDSTONE_ORE_LIT = 74;
    constexpr int DIAMOND_ORE = 56;
    constexpr int EMERALD_ORE = 129;
    constexpr int GOLD_ORE = 14;
    constexpr int IRON_ORE = 15;
    constexpr int LAPIS_ORE = 21;
    constexpr int COAL_ORE = 16;
    constexpr int DEEPSLATE_REDSTONE_ORE = 658;
    constexpr int DEEPSLATE_LIT_REDSTONE_ORE = 659;
    constexpr int DEEPSLATE_DIAMOND_ORE = 660;
    constexpr int DEEPSLATE_EMERALD_ORE = 662;
    constexpr int DEEPSLATE_GOLD_ORE = 657;
    constexpr int DEEPSLATE_IRON_ORE = 656;
    constexpr int DEEPSLATE_LAPIS_ORE = 655;
    constexpr int DEEPSLATE_COAL_ORE = 661;
    constexpr int PORTAL = 90;
    constexpr int CHEST = 54;
    constexpr int ENDER_CHEST = 130;
    constexpr int TRAPPED_CHEST = 146;
    constexpr int BARREL = 458;

    constexpr uint64_t kRescanIntervalMs = 10000;
    constexpr int kDefaultScanRadiusChunks = 8;
    constexpr int kCalibrationSamplesPerTick = 4;
    constexpr int kMaxCalibrationProbeChunks = 128;
    constexpr const char* kCalibrationOverlayId = "blockesp.calibration";

    const std::unordered_map<int, ImColor> kBlockColors = {
        {REDSTONE_ORE, ImColor(1.f, 0.f, 0.f, 1.f)},
        {REDSTONE_ORE_LIT, ImColor(1.f, 0.f, 0.f, 1.f)},
        {DEEPSLATE_REDSTONE_ORE, ImColor(1.f, 0.f, 0.f, 1.f)},
        {DEEPSLATE_LIT_REDSTONE_ORE, ImColor(1.f, 0.f, 0.f, 1.f)},
        {DIAMOND_ORE, ImColor(0.f, 1.f, 1.f, 1.f)},
        {DEEPSLATE_DIAMOND_ORE, ImColor(0.f, 1.f, 1.f, 1.f)},
        {EMERALD_ORE, ImColor(0.f, 1.f, 0.f, 1.f)},
        {DEEPSLATE_EMERALD_ORE, ImColor(0.f, 1.f, 0.f, 1.f)},
        {GOLD_ORE, ImColor(1.f, 1.f, 0.f, 1.f)},
        {DEEPSLATE_GOLD_ORE, ImColor(1.f, 1.f, 0.f, 1.f)},
        {IRON_ORE, ImColor(1.f, 0.5f, 0.f, 1.f)},
        {DEEPSLATE_IRON_ORE, ImColor(1.f, 0.5f, 0.f, 1.f)},
        {LAPIS_ORE, ImColor(0.f, 0.f, 1.f, 1.f)},
        {DEEPSLATE_LAPIS_ORE, ImColor(0.f, 0.f, 1.f, 1.f)},
        {COAL_ORE, ImColor(0.f, 0.f, 0.f, 1.f)},
        {DEEPSLATE_COAL_ORE, ImColor(0.f, 0.f, 0.f, 1.f)},
        {PORTAL, ImColor(0.5f, 0.f, 0.5f, 1.f)},
        {CHEST, ImColor(255, 165, 0)},
        {ENDER_CHEST, ImColor(0.5f, 0.f, 0.5f, 1.f)},
        {TRAPPED_CHEST, ImColor(255, 165, 0)},
        {BARREL, ImColor(255, 165, 0)},
    };

    template <typename T>
    [[nodiscard]] bool isNullOrSentinel(const T* ptr) noexcept
    {
        if (ptr == nullptr) {
            return true;
        }

        return reinterpret_cast<uintptr_t>(ptr) == std::numeric_limits<uintptr_t>::max();
    }

    [[nodiscard]] uint64_t currentTimeMs()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::high_resolution_clock::now().time_since_epoch())
            .count();
    }

    [[nodiscard]] std::string normalizeBlockName(std::string blockName)
    {
        const std::string trimmedBlockName(StringUtils::trim(blockName));
        blockName = StringUtils::toLower(trimmedBlockName);
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

    [[nodiscard]] std::size_t resolveSubChunkBudgetPerTick(const int scanRadiusChunks)
    {
        const std::size_t side = static_cast<std::size_t>(scanRadiusChunks * 2 + 1);
        const std::size_t chunkCount = side * side;
        constexpr std::size_t estimatedSubChunksPerChunk = 24;
        constexpr std::size_t targetTicksToComplete = 260;
        const std::size_t chunksPerTick = std::max<std::size_t>(1, chunkCount / targetTicksToComplete);

        return std::clamp(
            chunksPerTick * estimatedSubChunksPerChunk,
            static_cast<std::size_t>(24),
            static_cast<std::size_t>(768)
        );
    }

    [[nodiscard]] ChunkPos resolveForwardChunkStep(Actor* player)
    {
        if (isNullOrSentinel(player))
        {
            return ChunkPos(0, 1);
        }

        const auto* rotation = player->getActorRotationComponent();
        if (isNullOrSentinel(rotation))
        {
            return ChunkPos(0, 1);
        }

        const float yawRadians = glm::radians(rotation->mYaw);
        const float forwardX = -std::sin(yawRadians);
        const float forwardZ = std::cos(yawRadians);

        int stepX = 0;
        int stepZ = 0;
        if (std::abs(forwardX) >= 0.35f)
        {
            stepX = forwardX > 0.f ? 1 : -1;
        }
        if (std::abs(forwardZ) >= 0.35f)
        {
            stepZ = forwardZ > 0.f ? 1 : -1;
        }

        if (stepX == 0 && stepZ == 0)
        {
            if (std::abs(forwardX) > std::abs(forwardZ))
            {
                stepX = forwardX >= 0.f ? 1 : -1;
            }
            else
            {
                stepZ = forwardZ >= 0.f ? 1 : -1;
            }
        }

        if (stepX == 0 && stepZ == 0)
        {
            stepZ = 1;
        }

        return ChunkPos(stepX, stepZ);
    }

    void pushCalibrationOverlayOpen(const std::string& status)
    {
        if (gFeatureManager == nullptr || gFeatureManager->mDispatcher == nullptr)
        {
            return;
        }

        auto holder = nes::make_holder<ProgressOverlayEvent>(
            ProgressOverlayEvent::Action::Open,
            kCalibrationOverlayId,
            "Calibrating BlockESP For Max Distance...",
            status,
            0.f);
        gFeatureManager->mDispatcher->trigger(holder);
    }

    void pushCalibrationOverlayUpdate(const float progress, const std::string& status)
    {
        if (gFeatureManager == nullptr || gFeatureManager->mDispatcher == nullptr)
        {
            return;
        }

        auto holder = nes::make_holder<ProgressOverlayEvent>(
            ProgressOverlayEvent::Action::Update,
            kCalibrationOverlayId,
            "",
            status,
            progress);
        gFeatureManager->mDispatcher->trigger(holder);
    }

    void pushCalibrationOverlayClose()
    {
        if (gFeatureManager == nullptr || gFeatureManager->mDispatcher == nullptr)
        {
            return;
        }

        auto holder = nes::make_holder<ProgressOverlayEvent>(
            ProgressOverlayEvent::Action::Close,
            kCalibrationOverlayId);
        gFeatureManager->mDispatcher->trigger(holder);
    }
}

BlockESP::BlockESP()
    : ModuleBase("BlockESP", "Safe chunk scanner for tracked blocks", ModuleCategory::Visual, 0, false)
{
    addSettings(
        &mTrackedBlocks,
        &mCalibrateDistance
    );

    mNames = {
        {Lowercase, "blockesp"},
        {LowercaseSpaced, "block esp"},
        {Normal, "BlockESP"},
        {NormalSpaced, "Block ESP"},
    };

    mTrackedBlocks.setRefreshCallback([this](ListSetting&) {
        syncTrackableBlockOptions();
    });
    mCalibrateDistance.setOnClick([this]() {
        startCalibration();
    });

    mAdaptiveScanRadiusChunks = kDefaultScanRadiusChunks;
    mCalibrationMaxProbeDistance = kMaxCalibrationProbeChunks;
    syncTrackableBlockOptions();
    refreshTrackedBlocksFromSettings();
}

ImColor BlockESP::colorForBlockId(const int blockId)
{
    if (const auto it = kBlockColors.find(blockId); it != kBlockColors.end()) {
        return it->second;
    }
    return ImColor(1.f, 1.f, 1.f, 1.f);
}

std::vector<int> BlockESP::getTrackedBlockIds() const
{
    std::vector<int> trackedIds;
    trackedIds.reserve(mTrackedBlockIdSet.size());
    for (const int blockId : mTrackedBlockIdSet) {
        trackedIds.push_back(blockId);
    }

    std::ranges::sort(trackedIds);
    return trackedIds;
}

bool BlockESP::isCalibrating() const noexcept
{
    return mCalibrationActive;
}

float BlockESP::calibrationProgress() const noexcept
{
    return mCalibrationProgress;
}

int BlockESP::calibratedScanRadius() const noexcept
{
    return mAdaptiveScanRadiusChunks;
}

int BlockESP::currentScanRadiusChunks() const noexcept
{
    return std::max(1, mAdaptiveScanRadiusChunks);
}

void BlockESP::startCalibration()
{
    if (mCalibrationActive)
    {
        return;
    }

    if (!mEnabled)
    {
        spdlog::debug("BlockESP calibration ignored because module is disabled.");
        return;
    }

    auto* ci = ClientInstance::get();
    if (ci == nullptr)
    {
        return;
    }

    auto* player = ci->getLocalPlayer();
    auto* blockSource = ci->getBlockSource();
    if (isNullOrSentinel(player) || isNullOrSentinel(blockSource))
    {
        return;
    }

    const ChunkPos originChunk(*player->getPos());
    const ChunkPos directionStep = resolveForwardChunkStep(player);
    const bool started = mScanRadiusCalibrator.begin(ChunkFindingUtils::CalibrationRequest{
        .blockSource = blockSource,
        .originChunk = originChunk,
        .directionStep = directionStep,
        .maxProbeDistanceChunks = std::max(1, mCalibrationMaxProbeDistance),
        .probesPerStep = kCalibrationSamplesPerTick,
    });
    if (!started)
    {
        spdlog::warn("BlockESP calibration failed to start due to invalid chunk calibration request.");
        return;
    }

    mCalibrationActive = true;
    mCalibrationProgress = 0.f;
    mCalibrationLastProbeDistance = 0;
    mCalibrationStartTime = currentTimeMs();
    mCalibrationMaxProbeDistance = std::max(mCalibrationMaxProbeDistance, 1);

    pushCalibrationOverlayOpen(
        "Progress: 0%  |  Radius: " + std::to_string(mAdaptiveScanRadiusChunks) + " chunks");

    spdlog::info(
        "BlockESP calibration started: origin=({}, {}), direction=({}, {}), maxProbe={}",
        originChunk.x,
        originChunk.y,
        directionStep.x,
        directionStep.y,
        mCalibrationMaxProbeDistance);
}

void BlockESP::updateCalibration()
{
    if (!mCalibrationActive)
    {
        return;
    }

    auto* ci = ClientInstance::get();
    if (ci == nullptr)
    {
        mScanRadiusCalibrator.reset();
        mCalibrationActive = false;
        mCalibrationProgress = 0.f;
        pushCalibrationOverlayClose();
        return;
    }

    auto* currentBlockSource = ci->getBlockSource();
    if (isNullOrSentinel(currentBlockSource) || currentBlockSource != mScanRadiusCalibrator.request().blockSource)
    {
        mScanRadiusCalibrator.reset();
        mCalibrationActive = false;
        mCalibrationProgress = 0.f;
        mCalibrationLastProbeDistance = 0;
        pushCalibrationOverlayClose();
        spdlog::warn("BlockESP calibration aborted because block source changed during calibration.");
        return;
    }

    const ChunkFindingUtils::CalibrationStepResult result = mScanRadiusCalibrator.step();
    if (!result.succeeded && result.finished)
    {
        mCalibrationActive = false;
        mCalibrationProgress = 0.f;
        mCalibrationLastProbeDistance = 0;
        pushCalibrationOverlayClose();
        spdlog::warn("BlockESP calibration aborted because chunk calibration context became invalid.");
        return;
    }

    mCalibrationLastProbeDistance = result.probeDistance;
    mCalibrationProgress = std::clamp(result.progress, 0.f, 1.f);
    const int percent = static_cast<int>(std::round(mCalibrationProgress * 100.f));
    pushCalibrationOverlayUpdate(
        mCalibrationProgress,
        "Progress: " + std::to_string(percent) + "%  |  Radius: " + std::to_string(std::max(1, result.lastValidDistance)) + " chunks");

    if (!result.finished)
    {
        return;
    }

    mAdaptiveScanRadiusChunks = std::max(1, result.resolvedRadiusChunks);
    mCalibrationActive = false;
    mCalibrationProgress = 1.f;
    mForceFullScan = true;
    mChunkScanner.reset();
    mHasCommittedScan = false;
    mCommitIncrementalThisScan = false;
    mLastScanRequest = 0;
    pushCalibrationOverlayClose();

    const uint64_t elapsed = currentTimeMs() - mCalibrationStartTime;
    spdlog::info(
        "BlockESP calibration completed: radius={}, probes={}, elapsedMs={}",
        mAdaptiveScanRadiusChunks,
        mCalibrationLastProbeDistance,
        elapsed);
}

bool BlockESP::registerTrackableBlock(const std::string& name, const int blockId)
{
    if (blockId <= 0)
    {
        return false;
    }

    const std::string normalizedName = normalizeBlockName(name);
    if (normalizedName.empty())
    {
        return false;
    }

    bool added = false;
    auto& mappedIds = mTrackableBlockIdsByName[normalizedName];
    if (std::ranges::find(mappedIds, blockId) == mappedIds.end())
    {
        mappedIds.push_back(blockId);
        std::ranges::sort(mappedIds);
        added = true;
    }

    mTrackableBlockNameById[blockId] = normalizedName;
    return added;
}

bool BlockESP::syncTrackableBlockOptions()
{
    bool changed = false;

    for (const auto& [blockId, blockName] : mTrackableBlockNameById)
    {
        changed = registerTrackableBlock(blockName, blockId) || changed;
    }

    if (changed)
    {
        rebuildTrackableBlockOptions();
    }

    return changed;
}

void BlockESP::rebuildTrackableBlockOptions()
{
    std::vector<std::string> options;
    options.reserve(mTrackableBlockIdsByName.size());

    for (const auto& [name, ids] : mTrackableBlockIdsByName)
    {
        if (name.empty() || ids.empty())
        {
            continue;
        }

        options.push_back(name);
    }

    std::ranges::sort(options);
    mTrackedBlocks.setOptions(std::move(options), true);
}

void BlockESP::reset()
{
    {
        std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
        mFoundBlocks.clear();
    }

    mScanScratch.clear();
    mCommittedScanCount = 0;
    mChunkScanner.reset();

    mLastScanRequest = 0;
    mScanStartTime = 0;
    mLastScanCenter = ChunkPos(0, 0);
    mForceFullScan = true;
    mHasCommittedScan = false;
    mCommitIncrementalThisScan = false;
    mCalibrationActive = false;
    mCalibrationProgress = 0.f;
    mCalibrationLastProbeDistance = 0;
    mScanRadiusCalibrator.reset();
    mCalibrationStartTime = 0;
    pushCalibrationOverlayClose();
}

void BlockESP::refreshTrackedBlocksFromSettings()
{
    std::unordered_set<int> nextTracked;
    nextTracked.reserve(mTrackedBlocks.mSelectedValues.size() * 2);

    for (const auto& selectedName : mTrackedBlocks.mSelectedValues)
    {
        const std::string normalizedName = normalizeBlockName(selectedName);
        if (normalizedName.empty())
        {
            continue;
        }

        if (const auto it = mTrackableBlockIdsByName.find(normalizedName); it != mTrackableBlockIdsByName.end())
        {
            nextTracked.insert(it->second.begin(), it->second.end());
            continue;
        }

        if (std::ranges::all_of(normalizedName, [](const unsigned char c) { return std::isdigit(c) != 0; }))
        {
            try
            {
                nextTracked.insert(std::stoi(normalizedName));
            }
            catch (const std::exception&)
            {
            }
        }
    }

    if (nextTracked == mTrackedBlockIdSet) {
        return;
    }

    mTrackedBlockIdSet = std::move(nextTracked);
    mForceFullScan = true;
    mChunkScanner.reset();
    mScanScratch.clear();
    mCommittedScanCount = 0;
    mLoggedEmptyTrackedSet = false;
    mHasCommittedScan = false;
    mCommitIncrementalThisScan = false;

    std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
    for (auto it = mFoundBlocks.begin(); it != mFoundBlocks.end();) {
        if (!mTrackedBlockIdSet.contains(it->second.blockId)) {
            it = mFoundBlocks.erase(it);
            continue;
        }
        ++it;
    }
}

bool BlockESP::isInsideScanRange(const BlockPos& position, const ChunkPos& centerChunk) const
{
    const int radius = currentScanRadiusChunks();
    const ChunkPos blockChunk(position);
    return std::abs(blockChunk.x - centerChunk.x) <= radius && std::abs(blockChunk.y - centerChunk.y) <= radius;
}

bool BlockESP::isInsideRenderRange(const BlockPos& position, const glm::vec3& playerPos) const
{
    const float maxRenderDistance = std::max(mMinimumRenderRadius, static_cast<float>(currentScanRadiusChunks() * 16 + 16));
    return glm::distance(glm::vec3(position), playerPos) <= maxRenderDistance;
}

void BlockESP::runScanIfNeeded(const ChunkPos& centerChunk)
{
    auto* blockSource = ClientInstance::get()->getBlockSource();
    const bool hasTrackedBlocks = !mTrackedBlockIdSet.empty();
    if (blockSource == nullptr) {

        mChunkScanner.reset();
        mScanScratch.clear();
        mCommittedScanCount = 0;
        mHasCommittedScan = false;
        mCommitIncrementalThisScan = false;
        {
            std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
            mFoundBlocks.clear();
        }
        return;
    }

    if (!hasTrackedBlocks)
    {
        if (!mLoggedEmptyTrackedSet)
        {
            spdlog::debug("BlockESP: no tracked block types selected, scanning in discovery mode.");
            mLoggedEmptyTrackedSet = true;
        }

        std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
        if (!mFoundBlocks.empty())
        {
            mFoundBlocks.clear();
        }
    }
    else
    {
        mLoggedEmptyTrackedSet = false;
    }

    const uint64_t now = currentTimeMs();
    if (mChunkScanner.isActive()) {
        const std::size_t discoveredBeforeStep = mTrackableBlockNameById.size();
        const bool completed = mChunkScanner.step(mScanScratch);
        if (mTrackableBlockNameById.size() != discoveredBeforeStep)
        {
            syncTrackableBlockOptions();
        }

        if (mCommitIncrementalThisScan && mCommittedScanCount < mScanScratch.size()) {
            std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
            for (std::size_t i = mCommittedScanCount; i < mScanScratch.size(); ++i) {
                const auto& entry = mScanScratch[i];
                mFoundBlocks[entry.position] = FoundBlock{
                    .blockId = entry.blockId,
                    .aabb = AABB(glm::vec3(entry.position), glm::vec3(1.f, 1.f, 1.f)),
                    .color = colorForBlockId(entry.blockId),
                };
            }
            mCommittedScanCount = mScanScratch.size();
        }

        if (completed) {
            if (!mCommitIncrementalThisScan) {
                FoundBlockMap updatedMap;
                updatedMap.reserve(mScanScratch.size());
                for (const auto& entry : mScanScratch) {
                    updatedMap[entry.position] = FoundBlock{
                        .blockId = entry.blockId,
                        .aabb = AABB(glm::vec3(entry.position), glm::vec3(1.f, 1.f, 1.f)),
                        .color = colorForBlockId(entry.blockId),
                    };
                }

                std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
                mFoundBlocks = std::move(updatedMap);
            }

            mHasCommittedScan = true;
            const uint64_t elapsed = (mScanStartTime == 0) ? 0 : (now - mScanStartTime);
            spdlog::debug(
                "BlockESP scan completed: found={}, elapsedMs={}, center=({}, {})",
                mScanScratch.size(),
                elapsed,
                mLastScanCenter.x,
                mLastScanCenter.y
            );
        }
        return;
    }

    const bool centerMoved = centerChunk != mLastScanCenter;
    const bool intervalElapsed = (now - mLastScanRequest) >= kRescanIntervalMs;
    const bool shouldStartScan = mForceFullScan || !mHasCommittedScan || centerMoved || intervalElapsed;
    if (!shouldStartScan) {
        return;
    }

    const int scanRadius = currentScanRadiusChunks();
    const std::size_t scanBudget = resolveSubChunkBudgetPerTick(scanRadius);

    mScanScratch.clear();
    mCommittedScanCount = 0;
    mCommitIncrementalThisScan = !mHasCommittedScan;
    if (mCommitIncrementalThisScan) {
        std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
        mFoundBlocks.clear();
    }

    mChunkScanner.begin(ChunkFindingUtils::ScanRequest{
        .blockSource = blockSource,
        .centerChunk = centerChunk,
        .scanRadiusChunks = scanRadius,
        .trackedBlockIds = hasTrackedBlocks ? &mTrackedBlockIdSet : nullptr,
        .discoveredBlockNamesById = &mTrackableBlockNameById,
        .maxSubChunksPerStep = scanBudget,
    });

    mLastScanRequest = now;
    mScanStartTime = now;
    mLastScanCenter = centerChunk;
    mForceFullScan = false;

    if (mChunkScanner.isActive()) {
        spdlog::debug(
            "BlockESP scan started: center=({}, {}), scanRadius={}, trackedIds={}, chunkCount={}, subChunkBudget={}, incremental={}",
            centerChunk.x,
            centerChunk.y,
            scanRadius,
            mTrackedBlockIdSet.size(),
            mChunkScanner.chunkCount(),
            scanBudget,
            mCommitIncrementalThisScan
        );
    }
}

void BlockESP::onEnable()
{
    gFeatureManager->mDispatcher->listen<RenderEvent, &BlockESP::onRenderEvent, nes::event_priority::VERY_FIRST>(this);
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &BlockESP::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<BlockChangedEvent, &BlockESP::onBlockChangedEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &BlockESP::onPacketInEvent>(this);

    reset();
    syncTrackableBlockOptions();
    refreshTrackedBlocksFromSettings();
    spdlog::debug(
        "BlockESP enabled: scanRadius={}, trackedIds={}",
        currentScanRadiusChunks(),
        mTrackedBlockIdSet.size()
    );
}

void BlockESP::onDisable()
{
    gFeatureManager->mDispatcher->deafen<RenderEvent, &BlockESP::onRenderEvent>(this);
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &BlockESP::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<BlockChangedEvent, &BlockESP::onBlockChangedEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &BlockESP::onPacketInEvent>(this);

    std::size_t foundCount = 0;
    {
        std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
        foundCount = mFoundBlocks.size();
    }

    spdlog::debug("BlockESP disabled: clearing {} tracked blocks.", foundCount);
    reset();
}

void BlockESP::onBlockChangedEvent(BlockChangedEvent& event)
{
    const bool handled = TryCallWrapper([&]() {
        auto* ci = ClientInstance::get();
        if (ci == nullptr || ci->getLevelRenderer() == nullptr) {
            return;
        }

        auto* player = ci->getLocalPlayer();
        if (isNullOrSentinel(player) || isNullOrSentinel(event.mNewBlock)) {
            return;
        }

        auto* legacy = event.mNewBlock->mLegacy;
        if (isNullOrSentinel(legacy)) {
            return;
        }

        const ChunkPos centerChunk(*player->getPos());
        if (!isInsideScanRange(event.mBlockPos, centerChunk)) {
            return;
        }

        const int blockId = legacy->getBlockId();
        if (registerTrackableBlock(legacy->mName, blockId))
        {
            rebuildTrackableBlockOptions();
        }

        if (mTrackedBlockIdSet.empty()) {
            return;
        }

        std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
        if (!mTrackedBlockIdSet.contains(blockId)) {
            mFoundBlocks.erase(event.mBlockPos);
            return;
        }

        mFoundBlocks[event.mBlockPos] = FoundBlock{
            .blockId = blockId,
            .aabb = AABB(glm::vec3(event.mBlockPos), glm::vec3(1.f, 1.f, 1.f)),
            .color = colorForBlockId(blockId),
        };
    });

    (void)handled;
}

void BlockESP::onBaseTickEvent(BaseTickEvent& event)
{
    auto* ci = ClientInstance::get();
    if (ci == nullptr || ci->getLevelRenderer() == nullptr) {
        reset();
        return;
    }

    auto* player = ci->getLocalPlayer();
    if (isNullOrSentinel(player)) {
        return;
    }

    refreshTrackedBlocksFromSettings();
    updateCalibration();
    if (mCalibrationActive)
    {
        return;
    }

    const ChunkPos centerChunk(*player->getPos());
    runScanIfNeeded(centerChunk);
}

void BlockESP::onPacketInEvent(PacketInEvent& event)
{
    if (event.mPacket->getId() == PacketID::ChangeDimension) {
        reset();
        return;
    }

    if (event.mPacket->getId() != PacketID::PlayerAction) {
        return;
    }

    auto packet = event.getPacket<PlayerActionPacket>();
    if (packet->mAction == PlayerActionType::Respawn) {
        reset();
    }
}

void BlockESP::onRenderEvent(RenderEvent& event)
{
    auto* ci = ClientInstance::get();
    if (ci == nullptr || ci->getLevelRenderer() == nullptr || ci->getMouseGrabbed()) {
        return;
    }

    auto* player = ci->getLocalPlayer();
    if (isNullOrSentinel(player)) {
        return;
    }

    const ChunkPos centerChunk(*player->getPos());
    const int scanRadius = currentScanRadiusChunks();
    const glm::vec3 playerPos = *player->getPos();
    const glm::vec3 eyePos = playerPos + glm::vec3(0.0f, PLAYER_HEIGHT, 0.0f);
    const float effectiveRenderRadius = std::max(
        mMinimumRenderRadius,
        static_cast<float>(scanRadius * 16 + 16)
    );
    const float effectiveRenderRadiusSq = effectiveRenderRadius * effectiveRenderRadius;
    auto* drawList = ImGui::GetBackgroundDrawList();
    auto* guiData = ci->getGuiData();
    if (guiData == nullptr)
    {
        return;
    }

    const glm::vec2 displaySize = guiData->mResolution;
    auto projection = RenderUtils::transform.mMatrix;
    const glm::vec3 transformOrigin = RenderUtils::transform.mOrigin;
    const glm::vec2 transformFov = RenderUtils::transform.mFov;

    glm::vec3 viewForward(0.f, 0.f, 1.f);
    if (auto* rotation = player->getActorRotationComponent(); !isNullOrSentinel(rotation))
    {
        const float yaw = glm::radians(rotation->mYaw);
        const float pitch = glm::radians(rotation->mPitch);
        const float cosPitch = std::cos(pitch);
        viewForward = glm::normalize(glm::vec3(
            -std::sin(yaw) * cosPitch,
            -std::sin(pitch),
            std::cos(yaw) * cosPitch));
    }

    if (mRenderScanBounds) {
        auto* blockSource = ci->getBlockSource();
        const float minY = blockSource ? static_cast<float>(blockSource->getBuildDepth()) : (playerPos.y - 2.f);
        const float maxY = blockSource ? static_cast<float>(blockSource->getBuildHeight()) : (playerPos.y + 18.f);
        const float boxHeight = std::max(4.f, maxY - minY);
        const glm::vec3 scanMin(
            static_cast<float>((centerChunk.x - scanRadius) * 16),
            minY,
            static_cast<float>((centerChunk.y - scanRadius) * 16));
        const glm::vec3 scanSize(
            static_cast<float>((scanRadius * 2 + 1) * 16),
            boxHeight,
            static_cast<float>((scanRadius * 2 + 1) * 16));

        const AABB scanBounds(scanMin, scanSize);
        const std::vector<ImVec2> scanPoints = MathUtils::getImBoxPoints(scanBounds);
        if (scanPoints.size() > 1) {
            drawList->AddPolyline(scanPoints.data(), static_cast<int>(scanPoints.size()), ImColor(1.f, 1.f, 1.f, 0.35f), ImDrawFlags_Closed, 1.75f);
        }
    }

    std::lock_guard<std::mutex> lock(mFoundBlocksMutex);
    for (const auto& [pos, block] : mFoundBlocks) {
        if (!isInsideScanRange(pos, centerChunk)) {
            continue;
        }

        const glm::vec3 blockCenter = glm::vec3(pos) + glm::vec3(0.5f, 0.5f, 0.5f);
        const glm::vec3 delta = blockCenter - eyePos;
        const float distSq = glm::dot(delta, delta);
        if (distSq > effectiveRenderRadiusSq) {
            continue;
        }

        // Skip blocks well behind the camera to reduce draw + projection pressure.
        if (glm::dot(viewForward, delta) < -6.f)
        {
            continue;
        }

        ImVec4 screenRect = projection.getRectForAABB(block.aabb, transformOrigin, transformFov, displaySize);
        if (screenRect.z <= screenRect.x || screenRect.w <= screenRect.y)
        {
            continue;
        }

        if (screenRect.z < 0.f || screenRect.w < 0.f || screenRect.x > displaySize.x || screenRect.y > displaySize.y)
        {
            continue;
        }

        screenRect.x = std::clamp(screenRect.x, 0.f, displaySize.x);
        screenRect.y = std::clamp(screenRect.y, 0.f, displaySize.y);
        screenRect.z = std::clamp(screenRect.z, 0.f, displaySize.x);
        screenRect.w = std::clamp(screenRect.w, 0.f, displaySize.y);

        if ((screenRect.z - screenRect.x) < 1.0f || (screenRect.w - screenRect.y) < 1.0f)
        {
            continue;
        }

        if (mRenderMode == BlockRenderMode::Both || mRenderMode == BlockRenderMode::Filled) {
            drawList->AddRectFilled(
                ImVec2(screenRect.x, screenRect.y),
                ImVec2(screenRect.z, screenRect.w),
                ImColor(block.color.Value.x, block.color.Value.y, block.color.Value.z, 0.16f),
                0.0f);
        }

        if (mRenderMode == BlockRenderMode::Both || mRenderMode == BlockRenderMode::Outline) {
            drawList->AddRect(
                ImVec2(screenRect.x, screenRect.y),
                ImVec2(screenRect.z, screenRect.w),
                block.color,
                0.0f,
                ImDrawFlags_None,
                1.35f);
        }
    }
}
