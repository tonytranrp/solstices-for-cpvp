//
// Created by vastrakai on 11/8/2024.
//

#include "InfiniteAura.hpp"

#include <Features/Modules/Movement/AutoPath.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Network/MinecraftPackets.hpp>
#include <SDK/Minecraft/Network/Packets/MovePlayerPacket.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <Hook/Hooks/NetworkHooks/PacketSendHook.hpp>
#include <SDK/Minecraft/World/HitResult.hpp>
#include <SDK/Minecraft/Network/MinecraftPackets.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Network/LoopbackPacketSender.hpp>

void InfiniteAura::onEnable()
{
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &InfiniteAura::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketOutEvent, &InfiniteAura::onPacketOutEvent, nes::event_priority::ABSOLUTE_LAST>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &InfiniteAura::onPacketInEvent>(this);
    gFeatureManager->mDispatcher->listen<RenderEvent, &InfiniteAura::onRenderEvent>(this);
}

void InfiniteAura::onDisable()
{
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &InfiniteAura::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketOutEvent, &InfiniteAura::onPacketOutEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &InfiniteAura::onPacketInEvent>(this);
    gFeatureManager->mDispatcher->deafen<RenderEvent, &InfiniteAura::onRenderEvent>(this);
    mHasTarget = false;
    mNeedsToPathBack = false;
}

std::shared_ptr<MovePlayerPacket> InfiniteAura::createPacketForPos(glm::vec3 pos)
{
    auto player = ClientInstance::get()->getLocalPlayer();
    auto packet = MinecraftPackets::createPacket<MovePlayerPacket>();
    packet->mPos = pos;
    packet->mPlayerID = player->getRuntimeID();
    packet->mRot = { mRots.x, mRots.y };
    packet->mYHeadRot = mRots.z;
    packet->mResetPosition = PositionMode::Teleport;
    packet->mOnGround = true;
    packet->mRidingID = -1;
    packet->mCause = TeleportationCause::Unknown;
    packet->mSourceEntityType = ActorType::Player;
    packet->mTick = 0;

    return packet;
}
struct HashVec3 {
    size_t operator()(const glm::vec3& v) const {
        return std::hash<float>()(v.x) ^ std::hash<float>()(v.y) ^ std::hash<float>()(v.z);
    }
};
std::vector<std::shared_ptr<PlayerAuthInputPacket>> InfiniteAura::pathToPos(glm::vec3 from, glm::vec3 to) {
    auto ci = ClientInstance::get();
    if (!ci || !ci->getBlockSource()) return {};

    struct Node {
        glm::vec3 pos;
        float g, h, f;
        Node* parent;

        Node(glm::vec3 p, float g_, float h_, Node* par) : pos(p), g(g_), h(h_), f(g_ + h_), parent(par) {}
        bool operator<(const Node& other) const { return f > other.f; }
    };

    const float stepSize = mBlocksPerPacket.mValue;
    std::vector<std::shared_ptr<PlayerAuthInputPacket>> packets;
    std::priority_queue<Node> openSet;
    std::unordered_map<glm::vec3, bool, HashVec3> visited;
    std::vector<glm::vec3> directions = {
        {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1},
        {1,1,0}, {-1,1,0}, {1,-1,0}, {-1,-1,0},
        {1,0,1}, {-1,0,1}, {1,0,-1}, {-1,0,-1}
    };

    openSet.push(Node(from, 0, glm::distance(from, to), nullptr));

    while (!openSet.empty()) {
        Node current = openSet.top();
        openSet.pop();

        if (glm::distance(current.pos, to) < stepSize) {
            // Reconstruct path
            std::vector<glm::vec3> path;
            Node* curr = &current;
            while (curr) {
                path.push_back(curr->pos);
                curr = curr->parent;
            }
            std::reverse(path.begin(), path.end());

            // Create packets with proper spacing
            for (size_t i = 0; i < path.size(); i++) {
                auto pkt = MinecraftPackets::createPacket<PlayerAuthInputPacket>();
                pkt->mPos = path[i];
                pkt->mInputData = AuthInputAction::NONE;
                pkt->mRot = MathUtils::getRots(path[i], to);
                pkt->mYHeadRot = pkt->mRot.y;
                packets.push_back(pkt);
            }

            mPacketPositions = path;
            mLastPathTime = NOW;
            return packets;
        }

        if (visited[current.pos]) continue;
        visited[current.pos] = true;

        for (const auto& dir : directions) {
            glm::vec3 newPos = current.pos + dir * stepSize;
            if (visited[newPos]) continue;

            // Check for collisions
            if (ci->getBlockSource()->getBlock(newPos)->mLegacy->mSolid) continue;

            float g = current.g + glm::distance(current.pos, newPos);
            float h = glm::distance(newPos, to);
            openSet.push(Node(newPos, g, h, new Node(current)));
        }
    }

    return packets;
}


