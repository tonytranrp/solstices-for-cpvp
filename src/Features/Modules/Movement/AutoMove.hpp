#pragma once
//
// Created by Trae AI on 7/15/2024.
//

#include <Features/Modules/Module.hpp>
#include <Utils/Structs.hpp>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <functional>

class AutoMove : public ModuleBase<AutoMove>
{
public:
    enum class BlockRenderMode {
        Filled,
        Outline,
        Both
    };

    // Settings for the module
    EnumSettingT<BlockRenderMode> mRenderMode = EnumSettingT("Render Mode", "The mode to render blocks", BlockRenderMode::Outline, "Filled", "Outline", "Both");
    NumberSetting mRadius = NumberSetting("Radius", "The radius to search for walkable blocks", 20.f, 1.f, 100.f, 0.01f);
    NumberSetting mChunkRadius = NumberSetting("Chunk Radius", "The max chunk radius to search for blocks", 4.f, 1.f, 32.f, 1.f);
    NumberSetting mUpdateFrequency = NumberSetting("Update Frequency", "The frequency of the block update (in ticks)", 1.f, 1.f, 40.f, 0.01f);
    NumberSetting mChunkUpdatesPerTick = NumberSetting("Chunk Updates Per Tick", "The number of subchunks to update per tick", 5.f, 1.f, 24.f, 1.f);
    BoolSetting mRenderCurrentChunk = BoolSetting("Render Current Chunk", "Renders the current chunk being searched", false);
    BoolSetting mUseLineOfSight = BoolSetting("Use Line of Sight", "Use line of sight checking for pathfinding", true);
    BoolSetting mRenderPath = BoolSetting("Render Path", "Renders the calculated path", true);
    
    // Target position settings (used by command)
    NumberSetting mTargetX = NumberSetting("Target X", "X coordinate of target position", 0.f, -150.f, 150.f, 1.f);
    NumberSetting mTargetY = NumberSetting("Target Y", "Y coordinate of target position", 64.f, 0.f, 256.f, 1.f);
    NumberSetting mTargetZ = NumberSetting("Target Z", "Z coordinate of target position", 0.f, -150.f, 150.f, 1.f);

    AutoMove() : ModuleBase("AutoMove", "Automatically searches for walkable blocks", ModuleCategory::Movement, 0, false) {
        addSettings(
            &mRenderMode,
            &mRadius,
            &mChunkRadius,
            &mUpdateFrequency,
            &mChunkUpdatesPerTick,
            &mRenderCurrentChunk,
            &mUseLineOfSight,
            &mRenderPath,
            &mTargetX,
            &mTargetY,
            &mTargetZ
        );

        mNames = {
            {Lowercase, "automove"},
            {LowercaseSpaced, "auto move"},
            {Normal, "AutoMove"},
            {NormalSpaced, "Auto Move"}
        };
    }

    // Event Handlers
    void onEnable() override;
    void onDisable() override;
    void onBlockChangedEvent(class BlockChangedEvent& event);
    void onBaseTickEvent(class BaseTickEvent& event);
    void onPacketInEvent(class PacketInEvent& event);
    void onRenderEvent(class RenderEvent& event);

    // A* Node structures
    struct BlockNode {
        BlockPos pos;
        float gCost; // Cost from start
        float hCost; // Heuristic cost to target
        BlockNode* parent;
        bool walkable = true;
        
        float getFCost() const { return gCost + hCost; }
        
        bool operator==(const BlockNode& other) const {
            return pos == other.pos;
        }
    };

    struct ChunkNode {
        ChunkPos pos;
        float gCost; // Cost from start
        float hCost; // Heuristic cost to target
        ChunkNode* parent;
        bool walkable = true;
        
        float getFCost() const { return gCost + hCost; }
        
        bool operator==(const ChunkNode& other) const {
            return pos == other.pos;
        }
    };

