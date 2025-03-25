#include "FakePlayer.hpp"
#include <SDK/Minecraft/ClientInstance.hpp>
#include <Features/Events/ActorRenderEvent.hpp>
#include <Hook/Hooks/RenderHooks/ActorRenderDispatcherHook.hpp>
#include <spdlog/spdlog.h>
void FakePlayer::onEnable() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) {
        spdlog::error("[FakePlayer] No local player found.");
        setEnabled(false);
        return;
    }

    // Store current player state for the static player
    mLastRot = *player->getActorRotationComponent();
    mLastHeadRot = *player->getActorHeadRotationComponent();
    mLastBodyRot = *player->getMobBodyRotationComponent();
    aabb = player->getAABB();
    mAABBMin = player->getAABBShapeComponent()->mMin;
    mAABBMax = player->getAABBShapeComponent()->mMax;
    mSvPos = player->getStateVectorComponent()->mPos;
    mSvPosOld = player->getStateVectorComponent()->mPos;
    mOldPos = player->getRenderPositionComponent()->mPosition;
    aabbmin = mAABBMin;
    aabbmax = mAABBMax;
    mStaticPos = mOldPos;

    // Store static rotations
    mStaticRot = mLastRot;
    mStaticHeadRot = mLastHeadRot;
    mStaticBodyRot = mLastBodyRot;

    // Enable third person camera rendering
    player->setFlag<RenderCameraComponent>(true);
    player->setFlag<CameraRenderPlayerModelComponent>(true);
    
    ChatUtils::displayClientMessage("FakePlayer enabled. You should now see both a static player and your moving player.");

    gFeatureManager->mDispatcher->listen<ActorRenderEvent, &FakePlayer::onActorRenderEvent, nes::event_priority::VERY_FIRST>(this);
}

void FakePlayer::onDisable() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (player) {
        player->setFlag<CameraRenderPlayerModelComponent>(false);
        player->setFlag<RenderCameraComponent>(false);
    }

    gFeatureManager->mDispatcher->deafen<ActorRenderEvent, &FakePlayer::onActorRenderEvent>(this);
    ChatUtils::displayClientMessage("FakePlayer disabled.");
}

void FakePlayer::onActorRenderEvent(ActorRenderEvent& event) {
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    if (event.mEntity != player) return;

    auto original = event.mDetour->getOriginal<&ActorRenderDispatcherHook::render>();

    // Use stored static position and rotation
    // Calculate static position relative to camera
    auto staticPos = mStaticPos - *event.mCameraTargetPos;
    
    // Use stored static rotation without any modifications
    auto staticRot = glm::vec2(mStaticRot.mPitch, mStaticRot.mYaw);
    
    // Save current rotations
    auto currentRot = *player->getActorRotationComponent();
    auto currentHeadRot = *player->getActorHeadRotationComponent();
    auto currentBodyRot = *player->getMobBodyRotationComponent();

    // Set static rotations for the fake player
    *player->getActorRotationComponent() = mStaticRot;
    *player->getActorHeadRotationComponent() = mStaticHeadRot;
    *player->getMobBodyRotationComponent() = mStaticBodyRot;

    // Render the static player
    original(event._this, event.mEntityRenderContext, event.mEntity, event.mCameraTargetPos, &staticPos, event.mRot, event.mIgnoreLighting);

    // Restore rotations for the real player
    *player->getActorRotationComponent() = currentRot;
    *player->getActorHeadRotationComponent() = currentHeadRot;
    *player->getMobBodyRotationComponent() = currentBodyRot;
    
    // Don't cancel the event so the real player will also be rendered
    // This allows both the static fake player and the moving real player to be visible
}