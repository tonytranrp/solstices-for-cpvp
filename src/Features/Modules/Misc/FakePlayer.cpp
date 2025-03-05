#include "FakePlayer.hpp"
#include <SDK/Minecraft/ClientInstance.hpp>
#include <Features/Events/ActorRenderEvent.hpp>
#include <Hook/Hooks/RenderHooks/ActorRenderDispatcherHook.hpp>
#include <spdlog/spdlog.h>

FakePlayer::FakePlayer() : ModuleBase<FakePlayer>("FakePlayer", "Render a static copy of your local player", ModuleCategory::Player, 0, false) {
    // Listen to the actor render event so we can inject our fake render.
    
}

void FakePlayer::onEnable() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) {
        spdlog::error("[FakePlayer] No local player found.");
        setEnabled(false);
        return;
    }
    std::unique_ptr< ActorRenderEvent> a;
    auto render = a.get()->mDetour->getOriginal<&ActorRenderDispatcherHook::render>();

    gFeatureManager->mDispatcher->listen<ActorRenderEvent, &FakePlayer::onActorRenderEvent>(this);
    // Save the local player's data one time when enabling.
    mSavedPos = player->getRenderPositionComponent()->mPosition; // or use state vector if preferred
    mSavedAABBMin = player->getAABBShapeComponent()->mMin;
    mSavedAABBMax = player->getAABBShapeComponent()->mMax;
    mSavedRot = *player->getActorRotationComponent();
    mSavedHeadRot = *player->getActorHeadRotationComponent();
    mSavedBodyRot = *player->getMobBodyRotationComponent();
    mSaved = true;
    glm::vec2 fakeRot(mSavedRot.mYaw, mSavedRot.mPitch);
    render(a.get()->_this, a.get()->mEntityRenderContext, a.get()->mEntity, a.get()->mCameraTargetPos, &mSavedPos, &fakeRot, a.get()->mIgnoreLighting);
}

void FakePlayer::onDisable() {
    // Stop listening to the render event.
    gFeatureManager->mDispatcher->deafen<ActorRenderEvent, &FakePlayer::onActorRenderEvent>(this);
    mSaved = false;
}

void FakePlayer::onActorRenderEvent(ActorRenderEvent& event) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // Only process when the entity being rendered is the local player.
    if (event.mEntity != player) return;
    if (!mSaved) return;  // we must have saved data

    // Prepare fake rendering parameters using the stored data.
    glm::vec3 fakePos = mSavedPos;
    // Use the saved yaw and pitch (for example, from ActorRotationComponent)
    glm::vec2 fakeRot(mSavedRot.mYaw, mSavedRot.mPitch);

    // (Optionally, you could also inject the saved AABB into a shader or similar if needed.)

    // Retrieve the original render function from the detour.
    auto original = event.mDetour->getOriginal<&ActorRenderDispatcherHook::render>();
    // Render the fake copy using our stored data.
    //base = *event.mEntityRenderContext;
    original(event._this, event.mEntityRenderContext, event.mEntity, event.mCameraTargetPos, &fakePos, &fakeRot, event.mIgnoreLighting);

    // Do not cancel the event—this way the actual local player will also render.
}