    // Hash functions for unordered_map
    struct BlockPosHash {
        std::size_t operator()(const BlockPos& pos) const {
            return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.y) << 1) ^ (std::hash<int>()(pos.z) << 2);
        }
    };

    struct ChunkPosHash {
        std::size_t operator()(const ChunkPos& pos) const {
            return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.y) << 1);
        }
    };

    // Comparators for priority queue
    struct BlockNodeCompare {
        bool operator()(const BlockNode* a, const BlockNode* b) const {
            float fCostA = a->gCost + a->hCost;
            float fCostB = b->gCost + b->hCost;
            if (fCostA != fCostB) {
                return fCostA > fCostB; // Min-heap by f-cost
            }
            return a->hCost > b->hCost; // Tie-breaker: prefer lower h-cost
        }
    };

    struct ChunkNodeCompare {
        bool operator()(const ChunkNode* a, const ChunkNode* b) const {
            float fCostA = a->gCost + a->hCost;
            float fCostB = b->gCost + b->hCost;
            if (fCostA != fCostB) {
                return fCostA > fCostB; // Min-heap by f-cost
            }
            return a->hCost > b->hCost; // Tie-breaker: prefer lower h-cost
        }
    };

    // Inline utility functions
    inline float calculateBlockHeuristic(const BlockPos& a, const BlockPos& b) const {
        float dx = static_cast<float>(a.x - b.x);
        float dy = static_cast<float>(a.y - b.y);
        float dz = static_cast<float>(a.z - b.z);
        return std::sqrt(dx * dx + dy * dy + dz * dz);
    }

    inline float calculateChunkHeuristic(const ChunkPos& a, const ChunkPos& b) const {
        float dx = static_cast<float>(a.x - b.x);
        float dy = static_cast<float>(a.y - b.y);
        return std::sqrt(dx * dx + dy * dy);
    }

    inline bool isWalkableBlock(const Block* block) const {
        if (!block) return false;
        int blockId = block->mLegacy->getBlockId();
        bool isLiquid = (blockId >= 8 && blockId <= 11);
        return blockId != 0 && !isLiquid && block->mLegacy->mMaterial->mIsBlockingMotion;
    }

    inline std::vector<BlockPos> getNeighborBlocks(const BlockPos& pos) const {
        std::vector<BlockPos> neighbors;
        static const std::array<std::tuple<int, int, int>, 26> directions = {{
            // Direct neighbors (6)
            {0, 1, 0}, {0, -1, 0}, {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
            // Diagonal neighbors (20)
            {1, 1, 0}, {1, -1, 0}, {-1, 1, 0}, {-1, -1, 0},
            {1, 0, 1}, {1, 0, -1}, {-1, 0, 1}, {-1, 0, -1},
            {0, 1, 1}, {0, 1, -1}, {0, -1, 1}, {0, -1, -1},
            {1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1},
            {-1, 1, 1}, {-1, 1, -1}, {-1, -1, 1}, {-1, -1, -1}
        }};

        for (const auto& [dx, dy, dz] : directions) {
            BlockPos neighbor(pos.x + dx, pos.y + dy, pos.z + dz);
            if (glm::distance(glm::vec3(neighbor), glm::vec3(pos)) <= mRadius.mValue) {
                neighbors.push_back(neighbor);
            }
        }
        return neighbors;
    }

    inline std::vector<ChunkPos> getNeighborChunks(const ChunkPos& chunk) const {
        std::vector<ChunkPos> neighbors;
        static const std::array<std::pair<int, int>, 8> directions = {{
            {0, 1}, {0, -1}, {1, 0}, {-1, 0},  // Straight directions first
            {1, 1}, {1, -1}, {-1, 1}, {-1, -1}  // Diagonal directions
        }};

        for (const auto& [dx, dy] : directions) {
            ChunkPos neighbor(chunk.x + dx, chunk.y + dy);
            float distance = std::sqrt(
                (neighbor.x - mSearchCenter.x) * (neighbor.x - mSearchCenter.x) + 
                (neighbor.y - mSearchCenter.y) * (neighbor.y - mSearchCenter.y)
            );
            if (distance <= mChunkRadius.mValue) {
                neighbors.push_back(neighbor);
            }
        }
        return neighbors;
    }

    // Line of sight function
    inline float getLineOfSightPercent(const BlockPos& from, const BlockPos& to) const {
        auto blockSource = ClientInstance::get()->getBlockSource();
        if (!blockSource) return 0.0f;
        
       // return blockSource->getSeenPercent(glm::vec3(from), glm::vec3(to));
    }

    inline bool hasLineOfSight(const BlockPos& from, const BlockPos& to) const {
        return getLineOfSightPercent(from, to) > 0.8f; // 80% visibility threshold
    }

private:
    // Search state
    ChunkPos mSearchCenter;
    ChunkPos mCurrentChunkPos;
    int mSubChunkIndex = 0;
    int64_t mSearchStart = 0;
    uint64_t mLastSearchUpdate = 0;
    bool mSearchActive = false;
    
    // A* pathfinding for blocks
    std::vector<BlockPos> mBlockPath;
    size_t mCurrentBlockPathIndex = 0;
    bool mBlockPathCalculated = false;
    
    // A* pathfinding for chunks
    std::vector<ChunkPos> mChunkPath;
    size_t mCurrentChunkPathIndex = 0;
    bool mChunkPathCalculated = false;
    
    // Block tracking
    std::unordered_set<BlockPos, BlockPosHash> mWalkableBlocks;
    int mWalkableBlocksInCurrentChunk = 0;

    // Core pathfinding functions
    bool calculateBlockPath(const BlockPos& start, const BlockPos& target);
    bool calculateChunkPath(const ChunkPos& start, const ChunkPos& target);
    
    // Search functions
    void updateSearch();
    void moveToNextChunk();
    void reset();
    bool processSub(ChunkPos processChunk, int subChunk);
    void tryProcessSub(bool& processed, ChunkPos currentChunkPos, int subChunkIndex);
    bool isChunkWalkable(const ChunkPos& chunk) const;
    
    // Block color handling
    static inline const ImColor WALKABLE_COLOR = ImColor(0.f, 0.5f, 1.f, 1.f);
    static inline const ImColor PATH_COLOR = ImColor(1.f, 1.f, 0.f, 1.f);
    static inline const ImColor TARGET_COLOR = ImColor(1.f, 0.f, 0.f, 1.f);
};