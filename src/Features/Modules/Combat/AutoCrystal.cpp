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
#include <random>

void AutoCrystal::onEnable() {
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoCrystal::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<RenderEvent, &AutoCrystal::onRenderEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &AutoCrystal::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &AutoCrystal::onPacketInEvent>(this);
   // rots = {};
}

void AutoCrystal::onDisable() {
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoCrystal::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<RenderEvent, &AutoCrystal::onRenderEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &AutoCrystal::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &AutoCrystal::onPacketInEvent>(this);
    mPossiblePlacements.clear();
    //rots = {};
    switchBack();
    mLastTarget = nullptr;
    mRotating = false;
}

float AutoCrystal::calculateDamage(const BlockPos& crystalPos, Actor* target) {
    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource || !target) return 0.f;

    glm::vec3 explosionPos(crystalPos.x + 0.5f, crystalPos.y + 1.0f, crystalPos.z + 0.5f);
    glm::vec3 targetPos = *target->getPos();

    // Create a CalcUnit to wrap the data
    CalcUnit calc(crystalPos, explosionPos, targetPos);

    // Early-out if the target is too far
    if (calc.distance > 12.0f) return 0.f;

    float visibility = blockSource->getSeenPercent(explosionPos, target->getAABB());
    if (visibility < 0.1f) return 0.f;

    // Java-style explosion damage calculation
    float impact = (1.0f - (calc.distance / 12.0f)) * visibility;
    float baseDamage = ((impact * impact) * 7.0f + impact * 0.5f) * 12.0f + 1.0f;

    // Armor reduction logic (simplified)
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
bool AutoCrystal::canPlaceCrystal(const BlockPos& pos, const std::vector<Actor*>& runtimeActors) {
    auto* bs = ClientInstance::get()->getBlockSource();
    if (!bs) return false;

    // Quickly retrieve the block at the candidate position.
    const Block* block = bs->getBlock(pos);
    if (!block) return false;
    int blockId = block->mLegacy->getBlockId();
    // Only allow obsidian (ID 49) or bedrock (ID 7)
    if (blockId != 49 && blockId != 7) return false;

    // Check that the two blocks above are air.
    BlockPos above = pos;
    above.y += 1;
    if (bs->getBlock(above)->mLegacy->getBlockId() != 0) return false;
    above.y += 1;
    if (bs->getBlock(above)->mLegacy->getBlockId() != 0) return false;

    // Define the crystal placement AABB.
    AABB placeAABB(
        glm::vec3(pos.x, pos.y + 1.f, pos.z),
        glm::vec3(pos.x + 1.f, pos.y + 2.f, pos.z + 1.f),
        true
    );

    // Iterate over the runtime actors (only one loop now)
    for (auto* entity : runtimeActors) {
        if (!entity->isValid()) continue;
        // Ignore existing crystals.
        if (entity->getActorTypeComponent()->mType == ActorType::EnderCrystal)
            continue;

        AABB entityAABB = entity->getAABB();
        // Skip invalid/degenerated bounding boxes.
        if (entityAABB.mMin == entityAABB.mMax) continue;
        // Slightly expand the entity's AABB.
        entityAABB.mMin -= glm::vec3(0.1f, 0.f, 0.1f);
        entityAABB.mMax += glm::vec3(0.1f, 0.f, 0.1f);

        // Use a simple intersection check.
        if (placeAABB.intersects(entityAABB))
            return false;
    }

    // Ensure the player is within placement range.
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return false;
    glm::vec3 crystalCenter(pos.x + 0.5f, pos.y + 1.0f, pos.z + 0.5f);
    return glm::distance(*player->getPos(), crystalCenter) <= mPlaceRange.mValue;
}

std::vector<AutoCrystal::PlacePosition> AutoCrystal::findPlacePositions(const std::vector<Actor*>& runtimeActors) {
    std::vector<PlacePosition> positions;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return positions;
    auto* bs = ClientInstance::get()->getBlockSource();
    if (!bs) return positions;

    // Select the closest enemy actor within range
    Actor* targetActor = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    BlockPos targetPos{};
    for (auto* actor : runtimeActors) {
        if (actor == player || !actor->isValid() || !actor->isPlayer())
            continue;
        float dist = glm::distance(*actor->getPos(), *player->getPos());
        if (dist > mRange.mValue)
            continue;
        if (dist < closestDist) {
            closestDist = dist;
            targetActor = actor;
        }
    }
    if (!targetActor)
        return positions;  // No enemy within range

    targetPos = *targetActor->getPos();
    int range = static_cast<int>(mPlaceRange.mValue);
    // Loop over candidate offsets in a cube around the target.
    for (int x = -range; x <= range; x++) {
        for (int y = -3; y <= 3; y++) {
            for (int z = -range; z <= range; z++) {
                BlockPos checkPos = targetPos + BlockPos(x, y, z);
                // Now perform the full placement check using our runtime list.
                if (!canPlaceCrystal(checkPos, runtimeActors))
                    continue;
                float targetDamage = calculateDamage(checkPos, targetActor);
                if (targetDamage >= mMinimumDamage.mValue) {
                    // For now, self damage is set to 0; add calculation if desired.
                    positions.emplace_back(checkPos, targetDamage, 0.f);
                
                }
            }
        }
    }

    // Sort placements so that higher target damage comes first.
    std::sort(positions.begin(), positions.end(), PlacePositionCompare());
    return positions;
}


