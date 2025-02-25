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
/**
 * @brief Calculates the explosion damage inflicted by a crystal on a target.
 *
 * The damage is computed using the distance between the explosion and the target,
 * the visibility (using a ray trace based method), and a simplified armor reduction.
 * An early-out is performed if the target is beyond 12 blocks or if visibility is low.
 *
 * @param crystalPos The block position of the crystal.
 * @param target The target actor.
 * @return The calculated damage, or 0 if the target is too far or not visible enough.
 */
float AutoCrystal::calculateDamage(const BlockPos& crystalPos, Actor* target) {
    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource || !target)
        return 0.f;

    // Compute the explosion's center position (above the crystal).
    const glm::vec3 explosionPos(crystalPos.x + 0.5f, crystalPos.y + 1.0f, crystalPos.z + 0.5f);
    const glm::vec3 targetPos = *target->getPos();

    // Create a calculation unit to capture distance and other parameters.
    CalcUnit calc(crystalPos, explosionPos, targetPos);

    // Early-out if the target is beyond the effective range (12 blocks).
    if (calc.distance > 12.0f)
        return 0.f;

    // Compute visibility; if below a threshold, cancel damage.
    float visibility = blockSource->getSeenPercent(explosionPos, target->getAABB());
    if (visibility < 0.1f)
        return 0.f;

    // Calculate the impact factor based on distance and visibility.
    float impact = (1.0f - (calc.distance / 12.0f)) * visibility;
    float baseDamage = ((impact * impact) * 7.0f + impact * 0.5f) * 12.0f + 1.0f;

    // Apply armor reduction based on target's equipped armor.
    if (auto* equipment = target->getArmorContainer()) {
        float armorValue = 0.f;
        for (int i = 0; i < 4; i++) {
            if (auto* item = equipment->getItem(i)) {
                if (item->mItem) {
                    armorValue += 2.0f
                        + item->getEnchantValue(Enchant::PROTECTION) * 0.04f
                        + item->getEnchantValue(Enchant::BLAST_PROTECTION) * 0.08f;
                }
            }
        }
        baseDamage *= (1.0f - (std::min(20.0f, armorValue) / 25.0f));
    }
    return baseDamage;
}

/**
 * @brief Checks if a crystal can be placed at the given block position.
 *
 * This function minimizes getBlock calls by pre-fetching the base and two blocks
 * above the candidate position and performing early-out checks, including a distance
 * check against the local player.
 *
 * @param pos The candidate block position.
 * @param runtimeActors List of runtime actors for collision checking.
 * @return true if the crystal placement is valid, false otherwise.
 */
bool AutoCrystal::canPlaceCrystal(const BlockPos& pos, const std::vector<Actor*>& runtimeActors) {
    auto* bs = ClientInstance::get()->getBlockSource();
    if (!bs) return false;

    // Pre-fetch blocks for the candidate position and the two blocks above it.
    const Block* baseBlock = bs->getBlock(pos);
    const Block* blockAbove1 = bs->getBlock({ pos.x, pos.y + 1, pos.z });
    const Block* blockAbove2 = bs->getBlock({ pos.x, pos.y + 2, pos.z });
    if (!baseBlock || !blockAbove1 || !blockAbove2)
        return false;

    // Validate that the base is either obsidian (ID 49) or bedrock (ID 7)
    // and that the two blocks above are air (ID 0).
    int baseId = baseBlock->mLegacy->getBlockId();
    if (baseId != 49 && baseId != 7)
        return false;
    if (blockAbove1->mLegacy->getBlockId() != 0 || blockAbove2->mLegacy->getBlockId() != 0)
        return false;

    // Verify that the candidate is within the placement range of the player.
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return false;
    glm::vec3 crystalCenter(pos.x + 0.5f, pos.y + 1.0f, pos.z + 0.5f);
    if (glm::distance(*player->getPos(), crystalCenter) > mPlaceRange.mValue)
        return false;

    // Construct the crystal placement bounding box.
    AABB placeAABB(glm::vec3(pos.x, pos.y + 1.f, pos.z), glm::vec3(pos.x + 1.f, pos.y + 2.f, pos.z + 1.f), true);

    // Check for collisions with other runtime actors.
    for (auto* entity : runtimeActors) {
        if (!entity->isValid() || entity->getActorTypeComponent()->mType == ActorType::EnderCrystal)
            continue;

        AABB entityAABB = entity->getAABB();
        if (entityAABB.mMin == entityAABB.mMax)
            continue; // Skip invalid bounding boxes

        // Expand the actor's AABB slightly to allow some tolerance.
        entityAABB.mMin -= glm::vec3(0.1f, 0.f, 0.1f);
        entityAABB.mMax += glm::vec3(0.1f, 0.f, 0.1f);

        if (placeAABB.intersects(entityAABB))
            return false;
    }
    return true;
}
/**
 * @brief Searches for valid crystal placement positions around the target actor using a radial search.
 *
 * Instead of checking every candidate in a large cubic area, this version precomputes candidate offsets
 * sorted by their distance from the target. The search then proceeds outward in "rings" and stops as soon as
 * any valid candidate is found in the current ring (i.e. once the offsets start getting larger than the best
 * candidate's ring). Additionally, it calculates the self damage (to the local player) and only accepts
 * candidates where the self damage does not exceed mMaxSelfDamage. Finally, an extra filter using mPlaceRangePlayer
 * ensures that candidates are within a specified range of the local player's position.
 *
 * @param runtimeActors The current list of runtime actors.
 * @return A vector containing the best candidate placement position (or empty if none found).
 */
