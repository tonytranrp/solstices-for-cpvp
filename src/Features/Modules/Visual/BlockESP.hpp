#pragma once

#include <cstddef>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <Features/Modules/Module.hpp>
#include <SDK/Minecraft/World/Chunk/ChunkSource.hpp>
#include <Utils/Chunk/ChunkFindingUtils.hpp>
#include <Utils/Structs.hpp>

class BlockESP : public ModuleBase<BlockESP> {
public:
    enum class BlockRenderMode {
        Filled,
        Outline,
        Both
    };

    ListSetting mTrackedBlocks = ListSetting(
        "Tracked Blocks",
        "Choose which block names BlockESP should scan and render",
        {},
        {}
    );
    ButtonSetting mCalibrateDistance = ButtonSetting(
        "Calibrate Distance",
        "Detect and apply the furthest safe chunk scan radius around your current direction",
        "Calibrate");

    BlockESP();

    struct FoundBlock {
        int blockId = 0;
        AABB aabb;
        ImColor color;
    };

    void onEnable() override;
    void onDisable() override;
    void onBlockChangedEvent(class BlockChangedEvent& event);
    void onBaseTickEvent(class BaseTickEvent& event);
    void onPacketInEvent(class PacketInEvent& event);
    void onRenderEvent(class RenderEvent& event);

    std::vector<int> getTrackedBlockIds() const;
    [[nodiscard]] bool isCalibrating() const noexcept;
    [[nodiscard]] float calibrationProgress() const noexcept;
    [[nodiscard]] int calibratedScanRadius() const noexcept;

private:
    using FoundBlockMap = std::unordered_map<BlockPos, FoundBlock>;

    mutable std::mutex mFoundBlocksMutex;
    FoundBlockMap mFoundBlocks;
    std::vector<ChunkFindingUtils::FoundBlock> mScanScratch;
    std::size_t mCommittedScanCount = 0;
    ChunkFindingUtils::IncrementalScanner mChunkScanner;
    uint64_t mLastScanRequest = 0;
    uint64_t mScanStartTime = 0;
    ChunkPos mLastScanCenter = ChunkPos(0, 0);
    bool mForceFullScan = true;
    bool mLoggedEmptyTrackedSet = false;
    bool mHasCommittedScan = false;
    bool mCommitIncrementalThisScan = false;
    int mAdaptiveScanRadiusChunks = 8;

    BlockRenderMode mRenderMode = BlockRenderMode::Outline;
    bool mRenderScanBounds = true;
    float mMinimumRenderRadius = 0.f;

    bool mCalibrationActive = false;
    float mCalibrationProgress = 0.f;
    int mCalibrationMaxProbeDistance = 96;
    uint64_t mCalibrationStartTime = 0;
    int mCalibrationLastProbeDistance = 0;

    ChunkFindingUtils::ScanRadiusCalibrator mScanRadiusCalibrator;

    std::unordered_set<int> mTrackedBlockIdSet;
    std::unordered_map<std::string, std::vector<int>> mTrackableBlockIdsByName;
    std::unordered_map<int, std::string> mTrackableBlockNameById;

    void reset();
    void startCalibration();
    void updateCalibration();
    [[nodiscard]] int currentScanRadiusChunks() const noexcept;
    void refreshTrackedBlocksFromSettings();
    void rebuildTrackableBlockOptions();
    bool registerTrackableBlock(const std::string& name, int blockId);
    bool syncTrackableBlockOptions();
    void runScanIfNeeded(const ChunkPos& centerChunk);
    bool isInsideScanRange(const BlockPos& position, const ChunkPos& centerChunk) const;
    bool isInsideRenderRange(const BlockPos& position, const glm::vec3& playerPos) const;

    static ImColor colorForBlockId(int blockId);
};
