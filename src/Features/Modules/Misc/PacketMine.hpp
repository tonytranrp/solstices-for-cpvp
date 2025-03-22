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
#include <SDK/Minecraft/Network/MinecraftPackets.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerActionPacket.hpp>
#include <Utils/GameUtils/MiningState.hpp>
#include <Utils/GameUtils/QueueManager.hpp>

class PacketMine : public ModuleBase<PacketMine> {
public:
    enum class SwitchMode {
        None,
        Silent,
        Normal
    };

    // Use the MiningState enum from MiningState.hpp
    using MiningState = MiningStates::PacketMineState;
    
    // Get current mining state from QueueManager
    MiningState getMiningState() {
        return QueueManager::convertToPacketMineState(
            QueueManager::get()->getState<QueueState::Mining>(this, QueueState::Mining::Idle));
    }
    
    // Set mining state in QueueManager
    void setMiningState(MiningState state) {
        QueueManager::get()->setState<QueueState::Mining>(this, 
            QueueManager::convertPacketMineState(state));
    }

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
        gFeatureManager->mDispatcher->listen<BaseTickEvent, &PacketMine::onBaseTickEvent, nes::event_priority::ABSOLUTE_FIRST>(this);
        gFeatureManager->mDispatcher->listen<PacketOutEvent, &PacketMine::onPacketOutEvent, nes::event_priority::ABSOLUTE_FIRST>(this);
        gFeatureManager->mDispatcher->listen<RenderEvent, &PacketMine::onRenderEvent, nes::event_priority::ABSOLUTE_FIRST>(this);
    }

    void onDisable() override {
        gFeatureManager->mDispatcher->deafen<BaseTickEvent, &PacketMine::onBaseTickEvent>(this);
        gFeatureManager->mDispatcher->deafen<PacketOutEvent, &PacketMine::onPacketOutEvent>(this);
        gFeatureManager->mDispatcher->deafen<RenderEvent, &PacketMine::onRenderEvent>(this);
        resetMining();
    }

    // Utility: Reset mining and clear state.
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
        
        // Update state using QueueManager
        QueueManager::get()->setState<QueueState::Mining>(this, QueueState::Mining::Idle);
        mWaitUntil = 0.0;
    }

    // Set the target block to mine.
    void setTargetBlock(const glm::ivec3& pos, int face) {
        auto player = ClientInstance::get()->getLocalPlayer();
        if (!player) return;

        if (!BlockUtils::isGoodBlock(pos)) {
            resetMining();
            ChatUtils::displayClientMessage("PacketMine: Block at " + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + " is not good.");
            return;
        }

        auto block = ClientInstance::get()->getBlockSource()->getBlock(pos);
        if (!block) {
            resetMining();
            ChatUtils::displayClientMessage("PacketMine: No block found at target pos.");
            return;
        }

        // Reset break progress on the GameMode
        if (auto gm = player->getGameMode()) {
            gm->mBreakProgress = 0.0f;
        }

        mTargetPos = pos;
        mTargetFace = face;
        mIsMining = true;
        
        // Update state using QueueManager
        QueueManager::get()->setState<QueueState::Mining>(this, QueueState::Mining::Waiting);
        
        ChatUtils::displayClientMessage("PacketMine: Target set at (" + std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")");

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

    // Utility to set a delay (in seconds) before mining can proceed.
    void setMiningWait(double seconds) {
        mWaitUntil = NOW + seconds;
        ChatUtils::displayClientMessage("PacketMine: Waiting for " + std::to_string(seconds) + " seconds.");
    }

    // Utility: Check if PacketMine is actively mining (not idle).
    bool isMiningActive() const {
        return QueueManager::get()->getState<QueueState::Mining>(this, QueueState::Mining::Idle) != QueueState::Mining::Idle;
    }
    void breakBlockTransac(const glm::ivec3& blockPos, int side) {
        auto* player = ClientInstance::get()->getLocalPlayer();
        if (!player) return;
        // Send START_DESTROY_BLOCK packet.
        auto startPkt = MinecraftPackets::createPacket<PlayerActionPacket>();
        startPkt->mPos = blockPos;
        startPkt->mResultPos = blockPos;
        startPkt->mFace = side;
        startPkt->mAction = PlayerActionType::StartDestroyBlock;
        startPkt->mRuntimeId = player->getRuntimeID();
        startPkt->mtIsFromServerPlayerMovementSystem = false;
        PacketUtils::queueSend(startPkt,true );

        // Send STOP_DESTROY_BLOCK packet.
        auto stopPkt = MinecraftPackets::createPacket<PlayerActionPacket>();
        stopPkt->mPos = blockPos;
        stopPkt->mResultPos = blockPos;
        stopPkt->mFace = side;
        stopPkt->mAction = PlayerActionType::StopDestroyBlock;
        stopPkt->mRuntimeId = player->getRuntimeID();
        stopPkt->mtIsFromServerPlayerMovementSystem = false;
        PacketUtils::queueSend(stopPkt, true);

        // Finally, clear the block visually.
        //BlockUtils::clearBlock(blockPos);
        spdlog::info("Destroyed block at ({}, {}, {}) [using transac]", blockPos.x, blockPos.y, blockPos.z);
    }

    // Revised onBaseTickEvent: updates break progress and state.
    void onBaseTickEvent(BaseTickEvent& event) {
        auto* player = event.mActor;
        if (!player || !mIsMining) {
            // Update state using our helper method
            setMiningState(MiningState::Idle);
            return;
        }

        // Get the item use duration (e.g., for eating food).
        int eatprog = player->getItemUseDuration();
        // If the player is eating (duration > 0), pause break progress.
        if (eatprog > 0) {
            // Update state using our helper method
            setMiningState(MiningState::Waiting);
            ChatUtils::displayClientMessage("PacketMine: Paused due to eating (" + std::to_string(eatprog) + ")");
            return;
        }

        // If waiting for delay, do not proceed.
        if (NOW < mWaitUntil) {
            // Update state using our helper method
            setMiningState(MiningState::Waiting);
            return;
        }

        // Check if the block is minable (skip air/bedrock).
        if (!BlockUtils::isGoodBlock(mTargetPos)) {
            ChatUtils::displayClientMessage("PacketMine: Target block at (" +
                std::to_string(mTargetPos.x) + ", " + std::to_string(mTargetPos.y) + ", " +
                std::to_string(mTargetPos.z) + ") is not minable.");
            resetMining();
            return;
        }

        auto* block = ClientInstance::get()->getBlockSource()->getBlock(mTargetPos);
        if (!block) {
            ChatUtils::displayClientMessage("PacketMine: Block not found at target pos.");
            resetMining();
            return;
        }

        auto* gm = player->getGameMode();
        if (!gm) {
            // Update state using our helper method
            setMiningState(MiningState::Idle);
            return;
        }

        // While progress is below threshold, remain in Waiting state.
        if (gm->mBreakProgress < 1.0f) {
            // Update state using our helper method
            setMiningState(MiningState::Waiting);
            float rate = 0.0f;
            if (mSwitchMode.mValue == SwitchMode::Silent) {
                rate = ItemUtils::getDestroySpeed(mToolSlot, block);
            }
            else {
                rate = gm->getDestroyRate(*block);
            }
            gm->mBreakProgress += rate;
            if (gm->mBreakProgress > 1.0f)
                gm->mBreakProgress = 1.0f;
        }

        // When break progress is near completion, update state to Breaking.
        if (gm->mBreakProgress >= 0.96f) {
            // Update state using our helper method
            setMiningState(MiningState::Breaking);
        }
        if (gm->mBreakProgress >= 1.0f) {
            ChatUtils::displayClientMessage("PacketMine: Breaking block at (" +
                std::to_string(mTargetPos.x) + ", " + std::to_string(mTargetPos.y) + ", " +
                std::to_string(mTargetPos.z) + ")");

            if (mSwitchMode.mValue == SwitchMode::Silent) {
                PacketUtils::spoofSlot(mToolSlot, false);
            }
            player->swing();
            mForceBreak = true;
            // Instead of calling destroyBlock, use breakBlockTransac:

            breakBlockTransac(mTargetPos, mTargetFace);
            gm->destroyBlock(mTargetPos, mTargetFace);
            mForceBreak = false;

            if (mSwitchMode.mValue == SwitchMode::Silent && mSwitchBack.mValue) {
                mShouldSpoofSlot = true;
            }

            if (mContinue.mValue) {
                gm->mBreakProgress = mContinueSpeed.mValue;
                // Update state using QueueManager
                QueueManager::get()->setState<QueueState::Mining>(this, QueueState::Mining::Waiting);
            }
            else {
                resetMining();
            }
        }


        // Debug output throttled to once per second.
        static double lastDebugTime = 0.0;
        if (NOW - lastDebugTime > 1.0) {
            // Get state from QueueManager for debugging
            auto queueState = QueueManager::get()->getState<QueueState::Mining>(this);
            auto currentState = getMiningState();
            ChatUtils::displayClientMessage("PacketMine Debug: State=" + miningStateToString(currentState) + 
                                          ", QueueState=" + std::to_string(static_cast<int>(queueState)));
            lastDebugTime = NOW;
        }
    }


    // onPacketOutEvent: send rotation only if we're in the Breaking state.
    void onPacketOutEvent(PacketOutEvent& event) {
        if (!mIsMining) return;

        if (event.mPacket->getId() == PacketID::PlayerAuthInput) {
            // Get current state from QueueManager
            auto currentState = QueueManager::get()->getState<QueueState::Mining>(this);
            
            // Only send rotation packets if we're in the Breaking state.
            if (currentState == QueueState::Mining::Breaking) {
                auto player = ClientInstance::get()->getLocalPlayer();
                if (!player) return;

                auto packet = event.getPacket<PlayerAuthInputPacket>();
                glm::vec2 rots = MathUtils::getRots(*player->getPos(), AABB(mTargetPos, glm::vec3(1)));
                packet->mRot = rots;
                packet->mYHeadRot = rots.y;
                ChatUtils::displayClientMessage("PacketMine: Rotation packet sent. (Breaking state)");
            }
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

    // Expose our flag so hooks can check it.
    bool shouldForceBreak() const { return mForceBreak; }

private:
    // Target block position & face.
    glm::ivec3 mTargetPos = glm::ivec3(INT_MAX);
    int mTargetFace = -1;
    bool mIsMining = false;
    bool mShouldSpoofSlot = true;
    int mToolSlot = -1;
    bool mForceBreak = false; // flag to force block break

    // New state variable and wait timer.
   
    double mWaitUntil = 0.0;

    // Helper: Convert mining state to string.
    std::string miningStateToString(MiningState state) {
        switch (state) {
        case MiningState::Idle: return "Idle";
        case MiningState::Waiting: return "Waiting";
        case MiningState::Rotating: return "Rotating";
        case MiningState::Breaking: return "Breaking";
        default: return "Unknown";
        }
    }
public:
};
