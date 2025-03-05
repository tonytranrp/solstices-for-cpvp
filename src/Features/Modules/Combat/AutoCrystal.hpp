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

// Custom render mode enum for AutoCrystal visuals.
enum class ACVisualRenderMode {
    Fade,
    Square,
    Low
};

class AutoCrystal : public ModuleBase<AutoCrystal> {
public:
    // --- Advanced Placement Struct ---
    struct PlacePosition {
        BlockPos position{};
        float targetDamage{};
        float selfDamage{};

        PlacePosition(const BlockPos& pos, float targetDmg, float selfDmg)
            : position(pos), targetDamage(targetDmg), selfDamage(selfDmg) {
        }
        PlacePosition() : position(), targetDamage(0.f), selfDamage(0.f) {}
    };

    // --- Break Target Struct ---
    struct BreakTarget {
        Actor* crystal{};       // The crystal actor to break
        float targetDamage{};
        float selfDamage{};

        BreakTarget(Actor* act, float targetDmg, float selfDmg)
            : crystal(act), targetDamage(targetDmg), selfDamage(selfDmg) {
        }
    };

    // --- Calculation Unit Struct ---
    struct CalcUnit {
        BlockPos position{};
        glm::vec3 explosionPos{};
        glm::vec3 targetPos{};
        float distance{};
        float visibility{};

        CalcUnit(const BlockPos& pos, const glm::vec3& explosion, const glm::vec3& target)
            : position(pos), explosionPos(explosion), targetPos(target) {
            distance = glm::distance(target, explosion);
            // For demonstration, assume full visibility.
            visibility = 1.0f;
        }
    };

    // --- Compare Struct ---
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

    enum class PredictMode {
        Packet,
        Send
    };

    std::mutex blockmutex = {};

    // Core module settings
    EnumSettingT<Mode> mMode = EnumSettingT<Mode>("Mode", "The mode of the crystal aura", Mode::Switch, "Single", "Switch");
    EnumSettingT<SwitchMode> mSwitchMode = EnumSettingT<SwitchMode>("Switch Mode", "How to switch slots", SwitchMode::Silent, "None", "Silent", "Normal");
    BoolSetting mAutoPlace = BoolSetting("Auto Place", "Automatically places crystals", true);
    NumberSetting mRange = NumberSetting("Range", "Range to search for targets", 6.0f, 3.0f, 10.0f, 0.1f);
    NumberSetting mPlaceRange = NumberSetting("Place Range", "Range to place crystals", 4.5f, 1.0f, 8.0f, 0.1f);
    NumberSetting mPlaceRangePlayer = NumberSetting("Place Range Player", "Range to place crystals that compare to the localplayer position", 4.5f, 1.0f, 8.0f, 0.1f);
    NumberSetting mPlaceDelay = NumberSetting("Place Delay", "Delay between placements (ms)", 100.0f, 0.0f, 1000.0f, 1.0f);
    NumberSetting mPlaceSearchDelay = NumberSetting("Place Search Delay", "Delay between searching placements (ms)", 100.0f, 0.0f, 1000.0f, 1.0f);
    NumberSetting mMinimumDamage = NumberSetting("Minimum Damage", "Minimum damage to place/break", 6.0f, 1.0f, 20.0f, 0.5f);
    NumberSetting mMaxSelfDamage = NumberSetting("Max Self Damage", "Maximum self damage", 8.0f, 1.0f, 100.0f, 0.5f);
   
