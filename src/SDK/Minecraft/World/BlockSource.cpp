//
// Created by vastrakai on 7/3/2024.
//

#include "BlockSource.hpp"

#include <memory>
#include <SDK/OffsetProvider.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Network/MinecraftPackets.hpp>
#include <SDK/Minecraft/Network/Packets/UpdateBlockPacket.hpp>
#include <Utils/GameUtils/PacketUtils.hpp>
#include "BlockLegacy.hpp"
#include "HitResult.hpp"
#include "Chunk/LevelChunk.hpp"

LevelChunk* BlockSource::getChunk(ChunkPos const& pos)
{
    static int index = OffsetProvider::BlockSource_getChunk;
    return MemUtils::callVirtualFunc<LevelChunk*, ChunkPos const&>(index, this, pos);
}

void BlockSource::setBlock(BlockPos const& pos, Block* block)
{
    //virtual bool BlockSource::setBlock(class BlockPos const &,class Block const &,int,struct ActorBlockSyncMessage const *,class Actor *)
    static int index = OffsetProvider::BlockSource_setBlock;
    auto player = ClientInstance::get()->getLocalPlayer();
    static ActorBlockSyncMessage syncMessage;
    syncMessage.mEntityUniqueID = player->getActorUniqueIDComponent()->mUniqueID;
    syncMessage.mMessage = ActorBlockSyncMessage::MessageId::CREATE;
    MemUtils::callVirtualFunc<void, BlockPos const&, Block*, int, ActorBlockSyncMessage const*, Actor*>(index, this, pos, block, 0, &syncMessage, player);
}


HitResult BlockSource::clip(glm::vec3 start, glm::vec3 end, bool checkAgainstLiquid, ShapeType shapeType, int range, bool ignoreBorderBlocks, bool fullOnly, void* player)
{
    // BlockSource::clip(struct IConstBlockSource *, HitResult *this, Vec3 *this, Vec3 *this)
    using clipBlock = class HitResult *(__fastcall *)(class BlockSource * blockSource, class HitResult * rayTrace, glm::vec3 & start, glm::vec3 & end, bool checkAgainstLiquid, ShapeType, int range, bool ignoreBorderBlocks, bool fullOnly, void *player, uintptr_t **function);
    static clipBlock clip_f = nullptr;
    if (clip_f == nullptr)
    {
        int index = OffsetProvider::BlockSource_clip;
        uintptr_t func = reinterpret_cast<uintptr_t>(mVfTable[index]);
        clip_f = reinterpret_cast<clipBlock>(func);
    }

    static uintptr_t **checkBlocks = nullptr;
    if (checkBlocks == nullptr) {
        uintptr_t sigOffset = SigManager::checkBlocks;
        checkBlocks = reinterpret_cast<uintptr_t **>(sigOffset + *reinterpret_cast<int *>(sigOffset + 3) + 7);
    }
    HitResult _temp;
    return *clip_f(this, &_temp,  start, end, checkAgainstLiquid, shapeType, range, ignoreBorderBlocks, fullOnly, player, checkBlocks);
}

HitResult BlockSource::checkRayTrace(glm::vec3 start, glm::vec3 end, void* player)
{
    return clip(start, end, false, ShapeType::All, 200, false, false, player);
}
/**
 * @brief Computes the percentage of a bounding box that is visible from a given viewer position.
 *
 * This function samples the eight corners of the bounding box (with a slight vertical offset)
 * and uses ray tracing to determine which sample points are unobstructed. If the viewer is inside
 * the bounding box, full visibility (1.0) is returned.
 *
 * @param viewerPos The position of the viewer.
 * @param boundingBox The axis-aligned bounding box to test.
 * @return The visibility percentage (0.0 to 1.0).
 */
float BlockSource::getSeenPercent(const glm::vec3& viewerPos, const AABB& boundingBox) {
    // If the viewer is inside the bounding box, return full visibility.
    if (viewerPos.x >= boundingBox.mMin.x && viewerPos.x <= boundingBox.mMax.x &&
        viewerPos.y >= boundingBox.mMin.y && viewerPos.y <= boundingBox.mMax.y &&
        viewerPos.z >= boundingBox.mMin.z && viewerPos.z <= boundingBox.mMax.z) {
        return 1.0f;
    }

    // For a fixed sampleCount of 2, the eight sample points are the corners.
    const int totalPoints = 8;
    int visiblePoints = 0;

    const glm::vec3 corners[totalPoints] = {
        {boundingBox.mMin.x, boundingBox.mMin.y, boundingBox.mMin.z},
        {boundingBox.mMin.x, boundingBox.mMin.y, boundingBox.mMax.z},
        {boundingBox.mMin.x, boundingBox.mMax.y, boundingBox.mMin.z},
        {boundingBox.mMin.x, boundingBox.mMax.y, boundingBox.mMax.z},
        {boundingBox.mMax.x, boundingBox.mMin.y, boundingBox.mMin.z},
        {boundingBox.mMax.x, boundingBox.mMin.y, boundingBox.mMax.z},
        {boundingBox.mMax.x, boundingBox.mMax.y, boundingBox.mMin.z},
        {boundingBox.mMax.x, boundingBox.mMax.y, boundingBox.mMax.z}
    };

    // Apply a slight vertical offset to each sample point and perform ray tracing.
    for (int i = 0; i < totalPoints; ++i) {
        glm::vec3 samplePoint = corners[i];
        samplePoint.y += 0.1f; // vertical offset
        if (checkRayTrace(viewerPos, samplePoint).mType == HitType::NOTHING) {
            ++visiblePoints;
        }
    }
    return static_cast<float>(visiblePoints) / totalPoints;
}