std::vector<AutoCrystal::PlacePosition> AutoCrystal::findPlacePositions(const std::vector<Actor*>& runtimeActors) {
    std::vector<PlacePosition> positions{};
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return positions;

    // --- Find the closest enemy actor within mRange ---
    Actor* targetActor = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    for (auto* actor : runtimeActors) {
        // Skip self, invalid actors, and crystals.
        if (actor == player || !actor->isValid() || actor->getActorTypeComponent()->mType == ActorType::EnderCrystal)
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
        return positions;

    // Use the target actor's position as the center of our search.
    BlockPos targetPos = *targetActor->getPos();

    // --- Define search parameters ---
    int range = static_cast<int>(mPlaceRange.mValue);
    // Y offsets based on empirical values.
    const int yMin = -5, yMax = 1;

    // --- Precompute candidate offsets relative to targetPos ---
    struct Offset {
        int x, y, z;
        float sqrDist; // squared distance from (0,0,0)
    };
    std::vector<Offset> offsets;
    for (int x = -range; x <= range; x++) {
        for (int y = yMin; y <= yMax; y++) {
            for (int z = -range; z <= range; z++) {
                float sqr = static_cast<float>(x * x + y * y + z * z);
                offsets.push_back({ x, y, z, sqr });
            }
        }
    }
    // Sort offsets by increasing squared distance.
    std::sort(offsets.begin(), offsets.end(), [](const Offset& a, const Offset& b) {
        return a.sqrDist < b.sqrDist;
        });

    // --- Radial search for a valid candidate ---
    PlacePosition bestCandidate;
    bool foundCandidate = false;
    float currentRing = -1.f;  // squared distance where we first found a candidate

    for (size_t i = 0; i < offsets.size(); i++) {
        const auto& off = offsets[i];

        // Break out if candidate was found and now we are in a further ring.
        if (foundCandidate && off.sqrDist > currentRing)
            break;
        if (!foundCandidate)
            currentRing = off.sqrDist;

        // Candidate block position.
        BlockPos checkPos = targetPos + BlockPos(off.x, off.y, off.z);
        // Candidate's center position (offset by 0.5 in x/z and 1.0 in y).
        glm::vec3 center(checkPos.x + 0.5f, checkPos.y + 1.0f, checkPos.z + 0.5f);

        // Ensure candidate is within the general placement range.
        if (glm::distance(*player->getPos(), center) > mPlaceRange.mValue)
            continue;
        // Additional filter: candidate must be within mPlaceRangePlayer from the local player.
        if (glm::distance(*player->getPos(), center) > mPlaceRangePlayer.mValue)
            continue;
        // Perform the expensive placement check.
        if (!canPlaceCrystal(checkPos, runtimeActors))
            continue;

        // Calculate the expected damage on the target.
        float targetDamage = calculateDamage(checkPos, targetActor);
        if (targetDamage < mMinimumDamage.mValue)
            continue;

        // Calculate self damage (damage to the local player).
        float selfDamage = calculateDamage(checkPos, player);
        if (selfDamage > mMaxSelfDamage.mValue)
            continue;

        // Update best candidate if this candidate yields higher target damage.
        if (!foundCandidate || targetDamage > bestCandidate.targetDamage) {
            bestCandidate = PlacePosition(checkPos, targetDamage, selfDamage);
            foundCandidate = true;
        }
    }

    if (foundCandidate)
        positions.push_back(bestCandidate);
    return positions;
}


/**
 * @brief Finds valid crystals to break.
 *
 * Iterates through runtime actors to locate EnderCrystals within range.
 * For each crystal, calculates the expected damage (using calculateDamage) and
 * only adds those that exceed the minimum damage threshold.
 *
 * If the "Break Own" setting is enabled, only crystals located near the last placed
 * crystal position are considered.
 *
 * @param runtimeActors The current list of runtime actors.
 * @return A sorted vector of break targets (highest target damage first).
 */
std::vector<AutoCrystal::BreakTarget> AutoCrystal::findBreakTargets(const std::vector<Actor*>& runtimeActors) {
    std::vector<BreakTarget> breakTargets{};
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return breakTargets;

    for (auto* actor : runtimeActors) {
        if (!actor->isValid() || actor->getActorTypeComponent()->mType != ActorType::EnderCrystal)
            continue;
        float dist = glm::distance(*actor->getPos(), *player->getPos());
        if (dist > mRange.mValue)
            continue;

        // Calculate the target damage; if it doesn't meet our threshold, skip.
        float targetDmg = calculateDamage(BlockPos(actor->getPos()->x, actor->getPos()->y, actor->getPos()->z), player);
        if (targetDmg < mMinimumDamage.mValue)
            continue;

        // If "Break Own" is enabled, only consider crystals near our last placed position.
        if (mBreakOnlyOwn.mValue) {
            if (!mHasPlacedCrystal)
                continue; // We haven't placed a crystal yet.
            glm::vec3 placedCenter(mLastPlacedPos.x + 0.5f, mLastPlacedPos.y + 1.0f, mLastPlacedPos.z + 0.5f);
            glm::vec3 crystalCenter = *actor->getPos(); // Assuming the actor position is its center.
            if (glm::distance(placedCenter, crystalCenter) > 1.0f)
                continue;
        }

        // Self damage can be set to 0.f here (or calculate if needed).
        breakTargets.emplace_back(actor, targetDmg, 0.f);
    }

    // Sort break targets so that those with the highest target damage come first.
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
/**
 * @brief Places a crystal at the given placement position.
 *
 * If using silent switch mode, the function spoofs the crystal slot so that the actual
 * selected slot remains unchanged. The function also respects the placement delay.
 *
 * @param pos The candidate crystal placement position.
 */
void AutoCrystal::placeCrystal(const PlacePosition& pos) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->getGameMode())
        return;

    if (NOW - mLastPlace < mPlaceDelay.mValue)
        return;

    // If using silent switching, spoof the crystal slot before placing.
    if (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot) {
        PacketUtils::spoofSlot(mCrystalSlot, false);
    }

    player->getGameMode()->buildBlock(pos.position, randomFrom(0, 5), false);
    mLastPlace = NOW;

    // Record the placement position and set our flag.
    mLastPlacedPos = pos.position;
    mHasPlacedCrystal = true;
}
/**
 * @brief Breaks a crystal if it meets all conditions.
 *
 * Checks if the crystal is within break range and, if "Break Own" is enabled,
 * only breaks the crystal if it is located near the last placed position.
 * Uses prediction mode if enabled.
 *
 * @param crystal The crystal actor to break.
 */
