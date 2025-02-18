#pragma once
//
// Created by vastrakai on 7/8/2024.
//

#include <Features/Modules/Module.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>


class Aura : public ModuleBase<Aura> {
public:
    enum class Mode {
        Single,
        Multi,
        Switch
    };

    enum class AttackMode {
        Earliest,
        Synched
    };

    enum class RotateMode {
        None,
        Normal,
        Flick
    };

    enum class SwitchMode {
        None,
        Full,
        Spoof
    };

    enum class BypassMode {
        None,
        FlareonV2,
        Raycast
    };
    enum class RenderMode {
        Spheres,
        Corners,
        Both
    };

    EnumSettingT<Mode> mMode = EnumSettingT("Mode", "The mode of the aura", Mode::Switch, "Single", "Multi", "Switch");
    EnumSettingT<AttackMode> mAttackMode = EnumSettingT("Attack Mode", "The mode of attack", AttackMode::Earliest, "Earliest", "Synched");
    EnumSettingT<RotateMode> mRotateMode = EnumSettingT("Rotate Mode", "The mode of rotation", RotateMode::Normal, "None", "Normal", "Flick");
    EnumSettingT<SwitchMode> mSwitchMode = EnumSettingT("Switch Mode", "The mode of switching", SwitchMode::None, "None", "Full", "Spoof");
    EnumSettingT<BypassMode> mBypassMode = EnumSettingT("Bypass Mode", "The type of bypass", BypassMode::Raycast, "None", "FlareonV2", "Raycast");
    BoolSetting mAutoFireSword = BoolSetting("Auto Fire Sword", "Whether or not to automatically use the fire sword", false);
    BoolSetting mFireSwordSpoof = BoolSetting("Fire Sword Spoof", "Whether or not to spoof the fire sword", false);
    BoolSetting mHotbarOnly = BoolSetting("Hotbar Only", "Whether or not to only attack with items in the hotbar", false);
    BoolSetting mFistFriends = BoolSetting("Fist Friends", "Whether or not to fist friends", false);
    NumberSetting mRange = NumberSetting("Range", "The range at which to attack enemies", 5, 0, 10, 0.01);
    BoolSetting mDynamicRange = BoolSetting("Dynamic Range", "Sets the range to the specified value when not moving", false);
    NumberSetting mDynamicRangeValue = NumberSetting("Dynamic Value", "The value for the dynamic range", 3, 0, 10, 0.01);
    BoolSetting mRandomizeAPS = BoolSetting("Randomize APS", "Whether or not to randomize the APS", false);
    NumberSetting mAPS = NumberSetting("APS", "The amount of attacks per second", 10, 0, 20, 0.01);
    NumberSetting mAPSMin = NumberSetting("APS Min", "The minimum APS to randomize", 10, 0, 20, 0.01);
    NumberSetting mAPSMax = NumberSetting("APS Max", "The maximum APS to randomize", 20, 0, 20, 0.01);
    BoolSetting mInteract = BoolSetting("Interact", "Whether or not to interact with the target", false);
    BoolSetting mThrowProjectiles = BoolSetting("Throw Projectiles", "Whether or not to throw projectiles at the target", false);
    NumberSetting mThrowDelay = NumberSetting("Throw Delay", "The delay between throwing projectiles (in ticks)", 1, 0, 20, 0.01);
    BoolSetting mAutoBow = BoolSetting("Auto Bow", "Whether or not to automatically shoot the bow", false);
    BoolSetting mSwing = BoolSetting("Swing", "Whether or not to swing the arm", true);
    BoolSetting mSwingDelay = BoolSetting("Swing Delay", "Whether or not to delay the swing", false);
    NumberSetting mSwingDelayValue = NumberSetting("Swing Delay Value", "The delay between swings (in seconds)", 4.5f, 0.f, 10.f, 0.01f);
    BoolSetting mStrafe = BoolSetting("Strafe", "Whether or not to strafe around the target", true);
    BoolSetting mAttackThroughWalls = BoolSetting("Attack through walls", "Whether or not to attack through walls", true);
    BoolSetting mThirdPerson = BoolSetting("Third Person", "Whether or not switch to third-person camera view on enable", false);
    BoolSetting mThirdPersonOnlyOnAttack = BoolSetting("Only On Attack", "Switch to third-person view only when attacking", false);
    EnumSettingT<RenderMode> mRenderMode = EnumSettingT("Render Mode", "The visual style to use", RenderMode::Both, "Spheres", "Corners", "Both");
    BoolSetting mVisuals = BoolSetting("Visuals", "Whether or not to render visuals around the target", true);
    NumberSetting mUpDownSpeed = NumberSetting("Up-Down Speed", "Speed of spheres rotate", 1.2, 0, 20, 0.01);
    NumberSetting mSpheresAmount = NumberSetting("Spheres Amount", "Amount of spheres to draw", 12, 0, 20, 1);
    NumberSetting mSpheresSizeMultiplier = NumberSetting("Spheres Size multiplier", "Multiplied size of spheres", 0.2, 0, 3, 0.01);
    NumberSetting mSpheresSize = NumberSetting("Spheres Size", "Size of spheres", 4, 0, 20, 0.01);
    NumberSetting mSpheresMinSize = NumberSetting("Spheres Min Size", "Min size of spheres", 2.60, 0, 20, 0.01);
    NumberSetting mSpheresRadius = NumberSetting("Spheres radius", "Distance from spheres to target", 0.9, 0, 2, 0.1);
    // new rener kind type 
    NumberSetting mBaseSize = NumberSetting("Corner Size", "Base size of the corner indicators", 20.0f, 5.0f, 50.0f, 0.5f);
    NumberSetting mAnimationDuration = NumberSetting("Corner Anim Time", "Duration of corner animation cycle", 2.0f, 0.5f, 5.0f, 0.1f);
    NumberSetting mAccelTime = NumberSetting("Corner Accel Time", "Time taken to reach max speed", 0.5f, 0.1f, 2.0f, 0.1f);
    NumberSetting mRotationSpeed = NumberSetting("Corner Base Rotation", "Base rotation speed", 90.0f, 0.0f, 360.0f, 5.0f);
    NumberSetting mMaxRotationSpeed = NumberSetting("Corner Max Rotation", "Maximum rotation speed", 360.0f, 90.0f, 720.0f, 5.0f);
    NumberSetting mScaleMin = NumberSetting("Corner Min Scale", "Minimum scale of corners", 0.8f, 0.1f, 1.0f, 0.05f);
    NumberSetting mScaleMax = NumberSetting("Corner Max Scale", "Maximum scale of corners", 1.2f, 1.0f, 2.0f, 0.05f);
    NumberSetting mLineThickness = NumberSetting("Line Thickness", "Thickness of corner lines", 2.0f, 1.0f, 5.0f, 0.1f);
    NumberSetting mGlowStrength = NumberSetting("Glow Strength", "Strength of the corner glow effect", 1.0f, 0.0f, 3.0f, 0.1f);
    NumberSetting mCornerSize = NumberSetting("Corner Length", "Length of each corner segment", 35.0f, 10.0f, 100.0f, 1.0f);
    //
    BoolSetting mDisableOnDimensionChange = BoolSetting("Auto Disable", "Whether or not to disable the aura on dimension change", true);
    BoolSetting mDebug = BoolSetting("Debug", "Whether or not to display debug information", false);

