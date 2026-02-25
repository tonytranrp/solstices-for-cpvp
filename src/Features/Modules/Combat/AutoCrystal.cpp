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
#include <SDK/Minecraft/World/Chunk/LevelChunk.hpp>
#include <random>
#include <limits>
#include <algorithm>
#include <cmath>

namespace {
int floorDiv16(int value) {
    return (value >= 0) ? (value / 16) : ((value - 15) / 16);
}
}

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
    {
        std::lock_guard<std::mutex> lock(blockmutex);
        mPossiblePlacements.clear();
        mBreakTargetPos = {};
        mHasBreakTarget = false;
    }
    //rots = {};
    switchBack();
    mLastTarget = nullptr;
    mRotating = false;
}

float AutoCrystal::calculateDamage(const BlockPos& crystalPos, Actor* target) {
    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource || !target) return 0.f;

    // Calculate the explosion origin (center of the crystal explosion)
    glm::vec3 explosionPos(crystalPos.x + 0.5f, crystalPos.y + 1.0f, crystalPos.z + 0.5f);
    glm::vec3 targetPos = *target->getPos();

    // Compute distance using glm (early-out if too far)
    float distance = glm::distance(explosionPos, targetPos);
    if (distance > 12.0f) return 0.f;

    // Get the fraction of visibility between the explosion and the target’s AABB
    float visibility = blockSource->getSeenPercent(explosionPos, target->getAABB());
    if (visibility < 0.1f) return 0.f;

    // Calculate impact factor (the higher, the better)
    double impact = (1.0 - (distance / 12.0)) * visibility;
    // Java-style explosion damage calculation (adjust as needed for your game’s physics)
    double rawDamage = ((impact * impact) * 7.0 + impact * 0.5) * 12.0 + 1.0;

    // Apply a simplified armor reduction
    if (auto* equipment = target->getArmorContainer()) {
        double armorValue = 0.0;
        for (int i = 0; i < 4; i++) {
            auto* item = equipment->getItem(i);
            if (item && item->mItem) {
                double protection = item->getEnchantValue(Enchant::PROTECTION) * 0.04;
                double blastProtection = item->getEnchantValue(Enchant::BLAST_PROTECTION) * 0.08;
                armorValue += 2.0 + protection + blastProtection;
            }
        }
        // Clamp armor value to 20, then reduce damage accordingly
        rawDamage *= (1.0 - (std::min(20.0, armorValue) / 25.0));
    }

    float finalDamage = static_cast<float>(rawDamage);
    // Clamp the damage between 0 and 20 for better precision and to simulate maximum damage limits.
    finalDamage = std::clamp(finalDamage, 0.0f, 20.0f);
    return finalDamage;
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

Actor* AutoCrystal::findClosestTarget(const std::vector<Actor*>& runtimeActors, Actor* player) const {
    if (!player) return nullptr;

    Actor* targetActor = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    const glm::vec3 playerPos = *player->getPos();

    for (auto* actor : runtimeActors) {
        if (!actor || actor == player || !actor->isValid() || !actor->isPlayer()) continue;
        if (Friends::isFriend(actor->getNameTag())) continue;

        const float dist = glm::distance(*actor->getPos(), playerPos);
        if (dist > mRange.mValue) continue;
        if (dist < closestDist) {
            closestDist = dist;
            targetActor = actor;
        }
    }

    return targetActor;
}

