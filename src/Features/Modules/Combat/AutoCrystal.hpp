#pragma once

#include <Features/Modules/Module.hpp>
#include <unordered_set>
#include <SDK/Minecraft/World/Chunk/SubChunkBlockStorage.hpp>
#include <Features/FeatureManager.hpp>
#include <Features/Events/BlockChangedEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerActionPacket.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/World/Chunk/LevelChunk.hpp>
#include <SDK/Minecraft/World/Chunk/SubChunkBlockStorage.hpp>
class AutoCrystal : public ModuleBase<AutoCrystal> {
public:
    struct PlacePosition {
        BlockPos position;
        float targetDamage;
        float selfDamage;

        PlacePosition(const BlockPos& pos, float tDmg)
            : position(pos), targetDamage(tDmg) {
        }
    };

    enum class Mode {
        Single,
        Switch
    };

    enum class SwitchMode {
        None,
        Silent,
        Normal
    };
    std::mutex blockmutex = {};
    EnumSettingT<Mode> mMode = EnumSettingT<Mode>("Mode", "The mode of the crystal aura", Mode::Switch, "Single", "Switch");
    EnumSettingT<SwitchMode> mSwitchMode = EnumSettingT<SwitchMode>("Switch Mode", "How to switch slots", SwitchMode::Silent, "None", "Silent", "Normal");
    BoolSetting mAutoPlace = BoolSetting("Auto Place", "Automatically places crystals", true);
    NumberSetting mRange = NumberSetting("Range", "Range to search for targets", 6.0f, 3.0f, 10.0f, 0.1f);
    NumberSetting mPlaceRange = NumberSetting("Place Range", "Range to place crystals", 4.5f, 1.0f, 6.0f, 0.1f);
    NumberSetting mPlaceDelay = NumberSetting("Place Delay", "Delay between placements (ms)", 100.0f, 0.0f, 1000.0f, 1.0f);
    NumberSetting mMinimumDamage = NumberSetting("Minimum Damage", "Minimum damage to place/break", 6.0f, 1.0f, 100.0f, 0.5f);
    NumberSetting mMaxSelfDamage = NumberSetting("Max Self Damage", "Maximum self damage", 8.0f, 1.0f, 100.0f, 0.5f);
    BoolSetting mVisualizePlace = BoolSetting("Visualize", "Show crystal placement spots", true);
    BoolSetting mRaycast = BoolSetting("Raycast", "Check line of sight to crystals", true);
    BoolSetting mIdPredict = BoolSetting("ID Predict", "Predicts crystal entity IDs for faster breaking", false);
    NumberSetting mPredictAmount = NumberSetting("Predict Amount", "Number of packets to send per predict", 15, 1, 30, 1);
    BoolSetting mSwitchBack = BoolSetting("Switch Back", "Switch back to previous slot", true);

    AutoCrystal() : ModuleBase("AutoCrystal", "Automatically places and breaks end crystals", ModuleCategory::Combat, 0, false) {
        addSettings(
            &mMode,
            &mSwitchMode,
            &mAutoPlace,
            &mRange,
            &mPlaceRange,
            &mPlaceDelay,
            &mMinimumDamage,
            &mMaxSelfDamage,
            &mVisualizePlace,
            &mRaycast,
            &mIdPredict,
            &mPredictAmount,
            &mSwitchBack
        );

        VISIBILITY_CONDITION(mSwitchBack, mSwitchMode.mValue != SwitchMode::None);

        mNames = {
            {Lowercase, "autocrystal"},
            {LowercaseSpaced, "auto crystal"},
            {Normal, "AutoCrystal"},
            {NormalSpaced, "Auto Crystal"}
        };
    }

