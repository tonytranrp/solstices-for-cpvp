#pragma once
//
// Created by vastrakai on 7/7/2024.
//

#include <Features/Modules/Module.hpp>


class AutoCrystalRecode : public ModuleBase<AutoCrystalRecode>
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

    AutoCrystalRecode() : ModuleBase("AutoCrystalRecode", "Rendering Hole surrounded by Obi/bedrock to protect u from crystal", ModuleCategory::Misc, 0, false) {
        addSettings(
            &mRenderMode,
            &mRadius,
            &mChunkRadius,
            &mUpdateFrequency,
            &mChunkUpdatesPerTick,
            &mRenderCurrentChunk
        );

        mNames = {
            {Lowercase, "AutoCrystalRecode"},
            {LowercaseSpaced, "hole esp"},
            {Normal, "AutoCrystalRecode"},
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
    Actor* Target = nullptr;
    struct FoundBlock
    {
        const Block* block;
        AABB aabb;
        ImColor color;
    };
    // --- Advanced Placement Struct ---
    struct PlacePosition {
        BlockPos position;
        float targetDamage;
        float selfDamage;

        PlacePosition(const BlockPos& pos, float targetDmg, float selfDmg)
            : position(pos), targetDamage(targetDmg), selfDamage(selfDmg) {
        }

        // Default constructor if needed
        PlacePosition() : position(), targetDamage(0.f), selfDamage(0.f) {}
    };

    // --- Calculation Unit Struct ---
    // This unit wraps the data used in calculating explosion damage.
    struct CalcUnit {
        BlockPos position;
        glm::vec3 explosionPos;
        glm::vec3 targetPos;
        float distance;
        float visibility;

        CalcUnit(const BlockPos& pos, const glm::vec3& explosion, const glm::vec3& target)
            : position(pos), explosionPos(explosion), targetPos(target) {
            distance = glm::distance(target, explosion);
            // For demonstration, assume full visibility (this could be replaced with an actual calculation)
            visibility = 1.0f;
        }
    };

    // --- Compare Struct ---
    // Functor to sort PlacePosition candidates (higher targetDamage first)
    struct PlacePositionCompare {
        bool operator()(const PlacePosition& a, const PlacePosition& b) const {
            return a.targetDamage > b.targetDamage;
        }
    };
    bool canPlaceCrystal(const BlockPos& pos,Actor* target) {
        auto* bs = ClientInstance::get()->getBlockSource();
        if (!bs)
            return false;
        auto* player = ClientInstance::get()->getLocalPlayer();
        if (!player)
            return false;
        if (!target) return false;
        // Define the crystal placement AABB.
        AABB placeAABB(
            glm::vec3(pos.x, pos.y + 1.f, pos.z),
            glm::vec3(pos.x + 1.f, pos.y + 2.f, pos.z + 1.f),
            true
        );

        // Retrieve the local player's AABB.
        AABB playerAABB = target->getAABB();
        // Optionally expand the player's AABB slightly to account for margin.
        playerAABB.mMin -= glm::vec3(0.1f, 0.f, 0.1f);
        playerAABB.mMax += glm::vec3(0.1f, 0.f, 0.1f);

        // If the crystal's placement AABB intersects with the player's AABB, disallow placement.
        if (placeAABB.intersects(playerAABB))
            return false;

        // Check that the player is within 10 units of the candidate crystal's center.
        glm::vec3 crystalCenter(pos.x + 0.5f, pos.y + 1.0f, pos.z + 0.5f);
        return glm::distance(*player->getPos(), crystalCenter) <= 10.f;
    }

    float calculateDamage(const BlockPos& crystalPos, Actor* target) {
        auto* blockSource = ClientInstance::get()->getBlockSource();
        if (!blockSource || !target) return 0.f;

        glm::vec3 explosionPos(crystalPos.x + 0.5f, crystalPos.y + 1.0f, crystalPos.z + 0.5f);
        glm::vec3 targetPos = *target->getPos();

        // Create a CalcUnit to wrap the data
        CalcUnit calc(crystalPos, explosionPos, targetPos);

        // Early-out if the target is too far
        if (calc.distance > 12.0f) return 0.f;

        float visibility = blockSource->getSeenPercent(explosionPos, target->getAABB());
        if (visibility < 0.1f) return 0.f;

        // Java-style explosion damage calculation
        float impact = (1.0f - (calc.distance / 12.0f)) * visibility;
        float baseDamage = ((impact * impact) * 7.0f + impact * 0.5f) * 12.0f + 1.0f;

        // Armor reduction logic (simplified)
        if (auto* equipment = target->getArmorContainer()) {
            float armorValue = 0.f;
            for (int i = 0; i < 4; i++) {
                auto* item = equipment->getItem(i);
                if (item && item->mItem) {
                    float protection = item->getEnchantValue(Enchant::PROTECTION) * 0.04f;
                    float blastProtection = item->getEnchantValue(Enchant::BLAST_PROTECTION) * 0.08f;
                    armorValue += 2.0f + protection + blastProtection;
                }
            }
            baseDamage *= (1.0f - (std::min(20.0f, armorValue) / 25.0f));
        }
        return baseDamage;
    }
    std::vector<PlacePosition> findPlacePositions();
    static std::unordered_map<BlockPos, FoundBlock> mFoundBlocks;
    std::vector<PlacePosition> mPossiblePlacements;
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