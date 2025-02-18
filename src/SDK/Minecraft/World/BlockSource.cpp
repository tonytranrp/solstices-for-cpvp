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
float BlockSource::getSeenPercent(const glm::vec3& viewerPos, const AABB& boundingBox) {
    if (boundingBox.mMin.x <= viewerPos.x && viewerPos.x <= boundingBox.mMax.x &&
        boundingBox.mMin.y <= viewerPos.y && viewerPos.y <= boundingBox.mMax.y &&
        boundingBox.mMin.z <= viewerPos.z && viewerPos.z <= boundingBox.mMax.z) {
        return 1.0f;
    }

    float xSize = boundingBox.mMax.x - boundingBox.mMin.x;
    float ySize = boundingBox.mMax.y - boundingBox.mMin.y;
    float zSize = boundingBox.mMax.z - boundingBox.mMin.z;

    float xInc = 1.0f / ((xSize + xSize) + 1.0f);
    float yInc = 1.0f / ((ySize + ySize) + 1.0f);
    float zInc = 1.0f / ((zSize + zSize) + 1.0f);

    if (xInc < 0.0f || yInc < 0.0f || zInc < 0.0f) {
        return 0.0f;
    }

    int visiblePoints = 0;
    int totalPoints = 0;

    for (float x = 0.0f; x <= 1.0f; x += xInc) {
        for (float y = 0.0f; y <= 1.0f; y += yInc) {
            for (float z = 0.0f; z <= 1.0f; z += zInc) {
                glm::vec3 samplePoint(
                    boundingBox.mMin.x + (boundingBox.mMax.x - boundingBox.mMin.x) * x,
                    boundingBox.mMin.y + (boundingBox.mMax.y - boundingBox.mMin.y) * y + 0.1f,
                    boundingBox.mMin.z + (boundingBox.mMax.z - boundingBox.mMin.z) * z
                );

                auto result = checkRayTrace(viewerPos, samplePoint);
                totalPoints++;

                if (result.mType == HitType::NOTHING) {
                    visiblePoints++;
                }
            }
        }
    }

    return static_cast<float>(visiblePoints) / static_cast<float>(totalPoints);
}