    Aura() : ModuleBase("Aura", "Automatically attacks nearby enemies", ModuleCategory::Combat, 0, false) {
        addSettings(
            &mMode,
            &mAttackMode,
            &mRotateMode,
            &mSwitchMode,
            &mBypassMode,
#ifdef __PRIVATE_BUILD__
            & mAutoFireSword,
            &mFireSwordSpoof,
#endif
            & mHotbarOnly,
            &mFistFriends,
            &mVisuals,
            &mRenderMode,  // Add new render mode setting
            // Corner render settings
            &mBaseSize,
            &mAnimationDuration,
            &mAccelTime,
            &mRotationSpeed,
            &mMaxRotationSpeed,
            &mScaleMin,
            &mScaleMax,
            & mLineThickness,
            & mGlowStrength,
            & mCornerSize,
            // Sphere render settings
            &mUpDownSpeed,
            &mSpheresAmount,
            &mSpheresSizeMultiplier,
            &mSpheresSize,
            &mSpheresMinSize,
            &mSpheresRadius,
            // Rest of original settings
            &mRange,
            &mDynamicRange,
            &mDynamicRangeValue,
            &mRandomizeAPS,
            &mAPS,
            &mAPSMin,
            &mAPSMax,
            &mThrowProjectiles,
            &mThrowDelay,
#ifdef __PRIVATE_BUILD__
            & mAutoBow,
#endif
            & mAttackThroughWalls,
            &mSwing,
            &mSwingDelay,
            &mSwingDelayValue,
            &mStrafe,
            &mThirdPerson,
            &mThirdPersonOnlyOnAttack,
            &mDisableOnDimensionChange,
            &mDebug
        );

        VISIBILITY_CONDITION(mAutoFireSword, mSwitchMode.mValue != SwitchMode::None);
        VISIBILITY_CONDITION(mFireSwordSpoof, mAutoFireSword.mValue);
        VISIBILITY_CONDITION(mAPS, !mRandomizeAPS.mValue);
        VISIBILITY_CONDITION(mAPSMin, mRandomizeAPS.mValue);
        VISIBILITY_CONDITION(mAPSMax, mRandomizeAPS.mValue);
        VISIBILITY_CONDITION(mThrowDelay, mThrowProjectiles.mValue);
        VISIBILITY_CONDITION(mDynamicRangeValue, mDynamicRange.mValue);
        VISIBILITY_CONDITION(mSwingDelay, mSwing.mValue);
        VISIBILITY_CONDITION(mSwingDelayValue, mSwingDelay.mValue && mSwing.mValue);
        VISIBILITY_CONDITION(mThirdPersonOnlyOnAttack, mThirdPerson.mValue);

        // Visibility conditions for visual settings
        VISIBILITY_CONDITION(mUpDownSpeed, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Spheres || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mSpheresAmount, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Spheres || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mSpheresSizeMultiplier, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Spheres || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mSpheresSize, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Spheres || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mSpheresMinSize, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Spheres || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mSpheresRadius, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Spheres || mRenderMode.mValue == RenderMode::Both));