void AutoCrystal::breakCrystal(Actor* crystal) {
    if (!crystal)
        return;

    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->getGameMode())
        return;

    float distance = glm::distance(*crystal->getPos(), *player->getPos());
    if (distance > mPlaceRange.mValue)
        return; // Crystal is out of break range.

    // If "Break Own" is enabled, ensure the crystal is near our placed position.
    if (mBreakOnlyOwn.mValue && mHasPlacedCrystal) {
        glm::vec3 placedCenter(mLastPlacedPos.x + 0.5f, mLastPlacedPos.y + 1.0f, mLastPlacedPos.z + 0.5f);
        glm::vec3 crystalCenter = *crystal->getPos();
        if (glm::distance(placedCenter, crystalCenter) > 1.0f)
            return;
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
    player->getGameMode()->attack(crystal);
    player->swing();
    for (int i = 0; i < mPredictAmount.mValue; i++) {
        uint64_t predictedID = mLastAttackId + i;
        int attackSlot = (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot)
            ? mCrystalSlot
            : player->getSupplies()->mSelectedSlot;
        PacketUtils::spoofSlot(attackSlot, false);
        std::shared_ptr<InventoryTransactionPacket> attackTransaction =
            ActorUtils::createAttackTransactionPredictId(crystal, attackSlot, predictedID);
        PacketUtils::queueSend(attackTransaction, false);
    }
    // Clear prediction state.
    mLastAttackId += mPredictAmount.mValue;
    mShouldIdPredict = false;
}

/**
 * @brief Retrieves the candidate crystal placement positions.
 *
 * This function applies a search delay before querying the placement positions.
 *
 * @param runtimeActors The current list of runtime actors.
 * @return A vector of valid placement positions.
 */
std::vector<AutoCrystal::PlacePosition> AutoCrystal::getplacmenet(const std::vector<Actor*>& runtimeActors) {
    if (NOW - mLastsearchPlace < mPlaceSearchDelay.mValue)
        return {};
    mLastsearchPlace = NOW;
    return findPlacePositions(runtimeActors);
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
/**
 * @brief Main tick event handler.
 *
 * This function triggers crystal placement and breaking by:
 * - Switching to the crystal slot.
 * - Searching for valid placements.
 * - Placing a crystal if a valid candidate exists.
 * - Processing break targets.
 * - Switching back to the previous slot if required.
 *
 * @param event The base tick event.
 */
void AutoCrystal::onBaseTickEvent(BaseTickEvent& event) {
    if (!mAutoPlace.mValue)
        return;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return;
    auto* level = player->getLevel();
    if (!level)
        return;

    // Retrieve the current list of runtime actors.
    const auto& runtimeActors = level->getRuntimeActorList();
    switchToCrystal();

    // Get potential crystal placements.
    auto placements = getplacmenet(runtimeActors);
    mPossiblePlacements = placements;
    if (!placements.empty()) {
        placeCrystal(placements[0]);
    }

    // Process break targets for crystals within range.
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