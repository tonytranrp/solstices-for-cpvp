#include "Surround.hpp"

#include <imgui.h>
#include <cmath>
#include <vector>
#include <glm/glm.hpp>

#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Network/Packets/InventoryTransactionPacket.hpp>

#include <Utils/GameUtils/ItemUtils.hpp>


// Constructor: add settings and register the module name(s)
Surround::Surround() : ModuleBase("Surround", "Places blocks around you for protection", ModuleCategory::Player, 0, false) {
    addSettings(
        &mCenter,
        &mDisableComplete,
        &mSwitchMode,
        &mRender,
        &mPlaceAbove
    );
    mNames = {
        {Lowercase, "surround"},
        {LowercaseSpaced, "surround"},
        {Normal, "Surround"},
        {NormalSpaced, "Surround"}
    };
    // Listen for render events (if not already registered in onEnable)
    gFeatureManager->mDispatcher->listen<RenderEvent, &Surround::onRenderEvent>(this);
}

void Surround::onEnable() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // Center the player on the block grid if enabled
    if (mCenter.mValue) {
        auto pos = *player->getPos();
        pos.x = floor(pos.x) + 0.5f;
        pos.z = floor(pos.z) + 0.5f;
        player->setPos(pos.x, pos.y, pos.z);
    }
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &Surround::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &Surround::onPacketOutEvent>(this);
}

void Surround::onDisable() {
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &Surround::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &Surround::onPacketOutEvent>(this);
    mBlocksToPlace.clear();
}

//
// onBaseTickEvent: Calculate the surround positions and attempt block placement.
//
void Surround::onBaseTickEvent(BaseTickEvent& event) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    mBlocksToPlace.clear();

    // Get the player's current block position.
    // We use the player's feet position (subtract 1 from Y) so that the surround is placed around the base.
    glm::ivec3 basePos = glm::floor(*player->getPos());
    basePos.y -= 1;

    // Define the offsets (relative block positions) for the surround.
    // Four horizontal sides plus one below.
    static const std::vector<glm::ivec3> offsets = {
        { 1, 0, 0},
        { 0, 0, 1},
        {-1, 0, 0},
        { 0, 0, -1},
        { 0, -1, 0}
    };

    // Check each surrounding position
    for (const auto& offset : offsets) {
        glm::ivec3 pos = basePos + offset;
        auto* bs = ClientInstance::get()->getBlockSource();
        if (!bs) continue;
        auto* block = bs->getBlock(pos);
        if (!block) continue;
        int blockId = block->mLegacy->getBlockId();
        // Assume block ID 0 means air (replace with your project’s air ID if different)
        if (blockId == 0) {
            mBlocksToPlace.push_back(pos);
        }
    }

    // Optionally add a block above the player's head
    if (mPlaceAbove.mValue) {
        glm::ivec3 abovePos = basePos + glm::ivec3(0, 2, 0);
        auto* bs = ClientInstance::get()->getBlockSource();
        if (bs) {
            auto* block = bs->getBlock(abovePos);
            if (block && block->mLegacy->getBlockId() == 0) {
                mBlocksToPlace.push_back(abovePos);
            }
        }
    }

    // Attempt to place blocks at each position.
    for (const auto& pos : mBlocksToPlace) {
        placeBlockAt(pos);
    }

    // If the module should disable when complete and there are no blocks to place, turn off the module.
    if (mDisableComplete.mValue && mBlocksToPlace.empty()) {
        this->setEnabled(false);
    }
}

//
// placeBlockAt: Uses BlockUtils::placeBlock to place a block with proper placement checks and packet handling.
// It also supports hotbar switching modes (None, Full, Silent).
//
void Surround::placeBlockAt(const glm::ivec3& pos) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // Check if the position is valid and air.
    if (!BlockUtils::isValidPlacePos(pos)) return;
    if (!BlockUtils::isAirBlock(pos)) return;
    int side = BlockUtils::getBlockPlaceFace(pos);
    if (side == -1) return;

    int prevSlot = player->getSupplies()->mSelectedSlot;
    // Handle switching if enabled (0 = None, 1 = Full, 2 = Silent)
    if (mSwitchMode.mValue != 0) {
        // Try to find a placeable block at this position (e.g. obsidian)
        int slot = ItemUtils::getPlaceableItemOnBlock(pos, false, false);
        if (slot == -1) return;
        if (mSwitchMode.mValue == 1) { // Full mode
            player->getSupplies()->mSelectedSlot = slot;
        }
        else if (mSwitchMode.mValue == 2) { // Silent mode
            PacketUtils::spoofSlot(slot);
        }
    }

    // Use BlockUtils to place the block. This function performs proper packet placement
    // and swing/rotation adjustments according to our client style.
    BlockUtils::placeBlock(glm::vec3(pos), side);

    // If in Full switch mode, switch back to the previous slot.
    if (mSwitchMode.mValue == 1) {
        player->getSupplies()->mSelectedSlot = prevSlot;
    }
}

//
// onPacketOutEvent: Adjust outgoing block placement packets to randomize click offsets,
// following our Scaffold’s packet modification style.
//
void Surround::onPacketOutEvent(PacketOutEvent& event) {
    auto* packet = event.mPacket;
    if (packet->getId() == PacketID::InventoryTransaction) {
        auto pkt = event.getPacket<InventoryTransactionPacket>();
        if (!pkt || !pkt->mTransaction) return;
        if (pkt->mTransaction->type == ComplexInventoryTransaction::Type::ItemUseTransaction) {
            auto* transac = reinterpret_cast<ItemUseInventoryTransaction*>(pkt->mTransaction.get());
            // Set click position based on face offset
            transac->mClickPos = BlockUtils::clickPosOffsets[transac->mFace];
            // Randomize any 0.5 values to help bypass anti-cheat checks
            for (int i = 0; i < 3; i++) {
                if (transac->mClickPos[i] == 0.5f) {
                    transac->mClickPos[i] = MathUtils::randomFloat(-0.49f, 0.49f);
                }
            }
        }
    }
    // (Optionally, add rotation adjustments for PlayerAuthInput packets here if needed.)
}

//
// onRenderEvent: Renders an outline for each block position that we’re trying to place.
//
void Surround::onRenderEvent(RenderEvent& event) {
    if (!mRender.mValue || mBlocksToPlace.empty()) return;
    auto drawList = ImGui::GetBackgroundDrawList();
    for (const auto& pos : mBlocksToPlace) {
        glm::vec3 lower(pos.x, pos.y, pos.z);
        glm::vec3 upper = lower + glm::vec3(1.0f, 1.0f, 1.0f);
        AABB box(lower, upper, true);
        // Draw an outlined box using a chosen color (adjust as necessary)
        RenderUtils::drawOutlinedAABB(box, true, ImColor(255, 99, 202, 255));
    }
}