    BoolSetting mRaycast = BoolSetting("Raycast", "Check line of sight to crystals", true);
    BoolSetting mBreakOnlyOwn = BoolSetting("Break Own", "Only break crystals that we placed", true);
    BoolSetting mIdPredict = BoolSetting("ID Predict", "Predicts crystal entity IDs for faster breaking", false);
    EnumSettingT<PredictMode> mPredictMode = EnumSettingT<PredictMode>("Predict Mode", "How predict Id will work", PredictMode::Packet, "Packet", "Send");
    NumberSetting mPredictAmount = NumberSetting("Predict Amount", "Number of packets to send per predict", 15, 1, 30, 1);
    BoolSetting mSwitchBack = BoolSetting("Switch Back", "Switch back to previous slot", true);
    //
    BoolSetting mVisualizePlace = BoolSetting("Visualize", "Show crystal placement spots", true);
    // Visual render mode setting using our custom enum.
    EnumSettingT<ACVisualRenderMode> mRenderMode = EnumSettingT<ACVisualRenderMode>("Render Mode", "The visual style to use", ACVisualRenderMode::Low, "Fade", "Square", "Low");
    // --- Visual Settings for Fade Render Mode ---
    NumberSetting mFadeTime = NumberSetting("Fade Time", "Time for fade transition", 1.0f, 0.1f, 5.0f, 0.1f);
    NumberSetting mFadeDuration = NumberSetting("Fade Duration", "Duration of fade", 2.0f, 0.5f, 10.0f, 0.1f);
    NumberSetting mFadeLerpSpeed = NumberSetting("Fade Lerp Speed", "Speed of fade interpolation", 1.0f, 0.1f, 5.0f, 0.1f);
    ColorSetting mFadeColor = ColorSetting("Fade Color", "Color for fade render",  255, 0, 0, 255 );

    // --- Visual Settings for Square Render Mode ---
    ColorSetting mSquareColor = ColorSetting("Square Color", "Color for square render",  0, 255, 0, 255 );
    NumberSetting mSquareSize = NumberSetting("Square Size", "Size of square render", 20.0f, 5.0f, 50.0f, 0.5f);

    // --- Visual Settings for Low Render Mode ---
    // (These settings mirror the fade group for consistency.)
    NumberSetting mLowTime = NumberSetting("Low Time", "Time for low render transition", 1.0f, 0.1f, 5.0f, 0.1f);
    NumberSetting mLowDuration = NumberSetting("Low Duration", "Duration of low render", 2.0f, 0.5f, 10.0f, 0.1f);
    NumberSetting mLowLerpSpeed = NumberSetting("Low Lerp Speed", "Speed of low interpolation", 1.0f, 0.1f, 5.0f, 0.1f);
    ColorSetting mLowColor = ColorSetting("Low Color", "Color for low render",  0, 0, 255, 255 );

    AutoCrystal() : ModuleBase("AutoCrystal", "Automatically places and breaks end crystals", ModuleCategory::Combat, 0, false) {
        addSettings(
            &mMode,
            &mSwitchMode,
            &mAutoPlace,
            &mRange,
            &mPlaceRange,
            &mPlaceRangePlayer,
            &mPlaceDelay,
            &mPlaceSearchDelay,
            &mMinimumDamage,
            &mMaxSelfDamage,
            &mRaycast,
            &mBreakOnlyOwn,
            &mIdPredict,
            &mPredictMode,
            &mPredictAmount,
            &mSwitchBack,
            &mVisualizePlace,
            &mRenderMode,
            // Fade render settings
            &mFadeTime,
            &mFadeDuration,
            &mFadeLerpSpeed,
            &mFadeColor,
            // Square render settings
            &mSquareColor,
            &mSquareSize,
            // Low render settings
            &mLowTime,
            &mLowDuration,
            &mLowLerpSpeed,
            &mLowColor
        );

        // Set visibility conditions for each visual settings group.
        VISIBILITY_CONDITION(mFadeTime, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Fade);
        VISIBILITY_CONDITION(mFadeDuration, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Fade);
        VISIBILITY_CONDITION(mFadeLerpSpeed, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Fade);
        VISIBILITY_CONDITION(mFadeColor, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Fade);

        VISIBILITY_CONDITION(mSquareColor, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Square);
        VISIBILITY_CONDITION(mSquareSize, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Square);

        VISIBILITY_CONDITION(mLowTime, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Low);
        VISIBILITY_CONDITION(mLowDuration, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Low);
        VISIBILITY_CONDITION(mLowLerpSpeed, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Low);
        VISIBILITY_CONDITION(mLowColor, mVisualizePlace.mValue && mRenderMode.mValue == ACVisualRenderMode::Low);

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
    void onBlockChangedEvent(BlockChangedEvent& event) {}

private:
    glm::vec3 mBreakTargetPos = glm::vec3(0.0f);
    bool mHasBreakTarget = false;
    int mServerSlot = -1; // which slot the server currently thinks we’re holding

    BlockPos mLastPlacedPos{};      ///< Position where our last crystal was placed.
    bool mHasPlacedCrystal = false; ///< Whether our module has placed a crystal.
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
