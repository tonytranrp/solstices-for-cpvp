// PacketMine.hpp
#pragma once

#include <Features/Modules/Module.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <Features/FeatureManager.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Actor/GameMode.hpp>
#include <SDK/Minecraft/Network/Packets/MobEquipmentPacket.hpp>
#include <SDK/Minecraft/World/Level.hpp>
#include <SDK/Minecraft/World/HitResult.hpp>
#include <SDK/Minecraft/Network/PacketID.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>

class PacketMine : public ModuleBase<PacketMine> {
public:
    enum class SwitchMode {
        None,
        Silent,
        Normal
    };

    // Settings
    NumberSetting mRange = NumberSetting("Range", "Maximum mining range", 5.0f, 1.0f, 10.0f, 0.1f);
    BoolSetting mContinue = BoolSetting("Continue", "Continue mining block after completion", false);
    NumberSetting mContinueSpeed = NumberSetting("Continue Speed", "Mining speed after completion", 0.1f, 0.01f, 1.0f, 0.01f);
    EnumSettingT<SwitchMode> mSwitchMode = EnumSettingT("Switch Mode", "How to switch tools", SwitchMode::Silent, "None", "Silent", "Normal");
    BoolSetting mSwitchBack = BoolSetting("Switch Back", "Switch back to previous slot", true);

    // Visual settings
    BoolSetting mVisuals = BoolSetting("Visuals", "Show mining progress", true);
    NumberSetting mLineThickness = NumberSetting("Line Thickness", "Thickness of outline", 2.0f, 1.0f, 5.0f, 0.1f);
    NumberSetting mBoxOpacity = NumberSetting("Box Opacity", "Transparency of box", 0.4f, 0.0f, 1.0f, 0.05f);

    PacketMine() : ModuleBase("PacketMine", "Break blocks using packets", ModuleCategory::Player, 0, false) {
        addSettings(
            &mRange,
            &mContinue,
            &mContinueSpeed,
            &mSwitchMode,
            &mSwitchBack,
            &mVisuals,
            &mLineThickness,
            &mBoxOpacity
        );

        VISIBILITY_CONDITION(mContinueSpeed, mContinue.mValue);
        VISIBILITY_CONDITION(mSwitchBack, mSwitchMode.mValue != SwitchMode::None);
        VISIBILITY_CONDITION(mLineThickness, mVisuals.mValue);
        VISIBILITY_CONDITION(mBoxOpacity, mVisuals.mValue);

        mNames = {
            {Lowercase, "packetmine"},
            {LowercaseSpaced, "packet mine"},
            {Normal, "PacketMine"},
            {NormalSpaced, "Packet Mine"}
        };
    }

    void onEnable() override {
        gFeatureManager->mDispatcher->listen<BaseTickEvent, &PacketMine::onBaseTickEvent>(this);
        gFeatureManager->mDispatcher->listen<PacketOutEvent, &PacketMine::onPacketOutEvent>(this);
        gFeatureManager->mDispatcher->listen<RenderEvent, &PacketMine::onRenderEvent>(this);
    }

    void onDisable() override {
        gFeatureManager->mDispatcher->deafen<BaseTickEvent, &PacketMine::onBaseTickEvent>(this);
        gFeatureManager->mDispatcher->deafen<PacketOutEvent, &PacketMine::onPacketOutEvent>(this);
        gFeatureManager->mDispatcher->deafen<RenderEvent, &PacketMine::onRenderEvent>(this);
        resetMining();
    }

    void resetMining() {
        mTargetPos = glm::ivec3(INT_MAX);
        mTargetFace = -1;
        // Also reset the GameMode's break progress
        if (auto player = ClientInstance::get()->getLocalPlayer()) {
            if (auto gm = player->getGameMode()) {
                gm->mBreakProgress = 0.0f;
            }
        }
        mIsMining = false;
        mShouldSpoofSlot = true;
        mToolSlot = -1;
    }

