#pragma once

#include <Features/Modules/Module.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <glm/glm.hpp>
class FakePlayer : public ModuleBase<FakePlayer> {
public:
    enum class Mode
    {
        Normal,
        Detached
    };

    EnumSettingT<Mode> mMode = EnumSettingT<Mode>("Mode", "The mode of the FakePlayer\nNormal: Fakes the player's position\nDetached: Moves independently of the player", Mode::Normal, "Normal", "Detached");
    NumberSetting mSpeed = NumberSetting("Speed", "Speed of the FakePlayer", 5.5f, 0.1f, 10.0f, 0.1f);
    BoolSetting mDisableOnLagback = BoolSetting("Disable On Lagback", "Disable FakePlayer if you receive a teleport", true);

    FakePlayer() : ModuleBase<FakePlayer>("FakePlayer", "Move independently of your player", ModuleCategory::Player, 0, false) {
        addSetting(&mMode);
        addSetting(&mSpeed);
        addSetting(&mDisableOnLagback);

        mNames = {
            {Lowercase, "FakePlayer"},
            {LowercaseSpaced, "FakePlayer"},
            {Normal, "FakePlayer"},
            {NormalSpaced, "FakePlayer"}
        };

        //gFeatureManager->mDispatcher->listen<LookInputEvent, &FakePlayer::onLookInputEvent>(this);
    }

    void onEnable() override;
    void onDisable() override;

    // We'll listen to ActorRenderEvent to inject our fake render.
    void onActorRenderEvent(class ActorRenderEvent& event);
    glm::vec3 aabbmin;
    glm::vec3 aabbmax;
    AABB aabb;
    glm::vec3 mStaticPos;
private:
    ActorRotationComponent mLastRot;
    ActorHeadRotationComponent mLastHeadRot;
    MobBodyRotationComponent mLastBodyRot;
    glm::vec3 mAABBMin;
    glm::vec3 mAABBMax;
    glm::vec3 mSvPos;
    glm::vec3 mSvPosOld;
    glm::vec3 mOldPos;
    ActorHeadRotationComponent mStaticHeadRot;
    MobBodyRotationComponent mStaticBodyRot;
    ActorRotationComponent mStaticRot;
    // Detached mode vars
    glm::vec3 mOrigin;
    glm::vec3 mOldOrigin;
    glm::vec2 mRotRads;

    
};
