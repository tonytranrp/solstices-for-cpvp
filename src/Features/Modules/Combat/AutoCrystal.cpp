// ================================
// ================================
#include "AutoCrystal.hpp"

// Core SDK Includes
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/Actor/GameMode.hpp>
#include <SDK/Minecraft/World/Level.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>

// Network Includes
#include <SDK/Minecraft/Network/LoopbackPacketSender.hpp>
#include <SDK/Minecraft/Network/Packets/MovePlayerPacket.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>
#include <SDK/Minecraft/Network/Packets/RemoveActorPacket.hpp>
#include <SDK/Minecraft/Network/Packets/AddActorPacket.hpp>

// Feature System Includes
#include <Features/FeatureManager.hpp>
#include <Features/Modules/Misc/PacketMine.hpp>
#include <Features/Modules/Misc/FakePlayer.hpp>
#include <Features/Modules/Misc/Friends.hpp>

// Event Includes
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Events/BobHurtEvent.hpp>
#include <Features/Events/BoneRenderEvent.hpp>

// Utility Includes
#include <Utils/GameUtils/ActorUtils.hpp>
#include <random>

// ================================
// Constants & Helper Functions
// ================================
namespace {
    constexpr float MAX_CRYSTAL_DAMAGE = 20.0f;
    constexpr float MAX_CRYSTAL_RANGE = 12.0f;
    constexpr float MIN_VISIBILITY_THRESHOLD = 0.1f;
}

// ================================
// Module Lifecycle Management
// ================================

void AutoCrystal::onEnable() {
    // Register event listeners
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoCrystal::onBaseTickEvent, nes::event_priority::ABSOLUTE_FIRST>(this);
    gFeatureManager->mDispatcher->listen<RenderEvent, &AutoCrystal::onRenderEvent, nes::event_priority::ABSOLUTE_FIRST>(this);
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &AutoCrystal::onPacketOutEvent, nes::event_priority::ABSOLUTE_FIRST>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &AutoCrystal::onPacketInEvent, nes::event_priority::ABSOLUTE_FIRST>(this);

    // Initialize module state
    mPossiblePlacements.clear();
    mBreakTargetPos = glm::vec3(0.0f);
    mHasBreakTarget = false;
}

void AutoCrystal::onDisable() {
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoCrystal::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<RenderEvent, &AutoCrystal::onRenderEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &AutoCrystal::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &AutoCrystal::onPacketInEvent>(this);
    mPossiblePlacements.clear();
    //rots = {};
    switchBack();
    mBreakTargetPos = {};
    mHasBreakTarget = false;
    mLastTarget = nullptr;
    mRotating = false;
}

// ================================
// Damage Calculation Logic
// ================================
// Helper function to get explosion position
inline glm::vec3 getExplosionPosition(const BlockPos& crystalPos) {
    return glm::vec3(crystalPos.x + 0.5f, crystalPos.y + 1.0f, crystalPos.z + 0.5f);
}

