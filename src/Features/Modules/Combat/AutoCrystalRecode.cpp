//
// Created by vastrakai on 7/7/2024.
//

#include "AutoCrystalRecode.hpp"

#include <Features/FeatureManager.hpp>
#include <Features/Events/BlockChangedEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerActionPacket.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/World/Chunk/LevelChunk.hpp>
#include <SDK/Minecraft/World/Chunk/SubChunkBlockStorage.hpp>


std::mutex AutoCrystalRecode::blockmutex = {};
std::unordered_map<BlockPos, AutoCrystalRecode::FoundBlock> AutoCrystalRecode::mFoundBlocks = {};
void AutoCrystalRecode::moveToNext() {
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }

    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    auto* blockSource = ClientInstance::get()->getBlockSource();
    size_t numSubchunks = (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16;

    if (mSubChunkIndex < numSubchunks - 1) {
        ++mSubChunkIndex;
        return;
    }

    static constexpr std::array<std::pair<int, int>, 4> directions = { {
        { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 }
    } };

    const auto& [dx, dy] = directions[mDirectionIndex];
    mCurrentChunkPos.x += dx;
    mCurrentChunkPos.y += dy;

    if (++mStepsCount >= mSteps) {
        mStepsCount = 0;
        if (++mDirectionIndex % 2 == 0) ++mSteps;
        mDirectionIndex %= directions.size();
    }

    mSubChunkIndex = 0;
}