void InfiniteAura::onBaseTickEvent(BaseTickEvent& event) {
    auto player = event.mActor;
    if (!player) return;

    if (NOW - mLastAttack < 1000.0f / mAPS.mValue) return;

    auto actors = ActorUtils::getActorList(false, true); // Changed to include mobs
    if (actors.empty()) return;

    std::sort(actors.begin(), actors.end(), [&](Actor* a, Actor* b) {
        return mMode.mValue == Mode::Switch ? mLastHits[a] < mLastHits[b] :
            a->distanceTo(player) < b->distanceTo(player);
        });

    bool foundTarget = false;
    for (auto* target : actors) {
        if (!target || !target->isValid() || target == player ||
            target->distanceTo(player) > mRange.mValue) continue;

      

        if (mRayTrace.mValue) {
            auto bs = ClientInstance::get()->getBlockSource();
            auto hitResult = bs->checkRayTrace(*player->getPos(), *target->getPos() - PLAYER_HEIGHT_VEC, player);
            if (hitResult.mType == HitType::BLOCK) continue;
        }

        // Create silent position packet
        auto pkt = MinecraftPackets::createPacket<PlayerAuthInputPacket>();
        glm::vec3 targetPos = *target->getPos();

        // Calculate position slightly behind target
        float yaw = target->getActorRotationComponent()->mYaw * PI / 180.0f;
        glm::vec3 attackPos = targetPos - glm::vec3(
            cos(yaw) * 3.0f,
            0.0f,
            sin(yaw) * 3.0f
        );

        // Send position update
        if (NOW - mLastTeleport >= TELEPORT_DELAY) {
            pkt->mPos = attackPos;
            pkt->mRot = MathUtils::getRots(*player->getPos(), targetPos);
            pkt->mYHeadRot = pkt->mRot.y;
            pkt->mInputData = AuthInputAction::NONE;
            pkt->mPosDelta = attackPos - *player->getPos();
            PacketUtils::queueSend(pkt, false);
            mServerPos = attackPos;
            mLastTeleport = NOW;
        }

        // Attack
        auto attackPkt = ActorUtils::createAttackTransaction(target, player->getSupplies()->mSelectedSlot);
        PacketUtils::queueSend(attackPkt, false);

        // Reset position silently 
        auto resetPkt = MinecraftPackets::createPacket<PlayerAuthInputPacket>();
        resetPkt->mPos = *player->getPos();
        resetPkt->mRot = pkt->mRot;
        resetPkt->mYHeadRot = pkt->mYHeadRot;
        resetPkt->mInputData = AuthInputAction::NONE;
        PacketUtils::queueSend(resetPkt, false);

        player->swing();
        mLastHits[target] = NOW;
        mLastAttack = NOW;
        mLastTarget = target;
        foundTarget = true;

        if (mMode.mValue == Mode::Single) break;
    }

    mHasTarget = foundTarget;
}

void InfiniteAura::onPacketOutEvent(PacketOutEvent& event) {
    if (event.mPacket->getId() == PacketID::PlayerAuthInput) {
        auto pkt = event.getPacket<PlayerAuthInputPacket>();
        if (!mLastTarget) return;

        // Cancel original movement packets while attacking
        if (NOW - mLastAttack < 200) {
            event.cancel();
        }
    }
}


void InfiniteAura::onPacketInEvent(PacketInEvent& event)
{
    if (event.mPacket->getId() == PacketID::MovePlayer && mSilentAccept.mValue)
    {
        auto player = ClientInstance::get()->getLocalPlayer();
        if (!player) return;

        auto packet = event.getPacket<MovePlayerPacket>();
        //if (packet->mResetPosition != PositionMode::Teleport) return;
        if (packet->mPlayerID != player->getRuntimeID()) return;

        // reset packet and cancel event
        event.cancel();
        ClientInstance::get()->getPacketSender()->sendToServer(packet.get());
        ChatUtils::displayClientMessage("attempting to silently accept teleport [mResetPosition: {}]", std::string(magic_enum::enum_name(packet->mResetPosition)));
    }
}
void InfiniteAura::onRenderEvent(RenderEvent& event) {
    if (!mLastTarget) return;

    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    auto drawList = ImGui::GetBackgroundDrawList();

    if (mRenderMode.mValue == RenderMode::Lines) {
        ImVec2 start, end;
        if (RenderUtils::worldToScreen(mServerPos, start) &&
            RenderUtils::worldToScreen(*mLastTarget->getPos(), end)) {
            drawList->AddLine(start, end, IM_COL32(255, 0, 0, 255), 2.0f);
        }
    }
    else {
        AABB box(mServerPos, mServerPos + glm::vec3(1.0f));
        auto points = MathUtils::getImBoxPoints(box);
        drawList->AddConvexPolyFilled(points.data(), points.size(), IM_COL32(255, 0, 0, 100));
        drawList->AddPolyline(points.data(), points.size(), IM_COL32(255, 0, 0, 255), true, 2.0f);
    }
}