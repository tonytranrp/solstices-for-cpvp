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

    // Store static rotation
    mStaticRot = mLastRot;

    player->setFlag<RenderCameraComponent>(true);
    player->setFlag<CameraRenderPlayerModelComponent>(true);

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
    auto staticPos = mStaticPos - *event.mCameraTargetPos;
    auto staticRot = glm::vec2(mStaticRot.mPitch, mStaticRot.mYaw) - *event.mRot;
   // glm::vec2 staticRot(mStaticRot.mPitch, mStaticRot.mYaw);
    auto rots = glm::vec2{ 0,45 };
    auto mCameraTargetPos = glm::vec3{ 0,5,0 };
    original(event._this, event.mEntityRenderContext, event.mEntity, &mCameraTargetPos, &staticPos, &rots, event.mIgnoreLighting);
    event.cancel();
}