#pragma once

#include <Features/Modules/Module.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <glm/glm.hpp>

class FakePlayer : public ModuleBase<FakePlayer> {
public:
    FakePlayer();

    void onEnable() override;
    void onDisable() override;

    // We'll listen to ActorRenderEvent to inject our fake render.
    void onActorRenderEvent(class ActorRenderEvent& event);

private:
    bool mSaved = false;
    glm::vec3 mSavedPos = glm::vec3(0.0f);
    glm::vec3 mSavedAABBMin = glm::vec3(0.0f);
    glm::vec3 mSavedAABBMax = glm::vec3(0.0f);
    // Save player's rotation components.
    struct {
        float mYaw, mPitch;
    } mSavedRot;
    struct {
        float mHeadRot;
    } mSavedHeadRot;
    struct {
        float mBodyRot;
    } mSavedBodyRot;
};
