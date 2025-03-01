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
std::vector<AutoCrystal::PlacePosition> AutoCrystal::findPlacePositions(const std::vector<Actor*>& runtimeActors) {
    std::vector<PlacePosition> positions;
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player)
        return positions;

    // 🧭 Find the closest valid enemy actor within mRange
    Actor* targetActor = nullptr;
    float closestDist = std::numeric_limits<float>::max();
    glm::vec3 playerPos = *player->getPos();  // cache player position to avoid repeated calls
    for (auto* actor : runtimeActors) {
        // Skip self, invalid actors, non-players, and friends
        if (actor == player || !actor->isValid() || !actor->isPlayer()) continue;
        if (Friends::isFriend(actor->getNameTag())) continue;
        float dist = glm::distance(*actor->getPos(), playerPos);
        if (dist > mRange.mValue) continue;  // outside overall target range
        if (dist < closestDist) {
            closestDist = dist;
            targetActor = actor;
        }
    }
    if (!targetActor) {
        return positions;  // no target found
    }

    // Use target actor's position as center of search area
    BlockPos targetPos = *targetActor->getPos();
    int range = static_cast<int>(mPlaceRange.mValue);       // search radius around target
    const int yMin = -5, yMax = 1;                          // vertical search limits (relative to target)

    // 📌 Precompute candidate offsets sorted by distance (cache between calls)
    struct Offset { int x, y, z; float sqrDist; };
    static std::vector<Offset> sortedOffsets;
    static int lastRange = -1;
    if (range != lastRange || sortedOffsets.empty()) {
        sortedOffsets.clear();
        for (int dx = -range; dx <= range; ++dx) {
            for (int dy = yMin; dy <= yMax; ++dy) {
                for (int dz = -range; dz <= range; ++dz) {
                    float sqDist = static_cast<float>(dx * dx + dy * dy + dz * dz);
                    sortedOffsets.push_back({ dx, dy, dz, sqDist });
                }
            }
        }
        std::sort(sortedOffsets.begin(), sortedOffsets.end(), [](const Offset& a, const Offset& b) {
            return a.sqrDist < b.sqrDist;
            });
        lastRange = range;
    }

    // 🎯 Search outward from target for the best placement
    PlacePosition bestCandidate;
    bool foundCandidate = false;
    float bestRingDist = 0.0f;
    glm::vec3 targetCenter = *targetActor->getPos();  // exact target position for distance calc
    for (const Offset& off : sortedOffsets) {
        // If a candidate was found in a nearer ring, stop when we move to the next ring
        if (foundCandidate && off.sqrDist > bestRingDist) break;
        if (!foundCandidate) {
            bestRingDist = off.sqrDist; // initialize search ring distance
        }

        // Compute the absolute world position for this offset
        BlockPos checkPos = targetPos + BlockPos(off.x, off.y, off.z);
        // Calculate the crystal's center point at this position (for range checks)
        glm::vec3 crystalCenter(checkPos.x + 0.5f, checkPos.y + 1.0f, checkPos.z + 0.5f);

        // 📏 Basic range filtering (target and player range)
        if (glm::distance(crystalCenter, targetCenter) > mPlaceRange.mValue) continue;        // too far from target
        if (glm::distance(crystalCenter, playerPos) > mPlaceRangePlayer.mValue) continue;     // out of player's reach

        // 💎 Fast validity check for crystal placement (base block, air space, no entity blocking)
        if (!canPlaceCrystal(checkPos, runtimeActors)) continue;

        // 💥 Compute expected damage at this position
        float targetDamage = calculateDamage(checkPos, targetActor);
        if (targetDamage < mMinimumDamage.mValue) continue;  // skip if damage is below threshold

        // ⭐ If this is the first valid spot or has higher damage than the current best, select it
        if (!foundCandidate || targetDamage > bestCandidate.targetDamage) {
            bestCandidate = PlacePosition(checkPos, targetDamage, 0.0f);
            foundCandidate = true;
            bestRingDist = off.sqrDist;  // lock the search to this distance (current ring)
        }
    }

    // If we found at least one valid placement, return it
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