    void setTargetBlock(const glm::ivec3& pos, int face) {
        auto player = ClientInstance::get()->getLocalPlayer();
        if (!player) return;

        if (!BlockUtils::isGoodBlock(pos)) {
            resetMining();
            return;
        }

        auto block = ClientInstance::get()->getBlockSource()->getBlock(pos);
        if (!block) {
            resetMining();
            return;
        }

        // Reset break progress on the GameMode
        if (auto gm = player->getGameMode()) {
            gm->mBreakProgress = 0.0f;
        }

        mTargetPos = pos;
        mTargetFace = face;
        mIsMining = true;

        // Get best tool for the block
        mToolSlot = ItemUtils::getBestBreakingTool(block, true);
        if (mToolSlot != -1 && mSwitchMode.mValue != SwitchMode::None) {
            if (mSwitchMode.mValue == SwitchMode::Silent) {
                PacketUtils::spoofSlot(mToolSlot, false);
                mShouldSpoofSlot = false;
            }
            else {
                player->getSupplies()->mSelectedSlot = mToolSlot;
            }
        }
    }

    // Revised onBaseTickEvent: now uses the GameMode's break progress
    void onBaseTickEvent(BaseTickEvent& event) {
        auto player = event.mActor;
        if (!player || !mIsMining) return;

        if (!BlockUtils::isGoodBlock(mTargetPos)) {
            resetMining();
            return;
        }

        auto block = ClientInstance::get()->getBlockSource()->getBlock(mTargetPos);
        if (!block) {
            resetMining();
            return;
        }

        auto gm = player->getGameMode();
        if (!gm) return;

        // If block is still being mined, add progress using the appropriate destroy speed.
        if (gm->mBreakProgress < 1.0f) {
            float rate = 0.0f;
            if (mSwitchMode.mValue == SwitchMode::Silent) {
                // Use our computed destroy speed from our tool when in silent mode.
                rate = ItemUtils::getDestroySpeed(mToolSlot, block);
            }
            else {
                // Otherwise use the GameMode's native destroy rate.
                rate = gm->getDestroyRate(*block);
            }
            gm->mBreakProgress += rate;
            if (gm->mBreakProgress > 1.0f)
                gm->mBreakProgress = 1.0f;
        }

        // When break progress reaches 1.0, break the block.
        if (gm->mBreakProgress >= 1.0f) {
            if (mSwitchMode.mValue == SwitchMode::Silent) {
                PacketUtils::spoofSlot(mToolSlot, false);
            }

            player->swing();
            // Set our flag to force the block break to go through our hook.
            mForceBreak = true;
            gm->destroyBlock(mTargetPos, mTargetFace);
            mForceBreak = false;

            if (mSwitchMode.mValue == SwitchMode::Silent && mSwitchBack.mValue) {
                mShouldSpoofSlot = true;
            }

            if (mContinue.mValue) {
                gm->mBreakProgress = mContinueSpeed.mValue;
            }
            else {
                resetMining();
            }
        }
    }

    void onPacketOutEvent(PacketOutEvent& event) {
        if (!mIsMining) return;

        if (event.mPacket->getId() == PacketID::PlayerAuthInput) {
            auto player = ClientInstance::get()->getLocalPlayer();
            if (!player) return;

            auto packet = event.getPacket<PlayerAuthInputPacket>();
            glm::vec2 rots = MathUtils::getRots(*player->getPos(), AABB(mTargetPos, glm::vec3(1)));
            packet->mRot = rots;
            packet->mYHeadRot = rots.y;
        }
        else if (event.mPacket->getId() == PacketID::MobEquipment) {
            auto mpkt = event.getPacket<MobEquipmentPacket>();
            if (mpkt->mSlot == mToolSlot)
                mShouldSpoofSlot = false;
            else
                mShouldSpoofSlot = true;
        }
    }

    void onRenderEvent(RenderEvent& event) {
        if (!mVisuals.mValue || !mIsMining) return;

        AABB blockAABB = AABB(mTargetPos, glm::vec3(1));
        float progress = ClientInstance::get()->getLocalPlayer()->getGameMode()->mBreakProgress;
        glm::vec3 scale(progress);
        glm::vec3 center = glm::vec3(mTargetPos) + glm::vec3(0.5f) - (scale * 0.5f);
        AABB progressAABB(center, scale);

        ImColor color = ColorUtils::getThemedColor(0);
        color.Value.w = mBoxOpacity.mValue;

        RenderUtils::drawOutlinedAABB(progressAABB, true, color);
    }

private:
    glm::ivec3 mTargetPos = glm::ivec3(INT_MAX);
    int mTargetFace = -1;
    bool mIsMining = false;
    bool mShouldSpoofSlot = true;
    int mToolSlot = -1;
    bool mForceBreak = false; // flag to force block break

public:
    // Expose our flag so hooks can check it.
    bool shouldForceBreak() const { return mForceBreak; }
};
