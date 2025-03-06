#include "FakePlayer.hpp"
#include <SDK/Minecraft/ClientInstance.hpp>
#include <Features/Events/ActorRenderEvent.hpp>
#include <Hook/Hooks/RenderHooks/ActorRenderDispatcherHook.hpp>
#include <spdlog/spdlog.h>

FakePlayer::FakePlayer()
    : ModuleBase<FakePlayer>("FakePlayer", "Render a static copy of your local player", ModuleCategory::Player, 0, false) {
    // No extra initialization here.
}

void FakePlayer::onEnable() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) {
        spdlog::error("[FakePlayer] No local player found.");
        setEnabled(false);
        return;
    }

    // Capture the player's render data once when enabling.
    mSavedPos = player->getRenderPositionComponent()->mPosition;
    mSavedAABBMin = player->getAABBShapeComponent()->mMin;
    mSavedAABBMax = player->getAABBShapeComponent()->mMax;

    auto* rot = player->getActorRotationComponent();
    mSavedRot.mYaw = rot->mYaw;
    mSavedRot.mPitch = rot->mPitch;

    auto* headRot = player->getActorHeadRotationComponent();
    mSavedHeadRot.mHeadRot = headRot->mHeadRot;

    auto* bodyRot = player->getMobBodyRotationComponent();
    mSavedBodyRot.mBodyRot = bodyRot->yBodyRot;

    mSaved = true;
    ChatUtils::displayClientMessage("FakePlayer enabled, saved local player render data.");

    // Listen to the actor render event.
    gFeatureManager->mDispatcher->listen<ActorRenderEvent, &FakePlayer::onActorRenderEvent>(this);
}

void FakePlayer::onDisable() {
    // Stop listening to the render event.
    gFeatureManager->mDispatcher->deafen<ActorRenderEvent, &FakePlayer::onActorRenderEvent>(this);
    mSaved = false;
    ChatUtils::displayClientMessage("FakePlayer disabled.");
}

void FakePlayer::onActorRenderEvent(ActorRenderEvent& event) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // Process only if the entity being rendered is the local player.
    if (event.mEntity != player) return;
    if (!mSaved) return;

    // Retrieve the original render function from the detour.
    auto original = event.mDetour->getOriginal<&ActorRenderDispatcherHook::render>();
    if (!original) {
        spdlog::error("[FakePlayer] Unable to retrieve original render function.");
        return;
    }

    // Use our saved position and rotation.
    glm::vec3 fakePos = mSavedPos;
    glm::vec2 fakeRot(mSavedRot.mYaw, mSavedRot.mPitch);

    // Optionally, add a slight offset (e.g. 0.2 blocks to the side) so that the fake copy is visible.
    fakePos.x += 0.2f;

    // Render the fake (static) copy.
    original(event._this, event.mEntityRenderContext, event.mEntity, event.mCameraTargetPos, &fakePos, &fakeRot, event.mIgnoreLighting);

    // Do not cancel the event so that the actual local player also renders.
}
