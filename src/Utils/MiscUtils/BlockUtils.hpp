#pragma once
//
// Created by vastrakai on 7/7/2024.
//
#include <map>
#include <Utils/Structs.hpp>
#include <SDK/Minecraft/World/Block.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/World/Chunk/SubChunkBlockStorage.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <vector>



struct BlockInfo {
    Block* mBlock;
    glm::ivec3 mPosition;

    AABB getAABB() {
        return AABB(mPosition, glm::vec3(1, 1, 1));
    }

    float getDistance(glm::vec3 pos) {
        glm::vec3 closest = getAABB().getClosestPoint(pos);
        return glm::distance(closest, pos);
    }

    BlockInfo(Block* block, glm::ivec3 position) : mBlock(block), mPosition(position) {}
};

struct DestroySpeedInfo {
    std::string blockName;
    float destroySpeed;
};

class BlockUtils {
public:
    static inline std::map<int, glm::vec3> clickPosOffsets = {
        {0, {0.5, -0, 0.5}},
        {1, {0.5, 1, 0.5}},
        {2, {0.5, 0.5, 0}},
        {3, {0.5, 0.5, 1}},
        {4, {0, 0.5, 0.5}},
        {5, {1, 0.5, 0.5}},
    };
    static std::vector<BlockInfo> getBlockList(const glm::ivec3& position, float r);
    static bool isOverVoid(glm::vec3 vec);
    static glm::vec3 findClosestBlockToPos(glm::vec3 pos);
    //static std::vector<BlockInfo> getChunkBasedBlockList(const glm::ivec3& position, float r);

    /*if (!IsAirBlock(pos + BlockPos(0, -1, 0))) return 1;
if (!IsAirBlock(pos + BlockPos(0, 0, 1))) return 2;
if (!IsAirBlock(pos + BlockPos(0, 0, -1))) return 3;
if (!IsAirBlock(pos + BlockPos(1, 0, 0))) return 4;
if (!IsAirBlock(pos + BlockPos(-1, 0, 0))) return 5;
if (!IsAirBlock(pos + BlockPos(0, 1, 0))) return 0;*/
    static inline std::map<int, glm::vec3> blockFaceOffsets = {
        {1, glm::ivec3(0, -1, 0)},
        {2, glm::ivec3(0, 0, 1)},
        {3, glm::ivec3(0, 0, -1)},
        {4, glm::ivec3(1, 0, 0)},
        {5, glm::ivec3(-1, 0, 0)},
        {0, glm::ivec3(0, 1, 0)}
    };

    // Dynamic Destroy Speed
    static inline std::vector<DestroySpeedInfo> mDynamicSpeeds = { // make sure to include minecraft: before block names
        {"minecraft:green_concrete", 0.65f},
        {"minecraft:lime_terracotta", 0.65f},
        {"minecraft:sand", 0.67f},
        {"minecraft:dirt", 0.65f},
        {"minecraft:grass_block", 0.67f},
        {"minecraft:stone", 0.67f},
        {"minecraft:sandstone", 0.57f},
        {"minecraft:sandstone_slab", 0.57f},
        {"minecraft:moss_block", 0.57f},
    };

    // Dynamic Destroy Speed 2
    static inline std::vector<DestroySpeedInfo> mNukeSpeeds = { // make sure to include minecraft: before block names
        {"minecraft:sand", 0.24f},
        {"minecraft:dirt", 0.24f},
    };

    static int getBlockPlaceFace(glm::ivec3 blockPos);
    static int getExposedFace(glm::ivec3 blockPos);
    static int getExposedHeight(glm::ivec3 blockPos,int HeightAmount = 2)
    {
        int height = 0;
        for (int i = 1; i <= HeightAmount; i++) {
            if (isAirBlock(blockPos + glm::ivec3(0, i, 0))) {
                height++;
            }
            else {
                break;
            }
        }
        return height;
    }
    static void iterateSubChunkElements(
        SubChunkBlockStorage* blockReader,
        int chunkBaseX, int subChunkBaseY, int chunkBaseZ, int heightLimit,
        const std::function<void(const Block*, const BlockPos&)>& callback)
    {
        for (uint16_t y = 0; y < heightLimit; ++y) {
            for (uint16_t x = 0; x < 16; ++x) {
                for (uint16_t z = 0; z < 16; ++z) {
                    uint16_t elementId = (x * 0x10 + z) * 0x10 + (y & 0xf);
                    const Block* found = blockReader->getElement(elementId);
                    BlockPos pos(chunkBaseX + x, subChunkBaseY + y, chunkBaseZ + z);
                    callback(found, pos);
                }
            }
        }
    }
    static bool isHole(const glm::ivec3& pos)
    {
        // Ensure there's enough headspace (2 blocks of air above)
        if (getExposedHeight(pos) < 2) return false;

        // Check that all four surrounding blocks are Obsidian or Bedrock
        std::vector<int> surroundingBlocks = getSurroundingBlocks(pos);

        for (int blockId : surroundingBlocks) {
            if (blockId != 7 && blockId != 49) { // Must be Bedrock or Obsidian
                return false;
            }
        }

        return true; // Marked as a hole
    }

    static std::vector<int> getSurroundingBlocks(const glm::ivec3& pos)
    {
        std::vector<int> surroundingBlocks;

        std::vector<glm::ivec3> offsets = {
            {1, 1, 0},  {-1, 1, 0}, // +X, -X
            {0, 1, 1},  {0, 1, -1}  // +Z, -Z
        };

        auto blockSource = ClientInstance::get()->getBlockSource();
        if (!blockSource) return surroundingBlocks;

        for (const auto& offset : offsets) {
            glm::ivec3 checkPos = pos + offset;
            Block* block = blockSource->getBlock(checkPos);
            if (block) {
                surroundingBlocks.push_back(block->toLegacy()->getBlockId());
            }
        }

        return surroundingBlocks;
    }

    static bool isGoodBlock(glm::ivec3 blockPos);
    static bool isAirBlock(glm::ivec3 blockPos);
    static glm::ivec3 getClosestPlacePos(glm::ivec3 pos, float distance);
    static bool isValidPlacePos(glm::ivec3 pos);
    static void placeBlock(glm::vec3 pos, int side);
    static void startDestroyBlock(glm::vec3 pos, int side);
    static void clearBlock(const glm::ivec3& pos);
    static void setBlock(const glm::ivec3& pos, Block* block);
    static void setBlock(const glm::ivec3& pos, unsigned int runtimeId);
    static void destroyBlock(glm::vec3 pos, int side, bool useTransac = false);
    static bool isMiningPosition(glm::ivec3 blockPos);
    // Converts a min pos, and max pos, to a list of chunks that are in that range
    static std::vector<struct ChunkPos> getChunkList(const glm::ivec3 min, const glm::ivec3 max);

};