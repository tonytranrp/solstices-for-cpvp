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
        mBreakProgress = 0.0f;
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

        mTargetPos = pos;
        mTargetFace = face;
        mBreakProgress = 0.0f;
        mIsMining = true;

        // Get best tool
        mToolSlot = ItemUtils::getBestBreakingTool(block,true);
        if (mToolSlot != -1 && mSwitchMode.mValue != SwitchMode::None) {
            if (mSwitchMode.mValue == SwitchMode::Silent) {
                PacketUtils::spoofSlot(mToolSlot, false);
                mShouldSpoofSlot = false;
            }
            else {
                player->getSupplies()->mSelectedSlot = mToolSlot;
            }
        }

        //BlockUtils::startDestroyBlock(pos, face);
    }

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

        // Update breaking progress
        float destroySpeed = ItemUtils::getDestroySpeed(mToolSlot, block);
        mBreakProgress += destroySpeed;
        if (mBreakProgress > 1.0f) mBreakProgress = 1.0f;

        // Handle block breaking
        if (mBreakProgress >= 1.0f) {
            if (mSwitchMode.mValue == SwitchMode::Silent) {
                PacketUtils::spoofSlot(mToolSlot, false);
            }

            player->swing();
            BlockUtils::destroyBlock(mTargetPos, mTargetFace);

            if (mSwitchMode.mValue == SwitchMode::Silent && mSwitchBack.mValue) {
                mShouldSpoofSlot = true;
            }

            if (mContinue.mValue) {
                mBreakProgress = mContinueSpeed.mValue;
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
            if (mpkt->mSlot == mToolSlot) mShouldSpoofSlot = false;
            else mShouldSpoofSlot = true;
        }
    }

    void onRenderEvent(RenderEvent& event) {
        if (!mVisuals.mValue || !mIsMining) return;

        AABB blockAABB = AABB(mTargetPos, glm::vec3(1));

        glm::vec3 scale(mBreakProgress);
        glm::vec3 center = mTargetPos.operator+=(glm::vec3(0.5f - scale.x / 2.0f, 0.5f - scale.y / 2.0f, 0.5f - scale.z / 2.0f));
        AABB progressAABB(center, scale);

        ImColor color = ColorUtils::getThemedColor(0);
        color.Value.w = mBoxOpacity.mValue;

        RenderUtils::drawOutlinedAABB(progressAABB, true, color);
    }

private:
    glm::ivec3 mTargetPos = glm::ivec3(INT_MAX);
    int mTargetFace = -1;
    float mBreakProgress = 0.0f;
    bool mIsMining = false;
    bool mShouldSpoofSlot = true;
    int mToolSlot = -1;
};