    void onEnable() override;
    void onDisable() override;
    void onBaseTickEvent(class BaseTickEvent& event);
    void onRenderEvent(class RenderEvent& event);
    void onPacketOutEvent(class PacketOutEvent& event);
    void onPacketInEvent(class PacketInEvent& event);
    void onBlockChangedEvent(BlockChangedEvent& event)
    {
        if (!ClientInstance::get()->getLevelRenderer()) {
            reset();
            return;
        }
        std::lock_guard<std::mutex> lock(blockmutex); // Lock mutex

        auto dabl = BlockInfo(event.mNewBlock, event.mBlockPos);
        if (dabl.getDistance(*ClientInstance::get()->getLocalPlayer()->getPos()) > 9) return;

        ChunkPos chunkPos = ChunkPos(event.mBlockPos);
        int subChunk = (event.mBlockPos.y - ClientInstance::get()->getBlockSource()->getBuildDepth()) >> 4;
        bool result = false;
        tryProcessSub(result, chunkPos, subChunk);

        if (!result) {
            spdlog::critical("Failed to process subchunk [scIndex: {}/{}, chunkPos: ({}, {})]", subChunk, (ClientInstance::get()->getBlockSource()->getBuildHeight() - ClientInstance::get()->getBlockSource()->getBuildDepth()) / 16, chunkPos.x, chunkPos.y);
        }

        auto enabledBlocks = getEnabledBlocks();

        if (isValidBlock(event.mNewBlock->mLegacy->getBlockId()) && std::ranges::find(enabledBlocks, event.mNewBlock->mLegacy->getBlockId()) != enabledBlocks.end())
        {
            const Block* block = event.mNewBlock;
            mFoundBlocks[event.mBlockPos] = { block, AABB(event.mBlockPos, glm::vec3(1.f, 1.f, 1.f)), getColorFromId(block->mLegacy->getBlockId()) };
            spdlog::debug("event.mNewBlock->mLegacy->mName: {} event.mOldBlock->mLegacy->mName: {}", event.mNewBlock->mLegacy->mName, event.mOldBlock->mLegacy->mName);
        }
        else
        {
            mFoundBlocks.erase(event.mBlockPos);
        }
    };
private:
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