std::vector<BlockPos> AutoCrystal::collectSubChunkPlacementBases(const BlockPos& targetPos, float horizontalRange, int minYOffset, int maxYOffset) const {
    std::vector<BlockPos> candidates;

    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource) return candidates;

    const int radius = static_cast<int>(std::ceil(horizontalRange));
    const int minX = targetPos.x - radius;
    const int maxX = targetPos.x + radius;
    const int minZ = targetPos.z - radius;
    const int maxZ = targetPos.z + radius;
    const int minY = targetPos.y + minYOffset;
    const int maxY = targetPos.y + maxYOffset;

    const int buildDepth = blockSource->getBuildDepth();
    const int buildMaxY = blockSource->getBuildHeight() - 1;
    const int clampedMinY = std::max(minY, buildDepth);
    const int clampedMaxY = std::min(maxY, buildMaxY);
    if (clampedMinY > clampedMaxY) return candidates;

    const int minChunkX = floorDiv16(minX);
    const int maxChunkX = floorDiv16(maxX);
    const int minChunkZ = floorDiv16(minZ);
    const int maxChunkZ = floorDiv16(maxZ);
    const int minSubChunk = (clampedMinY - buildDepth) >> 4;
    const int maxSubChunk = (clampedMaxY - buildDepth) >> 4;

    for (int chunkX = minChunkX; chunkX <= maxChunkX; ++chunkX) {
        for (int chunkZ = minChunkZ; chunkZ <= maxChunkZ; ++chunkZ) {
            LevelChunk* chunk = blockSource->getChunk(ChunkPos(chunkX, chunkZ));
            if (!chunk) continue;

            auto* subChunks = chunk->getSubChunks();
            if (!subChunks || subChunks->empty()) continue;

            const int chunkBaseX = chunkX * 16;
            const int chunkBaseZ = chunkZ * 16;
            const int chunkMinSub = std::max(minSubChunk, 0);
            const int chunkMaxSub = std::min(maxSubChunk, static_cast<int>(subChunks->size()) - 1);
            if (chunkMinSub > chunkMaxSub) continue;

            for (int subIndex = chunkMinSub; subIndex <= chunkMaxSub; ++subIndex) {
                const auto& subChunk = (*subChunks)[subIndex];
                SubChunkBlockStorage* blockReader = subChunk.blockReadPtr;
                if (!blockReader) continue;

                const int subChunkBaseY = subChunk.subchunkIndex * 16;
                const int localMinY = std::max(clampedMinY - subChunkBaseY, 0);
                const int localMaxY = std::min(clampedMaxY - subChunkBaseY, 15);
                if (localMinY > localMaxY) continue;

                for (int x = 0; x < 16; ++x) {
                    const int worldX = chunkBaseX + x;
                    if (worldX < minX || worldX > maxX) continue;

                    for (int z = 0; z < 16; ++z) {
                        const int worldZ = chunkBaseZ + z;
                        if (worldZ < minZ || worldZ > maxZ) continue;

                        for (int y = localMinY; y <= localMaxY; ++y) {
                            const uint16_t elementId = static_cast<uint16_t>((x * 0x10 + z) * 0x10 + (y & 0xF));
                            const Block* found = blockReader->getElement(elementId);
                            if (!found || !found->mLegacy) continue;

                            const int blockId = found->mLegacy->getBlockId();
                            if (blockId != 49 && blockId != 7) continue;

                            BlockPos basePos(worldX, subChunkBaseY + y, worldZ);
                            if (BlockUtils::getExposedHeight(basePos, 2) < 2) continue;
                            candidates.emplace_back(basePos);
                        }
                    }
                }
            }
        }
    }

    return candidates;
}

