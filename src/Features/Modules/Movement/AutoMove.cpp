//
// Created by Trae AI on 7/15/2024.
//

#include "AutoMove.hpp"

#include <Features/FeatureManager.hpp>
#include <Features/Events/BlockChangedEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerActionPacket.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/World/Chunk/LevelChunk.hpp>
#include <SDK/Minecraft/World/Chunk/SubChunkBlockStorage.hpp>
#include <Utils/MiscUtils/BlockUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/MiscUtils/RenderUtils.hpp>
#include <Utils/Utils.hpp>
#include <algorithm>

static std::mutex pathMutex = {};

// A* pathfinding for blocks
bool AutoMove::calculateBlockPath(const BlockPos& start, const BlockPos& target) {
    mBlockPath.clear();
    mCurrentBlockPathIndex = 0;
    mBlockPathCalculated = false;

    if (start == target) {
        mBlockPath.push_back(start);
        mBlockPathCalculated = true;
        return true;
    }

    std::priority_queue<BlockNode*, std::vector<BlockNode*>, BlockNodeCompare> openSet;
    std::unordered_map<BlockPos, BlockNode*, BlockPosHash> openMap;
    std::unordered_set<BlockPos, BlockPosHash> closedSet;
    std::vector<BlockNode*> allNodes;

    BlockNode* startNode = new BlockNode{
        start,
        0.0f,
        calculateBlockHeuristic(start, target),
        nullptr,
        true
    };

    openSet.push(startNode);
    openMap[start] = startNode;
    allNodes.push_back(startNode);

    int iterations = 0;
    const int MAX_ITERATIONS = 1000;

    while (!openSet.empty() && iterations < MAX_ITERATIONS) {
        iterations++;

        BlockNode* currentNode = openSet.top();
        openSet.pop();
        openMap.erase(currentNode->pos);

        if (currentNode->pos == target) {
            // Reconstruct path
            std::vector<BlockPos> path;
            BlockNode* current = currentNode;
            while (current != nullptr) {
                path.push_back(current->pos);
                current = current->parent;
            }
            std::reverse(path.begin(), path.end());
            mBlockPath = path;

            // Clean up memory
            for (BlockNode* node : allNodes) {
                delete node;
            }

            mBlockPathCalculated = true;
            return true;
        }

        closedSet.insert(currentNode->pos);

        // Check neighbors
        for (const auto& neighborPos : getNeighborBlocks(currentNode->pos)) {
            if (closedSet.find(neighborPos) != closedSet.end()) {
                continue;
            }

            // Check if block is walkable
            auto blockSource = ClientInstance::get()->getBlockSource();
            if (!blockSource) continue;

            const Block* block = blockSource->getBlock(neighborPos);
            if (!isWalkableBlock(block)) continue;

            // Line of sight check if enabled
            if (mUseLineOfSight.mValue && !hasLineOfSight(currentNode->pos, neighborPos)) {
                continue;
            }

            // Calculate movement cost
            float movementCost = calculateBlockHeuristic(currentNode->pos, neighborPos);
            float newGCost = currentNode->gCost + movementCost;
            float hCost = calculateBlockHeuristic(neighborPos, target);

            auto neighborIt = openMap.find(neighborPos);
            bool inOpenSet = neighborIt != openMap.end();

            if (!inOpenSet || newGCost < neighborIt->second->gCost) {
                BlockNode* neighborNode = nullptr;
                
                if (!inOpenSet) {
                    neighborNode = new BlockNode{
                        neighborPos,
                        newGCost,
                        hCost,
                        currentNode,
                        true
                    };
                    openSet.push(neighborNode);
                    openMap[neighborPos] = neighborNode;
                    allNodes.push_back(neighborNode);
                } else {
                    neighborNode = neighborIt->second;
                    neighborNode->gCost = newGCost;
                    neighborNode->hCost = hCost;
                    neighborNode->parent = currentNode;
                    openSet.push(neighborNode);
                }
            }
        }
    }

    // Clean up memory if no path found
    for (BlockNode* node : allNodes) {
        delete node;
    }

    return false;
}