    std::unordered_map<BlockPos, FoundBlock> mFoundBlocks;
    glm::vec2 rots{};
    bool mRotating = false;
    int mCrystalSlot = -1;
    int mPrevSlot = -1;
    bool mShouldSpoofSlot = false;
    bool mHasSwitched = false;
    std::unordered_set<uint64_t> knownCrystals;
    /// <summary>
    ///search code*
    std::vector<int> getEnabledBlocks()
    {
        std::vector<int> enabledBlocks = {};

        // Insert deepslate ores as well

        enabledBlocks.push_back(7); //bedrock
        enabledBlocks.push_back(49);// obisidian

        return enabledBlocks;
    }
    void reset()
    {
        std::lock_guard<std::mutex> lock(blockmutex); // Lock mutex

        ClientInstance* ci = ClientInstance::get();
        Actor* player = ci->getLocalPlayer();
        mSearchStart = NOW;
        mFoundBlocks.clear();
        mStepsCount = 0;
        mSteps = 1;
        mDirectionIndex = 0;
        mSubChunkIndex = 0;
        if (!player) return;
        BlockSource* blockSource = ci->getBlockSource();

        mSearchCenter = ChunkPos(*player->getPos());
        mCurrentChunkPos = mSearchCenter;
    }
    void moveToNext()
    {
        if (!ClientInstance::get()->getLevelRenderer()) {
            reset();
            return;
        }
        ClientInstance* ci = ClientInstance::get();
        Actor* player = ci->getLocalPlayer();
        if (!player) return;
        BlockSource* blockSource = ci->getBlockSource();

        // da search pattern
        static const std::vector<std::pair<int, int>> directions = {
            { 1, 0 },
            { 0, 1 },
            { -1, 0 },
            { 0, -1 }
        };

        size_t numSubchunks = (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16;
        if (numSubchunks - 1 > mSubChunkIndex)
        {
            mSubChunkIndex++;
            //spdlog::debug("Moving to next subchunk [scIndex: {}/{}, chunkPos: ({}, {})]", mSubChunkIndex, numSubchunks, mCurrentChunkPos.x, mCurrentChunkPos.y);
            return;
        }

        mCurrentChunkPos.x += directions[mDirectionIndex].first;
        mCurrentChunkPos.y += directions[mDirectionIndex].second;

        mStepsCount++;
        if (mStepsCount >= mSteps) {
            mStepsCount = 0;
            mDirectionIndex = (mDirectionIndex + 1) % directions.size();
            if (mDirectionIndex % 2 == 0) {
                mSteps++;
            }
        }

        mSubChunkIndex = 0;
        //spdlog::debug("Moving to next subchunk [scIndex: {}/{}, chunkPos: ({}, {})]", mSubChunkIndex, numSubchunks, mCurrentChunkPos.x, mCurrentChunkPos.y);
    }

    void tryProcessSub(bool& processed, ChunkPos currentChunkPos, int subChunkIndex)
    {
        TRY_CALL([&]()
            {
                if (processSub(currentChunkPos, subChunkIndex))
                {
                    processed = true;
                }
            });
    }

    bool processSub(ChunkPos processChunk, int index)
    {
        if (!ClientInstance::get()->getLevelRenderer()) {
            reset();
            return false;
        }
        ClientInstance* ci = ClientInstance::get();
        Actor* player = ci->getLocalPlayer();
        if (!player) return false;
        BlockSource* blockSource = ci->getBlockSource();

        size_t numSubchunks = (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16;
        if (index < 0 || index >= numSubchunks) return false;

        LevelChunk* chunk = blockSource->getChunk(processChunk);
        if (!chunk) return false;

        auto subChunk = (*chunk->getSubChunks())[index];
        SubChunkBlockStorage* blockReader = subChunk.blockReadPtr;
        if (!blockReader) return false;

        std::vector<int> enabledBlocks = getEnabledBlocks();

        for (uint16_t x = 0; x < 16; x++)
        {
            for (uint16_t z = 0; z < 16; z++)
            {
                for (uint16_t y = 0; y < (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / chunk->getSubChunks()->size(); y++)
                {
                    uint16_t elementId = (x * 0x10 + z) * 0x10 + (y & 0xf);
                    const Block* found = blockReader->getElement(elementId);
                    if (found->mLegacy->getBlockId() == 0)
                    {
                        mFoundBlocks.erase(BlockPos((processChunk.x * 16) + x, y + (subChunk.subchunkIndex * 16), (processChunk.y * 16) + z));
                        continue;
                    }
                    if (std::ranges::find(enabledBlocks, found->mLegacy->getBlockId()) == enabledBlocks.end()) continue;

                    BlockPos pos;
                    pos.x = (processChunk.x * 16) + x;
                    pos.z = (processChunk.y * 16) + z;
                    pos.y = y + (subChunk.subchunkIndex * 16);

                    int exposedHeight = BlockUtils::getExposedHeight(pos);
                    if ( exposedHeight == 0) {
                        continue;
                    }

                    mFoundBlocks[pos] = { found, AABB(pos, glm::vec3(1.f, 1.f, 1.f)), getColorFromId(found->mLegacy->getBlockId()) };
                }
            }
        }

        return true;
    }
    int OBSIDIAN = 49;
    int BEDROCK = 7;

    std::unordered_map<int, ImColor> blockColors = {
        { 49, ImColor(1.f, 0.f, 0.f, 1.f) }
    };

    bool isValidBlock(int id)
    {
        return blockColors.contains(id);
    }

    ImColor getColorFromId(int id)
    {
        if (blockColors.contains(id))
        {
            return blockColors[id];
        }

        return ImColor(1.f, 1.f, 1.f, 1.f);
    }
    /// </summary>
    /// <param name="targetPos"></param>
    void updateRotations(glm::vec3 targetPos);
    void switchToCrystal();
    void switchBack();

    float calculateDamage(const BlockPos& crystalPos, Actor* target);
    std::vector<PlacePosition> findPlacePositions();
    bool canPlaceCrystal(const BlockPos& pos);
    void placeCrystal(const PlacePosition& pos);
    void breakCrystal(Actor* crystal);

    std::vector<PlacePosition> mPossiblePlacements;
    uint64_t mLastPlace = 0;
    uint64_t mLastAttackId = -1;
    bool mShouldIdPredict = false;
    Actor* mLastTarget = nullptr;

    float mCurrArmor = 0.f;

    std::unordered_set<uint64_t> mTrackedCrystals;
};