std::vector<AutoCrystal::PlacePosition> AutoCrystal::findPlacePositions(const std::vector<Actor*>& runtimeActors) {
    std::vector<PlacePosition> positions;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return positions;

    Actor* targetActor = findClosestTarget(runtimeActors, player);
    if (!targetActor) return positions;

    float bestScore = -std::numeric_limits<float>::infinity();
    PlacePosition bestCandidate;
    bool foundCandidate = false;
    const glm::vec3 targetCenter = *targetActor->getPos();
    const glm::vec3 playerPos = *player->getPos();
    const float placeRangeSq = mPlaceRange.mValue * mPlaceRange.mValue;
    const float placeRangePlayerSq = mPlaceRangePlayer.mValue * mPlaceRangePlayer.mValue;

    const std::vector<BlockPos> candidates = collectSubChunkPlacementBases(
        BlockPos(targetCenter),
        mPlaceRange.mValue,
        -5,
        1
    );

    for (const BlockPos& checkPos : candidates) {
        const glm::vec3 crystalCenter(checkPos.x + 0.5f, checkPos.y + 1.0f, checkPos.z + 0.5f);
        const glm::vec3 toTarget = crystalCenter - targetCenter;
        const glm::vec3 toPlayer = crystalCenter - playerPos;

        if (glm::dot(toTarget, toTarget) > placeRangeSq) continue;
        if (glm::dot(toPlayer, toPlayer) > placeRangePlayerSq) continue;
        if (!canPlaceCrystal(checkPos, runtimeActors)) continue;

        const float candidateDamage = calculateDamage(checkPos, targetActor);
        if (candidateDamage < mMinimumDamage.mValue) continue;
        const float selfDamage = calculateDamage(checkPos, player);
        if (selfDamage > mMaxSelfDamage.mValue) continue;

        const float distToTarget = glm::length(toTarget);
        const float score = candidateDamage - (selfDamage * 0.35f) - (distToTarget * 0.08f);

        if (!foundCandidate || score > bestScore) {
            bestScore = score;
            bestCandidate = PlacePosition(checkPos, candidateDamage, selfDamage);
            foundCandidate = true;
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
void AutoCrystal::switchToCrystal()
{
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    auto inventory = player->getSupplies()->getContainer();
    mCrystalSlot = -1;

    // 1) Find crystals in hotbar
    for (int i = 0; i < 9; i++)
    {
        auto item = inventory->getItem(i);
        if (item && item->mItem && item->getItem()->mItemId == 789)
        {
            mCrystalSlot = i;
            break;
        }
    }
    if (mCrystalSlot == -1) // none found
        return;

    // 2) Switch logic
    if (mSwitchMode.mValue == SwitchMode::Silent)
    {
        // Spoof once to tell server we hold crystals
        PacketUtils::spoofSlot(mCrystalSlot, true);
        mShouldSpoofSlot = true;
    }
    else
    {
        // Non-silent
        if (mPrevSlot == -1)
            mPrevSlot = player->getSupplies()->mSelectedSlot;
        else
            player->getSupplies()->mInHandSlot = mPrevSlot; // "fake" visuals if you want

        player->getSupplies()->mSelectedSlot = mCrystalSlot;
        mHasSwitched = true;
    }
}

void AutoCrystal::switchBack()
{
    // If we never actually switched, do nothing
    if (!mHasSwitched) return;

    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    if (mSwitchMode.mValue == SwitchMode::Silent)
    {
        // Spoof once back to old slot
        PacketUtils::spoofSlot(mPrevSlot, true);
        mShouldSpoofSlot = false;
    }
    else
    {
        // Non-silent revert
        if (mPrevSlot != -1)
        {
            player->getSupplies()->mSelectedSlot = mPrevSlot;
            // optionally also set inHandSlot
            mPrevSlot = -1;
            mHasSwitched = false;
        }
    }
}

void AutoCrystal::placeCrystal(const PlacePosition& pos)
{
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->getGameMode())
        return;

    // Throttle
    if (NOW - mLastPlace < mPlaceDelay.mValue)
        return;

    // If we have no crystal slot found, we can't place
    if (mCrystalSlot == -1)
        return;

    // Save the original slot in case we need to revert
    const int oldSlot = player->getSupplies()->mSelectedSlot;

    // If we are in silent mode AND we’re still supposed to spoof, 
    // briefly spoof to the crystal slot.
    if (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot)
    {
        // 1) Tell the server "we're holding crystals"
        PacketUtils::spoofSlot(mCrystalSlot, true);
    }
    else
    {
        // If not silent, physically set to the crystal slot 
        // (unless you already did so in switchToCrystal()).
        // Some want only the "switchToCrystal()" path – in that case, remove this line:
        player->getSupplies()->mSelectedSlot = mCrystalSlot;
    }

    // 2) Place the block
    player->getGameMode()->buildBlock(pos.position, randomFrom(0, 5), false);

    // 3) If we were in silent mode, revert the slot for the server
    if (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot)
    {
        // Return to old slot so the server sees we "switched back"
        PacketUtils::spoofSlot(oldSlot, true);
    }
    else
    {
        // If we physically switched, revert physically if you want to remain ephemeral
        // But if your logic is “remain on crystals,” skip this.
        player->getSupplies()->mSelectedSlot = oldSlot;
    }

    mLastPlace = NOW;
}
void AutoCrystal::breakCrystal(Actor* crystal)
{
    if (!crystal) return;

    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->getGameMode()) return;

    float distance = glm::distance(*crystal->getPos(), *player->getPos());
    if (distance > mPlaceRange.mValue)
        return;

    // Record break target rotation information.
    {
        std::lock_guard<std::mutex> lock(blockmutex);
        mBreakTargetPos = *crystal->getPos();
        mHasBreakTarget = true;
    }

    // If we are in silent mode & spoofing, do it once
    if (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot)
    {
        PacketUtils::spoofSlot(mCrystalSlot, true);
    }

    if (!mIdPredict.mValue)
    {
        // Normal break
        player->getGameMode()->attack(crystal);
        player->swing();
        return;
    }

    // Predicted break
    mShouldIdPredict = true;
    mLastAttackId = crystal->getRuntimeID();

    for (int i = 0; i < mPredictAmount.mValue; i++)
    {
        uint64_t predictedID = mLastAttackId + i;
        int attackSlot = (mSwitchMode.mValue == SwitchMode::Silent && mShouldSpoofSlot)
            ? mCrystalSlot
            : player->getSupplies()->mSelectedSlot;

        auto attackTx = ActorUtils::createAttackTransactionPredictId(crystal, attackSlot, predictedID);
        PacketUtils::queueSend(attackTx, true);
    }
    player->getGameMode()->attack(crystal);
    mLastAttackId += mPredictAmount.mValue;
    mShouldIdPredict = false;
    player->swing();
}

std::vector<AutoCrystal::PlacePosition> AutoCrystal::getPlacementCandidates(const std::vector<Actor*>& runtimeActors) {
    std::vector<AutoCrystal::PlacePosition> placementpos;
    if (NOW - mLastsearchPlace < mPlaceSearchDelay.mValue) return placementpos;
    placementpos = findPlacePositions(runtimeActors);
    mLastsearchPlace = NOW;
    return placementpos;
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

    auto placements = getPlacementCandidates(runtimeActors);


    {
        std::lock_guard<std::mutex> lock(blockmutex);
        mPossiblePlacements = placements;
    }
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
    (void)event;
}
void AutoCrystal::onPacketOutEvent(PacketOutEvent& event) {
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // Handle crystal placement inventory transactions.
    if (event.mPacket->getId() == PacketID::InventoryTransaction) {
        auto pkt = event.getPacket<InventoryTransactionPacket>();
        if (!pkt || !pkt->mTransaction) return;

        if (pkt->mTransaction->type == ComplexInventoryTransaction::Type::ItemUseTransaction) {
            auto* transac = static_cast<ItemUseInventoryTransaction*>(pkt->mTransaction.get());
            if (transac->mActionType == ItemUseInventoryTransaction::ActionType::Place) {
                const auto clickPosIt = BlockUtils::clickPosOffsets.find(static_cast<int>(transac->mFace));
                if (clickPosIt == BlockUtils::clickPosOffsets.end()) return;
                transac->mClickPos = clickPosIt->second;

                for (int i = 0; i < 3; i++) {
                    if (transac->mClickPos[i] == 0.5f) {
                        transac->mClickPos[i] = MathUtils::randomFloat(-0.49f, 0.49f);
                    }
                }
                transac->mClickPos.y -= 0.1f;
            }
        }
    }

    // Combine rotation for both crystal placement and break.
    if (event.mPacket->getId() == PacketID::PlayerAuthInput) {
        auto pkt = event.getPacket<PlayerAuthInputPacket>();
        if (!pkt) return;

        bool hasPlacementTarget = false;
        bool hasBreakTarget = false;
        glm::vec3 placementTargetPos{};
        glm::vec3 breakTargetPos{};
        {
            std::lock_guard<std::mutex> lock(blockmutex);
            if (!mPossiblePlacements.empty()) {
                placementTargetPos = glm::vec3(mPossiblePlacements[0].position);
                hasPlacementTarget = true;
            }
            breakTargetPos = mBreakTargetPos;
            hasBreakTarget = mHasBreakTarget;
        }

        // Only proceed if we have at least one valid target (placement or break).
        if (!hasPlacementTarget && !hasBreakTarget) return;

        glm::vec3 targetPos;
        if (hasPlacementTarget && hasBreakTarget) {
            // Average the positions for a combined rotation.
            targetPos = (placementTargetPos + breakTargetPos) * 0.5f;
        }
        else if (hasPlacementTarget) {
            targetPos = placementTargetPos;
        }
        else {
            targetPos = breakTargetPos;
        }

        auto rots = MathUtils::getRots(*player->getPos(), targetPos);
        pkt->mRot = rots;
        pkt->mYHeadRot = rots.y;
    }
}
void AutoCrystal::onRenderEvent(RenderEvent& event) {
    // If visualizations are disabled, there's nothing to do
    if (!mVisualizePlace.mValue) return;

    std::vector<PlacePosition> placementsSnapshot;
    {
        std::lock_guard<std::mutex> lock(blockmutex);
        placementsSnapshot = mPossiblePlacements;
    }

    ImDrawList* drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) return;

    // ------------------------------------------------------------------
    // Common data/variables to handle timing between frames
    // ------------------------------------------------------------------
    static auto lastRenderTime = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float deltaSeconds = std::chrono::duration<float>(now - lastRenderTime).count();
    lastRenderTime = now;

    // Clamp deltaSeconds so if we freeze for a while, we don't jump abruptly
    if (deltaSeconds > 0.5f) {
        deltaSeconds = 0.5f;
    }

    // ------------------------------------------------------------------
    // 1) FADE MODE
    // ------------------------------------------------------------------
    if (mRenderMode.mValue == ACVisualRenderMode::Fade)
    {
        // Store fade alphas for each block position
        static std::unordered_map<BlockPos, float> sFadeAlphaMap;

        // Step 1: Build a set of current positions so we know
        // which are newly added, which remain, and which have disappeared.
        std::unordered_set<BlockPos> currentPosSet;
        currentPosSet.reserve(placementsSnapshot.size());
        for (auto& place : placementsSnapshot) {
            currentPosSet.insert(place.position);
        }

        // Step 2: For any newly visible positions, ensure they exist in our fade map
        for (auto& place : placementsSnapshot) {
            if (sFadeAlphaMap.find(place.position) == sFadeAlphaMap.end()) {
                sFadeAlphaMap[place.position] = 0.0f; // start invisible
            }
        }

        // Step 3: Update fade alpha in or out for all known positions
        // We'll remove those that fade fully out.
        float fadeSpeed = mFadeLerpSpeed.mValue * deltaSeconds;
        for (auto it = sFadeAlphaMap.begin(); it != sFadeAlphaMap.end(); ) {
            float& alphaRef = it->second;
            bool inCurrentList = (currentPosSet.find(it->first) != currentPosSet.end());

            // If the position is in the new list, fade in; otherwise fade out
            float fadeDir = inCurrentList ? +1.0f : -1.0f;
            alphaRef += fadeDir * fadeSpeed;

            // Clamp to [0..1], and remove if fully faded out
            if (alphaRef <= 0.0f) {
                alphaRef = 0.0f;
                it = sFadeAlphaMap.erase(it);
                continue;
            }
            else if (alphaRef > 1.0f) {
                alphaRef = 1.0f;
            }
            ++it;
        }

        // Step 4: Render each block that has alpha > 0
        ImColor baseColor = mFadeColor.getAsImColor();
        for (auto& kv : sFadeAlphaMap) {
            const BlockPos& pos = kv.first;
            float alphaVal = kv.second;
            if (alphaVal <= 0.0f) continue; // skip fully transparent

            ImColor drawColor = ImColor(
                baseColor.Value.x, baseColor.Value.y, baseColor.Value.z,
                baseColor.Value.w * alphaVal
            );

            glm::vec3 boxMin(pos.x, pos.y, pos.z);
            glm::vec3 boxMax(pos.x + 1.0f, pos.y + 1.0f, pos.z + 1.0f);
            AABB box(boxMin, boxMax, true);

            // Convert the bounding box to 2D points for ImGui
            std::vector<ImVec2> pts = MathUtils::getImBoxPoints(box);
            if (!pts.empty()) {
                // Fill: half alpha for the fill so it's more subtle
                ImColor fillColor(drawColor.Value.x, drawColor.Value.y, drawColor.Value.z,
                    drawColor.Value.w * 0.5f);
                drawList->AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), fillColor);

                // Outline: normal alpha so it's visible
                drawList->AddPolyline(pts.data(), static_cast<int>(pts.size()), drawColor,
                    ImDrawFlags_Closed, 1.8f);
            }
        }
    }
    // ------------------------------------------------------------------
    // 2) SQUARE MODE
    // ------------------------------------------------------------------
    else if (mRenderMode.mValue == ACVisualRenderMode::Square)
    {
        // If there's nothing to show, skip
        if (placementsSnapshot.empty()) return;

        // Take the best candidate as an example
        const BlockPos& bestPos = placementsSnapshot[0].position;

        // Convert center of block to screen coordinates
        glm::vec3 worldCenter(bestPos.x + 0.5f, bestPos.y + 0.5f, bestPos.z + 0.5f);
        ImVec2 screenPos;
        if (!RenderUtils::worldToScreen(worldCenter, screenPos)) {
            return; // not in screen view
        }

        // We fill a rectangle and outline it
        float halfSize = mSquareSize.mValue * 0.5f;
        ImVec2 topLeft(screenPos.x - halfSize, screenPos.y - halfSize);
        ImVec2 bottomRight(screenPos.x + halfSize, screenPos.y + halfSize);

        ImColor rectColor = mSquareColor.getAsImColor();
        // Slightly more transparent fill
        ImColor fillColor(rectColor.Value.x, rectColor.Value.y, rectColor.Value.z, rectColor.Value.w * 0.3f);
        // Outline is a bit more opaque
        ImColor outlineColor(rectColor.Value.x, rectColor.Value.y, rectColor.Value.z, rectColor.Value.w * 0.8f);

        // Fill the square
        drawList->AddRectFilled(topLeft, bottomRight, fillColor, 4.0f); // small rounding
        // Draw the border
        drawList->AddRect(topLeft, bottomRight, outlineColor, 4.0f, 0, 2.5f);
    }
    // ------------------------------------------------------------------
    // 3) LOW MODE
    // ------------------------------------------------------------------
    else if (mRenderMode.mValue == ACVisualRenderMode::Low)
    {
        // We'll track a single 'currentLowPos' that moves or fades out if no placements
        static glm::vec3 sCurrentLowPos(0.0f);
        static float sCurrentAlpha = 0.0f;
        static bool sInitialized = false;
        static BlockPos sLastBestPos;

        // If we have no placements
        if (placementsSnapshot.empty())
        {
            // Fade out if we previously had a position
            if (sInitialized && sCurrentAlpha > 0.0f) {
                // Fade out over the chosen duration
                float fadeRate = 1.0f / std::max(0.01f, mLowDuration.mValue);
                sCurrentAlpha -= fadeRate * deltaSeconds;
                if (sCurrentAlpha < 0.0f) sCurrentAlpha = 0.0f;

                // If alpha is still above 0, keep drawing
                if (sCurrentAlpha > 0.0f) {
                    ImColor clr = mLowColor.getAsImColor();
                    clr.Value.w *= sCurrentAlpha;

                    glm::vec3 boxMin(sCurrentLowPos.x, sCurrentLowPos.y + 1.01f, sCurrentLowPos.z);
                    glm::vec3 boxMax(sCurrentLowPos.x + 1.0f, sCurrentLowPos.y + 1.08f, sCurrentLowPos.z + 1.0f);
                    AABB topBox(boxMin, boxMax, true);

                    auto topPts = MathUtils::getImBoxPoints(topBox);
                    if (!topPts.empty()) {
                        ImColor fillColor(clr.Value.x, clr.Value.y, clr.Value.z, clr.Value.w * 0.4f);
                        drawList->AddConvexPolyFilled(topPts.data(), (int)topPts.size(), fillColor);
                        drawList->AddPolyline(topPts.data(), (int)topPts.size(), clr, ImDrawFlags_Closed, 1.5f);
                    }
                }
                else {
                    // Reset once fully transparent
                    sInitialized = false;
                }
            }
            return;
        }

        // We do have placements
        const BlockPos& bestPos = placementsSnapshot[0].position;
        glm::vec3 targetPos(bestPos.x, bestPos.y, bestPos.z);

        // On first run or if we just started, set up
        if (!sInitialized) {
            sCurrentLowPos = targetPos;
            sCurrentAlpha = 1.0f;  // fade in instantly
            sInitialized = true;
            sLastBestPos = bestPos;
        }
        // If the best position changed, we can optionally do something extra
        else if (bestPos != sLastBestPos) {
            // You could do a quick fade out or color change if you want an effect
            sLastBestPos = bestPos;
        }

        // Smoothly lerp from our currentLowPos to the new targetPos
        // We'll use a better approach than naive linear stepping:
        //   factor = 1 - e^(-LERP_SPEED * dt)
        // This yields a smoother exponential approach.
        {
            glm::vec3 diff = targetPos - sCurrentLowPos;
            float dist = glm::length(diff);
            if (dist > 0.0001f) {
                float factor = 1.0f - std::exp(-mLowLerpSpeed.mValue * deltaSeconds);
                sCurrentLowPos += diff * factor;
            }
        }

        // Keep alpha full while we have a valid best position
        sCurrentAlpha = 1.0f;

        // Render the top face just above the block
        ImColor clr = mLowColor.getAsImColor();
        clr.Value.w *= sCurrentAlpha;

        glm::vec3 faceMin(sCurrentLowPos.x, sCurrentLowPos.y + 1.01f, sCurrentLowPos.z);
        glm::vec3 faceMax(sCurrentLowPos.x + 1.0f, sCurrentLowPos.y + 1.08f, sCurrentLowPos.z + 1.0f);
        AABB topBox(faceMin, faceMax, true);

        auto facePts = MathUtils::getImBoxPoints(topBox);
        if (!facePts.empty()) {
            ImColor fillColor(clr.Value.x, clr.Value.y, clr.Value.z, clr.Value.w * 0.4f);
            drawList->AddConvexPolyFilled(facePts.data(), (int)facePts.size(), fillColor);
            drawList->AddPolyline(facePts.data(), (int)facePts.size(), clr, ImDrawFlags_Closed, 1.5f);
        }
    }
}