// A* pathfinding for chunks
bool AutoMove::calculateChunkPath(const ChunkPos& start, const ChunkPos& target) {
    mChunkPath.clear();
    mCurrentChunkPathIndex = 0;
    mChunkPathCalculated = false;

    if (start == target) {
        mChunkPath.push_back(start);
        mChunkPathCalculated = true;
        return true;
    }

    std::priority_queue<ChunkNode*, std::vector<ChunkNode*>, ChunkNodeCompare> openSet;
    std::unordered_map<ChunkPos, ChunkNode*, ChunkPosHash> openMap;
    std::unordered_set<ChunkPos, ChunkPosHash> closedSet;
    std::vector<ChunkNode*> allNodes;

    ChunkNode* startNode = new ChunkNode{
        start,
        0.0f,
        calculateChunkHeuristic(start, target),
        nullptr,
        true
    };

    openSet.push(startNode);
    openMap[start] = startNode;
    allNodes.push_back(startNode);

    int iterations = 0;
    const int MAX_ITERATIONS = 500;

    while (!openSet.empty() && iterations < MAX_ITERATIONS) {
        iterations++;

        ChunkNode* currentNode = openSet.top();
        openSet.pop();
        openMap.erase(currentNode->pos);

        if (currentNode->pos == target) {
            // Reconstruct path
            std::vector<ChunkPos> path;
            ChunkNode* current = currentNode;
            while (current != nullptr) {
                path.push_back(current->pos);
                current = current->parent;
            }
            std::reverse(path.begin(), path.end());
            mChunkPath = path;

            // Clean up memory
            for (ChunkNode* node : allNodes) {
                delete node;
            }

            mChunkPathCalculated = true;
            return true;
        }

        closedSet.insert(currentNode->pos);

        // Check neighbors
        for (const auto& neighborPos : getNeighborChunks(currentNode->pos)) {
            if (closedSet.find(neighborPos) != closedSet.end()) {
                continue;
            }

            // Calculate movement cost
            float movementCost = 1.0f;
            if (neighborPos.x != currentNode->pos.x && neighborPos.y != currentNode->pos.y) {
                movementCost = 1.414f; // Diagonal movement
            }

            float newGCost = currentNode->gCost + movementCost;
            float hCost = calculateChunkHeuristic(neighborPos, target);

            auto neighborIt = openMap.find(neighborPos);
            bool inOpenSet = neighborIt != openMap.end();

            if (!inOpenSet || newGCost < neighborIt->second->gCost) {
                ChunkNode* neighborNode = nullptr;
                
                if (!inOpenSet) {
                    neighborNode = new ChunkNode{
                        neighborPos,
                        newGCost,
                        hCost,
                        currentNode,
                        true
                    };
                    openSet.push(neighborNode);
                    openMap[neighborPos] = neighborNode;
                    allNodes.push_back(neighborNode);
                } else {
                    neighborNode = neighborIt->second;
                    neighborNode->gCost = newGCost;
                    neighborNode->hCost = hCost;
                    neighborNode->parent = currentNode;
                    openSet.push(neighborNode);
                }
            }
        }
    }

    // Clean up memory if no path found
    for (ChunkNode* node : allNodes) {
        delete node;
    }

    return false;
}

void AutoMove::updateSearch() {
    if (!mSearchActive) return;

    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource) return;

    // Update search position automatically
    static uint64_t lastChunkUpdate = 0;
    uint64_t now = NOW;
    uint64_t chunkUpdateInterval = 100; // Update every 100ms

    if (now - lastChunkUpdate > chunkUpdateInterval) {
        lastChunkUpdate = now;
        moveToNextChunk();
    }

    // Process current chunk
    size_t numSubchunks = (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16;
    for (int i = 0; i < mChunkUpdatesPerTick.mValue; i++) {
        bool processed = false;
        tryProcessSub(processed, mCurrentChunkPos, mSubChunkIndex);
        
        if (mSubChunkIndex < numSubchunks - 1) {
            mSubChunkIndex++;
        } else {
            mSubChunkIndex = 0;
            moveToNextChunk();
        }
    }
}

void AutoMove::moveToNextChunk() {
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }

    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // Follow chunk path if available
    if (mChunkPathCalculated && mCurrentChunkPathIndex < mChunkPath.size()) {
        mCurrentChunkPos = mChunkPath[mCurrentChunkPathIndex];
        mCurrentChunkPathIndex++;
        
        // If we've reached the end of the path, recalculate
        if (mCurrentChunkPathIndex >= mChunkPath.size()) {
            ChunkPos playerChunk = ChunkPos(*player->getPos());
            ChunkPos targetChunk = ChunkPos(mTargetX.mValue, mTargetZ.mValue);
            
            if (playerChunk != targetChunk) {
                calculateChunkPath(playerChunk, targetChunk);
            }
        }
    } else {
        // No path available, move directly toward target
        ChunkPos playerChunk = ChunkPos(*player->getPos());
        ChunkPos targetChunk = ChunkPos(mTargetX.mValue, mTargetZ.mValue);
        
        if (playerChunk != targetChunk) {
            if (!calculateChunkPath(playerChunk, targetChunk)) {
                // Fallback: move one step toward target
                int dx = targetChunk.x - playerChunk.x;
                int dy = targetChunk.y - playerChunk.y;
                
                if (dx != 0) dx = dx > 0 ? 1 : -1;
                if (dy != 0) dy = dy > 0 ? 1 : -1;
                
                mCurrentChunkPos = ChunkPos(playerChunk.x + dx, playerChunk.y + dy);
            }
        }
    }
}