// Helper function to get target position
inline glm::vec3 getTargetPosition(Actor* target, bool isFakePlayer) {
    if (isFakePlayer) {
        auto* fakePlayer = gFeatureManager->mModuleManager->getModule<FakePlayer>();
        return fakePlayer->mStaticPos;
    }
    return *target->getPos();
}
inline double calculateBaseDamage(float distance, float visibility) {
    double impact = (1.0 - (distance / MAX_CRYSTAL_RANGE)) * visibility;
    return ((impact * impact) * 7.0 + impact * 0.5) * 12.0 + 1.0;
}
float AutoCrystal::calculateDamage(const BlockPos& crystalPos, Actor* target, bool fakeisfakeplayer) {
    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource || (!fakeisfakeplayer && !target)) return 0.f;

    

    glm::vec3 explosionPos = getExplosionPosition(crystalPos);
    glm::vec3 targetPos = getTargetPosition(target, fakeisfakeplayer);

    // Early exit if too far
    float distance = glm::distance(explosionPos, targetPos);
    if (distance > MAX_CRYSTAL_RANGE) return 0.f;

    // Get visibility percentage
    float visibility = blockSource->getSeenPercent(explosionPos, 
        fakeisfakeplayer ? 
        gFeatureManager->mModuleManager->getModule<FakePlayer>()->aabb : 
        target->getAABB());
    if (visibility < MIN_VISIBILITY_THRESHOLD) return 0.f;

    // Optimized damage calculation
    // Helper function to calculate base damage
   

    double rawDamage = calculateBaseDamage(distance, visibility);

    // Apply armor reduction if needed
    if (!fakeisfakeplayer && target) {
        if (auto* equipment = target->getArmorContainer()) {
            double armorValue = 8.0; // Base armor value
            for (int i = 0; i < 4; i++) {
                if (auto* item = equipment->getItem(i)) {
                    armorValue += item->getEnchantValue(Enchant::PROTECTION) * 0.04 +
                                 item->getEnchantValue(Enchant::BLAST_PROTECTION) * 0.08;
                }
            }
            rawDamage *= (1.0 - (std::min(20.0, armorValue) / 25.0));
        }
    }

    return std::clamp(static_cast<float>(rawDamage), 0.0f, MAX_CRYSTAL_DAMAGE);
}
// ================================
// Placement Validation Logic
// ================================