std::vector<AutoCrystal::BreakTarget> AutoCrystal::findBreakTargets(const std::vector<Actor*>& runtimeActors) {
    std::vector<BreakTarget> breakTargets;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return breakTargets;

    // Iterate over runtime actors to find valid crystals.
    for (auto* actor : runtimeActors) {
        if (!actor->isValid() || actor->getActorTypeComponent()->mType != ActorType::EnderCrystal)
            continue;
        float dist = glm::distance(*actor->getPos(), *player->getPos());
        if (dist > mRange.mValue)
            continue;
        // Calculate damage values; for demonstration we use calculateDamage.
        float targetDmg = calculateDamage(BlockPos(actor->getPos()->x, actor->getPos()->y, actor->getPos()->z), player);
        float selfDmg = 0.f;
        breakTargets.emplace_back(actor, targetDmg, selfDmg);
    }
    // Sort break targets by descending target damage.
    std::sort(breakTargets.begin(), breakTargets.end(), [](const BreakTarget& a, const BreakTarget& b) {
        return a.targetDamage > b.targetDamage;
        });
    return breakTargets;
}
//https://stackoverflow.com/questions/5743678/generate-random-number-between-0-and-10
template <typename T>
T randomFrom(const T min, const T max)
{
    static std::random_device rdev;
    static std::default_random_engine re(rdev());
    typedef typename std::conditional<
        std::is_floating_point<T>::value,
        std::uniform_real_distribution<T>,
        std::uniform_int_distribution<T>>::type dist_type;
    dist_type uni(min, max);
    return static_cast<T>(uni(re));
}
void AutoCrystal::placeCrystal(const PlacePosition& pos) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->getGameMode()) return;

    if (NOW - mLastPlace < mPlaceDelay.mValue) return;
    player->getGameMode()->buildBlock(pos.position, randomFrom(0,5), false);
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

    // Check if the crystal is within the allowed break range.
    // Ensure you have a setting like mBreakRange defined.
    float distance = glm::distance(*crystal->getPos(), *player->getPos());
    if (distance > mPlaceRange.mValue) {
        return; // Crystal is out of break range.
    }

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

        int attackSlot = (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot)
            ? mCrystalSlot
            : player->getSupplies()->mSelectedSlot;

        std::shared_ptr<InventoryTransactionPacket> attackTransaction =
            ActorUtils::createAttackTransactionPredictId(crystal, attackSlot, predictedID);
        PacketUtils::queueSend(attackTransaction, false);
    }
    player->getGameMode()->attack(crystal);

    // Clear the known crystals after attempting attacks
    mLastAttackId += mPredictAmount.mValue;
    mShouldIdPredict = false;
    player->swing();
}

std::vector<AutoCrystal::PlacePosition> AutoCrystal::getplacmenet(const std::vector<Actor*>& runtimeActors) {
    std::vector<AutoCrystal::PlacePosition> placementpos;
    if (NOW - mLastsearchPlace < mPlaceSearchDelay.mValue) return placementpos;
    placementpos = findPlacePositions(runtimeActors);
    return placementpos;
    mLastsearchPlace = NOW;
}
void AutoCrystal::onBaseTickEvent(BaseTickEvent& event) {
    if (!mAutoPlace.mValue) return;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;
    auto* level = player->getLevel();
    if (!level) return;

    // Grab the runtime actor list once.
    const auto& runtimeActors = level->getRuntimeActorList();

    switchToCrystal();

    // Get potential placements using the pre-obtained runtime list.
 
    auto placements = getplacmenet(runtimeActors);

    
    mPossiblePlacements = placements;
    if (!placements.empty()) {
        // Place crystal at best candidate.
        placeCrystal(placements[0]);
       // ChatUtils::displayClientMessage("position placed : " + std::to_string(placements[0].position.x) + " " + std::to_string(placements[0].position.y) + " " + std::to_string(placements[0].position.x));
       

    }

    // Process break targets using the same runtime list.
    auto breakTargets = findBreakTargets(runtimeActors);
    if (!breakTargets.empty()) {
        breakCrystal(breakTargets[0].crystal);
    }

    if (mSwitchBack.mValue) {
        switchBack();
    }
}
void AutoCrystal::onPacketInEvent(PacketInEvent& event) {
    if (event.mPacket->getId() == PacketID::AddActor) {
        auto packet = event.getPacket<AddActorPacket>();
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
        if (mPossiblePlacements.empty()) return;
        auto pkt = event.getPacket<PlayerAuthInputPacket>();
        if (!pkt) return;
        auto rots = MathUtils::getRots(*player->getPos(), mPossiblePlacements[0].position);
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