void AutoMove::tryProcessSub(bool& processed, ChunkPos currentChunkPos, int subChunkIndex) {
    TRY_CALL([&]() {
        if (processSub(currentChunkPos, subChunkIndex)) {
            processed = true;
        }
    });
}

bool AutoMove::processSub(ChunkPos processChunk, int index) {
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

    // Reset counter for current chunk
    mWalkableBlocksInCurrentChunk = 0;

    // Process blocks and add to walkable set
    BlockUtils::iterateSubChunkElements(blockReader, chunkBaseX, subChunkBaseY, chunkBaseZ, heightLimit,
        [&](const Block* found, const BlockPos& pos) {
            if (found && found->mLegacy->getBlockId() != 0 && isWalkableBlock(found)) {
                mWalkableBlocks.insert(pos);
                mWalkableBlocksInCurrentChunk++;
            }
        }
    );

    return true;
}

void AutoMove::reset() {
    std::lock_guard<std::mutex> lock(pathMutex);

    ClientInstance* ci = ClientInstance::get();
    Actor* player = ci->getLocalPlayer();
    mSearchStart = NOW;
    mLastSearchUpdate = 0;
    mSubChunkIndex = 0;
    mWalkableBlocksInCurrentChunk = 0;
    mSearchActive = false;
    
    // Clear paths
    mBlockPath.clear();
    mCurrentBlockPathIndex = 0;
    mBlockPathCalculated = false;
    
    mChunkPath.clear();
    mCurrentChunkPathIndex = 0;
    mChunkPathCalculated = false;
    
    // Clear walkable blocks
    mWalkableBlocks.clear();

    if (!player) return;

    mSearchCenter = ChunkPos(*player->getPos());
    mCurrentChunkPos = mSearchCenter;
    mSearchActive = true;

    // Calculate initial paths
    BlockPos playerPos = *player->getPos();
    BlockPos targetPos = BlockPos(mTargetX.mValue, mTargetY.mValue, mTargetZ.mValue);
    ChunkPos targetChunk = ChunkPos(mTargetX.mValue, mTargetZ.mValue);
    
    calculateBlockPath(playerPos, targetPos);
    calculateChunkPath(mSearchCenter, targetChunk);
}

bool AutoMove::isChunkWalkable(const ChunkPos& chunk) const {
    return true; // For now, assume all chunks are walkable
}

void AutoMove::onEnable() {
    gFeatureManager->mDispatcher->listen<RenderEvent, &AutoMove::onRenderEvent>(this);
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoMove::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<BlockChangedEvent, &AutoMove::onBlockChangedEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &AutoMove::onPacketInEvent>(this);
    reset();
}

void AutoMove::onDisable() {
    gFeatureManager->mDispatcher->deafen<RenderEvent, &AutoMove::onRenderEvent>(this);
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoMove::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<BlockChangedEvent, &AutoMove::onBlockChangedEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &AutoMove::onPacketInEvent>(this);
    mSearchActive = false;
}

void AutoMove::onBlockChangedEvent(BlockChangedEvent& event) {
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }
    std::lock_guard<std::mutex> lock(pathMutex);

    auto blockInfo = BlockInfo(event.mNewBlock, event.mBlockPos);
    if (blockInfo.getDistance(*ClientInstance::get()->getLocalPlayer()->getPos()) > mRadius.mValue) return;

    ChunkPos chunkPos = ChunkPos(event.mBlockPos);
    int subChunk = (event.mBlockPos.y - ClientInstance::get()->getBlockSource()->getBuildDepth()) >> 4;
    bool result = false;
    tryProcessSub(result, chunkPos, subChunk);

    if (!result) {
        spdlog::critical("Failed to process subchunk [scIndex: {}/{}, chunkPos: ({}, {})].", subChunk,
            (ClientInstance::get()->getBlockSource()->getBuildHeight() - ClientInstance::get()->getBlockSource()->getBuildDepth()) / 16,
            chunkPos.x, chunkPos.y);
    }
}