bool AutoCrystal::canPlaceCrystal(const BlockPos& pos, const std::vector<Actor*>& runtimeActors, bool isfakeplayer) {
    BlockSource* bs = ClientInstance::get()->getBlockSource();
    if (!bs) return false;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return false;

    const Block* baseBlock = bs->getBlock(pos);
    if (!baseBlock) return false;
    int baseId = baseBlock->mLegacy->getBlockId();
    if (baseId != 49 && baseId != 7) {
        //spdlog::info("[AutoCrystal] Invalid base block at ({}, {}, {}), ID: {}", pos.x, pos.y, pos.z, baseId);
        return false;
    }

    // Check if placement is only +1 above target's Y position
    Actor* targetActor = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    glm::vec3 playerPos = *player->getPos();
    
    for (auto* actor : runtimeActors) {
        if (actor == player || !actor->isValid() || !actor->isPlayer()) continue;
        if (Friends::isFriend(actor->getNameTag())) continue;
        float dist = glm::distance(*actor->getPos(), playerPos);
        if (dist < closestDist) {
            closestDist = dist;
            targetActor = actor;
        }
    }

    if (targetActor) {
        float targetY = targetActor->getPos()->y;
        if (pos.y > floor(targetY) + 1) {
            //spdlog::info("[AutoCrystal] Position too high above target: crystal Y={}, target Y={}", pos.y, targetY);
            return false;
        }
    }

    // Define crystal AABB with proper hitbox dimensions
    AABB placeAABB(
        glm::vec3(pos.x + 0.15f, pos.y + 1.f, pos.z + 0.15f),  // Slightly inset the min bounds
        glm::vec3(pos.x + 0.85f, pos.y + 2.f, pos.z + 0.85f),  // Slightly inset the max bounds
        true
    );

    auto* fakeplayer = gFeatureManager->mModuleManager->getModule<FakePlayer>();
    // Iterate over the runtime actors with improved collision checks
    for (auto* entity : runtimeActors) {
        if (!entity->isValid()) continue;
        
        // Skip crystal entities
        if (entity->getActorTypeComponent()->mType == ActorType::EnderCrystal)
            continue;

        AABB entityAABB;
        if (isfakeplayer) {
            entityAABB = fakeplayer->aabb;
        } else {
            entityAABB = entity->getAABB();
        }

        // Apply a smaller margin for more precise collision detection
        if (entity->getActorTypeComponent()->mType == ActorType::idkallentities) {
            entityAABB.mMin = entityAABB.mMin - glm::vec3(0.05f, 0.f, 0.05f);
            entityAABB.mMax = entityAABB.mMax + glm::vec3(0.05f, 0.f, 0.05f);
        }

        // Debug output for AABB checks
        //spdlog::info("[AutoCrystal] Entity AABB check: min({}, {}, {}), max({}, {}, {})",
           // entityAABB.mMin.x, entityAABB.mMin.y, entityAABB.mMin.z,
           // entityAABB.mMax.x, entityAABB.mMax.y, entityAABB.mMax.z);

        // Check intersection with adjusted hitboxes
        bool intersects = placeAABB.intersects(entityAABB);
        //spdlog::info("[AutoCrystal] AABB Intersection result: {}", intersects);

        if (intersects)
            return false;
    }
    return true;
}
std::vector<AutoCrystal::PlacePosition> AutoCrystal::findPlacePositions(const std::vector<Actor*>& runtimeActors, bool isfakeplayer) {
    std::vector<PlacePosition> bestCandidates;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return bestCandidates;

    auto* bs = ClientInstance::get()->getBlockSource();
    if (!bs)
        return bestCandidates;

    BlockPos targetPos{};
    Actor* targetActor = nullptr;
    int range = static_cast<int>(mPlaceRange.mValue);
    glm::vec3 playerPos = *player->getPos();
    auto* fakeplayer = gFeatureManager->mModuleManager->getModule<FakePlayer>();
    
    // Determine target position and actor
    if (fakeplayer->mEnabled && fakeplayer) {
        targetPos = BlockPos(fakeplayer->mStaticPos);
    } else {
        float bestDist = std::numeric_limits<float>::max();
        for (auto* actor : runtimeActors) {
            if (actor == player || !actor->isValid() || !actor->isPlayer())
                continue;
            if (Friends::isFriend(actor->getNameTag()))
                continue;
            float dist = glm::distance(*actor->getPos(), playerPos);
            if (dist <= mRange.mValue && dist < bestDist) {
                bestDist = dist;
                targetActor = actor;
            }
        }
        if (!targetActor)
            return bestCandidates;
        targetPos = *targetActor->getPos();
    }

    // Store all valid positions with their scores
    std::vector<PlacePosition> allCandidates;
    allCandidates.reserve(range * range * 7); // Pre-allocate for efficiency

    // Search in a more thorough pattern, starting from bottom to top
    for (int y = -5; y <= 1; y++) {
        for (int x = -range; x <= range; x++) {
            for (int z = -range; z <= range; z++) {
                BlockPos checkPos = targetPos + BlockPos(x, y, z);
                glm::vec3 checkCenter(checkPos.x + 0.5f, checkPos.y + 0.5f, checkPos.z + 0.5f);
                glm::vec3 targetVec3(targetPos.x, targetPos.y, targetPos.z);

                // Basic range checks
                float distToTarget = glm::distance(checkCenter, targetVec3);
                float distToPlayer = glm::distance(checkCenter, playerPos);
                if (distToTarget > mPlaceRange.mValue || distToPlayer > mPlaceRangePlayer.mValue)
                    continue;

                // Validate crystal placement
                if (!canPlaceCrystal(checkPos, runtimeActors, isfakeplayer))
                    continue;

                // Calculate damage
                float targetDamage = calculateDamage(checkPos, targetActor, isfakeplayer);
                if (targetDamage < mMinimumDamage.mValue)
                    continue;

                // Calculate vertical distance from target's feet
                float verticalDistFromFeet = std::abs(checkPos.y - targetPos.y);
                
                // Calculate a weighted score that prioritizes:
                // 1. Higher damage (primary factor)
                // 2. Closer to target's feet (secondary factor)
                // 3. Closer to target horizontally (tertiary factor)
                float damageWeight = 1.0f;
                float verticalWeight = 0.3f;
                float horizontalWeight = 0.2f;

                float horizontalDist = std::sqrt(
                    std::pow(checkPos.x - targetPos.x, 2) +
                    std::pow(checkPos.z - targetPos.z, 2)
                );

                float score = (targetDamage * damageWeight) -
                             (verticalDistFromFeet * verticalWeight) -
                             (horizontalDist * horizontalWeight);

                allCandidates.emplace_back(checkPos, score, targetDamage);
            }
        }
    }

    // Sort all candidates by their weighted score
    std::sort(allCandidates.begin(), allCandidates.end(),
        [](const PlacePosition& a, const PlacePosition& b) {
            return a.targetDamage > b.targetDamage;
        });

    // Take the top candidates (up to 5)
    size_t numToTake = std::min(allCandidates.size(), size_t(5));
    bestCandidates.insert(bestCandidates.end(),
        allCandidates.begin(),
        allCandidates.begin() + numToTake);

    return bestCandidates;
}
std::vector<AutoCrystal::BreakTarget> AutoCrystal::findBreakTargets(const std::vector<Actor*>& runtimeActors, bool isfakeplayer) {
    std::vector<BreakTarget> breakTargets;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return breakTargets;

    // First, try to find a valid enemy target.
    Actor* targetActor = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    glm::vec3 playerPos = *player->getPos();
    for (auto* actor : runtimeActors) {
        // Filter out invalid players and friends.
        if (actor == player || !actor->isValid() || !actor->isPlayer()) continue;
        if (Friends::isFriend(actor->getNameTag())) continue;
        float dist = glm::distance(*actor->getPos(), playerPos);
        if (dist > mRange.mValue) continue;
        if (dist < closestDist) {
            closestDist = dist;
            targetActor = actor;
        }
    }
   
   

    // Now iterate over runtime actors to find valid crystals.
    for (auto* actor : runtimeActors) {
        if (!actor->isValid() || actor->getActorTypeComponent()->mType != ActorType::EnderCrystal)
            continue;
        float dist = glm::distance(*actor->getPos(), playerPos);
        if (dist > mRange.mValue)
            continue;
        // Calculate damage values; for demonstration we use calculateDamage.
        float targetDmg = calculateDamage(BlockPos(actor->getPos()->x, actor->getPos()->y, actor->getPos()->z), player, isfakeplayer);
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
// ================================
// Inventory Management Logic
// ================================

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

// ================================
// Crystal Placement Logic
// ================================

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
// ================================
// Crystal Breaking Logic
// ================================

void AutoCrystal::breakCrystal(Actor* crystal)
{
    if (!crystal) return;

    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player || !player->getGameMode()) return;

    float distance = glm::distance(*crystal->getPos(), *player->getPos());
    if (distance > mPlaceRange.mValue)
        return;

    // Record break target rotation information.
    mBreakTargetPos = *crystal->getPos();
    mHasBreakTarget = true;
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
        // Reset break target after a successful break.
        mHasBreakTarget = false;
        mBreakTargetPos = glm::vec3(0.0f);
        return;
    }

    // Predicted break
    mShouldIdPredict = true;
    mLastAttackId = crystal->getActorUniqueIDComponent()->mUniqueID;

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
    // Reset break target after breaking.
    mHasBreakTarget = false;
    mBreakTargetPos = glm::vec3(0.0f);
}

