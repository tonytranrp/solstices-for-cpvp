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
#include <Features/Modules/Misc/Friends.hpp>  // For friend checking
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
 * @brief Throttled retrieval of placement positions to reduce CPU usage.
 *
 * Uses mPlaceSearchDelay to limit how often findPlacePositions is called. Updates mLastsearchPlace
 * appropriately to ensure we don’t recalc too frequently. Renamed from getplacmenet (typo) for clarity.
 *
 * @param runtimeActors The list of current actors in the world.
 * @return Vector of PlacePosition (potential crystal placements).
 */
std::vector<AutoCrystal::PlacePosition> AutoCrystal::getPlacement(const std::vector<Actor*>& runtimeActors) {
    std::vector<PlacePosition> placementPos;
    // ⏳ Throttle search frequency to save CPU
    if (NOW - mLastsearchPlace < mPlaceSearchDelay.mValue) {
        return placementPos;
    }
    // Perform placement position search and update last search time
    placementPos = findPlacePositions(runtimeActors);
    mLastsearchPlace = NOW;
    return placementPos;
}

std::vector<AutoCrystal::PlacePosition> AutoCrystal::findPlacePositions(const std::vector<Actor*>& runtimeActors) {
    std::vector<PlacePosition> positions;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return positions;

    // 🧭 **Find the closest valid enemy actor within mRange**
    Actor* targetActor = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    glm::vec3 playerPos = *player->getPos();  // Cache player position to avoid repeated calls
    for (auto* actor : runtimeActors) {
        // Skip self, invalid actors, non-players, and friends
        if (actor == player || !actor->isValid() || !actor->isPlayer()) continue;
        if (Friends::isFriend(actor->getNameTag())) continue;  // 🔒 BUGFIX: don't target friends
        float dist = glm::distance(*actor->getPos(), playerPos);
        if (dist > mRange.mValue) continue;  // outside overall range
        if (dist < closestDist) {
            closestDist = dist;
            targetActor = actor;
        }
    }
    if (!targetActor) {
        return positions;  // No target found, no placements needed
    }

    // Use the target actor's position as the center of our search area
    BlockPos targetPos = *targetActor->getPos();
    int range = static_cast<int>(mPlaceRange.mValue);  // radius to search around target
    const int yMin = -5, yMax = 1;                     // vertical search offsets (empirical limits)

    // 📌 **Precompute candidate offsets sorted by distance from origin (0,0,0)**
    struct Offset { int x, y, z; float sqrDist; };
    static std::vector<Offset> sortedOffsets;
    static int lastRange = -1;
    if (range != lastRange || sortedOffsets.empty()) {
        // ⚙️ Compute and sort offsets only when range changes or first run
        sortedOffsets.clear();
        for (int x = -range; x <= range; ++x) {
            for (int y = yMin; y <= yMax; ++y) {
                for (int z = -range; z <= range; ++z) {
                    float sqDist = static_cast<float>(x * x + y * y + z * z);
                    sortedOffsets.push_back({ x, y, z, sqDist });
                }
            }
        }
        std::sort(sortedOffsets.begin(), sortedOffsets.end(), [](const Offset& a, const Offset& b) {
            return a.sqrDist < b.sqrDist;
            });
        lastRange = range;
    }

    // 🎯 **Iterate outward from target in "rings" to find the best placement**
    PlacePosition bestCandidate;
    bool foundCandidate = false;
    float bestRingDist = 0.0f;
    glm::vec3 targetCenter = *targetActor->getPos();  // target's exact position (for distance calc)
    for (const Offset& off : sortedOffsets) {
        // If we found a candidate in a nearer ring, stop when reaching the next ring
        if (foundCandidate && off.sqrDist > bestRingDist) break;
        if (!foundCandidate) bestRingDist = off.sqrDist;

        // Translate offset to an absolute BlockPos in the world
        BlockPos checkPos = targetPos + BlockPos(off.x, off.y, off.z);
        // Compute the crystal's center position if placed at checkPos (0.5 offset in X/Z and +1.0 in Y)
        glm::vec3 crystalCenter(checkPos.x + 0.5f, checkPos.y + 1.0f, checkPos.z + 0.5f);

        // Range checks: ensure within allowable distance from target and from player
        if (glm::distance(crystalCenter, targetCenter) > mPlaceRange.mValue) continue;          // 📏 Skip if too far from target (accuracy improvement)
        if (glm::distance(crystalCenter, playerPos) > mPlaceRangePlayer.mValue) continue;       // 📏 Skip if out of player's placement reach

        // 💎 Check if we can place a crystal at checkPos (fast block & entity checks)
        if (!canPlaceCrystal(checkPos, runtimeActors)) continue;

        // 💥 Calculate expected damage to target and self for this position
        float targetDamage = calculateDamage(checkPos, targetActor);
        if (targetDamage < mMinimumDamage.mValue) continue;  // Skip if it won't deal enough damage to target
        float selfDamage = calculateDamage(checkPos, player);
        if (selfDamage > mMaxSelfDamage.mValue) continue;    // Skip if it would hurt the local player too much

        // ⭐ Update best candidate if this position is better (higher target damage)
        if (!foundCandidate || targetDamage > bestCandidate.targetDamage) {
            bestCandidate = PlacePosition(checkPos, targetDamage, selfDamage);
            foundCandidate = true;
            bestRingDist = off.sqrDist;  // lock search to this ring distance
        }
    }

    if (foundCandidate) {
        positions.push_back(bestCandidate);
    }
    return positions;
}