void AutoMove::onBaseTickEvent(BaseTickEvent& event) {
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }
    std::lock_guard<std::mutex> lock(pathMutex);

    static uint64_t lastUpdate = 0;
    uint64_t freq = mUpdateFrequency.mValue * 50.f;
    uint64_t now = NOW;

    if (lastUpdate + freq > now) return;
    lastUpdate = now;

    auto ci = ClientInstance::get();
    auto player = ci->getLocalPlayer();
    if (!player) return;

    ChunkPos playerChunk = ChunkPos(*player->getPos());
    ChunkPos targetChunk = ChunkPos(mTargetX.mValue, mTargetZ.mValue);

    // Check if we need to recalculate paths
    float distanceFromCenter = std::sqrt(
        (playerChunk.x - mSearchCenter.x) * (playerChunk.x - mSearchCenter.x) + 
        (playerChunk.y - mSearchCenter.y) * (playerChunk.y - mSearchCenter.y)
    );

    if (distanceFromCenter > mChunkRadius.mValue || (!mBlockPathCalculated && !mChunkPathCalculated)) {
        mSearchStart = NOW;
        mSearchCenter = playerChunk;
        mCurrentChunkPos = mSearchCenter;
        mSubChunkIndex = 0;
        mSearchActive = true;
        
        // Recalculate paths
        BlockPos playerPos = *player->getPos();
        BlockPos targetPos = BlockPos(mTargetX.mValue, mTargetY.mValue, mTargetZ.mValue);
        
        calculateBlockPath(playerPos, targetPos);
        calculateChunkPath(mSearchCenter, targetChunk);
    }

    // Update autonomous search
    updateSearch();
}

void AutoMove::onPacketInEvent(PacketInEvent& event) {
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }

    if (event.mPacket->getId() == PacketID::ChangeDimension) {
        reset();
    }

    if (event.mPacket->getId() == PacketID::PlayerAction) {
        auto packet = event.getPacket<PlayerActionPacket>();
        if (packet->mAction == PlayerActionType::Respawn) reset();
    }
}

