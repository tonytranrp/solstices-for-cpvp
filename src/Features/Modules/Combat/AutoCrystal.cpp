#include "AutoCrystal.hpp"
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <Utils/GameUtils/ActorUtils.hpp>
#include <Features/FeatureManager.hpp>
#include <Features/Events/BobHurtEvent.hpp>
#include <Features/Events/BoneRenderEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Modules/Misc/Friends.hpp>
#include <SDK/Minecraft/Options.hpp>
#include <SDK/Minecraft/Actor/GameMode.hpp>
#include <SDK/Minecraft/World/Level.hpp>
#include <SDK/Minecraft/World/HitResult.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Network/LoopbackPacketSender.hpp>
#include <SDK/Minecraft/Network/Packets/MovePlayerPacket.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>
#include <SDK/Minecraft/Network/Packets/RemoveActorPacket.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>
#include <SDK/Minecraft/Network/Packets/AddActorPacket.hpp>

void AutoCrystal::onEnable() {
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoCrystal::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<RenderEvent, &AutoCrystal::onRenderEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &AutoCrystal::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &AutoCrystal::onPacketInEvent>(this);
    rots = {};
}

void AutoCrystal::onDisable() {
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoCrystal::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<RenderEvent, &AutoCrystal::onRenderEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &AutoCrystal::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &AutoCrystal::onPacketInEvent>(this);
    mPossiblePlacements.clear();
    rots = {};
    switchBack();
    mLastTarget = nullptr;
    mRotating = false;
}

float AutoCrystal::calculateDamage(const BlockPos& crystalPos, Actor* target) {
    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource || !target) return 0.0f;

    glm::vec3 explosionPos = { crystalPos.x + 0.5f, crystalPos.y + 1.0f, crystalPos.z + 0.5f };
    glm::vec3 targetPos = *target->getPos();
    float dist = glm::distance(targetPos, explosionPos);
    if (dist > 12.0f) return 0.0f;

    float visibility = blockSource->getSeenPercent(explosionPos, target->getAABB());
    if (visibility < 0.1f) return 0.0f;

    // Java-style explosion damage calculation
    float impact = (1.0f - (dist / 12.0f)) * visibility;
    float baseDamage = ((impact * impact) * 7.0f + impact * 0.5f) * 42.0f + 1.0f;

    // Apply armor reduction
    if (auto* equipment = target->getArmorContainer()) {
        float armorValue = 0.f;
        for (int i = 0; i < 4; i++) {
            auto* item = equipment->getItem(i);
            if (item && item->mItem) {
                float protection = item->getEnchantValue(Enchant::PROTECTION) * 0.04f;
                float blastProtection = item->getEnchantValue(Enchant::BLAST_PROTECTION) * 0.08f;
                armorValue += 2.0f + protection + blastProtection;
            }
        }
        baseDamage *= (1.0f - (std::min(20.0f, armorValue) / 25.0f));
    }

    return baseDamage;
}


bool AutoCrystal::canPlaceCrystal(const BlockPos& pos) {
    auto* bs = ClientInstance::get()->getBlockSource();
    if (!bs) return false;

    auto* block = bs->getBlock(pos);
    if (!block) return false;

    int blockId = block->mLegacy->getBlockId();
    if (blockId != 49 && blockId != 7) return false; // Only obsidian/bedrock

    BlockPos above = pos;
    above.y += 1;
    if (bs->getBlock(above)->mLegacy->getBlockId() != 0) return false;

    above.y += 1;
    if (bs->getBlock(above)->mLegacy->getBlockId() != 0) return false;
    // Define the AABB for the crystal placement area
    AABB placeAABB(
        glm::vec3(pos.x, pos.y + 1.0f, pos.z),   // Lower bounds (above block)
        glm::vec3(pos.x + 1.0f, pos.y + 2.0f, pos.z + 1.0f),  // Upper bounds (2 blocks high)
        true
    );
    // Check for entity intersections within the placement area
    for (auto* entity : ActorUtils::getActorList(false, false)) {
        if (!entity->isValid()) continue;
        if (entity->getActorTypeComponent()->mType == ActorType::EnderCrystal) continue; // Ignore existing crystals

        AABB entityAABB = entity->getAABB();
        if (entityAABB.mMin == entityAABB.mMax) continue; // Ignore invalid AABBs

        // Expand the AABB slightly for non-crystals
        if (entity->getActorTypeComponent()->mType != ActorType::EnderCrystal) {
            entityAABB.mMin -= glm::vec3(0.1f, 0.0f, 0.1f);
            entityAABB.mMax += glm::vec3(0.1f, 0.0f, 0.1f);
        }

        // Skip placement if any entity intersects with the crystal placement area
        if (placeAABB.mMin.x < entityAABB.mMax.x && placeAABB.mMax.x > entityAABB.mMin.x &&
            placeAABB.mMin.y < entityAABB.mMax.y && placeAABB.mMax.y > entityAABB.mMin.y &&
            placeAABB.mMin.z < entityAABB.mMax.z && placeAABB.mMax.z > entityAABB.mMin.z) {
            return false;
        }
    }

    // Ensure player is within placement range
    auto* player = ClientInstance::get()->getLocalPlayer();
    glm::vec3 crystalPos(pos.x + 0.5f, pos.y + 1.0f, pos.z + 0.5f);
    return glm::distance(*player->getPos(), crystalPos) <= mPlaceRange.mValue;
}


