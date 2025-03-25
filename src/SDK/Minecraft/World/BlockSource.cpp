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
 * This function samples points on the bounding box using a grid-based approach and uses ray tracing
 * to determine which sample points are unobstructed. If the viewer is inside the bounding box,
 * full visibility (1.0) is returned.
 *
 * @param viewerPos The position of the viewer.
 * @param boundingBox The axis-aligned bounding box to test.
 * @return The visibility percentage (0.0 to 1.0).
 */
float BlockSource::getSeenPercent(const glm::vec3& viewerPos, const AABB& boundingBox) {
    // Check if viewer is inside the bounding box
    if (viewerPos.x >= boundingBox.mMin.x && viewerPos.x <= boundingBox.mMax.x &&
        viewerPos.y >= boundingBox.mMin.y && viewerPos.y <= boundingBox.mMax.y &&
        viewerPos.z >= boundingBox.mMin.z && viewerPos.z <= boundingBox.mMax.z) {
        return 1.0f;
    }

    // Calculate step sizes for sampling the bounding box
    const float xSize = boundingBox.mMax.x - boundingBox.mMin.x;
    const float ySize = boundingBox.mMax.y - boundingBox.mMin.y;
    const float zSize = boundingBox.mMax.z - boundingBox.mMin.z;
    
    // Calculate step sizes for each dimension
    // We use 2 samples per dimension, resulting in 8 sample points (corners)
    const float xStep = 1.0f / (2.0f * xSize + 1.0f);
    const float yStep = 1.0f / (2.0f * ySize + 1.0f);
    const float zStep = 1.0f / (2.0f * zSize + 1.0f);
    
    // Calculate offsets to center the sample points
    const float xOffset = (1.0f - 2.0f * xStep) * 0.5f;
    const float zOffset = (1.0f - 2.0f * zStep) * 0.5f;
    
    // Validate step sizes
    if (xStep < 0.0f || yStep < 0.0f || zStep < 0.0f) {
        return 0.0f;
    }
    
    int totalPoints = 0;
    int visiblePoints = 0;
    
    // Sample the bounding box using a grid-based approach
    for (float x = 0.0f; x <= 1.0f; x += xStep) {
        for (float y = 0.0f; y <= 1.0f; y += yStep) {
            for (float z = 0.0f; z <= 1.0f; z += zStep) {
                // Calculate the sample point position
                glm::vec3 samplePoint;
                samplePoint.x = boundingBox.mMin.x + (xSize * x) + xOffset;
                samplePoint.y = boundingBox.mMin.y + (ySize * y) + 0.1f; // Add vertical offset
                samplePoint.z = boundingBox.mMin.z + (zSize * z) + zOffset;
                
                // Perform ray tracing to check visibility
                HitResult result = checkRayTrace(viewerPos, samplePoint);
                totalPoints++;
                
                // Count visible points (when ray doesn't hit anything or hits the target)
                if (result.mType == HitType::NOTHING) {
                    visiblePoints++;
                }
            }
        }
    }
    
    // Return the percentage of visible points
    return static_cast<float>(visiblePoints) / static_cast<float>(totalPoints);

}
