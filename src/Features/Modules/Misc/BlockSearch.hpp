#pragma once
//
// Created by vastrakai on 7/7/2024.
//

#include <Features/Modules/Module.hpp>


class HoleEsp : public ModuleBase<HoleEsp>
{
public:
    enum class BlockRenderMode {
        Filled,
        Outline,
        Both
    };

    EnumSettingT<BlockRenderMode> mRenderMode = EnumSettingT("Render Mode", "The mode to render Holes", BlockRenderMode::Outline, "Filled", "Outline", "Both");
    NumberSetting mRadius = NumberSetting("Radius", "The radius of the block esp", 20.f, 1.f, 100.f, 0.01f);
    NumberSetting mChunkRadius = NumberSetting("Chunk Radius", "The max chunk radius to search for blocks", 4.f, 1.f, 32.f, 1.f);
    NumberSetting mUpdateFrequency = NumberSetting("Update Frequency", "The frequency of the block update (in ticks)", 1.f, 1.f, 40.f, 0.01f);
    NumberSetting mChunkUpdatesPerTick = NumberSetting("Chunk Updates Per Tick", "The number of subchunks to update per tick", 5.f, 1.f, 24.f, 1.f);
    BoolSetting mRenderCurrentChunk = BoolSetting("Render Current Chunk", "Renders the current chunk", false);

    HoleEsp() : ModuleBase("HoleEsp", "Rendering Hole surrounded by Obi/bedrock to protect u from crystal", ModuleCategory::Misc, 0, false) {
        addSettings(
            &mRenderMode,
            &mRadius,
            &mChunkRadius,
            &mUpdateFrequency,
            &mChunkUpdatesPerTick,
            &mRenderCurrentChunk
        );

        mNames = {
            {Lowercase, "HoleEsp"},
            {LowercaseSpaced, "hole esp"},
            {Normal, "HoleEsp"},
            {NormalSpaced, "hole ESP"}
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
    static auto getfoundblocks() { return mFoundBlocks; }
    ChunkPos mSearchCenter;
    ChunkPos mCurrentChunkPos;
    int mSubChunkIndex = 0;
    int mDirectionIndex = 0;
    int mSteps = 1;
    int mStepsCount = 0;
    int64_t mSearchStart = 0;

    struct FoundBlock
    {
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
    std::vector<int> getEnabledBlocks();

    static constexpr int OBSIDIAN = 49, BEDROCK = 7;
    static inline const std::unordered_map<int, ImColor> blockColors = { {OBSIDIAN, ImColor(1.f, 0.f, 0.f, 1.f)} };

    bool isValidBlock(int id) const { return blockColors.contains(id); }
    ImColor getColorFromId(int id) const { return blockColors.contains(id) ? blockColors.at(id) : ImColor(1.f, 1.f, 1.f, 1.f); }
};