void AutoMove::onRenderEvent(RenderEvent& event) {
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }

    if (ClientInstance::get()->getMouseGrabbed()) return;

    std::lock_guard<std::mutex> lock(pathMutex);

    auto drawList = ImGui::GetBackgroundDrawList();
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player || !ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }

    // Render block path
    if (mRenderPath.mValue && mBlockPathCalculated && !mBlockPath.empty()) {
        for (size_t i = 0; i < mBlockPath.size(); i++) {
            const auto& blockPos = mBlockPath[i];
            glm::vec3 pos = glm::vec3(blockPos);

            AABB blockAABB = AABB(pos, glm::vec3(1.f, 1.f, 1.f));
            std::vector<ImVec2> blockPoints = MathUtils::getImBoxPoints(blockAABB);

            ImColor pathColor;
            if (i == 0) {
                pathColor = ImColor(0.f, 1.f, 0.f, 1.f);  // Start (green)
            } else if (i == mBlockPath.size() - 1) {
                pathColor = ImColor(1.f, 0.f, 0.f, 1.f);  // End (red)
            } else {
                pathColor = PATH_COLOR;  // Path (yellow)
            }

            if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
                drawList->AddPolyline(blockPoints.data(), blockPoints.size(), pathColor, 0, 2.0f);
            }
            if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {
                drawList->AddConvexPolyFilled(blockPoints.data(), blockPoints.size(), 
                    ImColor(pathColor.Value.x, pathColor.Value.y, pathColor.Value.z, 0.3f));
            }
        }
    }

    // Render chunk path
    if (mRenderPath.mValue && mChunkPathCalculated && !mChunkPath.empty()) {
        for (size_t i = 0; i < mChunkPath.size(); i++) {
            const auto& chunkPos = mChunkPath[i];
            glm::vec3 pos = glm::vec3(chunkPos.x * 16, 0, chunkPos.y * 16);

            AABB chunkAABB = AABB(pos, glm::vec3(16.f, 1.f, 16.f));
            std::vector<ImVec2> chunkPoints = MathUtils::getImBoxPoints(chunkAABB);

            ImColor pathColor;
            if (i == 0) {
                pathColor = ImColor(0.f, 1.f, 0.f, 0.5f);  // Start chunk (green)
            } else if (i == mChunkPath.size() - 1) {
                pathColor = ImColor(1.f, 0.f, 0.f, 0.5f);  // End chunk (red)
            } else if (i == mCurrentChunkPathIndex - 1) {
                pathColor = ImColor(1.f, 1.f, 0.f, 0.5f);  // Current processing chunk (yellow)
            } else {
                pathColor = ImColor(0.f, 0.5f, 1.f, 0.3f);  // Path chunks (blue)
            }

            if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
                drawList->AddPolyline(chunkPoints.data(), chunkPoints.size(), pathColor, 0, 1.5f);
            }
            if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {
                drawList->AddConvexPolyFilled(chunkPoints.data(), chunkPoints.size(), 
                    ImColor(pathColor.Value.x, pathColor.Value.y, pathColor.Value.z, 0.1f));
            }
        }
    }

    // Render current chunk being processed
    if (mRenderCurrentChunk.mValue) {
        glm::vec3 pos = glm::vec3(mCurrentChunkPos.x * 16, 0, mCurrentChunkPos.y * 16);

        AABB chunkAABB = AABB(pos, glm::vec3(16.f, 1.f, 16.f));
        std::vector<ImVec2> chunkPoints = MathUtils::getImBoxPoints(chunkAABB);

        ImColor chunkColor = ImColor(0.f, 1.f, 0.f, 0.8f); // Green for current chunk

        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
            drawList->AddPolyline(chunkPoints.data(), chunkPoints.size(), chunkColor, 0, 3.0f);
        }
        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {
            drawList->AddConvexPolyFilled(chunkPoints.data(), chunkPoints.size(), 
                ImColor(chunkColor.Value.x, chunkColor.Value.y, chunkColor.Value.z, 0.2f));
        }
    }

    // Render walkable blocks
    for (const auto& blockPos : mWalkableBlocks) {
        glm::vec3 pos = glm::vec3(blockPos);

        AABB blockAABB = AABB(pos, glm::vec3(1.f, 1.f, 1.f));
        std::vector<ImVec2> blockPoints = MathUtils::getImBoxPoints(blockAABB);

        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
            drawList->AddPolyline(blockPoints.data(), blockPoints.size(), WALKABLE_COLOR, 0, 1.0f);
        }
        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {
            drawList->AddConvexPolyFilled(blockPoints.data(), blockPoints.size(), 
                ImColor(WALKABLE_COLOR.Value.x, WALKABLE_COLOR.Value.y, WALKABLE_COLOR.Value.z, 0.1f));
        }
    }

    // Render target position
    glm::vec3 targetPos(mTargetX.mValue, mTargetY.mValue, mTargetZ.mValue);
    AABB targetAABB = AABB(targetPos, glm::vec3(1.f, 1.f, 1.f));
    std::vector<ImVec2> targetPoints = MathUtils::getImBoxPoints(targetAABB);

    if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
        drawList->AddPolyline(targetPoints.data(), targetPoints.size(), TARGET_COLOR, 0, 3.0f);
    }
    if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {
        drawList->AddConvexPolyFilled(targetPoints.data(), targetPoints.size(), 
            ImColor(TARGET_COLOR.Value.x, TARGET_COLOR.Value.y, TARGET_COLOR.Value.z, 0.6f));
    }

    // Display info
    ImVec2 screenPos(50, 80);
    std::string searchInfo = "Search Active: " + std::string(mSearchActive ? "Yes" : "No");
    drawList->AddText(screenPos, ImColor(1.f, 1.f, 1.f, 1.f), searchInfo.c_str());
    
    screenPos.y += 20;
    std::string blockPathInfo = "Block Path: " + std::string(mBlockPathCalculated ? "Yes" : "No");
    drawList->AddText(screenPos, ImColor(1.f, 1.f, 1.f, 1.f), blockPathInfo.c_str());
    
    screenPos.y += 20;
    std::string chunkPathInfo = "Chunk Path: " + std::string(mChunkPathCalculated ? "Yes" : "No");
    drawList->AddText(screenPos, ImColor(1.f, 1.f, 1.f, 1.f), chunkPathInfo.c_str());
    
    screenPos.y += 20;
    std::string blocksInfo = "Walkable Blocks: " + std::to_string(mWalkableBlocks.size());
    drawList->AddText(screenPos, ImColor(1.f, 1.f, 1.f, 1.f), blocksInfo.c_str());
    
    if (mBlockPathCalculated) {
        screenPos.y += 20;
        std::string pathLengthInfo = "Block Path Length: " + std::to_string(mBlockPath.size());
        drawList->AddText(screenPos, ImColor(1.f, 1.f, 1.f, 1.f), pathLengthInfo.c_str());
    }
    
    if (mChunkPathCalculated) {
        screenPos.y += 20;
        std::string chunkPathLengthInfo = "Chunk Path Length: " + std::to_string(mChunkPath.size());
        drawList->AddText(screenPos, ImColor(1.f, 1.f, 1.f, 1.f), chunkPathLengthInfo.c_str());
    }
}