std::vector<AutoCrystal::PlacePosition> AutoCrystal::getplacmenet(const std::vector<Actor*>& runtimeActors, bool isfakeplayer) {
    std::vector<AutoCrystal::PlacePosition> placementpos;
    if (NOW - mLastsearchPlace < mPlaceSearchDelay.mValue) return placementpos;
    placementpos = findPlacePositions(runtimeActors, isfakeplayer);
    return placementpos;
    mLastsearchPlace = NOW;
}
void AutoCrystal::onBaseTickEvent(BaseTickEvent& event) {
    // If we are still waiting for our delay, decrement and do nothing.
    if (mActionDelayTicks > 0) {
        mActionDelayTicks--;
        return;
    }

    if (!mAutoPlace.mValue) return;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;
    auto* level = player->getLevel();
    if (!level) return;

    // Check if PacketMine is active using QueueManager. If so, reset AutoCrystal state and set delay.
    auto* packetMine = gFeatureManager->mModuleManager->getModule<PacketMine>();
    auto* fakeplayer = gFeatureManager->mModuleManager->getModule<FakePlayer>();
    
    // Check mining state through QueueManager instead of direct method call
    if (packetMine && QueueManager::get()->getState<QueueState::Mining>(packetMine, QueueState::Mining::Idle) != QueueState::Mining::Idle) {
        ChatUtils::displayClientMessage("AutoCrystal: Waiting, PacketMine is active.");
        mPossiblePlacements.clear();
        mBreakTargetPos = glm::vec3(); // reset to zero vector
        mHasBreakTarget = false;
        mLastTarget = nullptr;
        mRotating = false;
        mActionDelayTicks = 2;
        return;
    }
   
    // Grab the runtime actor list once.
    const auto& runtimeActors = level->getRuntimeActorList();

    switchToCrystal();

    // Get potential placements using the pre-obtained runtime list.
    auto placements = getplacmenet(runtimeActors, fakeplayer->mEnabled);
    mPossiblePlacements = placements;
    if (!placements.empty()) {
        // Place crystal at best candidate.
        placeCrystal(placements[0]);
    }

    // Process break targets using the same runtime list.
    auto breakTargets = findBreakTargets(runtimeActors, fakeplayer->mEnabled);
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

    // Handle crystal placement inventory transactions.
    if (event.mPacket->getId() == PacketID::InventoryTransaction) {
        auto pkt = event.getPacket<InventoryTransactionPacket>();
        if (!pkt || !pkt->mTransaction) return;

        if (pkt->mTransaction->type == ComplexInventoryTransaction::Type::ItemUseTransaction) {
            auto* transac = reinterpret_cast<ItemUseInventoryTransaction*>(pkt->mTransaction.get());
            if (transac->mActionType == ItemUseInventoryTransaction::ActionType::Place) {
                if (!mPossiblePlacements.empty()) {
                    transac->mClickPos = mPossiblePlacements[0].position;
                }
                else {
                    transac->mClickPos = BlockUtils::clickPosOffsets[transac->mFace];
                }

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

        // Only proceed if we have at least one valid target (placement or break).
        if (mPossiblePlacements.empty() && !mHasBreakTarget) return;

        glm::vec3 targetPos;
        if (!mPossiblePlacements.empty() && mHasBreakTarget) {
            // Average the positions for a combined rotation.
            targetPos = (glm::vec3(mPossiblePlacements[0].position) + mBreakTargetPos) * 0.5f;
        }
        else if (!mPossiblePlacements.empty()) {
            targetPos = glm::vec3(mPossiblePlacements[0].position);
        }
        else {
            targetPos = mBreakTargetPos;
        }

        auto rots = MathUtils::getRots(*player->getPos(), targetPos);
        pkt->mRot = rots;
        pkt->mYHeadRot = rots.y;
    }
}
void AutoCrystal::onRenderEvent(RenderEvent& event) {
    // If visualizations are disabled, there's nothing to do
    if (!mVisualizePlace.mValue) return;

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
        currentPosSet.reserve(mPossiblePlacements.size());
        for (auto& place : mPossiblePlacements) {
            currentPosSet.insert(place.position);
        }

        // Step 2: For any newly visible positions, ensure they exist in our fade map
        for (auto& place : mPossiblePlacements) {
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
        if (mPossiblePlacements.empty()) return;

        // Take the best candidate as an example
        const BlockPos& bestPos = mPossiblePlacements[0].position;

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
        if (mPossiblePlacements.empty())
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
        const BlockPos& bestPos = mPossiblePlacements[0].position;
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