std::vector<AutoCrystal::BreakTarget> AutoCrystal::findBreakTargets(const std::vector<Actor*>& runtimeActors) {
    std::vector<BreakTarget> breakTargets;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return breakTargets;
    glm::vec3 playerPos = *player->getPos();

    // 🧭 **Identify the closest enemy (target) within range for break calculations**
    Actor* targetActor = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    for (auto* actor : runtimeActors) {
        if (actor == player || !actor->isValid() || !actor->isPlayer()) continue;
        if (Friends::isFriend(actor->getNameTag())) continue;  // 🔒 BUGFIX: skip friends as targets
        float dist = glm::distance(*actor->getPos(), playerPos);
        if (dist > mRange.mValue) continue;
        if (dist < closestDist) {
            closestDist = dist;
            targetActor = actor;
        }
    }
    // 🎯 **Collect all end crystals in range and evaluate their threat/damage**
    for (auto* actor : runtimeActors) {
        // Filter for valid EnderCrystal entities within range
        if (!actor->isValid() || actor->getActorTypeComponent()->mType != ActorType::EnderCrystal)
            continue;
        float dist = glm::distance(*actor->getPos(), playerPos);
        if (dist > mRange.mValue)
            continue;

        // Calculate potential damage to target (if one is found) and to self
        BlockPos crystalPos(
            static_cast<int>(actor->getPos()->x),
            static_cast<int>(actor->getPos()->y),
            static_cast<int>(actor->getPos()->z)
        );
        float damageToTarget = 0.0f;
        if (targetActor) {
            damageToTarget = calculateDamage(crystalPos, targetActor);
            if (damageToTarget < mMinimumDamage.mValue) {
                // Skip crystals that don't threaten the target enough (offensive threshold)
                continue;
            }
        }
        float damageToSelf = calculateDamage(crystalPos, player);
        if (damageToSelf > mMaxSelfDamage.mValue) {
            // Skip crystals that would harm the player beyond acceptable limit
            continue;
        }

        breakTargets.emplace_back(actor, damageToTarget, damageToSelf);
    }

    // 🔃 **Sort break targets by descending target damage, with tie-breaker on self damage (descending)**
    std::sort(breakTargets.begin(), breakTargets.end(), [](const BreakTarget& a, const BreakTarget& b) {
        if (fabs(a.targetDamage - b.targetDamage) < 1e-3) {  // nearly equal target damage
            return a.selfDamage > b.selfDamage;  // prioritize breaking the crystal that poses higher self damage first (defensive priority)
        }
        return a.targetDamage > b.targetDamage;
        });
    return breakTargets;
}

