#pragma once
//
// Created by Trae AI on 7/15/2024.
//

#include <Features/Modules/Module.hpp>
#include <Utils/Structs.hpp>

class AutoMove : public ModuleBase<AutoMove>
{
public:
    enum class BlockRenderMode {
        Filled,
        Outline,
        Both
    };

    // Settings for the module
    EnumSettingT<BlockRenderMode> mRenderMode = EnumSettingT("Render Mode", "The mode to render path blocks", BlockRenderMode::Outline, "Filled", "Outline", "Both");
    NumberSetting mRadius = NumberSetting("Radius", "The radius to search for walkable blocks", 20.f, 1.f, 100.f, 0.01f);
    NumberSetting mChunkRadius = NumberSetting("Chunk Radius", "The max chunk radius to search for blocks", 4.f, 1.f, 32.f, 1.f);
    NumberSetting mUpdateFrequency = NumberSetting("Update Frequency", "The frequency of the block update (in ticks)", 1.f, 1.f, 40.f, 0.01f);
    NumberSetting mChunkUpdatesPerTick = NumberSetting("Chunk Updates Per Tick", "The number of subchunks to update per tick", 5.f, 1.f, 24.f, 1.f);
    BoolSetting mRenderCurrentChunk = BoolSetting("Render Current Chunk", "Renders the current chunk", false);
    BoolSetting mRenderPath = BoolSetting("Render Path", "Renders the calculated path", true);
    
    // Path optimization settings
    NumberSetting mCheckpointDistance = NumberSetting("Checkpoint Distance", "Distance between checkpoints for path recalculation", 100.f, 20.f, 200.f, 10.f);
    NumberSetting mVerticalPenalty = NumberSetting("Vertical Penalty", "Penalty for vertical movement to create smoother paths", 2.0f, 1.0f, 5.0f, 0.1f);
    BoolSetting mSmoothPath = BoolSetting("Smooth Path", "Applies path smoothing for more natural movement", true);
    BoolSetting mAvoidCollisions = BoolSetting("Avoid Collisions", "Uses ray tracing to prevent paths through blocks", true);
    BoolSetting mAutoRecalculate = BoolSetting("Auto Recalculate", "Automatically recalculates path when needed", true);
    
    // Target position setting
    NumberSetting mTargetX = NumberSetting("Target X", "X coordinate of target position", 0.f, -150.f, 150.f, 1.f);
    NumberSetting mTargetY = NumberSetting("Target Y", "Y coordinate of target position", 64.f, 0.f, 256.f, 1.f);
    NumberSetting mTargetZ = NumberSetting("Target Z", "Z coordinate of target position", 0.f, -150.f, 150.f, 1.f);

    AutoMove() : ModuleBase("AutoMove", "Automatically finds a path to a target location", ModuleCategory::Movement, 0, false) {
        addSettings(
            &mRenderMode,
            &mRadius,
            &mChunkRadius,
            &mUpdateFrequency,
            &mChunkUpdatesPerTick,
            &mRenderCurrentChunk,
            &mRenderPath,
            &mCheckpointDistance,
            &mVerticalPenalty,
            &mSmoothPath,
            &mAvoidCollisions,
            &mAutoRecalculate,
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

private:
    ChunkPos mSearchCenter;
    ChunkPos mCurrentChunkPos;
    int mSubChunkIndex = 0;
    int mDirectionIndex = 0;
    int mSteps = 1;
    int mStepsCount = 0;
    int64_t mSearchStart = 0;
    
    // Target position
    glm::vec3 mTargetPosition;
    
    // Path finding
    std::vector<glm::vec3> mPath;
    bool mPathFound = false;
    
    // Checkpoint system
    std::vector<glm::vec3> mCheckpoints;
    size_t mCurrentCheckpointIndex = 0;
    bool mReachedCheckpoint = false;

    struct PathNode {
        glm::vec3 position;
        float gCost; // Cost from start to this node
        float hCost; // Heuristic cost (estimated cost from this node to target)
        PathNode* parent;
        
        float getFCost() const { return gCost + hCost; }
        
        bool operator==(const PathNode& other) const {
            return position == other.position;
        }
    };

    struct FoundBlock {
        const Block* block;
        AABB aabb;
        ImColor color;
    };

    static std::unordered_map<BlockPos, FoundBlock> mFoundBlocks;

    // Core logic
    void moveToNext();
    void reset();
    bool processSub(ChunkPos processChunk, int subChunk);
    void tryProcessSub(bool& processed, ChunkPos currentChunkPos, int subChunkIndex);
    bool isWalkableBlock(const Block* block) const;
    bool findPath(const glm::vec3& start, const glm::vec3& target);
    float calculateHeuristic(const glm::vec3& a, const glm::vec3& b) const;
    std::vector<glm::vec3> getNeighbors(const glm::vec3& position) const;
    std::vector<glm::vec3> reconstructPath(PathNode* endNode);
    bool hasLineOfSight(const glm::vec3& start, const glm::vec3& end) const;
    void generateCheckpoints(const glm::vec3& start, const glm::vec3& target);
    void smoothPath();
    bool isPathVisible() const;
    
    // Block color handling
    static inline const ImColor PATH_COLOR = ImColor(0.f, 1.f, 0.f, 1.f);
    static inline const ImColor WALKABLE_COLOR = ImColor(0.f, 0.5f, 1.f, 1.f);
};