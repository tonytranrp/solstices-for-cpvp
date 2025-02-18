#include "AutoAnchor.hpp"
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Actor/GameMode.hpp>
#include <Features/FeatureManager.hpp>

#include <string>
#include <vector>
#include <algorithm>


AutoAnchor::AutoAnchor()
    : ModuleBase<AutoAnchor>("AutoAnchor", "Places and explodes respawn anchors", ModuleCategory::Combat, 0, false),
    mLastPlace(0), mLastGlow(0),
    mAnchorSlot(-1), mGlowSlot(-1), mPrevSlot(-1), mHasSwitched(false)
{
    addSettings(&mPlaceDelay, &mGlowDelay, &mPlaceRange);
    mNames = {
        {Lowercase,       "autoanchor"},
        {LowercaseSpaced, "auto anchor"},
        {Normal,          "AutoAnchor"},
        {NormalSpaced,    "Auto Anchor"}
    };
}

void AutoAnchor::onEnable() {
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoAnchor::onBaseTickEvent>(this);
    mPossiblePlacements.clear();
    mLastPlace = 0;
    mLastGlow = 0;
    mAnchorSlot = -1;
    mGlowSlot = -1;
    mPrevSlot = -1;
    mHasSwitched = false;
}

void AutoAnchor::onDisable() {
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoAnchor::onBaseTickEvent>(this);
    mPossiblePlacements.clear();
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (player && mHasSwitched && mPrevSlot != -1) {
        player->getSupplies()->mSelectedSlot = mPrevSlot;
    }
    mHasSwitched = false;
    mPrevSlot = -1;
    mAnchorSlot = -1;
    mGlowSlot = -1;
}

// Returns true if the block at pos is air.
bool AutoAnchor::canPlaceAnchor(const BlockPos& pos) {
    auto* bs = ClientInstance::get()->getBlockSource();
    if (!bs)
        return false;
    auto* block = bs->getBlock(pos);
    if (!block)
        return false;
    int id = block->mLegacy->getBlockId();
    return (id == 0);
}

// --- Inventory Switching Functions ---
// Search the hotbar (slots 0–8) for an item whose mName contains "respawn_anchor".
bool AutoAnchor::switchToAnchor() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return false;
    auto* inv = player->getSupplies()->getContainer();
    mAnchorSlot = -1;
    for (int i = 0; i < 9; i++) {
        auto* stack = inv->getItem(i);
        if (stack && stack->mItem) {
            if (stack->getItem()->mName.find("respawn_anchor") != std::string::npos) {
                mAnchorSlot = i;
                break;
            }
        }
    }
    if (mAnchorSlot == -1)
        return false;
    if (!mHasSwitched) {
        mPrevSlot = player->getSupplies()->mSelectedSlot;
        mHasSwitched = true;
    }
    player->getSupplies()->mSelectedSlot = mAnchorSlot;
    return true;
}

// Search the hotbar for an item whose mName contains "glowstone" (or mItemId equals 89).
bool AutoAnchor::switchToGlowstone() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return false;
    auto* inv = player->getSupplies()->getContainer();
    mGlowSlot = -1;
    for (int i = 0; i < 9; i++) {
        auto* stack = inv->getItem(i);
        if (stack && stack->mItem) {
            if (stack->getItem()->mName.find("glowstone") != std::string::npos) {
                mGlowSlot = i;
                break;
            }
        }
    }
    if (mGlowSlot == -1)
        return false;
    if (!mHasSwitched) {
        mPrevSlot = player->getSupplies()->mSelectedSlot;
        mHasSwitched = true;
    }
    player->getSupplies()->mSelectedSlot = mGlowSlot;
    return true;
}

// Restore the previously selected hotbar slot.
void AutoAnchor::switchBack() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !mHasSwitched)
        return;
    player->getSupplies()->mSelectedSlot = mPrevSlot;
    mHasSwitched = false;
    mPrevSlot = -1;
    mAnchorSlot = -1;
    mGlowSlot = -1;
}

// --- Placement Functions ---
// Place the respawn anchor by switching to the anchor slot and placing it via BlockUtils::placeBlock.
void AutoAnchor::placeAnchor(const BlockPos& pos) {
    int side = BlockUtils::getBlockPlaceFace(pos);
    if (side == -1)
        return;
    if (!switchToAnchor())
        return;
    BlockUtils::placeBlock(glm::vec3(pos), side);
    mLastPlace = NOW;
    ChatUtils::displayClientMessage("Placed anchor at (" +
        std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")");
    // We remain in the anchor slot.
}

// Place one glowstone block at the same candidate position.
// Here we do not adjust the Y coordinate—if the player is holding glowstone,
// then BlockUtils::placeBlock will place it at the candidate position.
void AutoAnchor::placeGlowstone(const BlockPos& pos) {
    if (NOW - mLastGlow < mGlowDelay.mValue)
        return;
    if (!switchToGlowstone())
        return;
    int side = BlockUtils::getBlockPlaceFace(pos);
    if (side == -1)
        return;
    // Place glowstone exactly at the candidate position.
    BlockUtils::placeBlock(glm::vec3(pos), -1);
    mLastGlow = NOW;
    ChatUtils::displayClientMessage("Placed glowstone at (" +
        std::to_string(pos.x) + ", " + std::to_string(pos.y) + ", " + std::to_string(pos.z) + ")");
    // After placing glowstone, remain in glowstone slot so that the next ignite action can work.
}

// --- onBaseTickEvent ---
// Check the block above the player's head. If it is air, switch to the anchor slot and place an anchor.
// If it already is a respawn anchor (detected by its legacy name), switch to glowstone and place a glowstone block.
void AutoAnchor::onBaseTickEvent(BaseTickEvent& event) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return;
    BlockPos headPos = *player->getPos();
    BlockPos candidate(headPos.x, headPos.y + 1, headPos.z);
    auto* bs = ClientInstance::get()->getBlockSource();
    if (!bs)
        return;
    auto* block = bs->getBlock(candidate);
    if (!block)
        return;
    int id = block->mLegacy->getBlockId();
    // If air, then no anchor is present: switch to anchor slot and place one.
    if (id == 0) {
        if (switchToAnchor()) {
            placeAnchor(candidate);
        }
    }
    else {
        // Otherwise, if the block's legacy name indicates it is a respawn anchor,
        // then switch to glowstone and place one glowstone block.
        if (block->mLegacy->mName.find("respawn_anchor") != std::string::npos) {
            if (switchToGlowstone()) {
                placeGlowstone(candidate);
            }
        }
    }
}