        // Visibility conditions for corner settings
        VISIBILITY_CONDITION(mBaseSize, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mAnimationDuration, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mAccelTime, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mRotationSpeed, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mMaxRotationSpeed, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mScaleMin, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mScaleMax, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mLineThickness, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mGlowStrength, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));
        VISIBILITY_CONDITION(mCornerSize, mVisuals.mValue && (mRenderMode.mValue == RenderMode::Corners || mRenderMode.mValue == RenderMode::Both));

        mNames = {
            {Lowercase, "aura"},
            {LowercaseSpaced, "aura"},
            {Normal, "Aura"},
            {NormalSpaced, "Aura"}
        };
    }

    AABB mTargetedAABB = AABB();
    bool mRotating = false;
    static inline bool sHasTarget = false;
    static inline Actor* sTarget = nullptr;
    static inline int64_t sTargetRuntimeID = 0;
    int64_t mLastSwing = 0;
    int64_t mLastTransaction = 0;
    int mLastSlot = 0;
    bool mIsThirdPerson = false;
    //fo the render thingi 
    float mRotation = 0.0f;
    float mRotationVelocity = 0.0f;
    float mAnimTimeCounter = 0.0f;  // Changed from mAnimationTime to avoid name conflict
    float mScale = 1.0f;
    float mEnableProgress = 0.0f;
    bool mIsEnabling = false;
    void renderSpheres(Actor* player, Actor* target);
    void renderCorners(const glm::vec2& center, float size, float rotation, float scale, float alpha = 1.0f);
    //
    int getSword(Actor* target);
    bool shouldUseFireSword(Actor* target);
    void onEnable() override;
    void onDisable() override;
    void rotate(Actor* target);
    void shootBow(Actor* target);
    void throwProjectiles(Actor* target);
    void onRenderEvent(class RenderEvent& event);
    void onBaseTickEvent(class BaseTickEvent& event);
    void onPacketOutEvent(class PacketOutEvent& event);
    void onPacketInEvent(class PacketInEvent& event);
    void onBobHurtEvent(class BobHurtEvent& event);
    void onBoneRenderEvent(class BoneRenderEvent& event);
    Actor* findObstructingActor(Actor* player, Actor* target);

    std::string getSettingDisplay() override {
        return mMode.mValues[mMode.as<int>()];
    }
};
