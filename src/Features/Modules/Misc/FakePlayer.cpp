#include "FakePlayer.hpp"

#include <Features/Events/ActorRenderEvent.hpp>
#include <Hook/Hooks/RenderHooks/ActorRenderDispatcherHook.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <spdlog/spdlog.h>

FakePlayer::FakePlayer()
    : ModuleBase<FakePlayer>("FakePlayer", "Render a static copy of your local player", ModuleCategory::Player, 0, false)
{
}

void FakePlayer::onEnable()
{
    auto* clientInstance = ClientInstance::get();
    auto* player = clientInstance ? clientInstance->getLocalPlayer() : nullptr;
    if (!player)
    {
        spdlog::warn("[FakePlayer] No local player found, disabling.");
        setEnabled(false);
        return;
    }

    auto* renderPos = player->getRenderPositionComponent();
    auto* aabb = player->getAABBShapeComponent();
    auto* rot = player->getActorRotationComponent();
    auto* headRot = player->getActorHeadRotationComponent();
    auto* bodyRot = player->getMobBodyRotationComponent();

    if (!renderPos || !aabb || !rot || !headRot || !bodyRot)
    {
        spdlog::warn("[FakePlayer] Missing required actor components, disabling.");
        setEnabled(false);
        return;
    }

    if (!gFeatureManager || !gFeatureManager->mDispatcher)
    {
        spdlog::warn("[FakePlayer] Event dispatcher is unavailable, disabling.");
        setEnabled(false);
        return;
    }

    // Snapshot the player pose once when the module is enabled.
    mSavedPos = renderPos->mPosition;
    mSavedAABBMin = aabb->mMin;
    mSavedAABBMax = aabb->mMax;
    mSavedRot = *rot;
    mSavedHeadRot = *headRot;
    mSavedBodyRot = *bodyRot;
    mSaved = true;

    gFeatureManager->mDispatcher->listen<ActorRenderEvent, &FakePlayer::onActorRenderEvent>(this);
}

void FakePlayer::onDisable()
{
    if (gFeatureManager && gFeatureManager->mDispatcher)
        gFeatureManager->mDispatcher->deafen<ActorRenderEvent, &FakePlayer::onActorRenderEvent>(this);

    mSaved = false;
}

void FakePlayer::onActorRenderEvent(ActorRenderEvent& event)
{
    auto* clientInstance = ClientInstance::get();
    auto* player = clientInstance ? clientInstance->getLocalPlayer() : nullptr;
    if (!player || !mSaved)
        return;

    if (event.mEntity != player)
        return;

    if (!event.mDetour || !event._this || !event.mEntityRenderContext || !event.mEntity || !event.mCameraTargetPos || !event.mPos)
        return;

    if (event.mRot && *event.mPos == glm::vec3(0.f, 0.f, 0.f) && *event.mRot == glm::vec2(0.f, 0.f))
        return;

    auto original = event.mDetour->getOriginal<&ActorRenderDispatcherHook::render>();
    if (!original)
        return;

    glm::vec3 fakePos = mSavedPos;
    glm::vec2 fakeRot(mSavedRot.mYaw, mSavedRot.mPitch);
    original(event._this, event.mEntityRenderContext, event.mEntity, event.mCameraTargetPos, &fakePos, &fakeRot, event.mIgnoreLighting);

    // Intentionally do not cancel so the real local player still renders too.
}