void AutoCrystalRecode::tryProcessSub(bool& processed, ChunkPos currentChunkPos, int subChunkIndex)
{
    TRY_CALL([&]()
        {
            if (processSub(currentChunkPos, subChunkIndex))
            {
                processed = true;
            }
        });
}
bool AutoCrystalRecode::processSub(ChunkPos processChunk, int index) {
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return false;
    }

    ClientInstance* ci = ClientInstance::get();
    Actor* player = ci->getLocalPlayer();
    if (!player) return false;

    BlockSource* blockSource = ci->getBlockSource();
    size_t numSubchunks = (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16;
    if (index < 0 || index >= numSubchunks) return false;

    LevelChunk* chunk = blockSource->getChunk(processChunk);
    if (!chunk) return false;

    auto& subChunks = *chunk->getSubChunks();
    if (index >= subChunks.size()) return false;

    auto& subChunk = subChunks[index];
    SubChunkBlockStorage* blockReader = subChunk.blockReadPtr;
    if (!blockReader) return false;

    const int chunkBaseX = processChunk.x * 16;
    const int chunkBaseZ = processChunk.y * 16;
    const int subChunkBaseY = subChunk.subchunkIndex * 16;
    const int heightLimit = (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / chunk->getSubChunks()->size();

    std::vector<BlockPos> blocksToErase;

    BlockUtils::iterateSubChunkElements(blockReader, chunkBaseX, subChunkBaseY, chunkBaseZ, heightLimit,
        [&](const Block* found, const BlockPos& pos) {
            auto it = mFoundBlocks.find(pos);

            if (!found || found->mLegacy->getBlockId() == 0) {
                if (it != mFoundBlocks.end()) blocksToErase.push_back(pos);
                return;
            }

            // Ensure the base block is either OBSIDIAN (49) or BEDROCK (7)
            int blockId = found->mLegacy->getBlockId();
            if (blockId != 49 && blockId != 7) {
                if (it != mFoundBlocks.end()) blocksToErase.push_back(pos);
                return;
            }

            int exposedHeight = BlockUtils::getExposedHeight(pos , 2);
            if (exposedHeight == 0) {
                if (it != mFoundBlocks.end()) blocksToErase.push_back(pos);
                return;
            }

            mFoundBlocks.emplace(pos, FoundBlock{ found, AABB(pos, glm::vec3(1.f, 1.f, 1.f)), ImColor(0.f, 1.f, 0.f, 1.f) });
            if (it != mFoundBlocks.end()) {
                blocksToErase.push_back(pos);
            }
        }
    );

    for (const auto& pos : blocksToErase) {
        mFoundBlocks.erase(pos);
    }

    return true;
}


void AutoCrystalRecode::reset()
{
    std::lock_guard<std::mutex> lock(blockmutex); // Lock mutex

    ClientInstance* ci = ClientInstance::get();
    Actor* player = ci->getLocalPlayer();
    mSearchStart = NOW;
    mFoundBlocks.clear();
    mStepsCount = 0;
    mSteps = 1;
    mDirectionIndex = 0;
    mSubChunkIndex = 0;
    if (!player) return;
    BlockSource* blockSource = ci->getBlockSource();
    if (!target) return;
    mSearchCenter = ChunkPos(*target->getPos());
    mCurrentChunkPos = mSearchCenter;
}

void AutoCrystalRecode::onEnable()
{
    gFeatureManager->mDispatcher->listen<RenderEvent, &AutoCrystalRecode::onRenderEvent, nes::event_priority::VERY_FIRST>(this);
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoCrystalRecode::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<BlockChangedEvent, &AutoCrystalRecode::onBlockChangedEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &AutoCrystalRecode::onPacketInEvent>(this);
    reset();
}

void AutoCrystalRecode::onDisable()
{
    gFeatureManager->mDispatcher->deafen<RenderEvent, &AutoCrystalRecode::onRenderEvent>(this);
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoCrystalRecode::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<BlockChangedEvent, &AutoCrystalRecode::onBlockChangedEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &AutoCrystalRecode::onPacketInEvent>(this);
    reset();
}

std::vector<int> AutoCrystalRecode::getEnabledBlocks()
{
    std::vector<int> enabledBlocks = {};

    // Insert deepslate ores as well

    enabledBlocks.push_back(OBSIDIAN);
    enabledBlocks.push_back(BEDROCK);

    return enabledBlocks;
}

void AutoCrystalRecode::onBlockChangedEvent(BlockChangedEvent& event)
{
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }
    std::lock_guard<std::mutex> lock(blockmutex); // Lock mutex
    if (!target) return;
    auto dabl = BlockInfo(event.mNewBlock, event.mBlockPos);
    if (dabl.getDistance(*target->getPos()) > 11) return;

    ChunkPos chunkPos = ChunkPos(event.mBlockPos);
    int subChunk = (event.mBlockPos.y - ClientInstance::get()->getBlockSource()->getBuildDepth()) >> 4;
    bool result = false;
    tryProcessSub(result, chunkPos, subChunk);

    if (!result) {
        spdlog::critical("Failed to process subchunk [scIndex: {}/{}, chunkPos: ({}, {})]", subChunk, (ClientInstance::get()->getBlockSource()->getBuildHeight() - ClientInstance::get()->getBlockSource()->getBuildDepth()) / 16, chunkPos.x, chunkPos.y);
    }

    auto enabledBlocks = getEnabledBlocks();

    if (isValidBlock(event.mNewBlock->mLegacy->getBlockId()) && std::ranges::find(enabledBlocks, event.mNewBlock->mLegacy->getBlockId()) != enabledBlocks.end())
    {
        const Block* block = event.mNewBlock;
        mFoundBlocks[event.mBlockPos] = { block, AABB(event.mBlockPos, glm::vec3(1.f, 1.f, 1.f)), getColorFromId(block->mLegacy->getBlockId()) };
        spdlog::debug("event.mNewBlock->mLegacy->mName: {} event.mOldBlock->mLegacy->mName: {}", event.mNewBlock->mLegacy->mName, event.mOldBlock->mLegacy->mName);
    }
    else
    {
        mFoundBlocks.erase(event.mBlockPos);
    }
};