bool AutoCrystal::canPlaceCrystal(const BlockPos& pos, const std::vector<Actor*>& runtimeActors) {
    BlockSource* bs = ClientInstance::get()->getBlockSource();
    if (!bs) return false;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return false;
    // ⚡ **Efficient block checks using BlockUtils and cached data**
    // Fetch base block and ensure it's a valid obsidian or bedrock platform
    const Block* baseBlock = bs->getBlock(pos);
    if (!baseBlock) return false;
    int baseId = baseBlock->mLegacy->getBlockId();
    if (baseId != 49 && baseId != 7) {
        return false;  // base block must be Obsidian (49) or Bedrock (7)
    }
    // Use BlockUtils to check exposure above the base (how many air blocks above)
    int exposedHeight = BlockUtils::getExposedHeight(pos);
    if (exposedHeight < 2) {
        return false;  // 🏗️ At least two blocks above must be clear (space for crystal)
    }
    // Verify the crystal would be within placeable range from the player (safety net check)
    glm::vec3 playerPos = *player->getPos();
    glm::vec3 crystalCenter(pos.x + 0.5f, pos.y + 1.0f, pos.z + 0.5f);
    if (glm::distance(playerPos, crystalCenter) > mPlaceRange.mValue) {
        return false;
    }

    // 📦 **Construct the crystal's bounding box for collision checks** 
    AABB crystalAABB(
        glm::vec3(pos.x, pos.y + 1.0f, pos.z),         // bottom face of the crystal (on top of base block)
        glm::vec3(pos.x + 1.0f, pos.y + 2.0f, pos.z + 1.0f), // top face (crystal height ~1 block)
        true
    );
    // 🚧 **Check for entity collisions in the placement area**
    for (auto* entity : runtimeActors) {
        if (!entity->isValid()) continue;
        if (entity->getActorTypeComponent()->mType == ActorType::EnderCrystal) continue;  // ignore existing crystals
        AABB entBB = entity->getAABB();
        if (entBB.mMin == entBB.mMax) continue;  // skip entities with no valid AABB (e.g., dead or phantoms)
        // Expand the entity's AABB slightly (0.1 in XZ) to be safe
        entBB.mMin -= glm::vec3(0.1f, 0.0f, 0.1f);
        entBB.mMax += glm::vec3(0.1f, 0.0f, 0.1f);
        if (crystalAABB.intersects(entBB)) {
            return false;  // 🚫 Another entity is occupying the spot where the crystal would be
        }
    }
    return true;
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
    player->getGameMode()->buildBlock(pos.position, randomFrom(0, 5), false);
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

    auto placements = getPlacement(runtimeActors);


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
    if (!mVisualizePlace.mValue) return;

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();

    static std::unordered_map<BlockPos, float> activeFadeAlphas;
    static std::chrono::steady_clock::time_point lastRenderTime = std::chrono::steady_clock::now();
    static BlockPos lastBestPos;
    static glm::vec3 currentLowPos;
    static float currentLowAlpha = 0.0f;
    static bool lowPosInitialized = false;

    auto now = std::chrono::steady_clock::now();
    float deltaSeconds = std::chrono::duration<float>(now - lastRenderTime).count();
    lastRenderTime = now;
    if (deltaSeconds > 1.0f) {
        deltaSeconds = 1.0f;
    }

    if (mRenderMode.mValue == ACVisualRenderMode::Fade) {
        std::unordered_set<BlockPos> currentPositions;
        currentPositions.reserve(mPossiblePlacements.size());
        for (auto& place : mPossiblePlacements) {
            currentPositions.insert(place.position);
        }

        for (auto& place : mPossiblePlacements) {
            const BlockPos& pos = place.position;
            if (activeFadeAlphas.find(pos) == activeFadeAlphas.end()) {
                activeFadeAlphas[pos] = 0.0f;
            }
        }

        for (auto it = activeFadeAlphas.begin(); it != activeFadeAlphas.end();) {
            const BlockPos& pos = it->first;
            float& alpha = it->second;
            if (currentPositions.find(pos) == currentPositions.end()) {
                float fadeStep = mFadeLerpSpeed.mValue * deltaSeconds;
                alpha -= fadeStep;
                if (alpha <= 0.0f) {
                    it = activeFadeAlphas.erase(it);
                    continue;
                }
            }
            else {
                float fadeStep = mFadeLerpSpeed.mValue * deltaSeconds;
                alpha += fadeStep;
                if (alpha > 1.0f) alpha = 1.0f;
            }
            ++it;
        }

        for (auto& entry : activeFadeAlphas) {
            const BlockPos& pos = entry.first;
            float alpha = entry.second;
            if (alpha <= 0.0f) continue;

            ImColor baseColor = mFadeColor.getAsImColor();
            ImColor drawColor = ImColor(baseColor.Value.x, baseColor.Value.y, baseColor.Value.z, baseColor.Value.w * alpha);

            glm::vec3 boxMin(pos.x, pos.y, pos.z);
            glm::vec3 boxMax(pos.x + 1.0f, pos.y + 1.0f, pos.z + 1.0f);
            AABB box(boxMin, boxMax, true);

            std::vector<ImVec2> screenPts = MathUtils::getImBoxPoints(box);
            if (!screenPts.empty()) {
                drawList->AddConvexPolyFilled(screenPts.data(), screenPts.size(),
                    ImColor(drawColor.Value.x, drawColor.Value.y, drawColor.Value.z, drawColor.Value.w * 0.5f));
                drawList->AddPolyline(screenPts.data(), screenPts.size(), drawColor, ImDrawFlags_Closed, 1.5f);
            }
        }
    }
    else if (mRenderMode.mValue == ACVisualRenderMode::Square) {
        if (mPossiblePlacements.empty()) return;
        const BlockPos& bestPos = mPossiblePlacements[0].position;

        ImColor outlineColor = mSquareColor.getAsImColor();
        outlineColor.Value.w *= 0.8f;

        ImVec2 screenCenter;
        glm::vec3 centerWorld(bestPos.x + 0.5f, bestPos.y + 0.5f, bestPos.z + 0.5f);
        if (RenderUtils::worldToScreen(centerWorld, screenCenter)) {
            float halfSize = mSquareSize.mValue / 2.0f;
            ImVec2 topLeft(screenCenter.x - halfSize, screenCenter.y - halfSize);
            ImVec2 bottomRight(screenCenter.x + halfSize, screenCenter.y + halfSize);

            drawList->AddRect(topLeft, bottomRight, outlineColor, 0.0f, 0, 2.0f);
        }
    }
    else if (mRenderMode.mValue == ACVisualRenderMode::Low) {
        if (mPossiblePlacements.empty()) {
            if (lowPosInitialized && currentLowAlpha > 0.0f) {
                float fadeStep = (1.0f / mLowDuration.mValue) * deltaSeconds;
                currentLowAlpha -= fadeStep;
                if (currentLowAlpha < 0.0f) currentLowAlpha = 0.0f;
                if (currentLowAlpha > 0.0f) {
                    ImColor lowColor = mLowColor.getAsImColor();
                    lowColor.Value.w *= currentLowAlpha;

                    glm::vec3 topMin(currentLowPos.x, currentLowPos.y + 1.01f, currentLowPos.z);
                    glm::vec3 topMax(currentLowPos.x + 1.0f, currentLowPos.y + 1.08f, currentLowPos.z + 1.0f);
                    AABB topBox(topMin, topMax, true);
                    std::vector<ImVec2> topPts = MathUtils::getImBoxPoints(topBox);
                    if (!topPts.empty()) {
                        drawList->AddConvexPolyFilled(topPts.data(), topPts.size(),
                            ImColor(lowColor.Value.x, lowColor.Value.y, lowColor.Value.z, lowColor.Value.w * 0.4f));
                        drawList->AddPolyline(topPts.data(), topPts.size(), lowColor, ImDrawFlags_Closed, 1.5f);
                    }
                }
                else {
                    lowPosInitialized = false;
                }
            }
            return;
        }

        const BlockPos& bestPos = mPossiblePlacements[0].position;
        glm::vec3 targetPos(bestPos.x, bestPos.y, bestPos.z);

        if (!lowPosInitialized) {
            currentLowPos = targetPos;
            currentLowAlpha = 1.0f;
            lowPosInitialized = true;
            lastBestPos = bestPos;
        }
        else if (bestPos != lastBestPos) {
            lastBestPos = bestPos;
        }

        glm::vec3 diff = targetPos - currentLowPos;
        float distance = glm::length(diff);
        if (distance > 0.001f) {
            float step = mLowLerpSpeed.mValue * deltaSeconds;
            if (step > distance) step = distance;
            currentLowPos += diff * (step / distance);
        }

        currentLowAlpha = 1.0f;
        ImColor lowColor = mLowColor.getAsImColor();

        glm::vec3 topMin(currentLowPos.x, currentLowPos.y + 1.01f, currentLowPos.z);
        glm::vec3 topMax(currentLowPos.x + 1.0f, currentLowPos.y + 1.08f, currentLowPos.z + 1.0f);
        AABB topBox(topMin, topMax, true);

        std::vector<ImVec2> topFacePts = MathUtils::getImBoxPoints(topBox);
        if (!topFacePts.empty()) {
            drawList->AddConvexPolyFilled(topFacePts.data(), topFacePts.size(),
                ImColor(lowColor.Value.x, lowColor.Value.y, lowColor.Value.z, lowColor.Value.w * 0.4f));
            drawList->AddPolyline(topFacePts.data(), topFacePts.size(), lowColor, ImDrawFlags_Closed, 1.5f);
        }
    }
}
