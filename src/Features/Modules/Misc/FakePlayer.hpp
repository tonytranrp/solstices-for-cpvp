#pragma once

#include <Features/Modules/Module.hpp>
#include <SDK/Minecraft/Actor/Components/ActorRotationComponent.hpp>
#include <SDK/Minecraft/Actor/Components/ActorHeadRotationComponent.hpp>
#include <SDK/Minecraft/Rendering/BaseActorRenderContext.hpp>
#include <SDK/Minecraft/Actor/Components/MobBodyRotationComponent.hpp>
#include <glm/vec3.hpp>
#include <glm/vec2.hpp>

class FakePlayer : public ModuleBase<FakePlayer> {
public:
    FakePlayer();

    void onEnable() override;
    void onDisable() override;

    // This event handler will render the fake local player.
    void onActorRenderEvent(class ActorRenderEvent& event);

private:
    // Saved data from the local player when FakePlayer is enabled.
    bool mSaved = false;
    glm::vec3 mSavedPos;          // saved render (or state) position
    glm::vec3 mSavedAABBMin;      // saved bounding box min
    glm::vec3 mSavedAABBMax;      // saved bounding box max
    ActorRotationComponent mSavedRot;
    ActorHeadRotationComponent mSavedHeadRot;
    MobBodyRotationComponent mSavedBodyRot;
    //class BaseActorRenderContext base;
};
