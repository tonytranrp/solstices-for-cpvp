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
        &mRotate,
        &mStabilityThreshold
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

    // Get the player's current position
    glm::vec3 playerPos = *player->getPos();
    
    // Check if player has moved significantly since last calculation
    if (!hasPlayerMovedSignificantly(playerPos)) {
        // Player hasn't moved enough to warrant recalculation
        return;
    }
    
    // Update last position for next check
    mLastPlayerPos = playerPos;
    
    mBlocksToPlace.clear();

    // Get the player's base position:
    // Floor the player's position and subtract 1 from Y to get the block under their feet.
    glm::vec3 playerPosF = playerPos;
    playerPosF = glm::floor(playerPosF);
    playerPosF.y -= 1.0f;
    glm::ivec3 playerBasePos = glm::ivec3(playerPosF);
    
    // Store the current base position for stability checks
    mLastBasePos = playerBasePos;

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

    // Check if we should use dynamic placement or standard placement
    if (mDynamic.mValue) {
        // Use dynamic placement (perimeter-only)
        generateDynamicPositions(playerBasePos, blocksSet);
    } else {
        // Standard placement logic
        // For each side offset, check intersections with the player's AABB.
        for (const auto& offset : sideOffsets) {
            glm::ivec3 posToCheck = playerBasePos + glm::ivec3(static_cast<int>(offset.x), static_cast<int>(offset.y), static_cast<int>(offset.z));
            
            // Always add the basic side blocks without intersection checks
            // This ensures a consistent 2x2 pattern regardless of minor movements
            addBlockToPlace(posToCheck);
            
            // Only check for extended placements if we're at an edge or corner
            // This prevents unnecessary interior blocks
            if (playerPos.x > posToCheck.x + 0.7f || playerPos.x < posToCheck.x + 0.3f ||
                playerPos.z > posToCheck.z + 0.7f || playerPos.z < posToCheck.z + 0.3f) {
                
                // Add diagonal blocks only when player is near corners
                for (int i : {-1, 1}) {
                    for (int j : {-1, 1}) {
                        // Calculate diagonal position
                        glm::ivec3 diagPos = playerBasePos + glm::ivec3(i, 0, j);
                        
                        // Only add if player is actually near this corner
                        float cornerDist = glm::distance(
                            glm::vec2(playerPos.x, playerPos.z),
                            glm::vec2(playerBasePos.x + i * 0.5f, playerBasePos.z + j * 0.5f)
                        );
                        
                        if (cornerDist < 0.7f) {
                            addBlockToPlace(diagPos);
                        }
                    }
                }
            }
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
    // If no valid face, try to �snap� to a nearby support block.
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

// Checks if the player has moved significantly from last position
bool Surround::hasPlayerMovedSignificantly(const glm::vec3& currentPos) {
    // If this is the first check, always return true
    if (mLastPlayerPos == glm::vec3(0.0f)) {
        return true;
    }
    
    // Calculate distance between current position and last position
    float distance = glm::distance(glm::vec2(currentPos.x, currentPos.z), glm::vec2(mLastPlayerPos.x, mLastPlayerPos.z));
    
    // Get the current base position (floor of player position)
    glm::ivec3 currentBasePos = glm::ivec3(glm::floor(currentPos.x), glm::floor(currentPos.y) - 1, glm::floor(currentPos.z));
    
    // If the base position has changed, we should recalculate
    bool baseChanged = currentBasePos != mLastBasePos;
    
    // Check if the player has moved more than the threshold or changed base position
    return distance > mStabilityThreshold.mValue || baseChanged;
}

// Generates dynamic surround positions based on range
void Surround::generateDynamicPositions(const glm::ivec3& basePos, std::unordered_set<glm::ivec3, Vec3iHash>& blocksSet) {
    int range = static_cast<int>(mRange.mValue);
    
    // Generate only the perimeter blocks at the specified range
    for (int x = -range; x <= range; x++) {
        for (int z = -range; z <= range; z++) {
            // Only add blocks that are on the perimeter (outer ring)
            if (abs(x) == range || abs(z) == range) {
                // Add block at the base level (feet level - 1)
                blocksSet.insert(basePos + glm::ivec3(x, 0, z));
                
                // If place above is enabled, also add blocks at head level
                if (mPlaceAbove.mValue) {
                    blocksSet.insert(basePos + glm::ivec3(x, 3, z)); // Head level is typically 2 blocks above base
                }
            }
        }
    }
}