std::vector<AutoCrystal::PlacePosition> AutoCrystal::findPlacePositions() {
    std::vector<PlacePosition> positions;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return positions;

    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource)
        return positions;

    auto* level = player->getLevel();
    if (!level)
        return positions;

    // Get the player's current block position.
    BlockPos playerPos = *player->getPos();
    int range = static_cast<int>(mPlaceRange.mValue);

    // Build a list of valid target actors (only players) within mRange.
    std::vector<Actor*> targetList;
    Actor* targets{};
    for (auto* target : level->getRuntimeActorList()) {
        if (target == player || !target->isValid() || !target->isPlayer())
            continue;
        float distance = glm::distance(*player->getPos(), *target->getPos());
        if (distance < mRange.mValue) {
            targetList.push_back(target);
           
        }
    }
    
    if (targetList.empty())
        return positions; // No valid targets within range.
    for (auto* target : targetList) {
        targets = target;
    }
    BlockPos targetpos = *targets->getPos();
    // Iterate over potential placement positions around the player.
    for (int x = -range; x <= range; x++) {
        for (int y = -3; y <= 3; y++) {
            for (int z = -range; z <= range; z++) {
                BlockPos checkPos = targetpos + BlockPos(x, y, z);
                if (!canPlaceCrystal(checkPos))
                    continue;

                float maxTargetDamage = 0.0f;
                float damage = calculateDamage(checkPos, targets);
                maxTargetDamage = std::max(maxTargetDamage, damage);

                // Only consider positions that yield enough target damage.
                if (maxTargetDamage >= mMinimumDamage.mValue) {
                    // We supply 0.0f for selfDamage because we no longer calculate it.
                    positions.emplace_back(checkPos, maxTargetDamage);
                }
            }
        }
    }

    // Sort positions by descending target damage.
    std::sort(positions.begin(), positions.end(), [](const PlacePosition& a, const PlacePosition& b) {
        return a.targetDamage > b.targetDamage;
        });

    return positions;
}



void AutoCrystal::placeCrystal(const PlacePosition& pos) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->getGameMode()) return;

    if (NOW - mLastPlace < mPlaceDelay.mValue) return;

    player->getGameMode()->buildBlock(pos.position, 1, false);
    mLastPlace = NOW;
}
void AutoCrystal::switchToCrystal() {
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    auto inventory = player->getSupplies()->getContainer();
    mCrystalSlot = -1;

    for (int i = 0; i < 9; i++) {
        auto item = inventory->getItem(i);
        if (item && item->mItem && item->getItem()->mItemId == 789) {
            mCrystalSlot = i;
            break;
        }
    }

    if (mCrystalSlot == -1) return;

    if (mSwitchMode.mValue == SwitchMode::Silent) {
        PacketUtils::spoofSlot(mCrystalSlot, false);
        mShouldSpoofSlot = true;  // Mark that we spoofed the slot
    }
    else {
        if (!mHasSwitched) {
            mPrevSlot = player->getSupplies()->mSelectedSlot;
            player->getSupplies()->mSelectedSlot = mCrystalSlot;
            mHasSwitched = true;
        }
    }
}

void AutoCrystal::switchBack() {
    if (!mHasSwitched) return;

    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    if (mSwitchMode.mValue == SwitchMode::Silent) {
        PacketUtils::spoofSlot(mPrevSlot, false);
        mShouldSpoofSlot = false;
    }
    else {
        player->getSupplies()->mSelectedSlot = mPrevSlot;
        mHasSwitched = false;
    }
}
void AutoCrystal::breakCrystal(Actor* crystal) {
    if (!crystal) return;

    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->getGameMode()) return;

    if (!mIdPredict.mValue) {
        if (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot) {
            PacketUtils::spoofSlot(mCrystalSlot, false);
        }
        player->getGameMode()->attack(crystal);
        player->swing();
        return;
    }

    // Enable prediction mode
    mShouldIdPredict = true;
    mLastAttackId = crystal->getRuntimeID();

    for (int i = 0; i < mPredictAmount.mValue; i++) {
        uint64_t predictedID = mLastAttackId + i;

        int attackSlot = (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot) ? mCrystalSlot : player->getSupplies()->mSelectedSlot;

        std::shared_ptr<InventoryTransactionPacket> attackTransaction =
            ActorUtils::createAttackTransactionPredictId(crystal, attackSlot, predictedID);

        PacketUtils::queueSend(attackTransaction, false);
    }

    // Additional check for newly spawned crystals
    for (auto runtimeID : knownCrystals) {
        if (runtimeID > mLastAttackId) {
            ChatUtils::displayClientMessage("Addactor predict spawned : " + std::to_string(runtimeID));
            std::shared_ptr<InventoryTransactionPacket> attackTransaction =
                ActorUtils::createAttackTransactionPredictId(crystal, player->getSupplies()->mSelectedSlot, runtimeID);
            PacketUtils::queueSend(attackTransaction, false);
        }
    }

    // Clear the known crystals after attempting attacks
    knownCrystals.clear();
    mLastAttackId += mPredictAmount.mValue;
    mShouldIdPredict = false;
    player->swing();
}


