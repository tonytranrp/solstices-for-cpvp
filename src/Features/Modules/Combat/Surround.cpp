#include "Surround.hpp"
#include <imgui.h>
#include <cmath>
#include <vector>
#include <unordered_set>
#include <glm/glm.hpp>

#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Network/Packets/InventoryTransactionPacket.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>
#include <Utils/GameUtils/ItemUtils.hpp>
#include <Utils/GameUtils/ActorUtils.hpp>

// A hash functor for glm::ivec3 so we can store it in an unordered_set.
struct Vec3iHash {
    std::size_t operator()(const glm::ivec3& v) const {
        std::size_t h1 = std::hash<int>()(v.x);
        std::size_t h2 = std::hash<int>()(v.y);
        std::size_t h3 = std::hash<int>()(v.z);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

Surround::Surround()
    : ModuleBase("Surround", "Places blocks around you for protection", ModuleCategory::Player, 0, false)
{
    addSettings(
        &mCenter,
        &mDisableComplete,
        &mSwitchMode,
        &mRender,
        &mPlaceAbove,
        &mDynamic,
        &mRange,
        &mRotate
    );
    mNames = {
        {Lowercase, "surround"},
        {LowercaseSpaced, "surround"},
        {Normal, "Surround"},
        {NormalSpaced, "Surround"}
    };
    gFeatureManager->mDispatcher->listen<RenderEvent, &Surround::onRenderEvent>(this);
}

void Surround::onEnable() {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;
    // If centering is enabled, snap the player to the center of their current block.
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

void Surround::onBaseTickEvent(BaseTickEvent& event) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    mBlocksToPlace.clear();

    // Get the player's base position:
    // Floor the player's position and subtract 1 from Y to get the block under their feet.
    glm::vec3 playerPosF = *player->getPos();
    playerPosF = glm::floor(playerPosF);
    playerPosF.y -= 1.0f;
    glm::ivec3 playerBasePos = glm::ivec3(playerPosF);

    // Retrieve the player's AABB.
    AABB playerAABB = player->getAABB();

    // We'll use an unordered_set to avoid duplicate positions.
    std::unordered_set<glm::ivec3, Vec3iHash> blocksSet;

    // Define side offsets (similar to sideBlocks in the Java code).
    std::vector<glm::vec3> sideOffsets = {
        glm::vec3(1, 0, 0),
        glm::vec3(0, 0, 1),
        glm::vec3(-1, 0, 0),
        glm::vec3(0, 0, -1)
    };

    // Helper lambda to add a position to the set.
    auto addBlockToPlace = [&](const glm::ivec3& pos) {
        blocksSet.insert(pos);
        };

    // For each side offset, check intersections with the player's AABB.
    for (const auto& offset : sideOffsets) {
        glm::ivec3 posToCheck = playerBasePos + glm::ivec3(static_cast<int>(offset.x), static_cast<int>(offset.y), static_cast<int>(offset.z));
        // Construct an AABB for this block position.
        glm::vec3 lower = glm::vec3(posToCheck);
        glm::vec3 upper = lower + glm::vec3(1.0f);
        AABB blockAABB(lower, upper, true);

        if (playerAABB.intersects(blockAABB)) {
            // If player's AABB intersects the block, add an extra offset block.
            addBlockToPlace(posToCheck + glm::ivec3(static_cast<int>(offset.x), static_cast<int>(offset.y), static_cast<int>(offset.z)));
            // Additionally, try to add side and corner positions.
            for (int i : {-1, 1}) {
                for (int j : {-1, 1}) {
                    glm::ivec3 sidePos = posToCheck + glm::ivec3(static_cast<int>(offset.z) * i, static_cast<int>(offset.y), static_cast<int>(offset.x) * j);
                    addBlockToPlace(sidePos);

                    glm::ivec3 cornerPos = posToCheck + glm::ivec3(static_cast<int>(offset.z) * i, static_cast<int>(offset.y), static_cast<int>(offset.x) * j);
                    glm::vec3 cornerLower = glm::vec3(cornerPos);
                    glm::vec3 cornerUpper = cornerLower + glm::vec3(1.0f);
                    AABB cornerAABB(cornerLower, cornerUpper, true);
                    if (playerAABB.intersects(cornerAABB)) {
                        glm::ivec3 adjustedPos = cornerPos + glm::ivec3(static_cast<int>(offset.z) * i, 0, static_cast<int>(offset.x) * j);
                        addBlockToPlace(adjustedPos);
                    }
                }
            }
        }
        else {
            addBlockToPlace(posToCheck);
        }
    }

    // If PlaceAbove is enabled, add the block above the player's head.
    if (mPlaceAbove.mValue) {
        glm::ivec3 abovePos = playerBasePos + glm::ivec3(0, 2, 0);
        addBlockToPlace(abovePos);
    }

    // Convert the set into our mBlocksToPlace vector.
    mBlocksToPlace.assign(blocksSet.begin(), blocksSet.end());

    // Now, for each candidate position, validate and place the block.
    auto* bs = ClientInstance::get()->getBlockSource();
    if (!bs) return;
    for (const auto& pos : mBlocksToPlace) {
        auto* block = bs->getBlock(pos);
        if (!block) continue;
        // Assuming block ID 0 means air.
        if (block->mLegacy->getBlockId() != 0)
            continue;
        placeBlockAt(pos);
    }

    // Optionally disable the module if there are no blocks to place.
    if (mDisableComplete.mValue && mBlocksToPlace.empty()) {
        this->setEnabled(false);
    }
}

void Surround::placeBlockAt(const glm::ivec3& pos) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // Validate that the block can be placed here.
    if (!BlockUtils::isValidPlacePos(pos)) return;
    if (!BlockUtils::isAirBlock(pos)) return;
    int side = BlockUtils::getBlockPlaceFace(pos);
    // If no valid face, try to “snap” to a nearby support block.
    if (side == -1) {
        glm::vec3 supportPosF = BlockUtils::getClosestPlacePos(glm::vec3(pos),4);
        glm::ivec3 supportPos = glm::ivec3(supportPosF);
        side = BlockUtils::getBlockPlaceFace(supportPos);
        if (side == -1) return;
    }

    int prevSlot = player->getSupplies()->mSelectedSlot;
    // Handle hotbar switching.
    if (mSwitchMode.mValue != 0) {
        int slot = ItemUtils::getPlaceableItemOnBlock(glm::vec3(pos), false, false);
        if (slot == -1) return;
        if (mSwitchMode.mValue == 1) { // Full mode.
            player->getSupplies()->mSelectedSlot = slot;
        }
        else if (mSwitchMode.mValue == 2) { // Silent mode.
            PacketUtils::spoofSlot(slot);
        }
    }

    // Place the block (this utility function handles sending the placement packet, swing animation, etc.).
    BlockUtils::placeBlock(glm::vec3(pos), side);

    if (mSwitchMode.mValue == 1) {
        player->getSupplies()->mSelectedSlot = prevSlot;
    }
}
void Surround::onPacketOutEvent(PacketOutEvent& event) {
    auto* packet = event.mPacket;
    if (packet->getId() == PacketID::InventoryTransaction) {
        auto pkt = event.getPacket<InventoryTransactionPacket>();
        if (!pkt || !pkt->mTransaction) return;
        if (pkt->mTransaction->type == ComplexInventoryTransaction::Type::ItemUseTransaction) {
            auto* transac = reinterpret_cast<ItemUseInventoryTransaction*>(pkt->mTransaction.get());
            transac->mClickPos = BlockUtils::clickPosOffsets[transac->mFace];
            for (int i = 0; i < 3; i++) {
                if (transac->mClickPos[i] == 0.5f) {
                    transac->mClickPos[i] = MathUtils::randomFloat(-0.49f, 0.49f);
                }
            }
            transac->mClickPos.y -= 0.1f;
        }
    }
   
}

void Surround::onRenderEvent(RenderEvent& event) {
    if (!mRender.mValue || mBlocksToPlace.empty()) return;
    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;
    for (const auto& pos : mBlocksToPlace) {
        glm::vec3 lower(pos.x, pos.y, pos.z);
        glm::vec3 upper = lower + glm::vec3(1.0f);
        AABB box(lower, upper, true);
        // Render an outlined box; adjust color as needed.
        RenderUtils::drawOutlinedAABB(box, true, ImColor(255, 99, 202, 255));
    }
}