void AutoCrystalRecode::onBaseTickEvent(BaseTickEvent& event)
{
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }
    std::lock_guard<std::mutex> lock(blockmutex); // Lock mutex

    static uint64_t lastUpdate = 0;
    uint64_t freq = mUpdateFrequency.mValue * 50.f;
    uint64_t now = NOW;

    if (lastUpdate + freq > now) return;

    lastUpdate = now;
    auto ci = ClientInstance::get();
    auto player = ci->getLocalPlayer();
    if (!player) return;
    auto blockSource = ci->getBlockSource();

    auto actors = ActorUtils::getActorList(true, false);
    if (actors.empty()) return;

  
    for (auto* actor : actors) {
        if (actor && actor->isValid() && actor != player && actor->isPlayer()&& actor->distanceTo(player) <= 11) {
            target = actor;
            break;
        }
    }

    if (!target) return;
    BlockPos targetPos = *target->getPos();

    if (glm::distance(glm::vec2(mCurrentChunkPos), glm::vec2(mSearchCenter)) > mChunkRadius.mValue)
    {
        //spdlog::debug("Resetting search, found {} block of interest in {}ms", mFoundBlocks.size(), NOW - mSearchStart);
        mSearchStart = NOW;
        mSearchCenter = ChunkPos(targetPos);
        mCurrentChunkPos = mSearchCenter;
        mStepsCount = 0;
        mSteps = 1;
        mDirectionIndex = 0;
        mSubChunkIndex = 0;
    }

    for (int i = 0; i < mChunkUpdatesPerTick.mValue; i++)
    {
        bool processed = false;
        tryProcessSub(processed, mCurrentChunkPos, mSubChunkIndex);
        if (!processed)
        {
            spdlog::critical("Failed to process subchunk [scIndex: {}/{}, chunkPos: ({}, {})]", mSubChunkIndex, (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16, mCurrentChunkPos.x, mCurrentChunkPos.y);
        }
        moveToNext();
    }

    BlockPos playerPos = *target->getPos();

    int subChunk = (playerPos.y - ClientInstance::get()->getBlockSource()->getBuildDepth()) >> 4;
    bool result = false;
    tryProcessSub(result, ChunkPos(playerPos), subChunk);

    if (!result)
    {
        spdlog::critical("Failed to process subchunk [scIndex: {}/{}, chunkPos: ({}, {})]", subChunk, (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16, playerPos.x, playerPos.z);
    }

}

void AutoCrystalRecode::onPacketInEvent(PacketInEvent& event)
{
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }

    if (event.mPacket->getId() == PacketID::ChangeDimension)
    {
        reset(); // Reset the search when changing dimensions
    }

    if (event.mPacket->getId() == PacketID::PlayerAction)
    {
        auto packet = event.getPacket<PlayerActionPacket>();
        if (packet->mAction == PlayerActionType::Respawn) reset();
    }
}

void AutoCrystalRecode::onRenderEvent(RenderEvent& event)
{
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }

    if (ClientInstance::get()->getMouseGrabbed()) return;

    std::lock_guard<std::mutex> lock(blockmutex); // Lock mutex

    auto drawList = ImGui::GetBackgroundDrawList();

    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player || !ClientInstance::get()->getLevelRenderer())
    {
        reset();
        return;
    }
    if (!target) return;
    glm::ivec3 playerPos = *target->getPos();

    if (mRenderCurrentChunk.mValue)
    {
        ChunkPos currentChunkPos = ChunkPos(mCurrentChunkPos);
        glm::vec3 pos = glm::vec3(currentChunkPos.x * 16, 0, currentChunkPos.y * 16);

        // Render the current chunk
        AABB chunkAABB = AABB(pos, glm::vec3(16.f, 1.f, 16.f));
        std::vector<ImVec2> chunkPoints = MathUtils::getImBoxPoints(chunkAABB);

        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
            drawList->AddPolyline(chunkPoints.data(), chunkPoints.size(), ImColor(1.f, 1.f, 1.f), 0, 2.0f);
        }
        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {

            drawList->AddConvexPolyFilled(chunkPoints.data(), chunkPoints.size(), ImColor(1.f, 1.f, 1.f, 0.25f));
        }
    }

    auto enabled = getEnabledBlocks();

    for (auto& [pos, block] : mFoundBlocks)
    {
        if (distance(glm::vec3(pos), glm::vec3(playerPos)) > mRadius.mValue) continue;

        ImColor& color = block.color;
        AABB& blockAABB = block.aabb;
        std::vector<ImVec2> imPoints = MathUtils::getImBoxPoints(blockAABB);

        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
            drawList->AddPolyline(imPoints.data(), imPoints.size(), color, 0, 2.0f);
        }
        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {
            drawList->AddConvexPolyFilled(imPoints.data(), imPoints.size(), ImColor(color.Value.x, color.Value.y, color.Value.z, 0.25f));
        }
    }
}