void AutoCrystal::onBaseTickEvent(BaseTickEvent& event) {
    if (!mAutoPlace.mValue) return;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    switchToCrystal();

    mPossiblePlacements = findPlacePositions();
    if (!mPossiblePlacements.empty()) {
        placeCrystal(mPossiblePlacements[0]);
        rots = MathUtils::getRots(*player->getPos(), mPossiblePlacements[0].position);
    }

    for (auto* actor : ActorUtils::getActorList(false, false)) {
        if (!actor->isValid() || actor->getActorTypeComponent()->mType != ActorType::EnderCrystal)
            continue;
        float dist = glm::distance(*actor->getPos(), *player->getPos());
        if (dist > mRange.mValue) continue;
        breakCrystal(actor);
        break;
    }

    if (mSwitchBack.mValue) {
        switchBack();
    }
}

void AutoCrystal::onPacketInEvent(PacketInEvent& event) {
    if (event.mPacket->getId() == PacketID::AddActor) {
        auto packet = event.getPacket<AddActorPacket>();

        
        ChatUtils::displayClientMessage("Addactor packet spawned : " + std::to_string(packet.get()->mRuntimeId));

        // Store the new crystal's runtime ID for later prediction
        knownCrystals.insert(packet.get()->mRuntimeId);
    }
}
void AutoCrystal::onPacketOutEvent(PacketOutEvent& event) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // 🧩 **Validate Inventory Transaction for Crystal Placement**
    if (event.mPacket->getId() == PacketID::InventoryTransaction) {
        auto pkt = event.getPacket<InventoryTransactionPacket>();
        if (!pkt || !pkt->mTransaction) return;

        // 🔍 Check if it's an item use transaction (placing a crystal)
        if (pkt->mTransaction->type == ComplexInventoryTransaction::Type::ItemUseTransaction) {
            auto* transac = reinterpret_cast<ItemUseInventoryTransaction*>(pkt->mTransaction.get());

            // ✅ Validate that we're placing a crystal
            if (transac->mActionType == ItemUseInventoryTransaction::ActionType::Place) {
                transac->mClickPos = BlockUtils::clickPosOffsets[transac->mFace];

                // 🎯 **Randomize click position to bypass anti-cheat**
                for (int i = 0; i < 3; i++) {
                    if (transac->mClickPos[i] == 0.5f) {
                        transac->mClickPos[i] = MathUtils::randomFloat(-0.49f, 0.49f);
                    }
                }

                // 🛠️ **Modify transaction to ensure correct placement**
                transac->mClickPos.y = transac->mClickPos.y - 0.1f; // Slight offset for better placement
            }
        }
    }

    // 🎯 **Ensure correct rotation when placing**
    if (event.mPacket->getId() == PacketID::PlayerAuthInput) {
        auto pkt = event.getPacket<PlayerAuthInputPacket>();
        if (!pkt) return;

        pkt->mRot = rots;
        pkt->mYHeadRot = rots.y;
    }
}




void AutoCrystal::onRenderEvent(RenderEvent& event) {
    if (!mVisualizePlace.mValue || mPossiblePlacements.empty()) return;

    auto drawList = ImGui::GetBackgroundDrawList();

    // **Only render the top placement position**
    const auto& bestPlace = mPossiblePlacements[0];

    float damagePercent = std::min(bestPlace.targetDamage / 20.0f, 1.0f);
    ImColor color = ImColor(damagePercent, 1.0f - damagePercent, 0.0f, 0.5f);

    glm::vec3 boxPos(bestPlace.position.x, bestPlace.position.y, bestPlace.position.z);
    AABB box(boxPos, boxPos + glm::vec3(1.0f, 1.0f, 1.0f), true);

    std::vector<ImVec2> placePoints = MathUtils::getImBoxPoints(box);
    drawList->AddConvexPolyFilled(placePoints.data(), placePoints.size(), ImColor(color.Value.x, color.Value.y, color.Value.z, 0.25f));
}