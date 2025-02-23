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

    // --- Break Target Struct ---
    // This struct encapsulates an actor to break and the calculated damage values
    struct BreakTarget {
        Actor* crystal;       // The crystal actor to break
        float targetDamage;
        float selfDamage;

        BreakTarget(Actor* act, float targetDmg, float selfDmg)
            : crystal(act), targetDamage(targetDmg), selfDamage(selfDmg) {
        }
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
    NumberSetting mPlaceSearchDelay = NumberSetting("Place Search Delay", "Delay between searching placements (ms)", 100.0f, 0.0f, 1000.0f, 1.0f);
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
            &mPlaceSearchDelay,
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
        
    };
private:
    AABB TargetAabb = AABB();
    bool mRotating = false;
    int mCrystalSlot = -1;
    int mPrevSlot = -1;
    bool mShouldSpoofSlot = false;
    bool mHasSwitched = false;
    void switchToCrystal();
    void switchBack();

    float calculateDamage(const BlockPos& crystalPos, Actor* target);
    bool canPlaceCrystal(const BlockPos& pos, const std::vector<Actor*>& runtimeActors);
    std::vector<PlacePosition> findPlacePositions(const std::vector<Actor*>& runtimeActors);
    std::vector<BreakTarget> findBreakTargets(const std::vector<Actor*>& runtimeActors);
    void placeCrystal(const PlacePosition& pos);
    void breakCrystal(Actor* crystal);
    std::vector<PlacePosition> getplacmenet(const std::vector<Actor*>& runtimeActors);
    std::vector<PlacePosition> mPossiblePlacements;
    uint64_t mLastPlace = 0;
    uint64_t mLastsearchPlace = 0;
    uint64_t mLastAttackId = -1;
    bool mShouldIdPredict = false;
    Actor* mLastTarget = nullptr;
    float mCurrArmor = 0.f;
};