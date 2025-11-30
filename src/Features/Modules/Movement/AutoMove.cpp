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
#include <queue>
#include <unordered_set>

static std::mutex pathMutex = {};
std::unordered_map<BlockPos, AutoMove::FoundBlock> AutoMove::mFoundBlocks = {};
struct PairComparator {
    bool operator()(const std::pair<float, ChunkPos>& a, const std::pair<float, ChunkPos>& b) const {
        return a.first > b.first; // Compare based on the float value (distance)
    }
};

// Maximum search distance for pathfinding
const float MAX_SEARCH_DISTANCE = 1000.0f;

void AutoMove::moveToNext() {
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
    
    // A* based chunk selection instead of spiral pattern
    // Use a priority queue to select chunks based on distance to target
    static std::priority_queue<std::pair<float, ChunkPos>,
        std::vector<std::pair<float, ChunkPos>>,
        PairComparator> chunkQueue;
    static std::unordered_set<ChunkPos> visitedChunks;
    
    // If queue is empty, initialize with current chunk
    if (chunkQueue.empty()) {
        // Clear visited chunks when starting a new search
        visitedChunks.clear();
        visitedChunks.insert(mCurrentChunkPos);
        
        // Add neighbors of current chunk to queue
        static const std::array<std::pair<int, int>, 8> directions = { {
            { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 },
            { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } // Include diagonals
        } };
        
        for (const auto& [dx, dy] : directions) {
            ChunkPos neighbor = mCurrentChunkPos;
            neighbor.x += dx;
            neighbor.y += dy;
            
            // Calculate priority based on distance to target
            glm::vec2 chunkCenter(neighbor.x * 16 + 8, neighbor.y * 16 + 8);
            glm::vec2 targetPos(mTargetPosition.x, mTargetPosition.z);
            float distance = glm::distance(chunkCenter, targetPos);
            
            // Add to queue if not visited
            if (visitedChunks.find(neighbor) == visitedChunks.end() && 
                glm::distance(glm::vec2(neighbor), glm::vec2(mSearchCenter)) <= mChunkRadius.mValue) {
                chunkQueue.push({distance, neighbor});
            }
        }
    }
    
    // Get next chunk from queue
    if (!chunkQueue.empty()) {
        auto [priority, nextChunk] = chunkQueue.top();
        chunkQueue.pop();
        
        // Set as current chunk
        mCurrentChunkPos = nextChunk;
        visitedChunks.insert(nextChunk);
        
        // Add neighbors to queue
        static const std::array<std::pair<int, int>, 8> directions = { {
            { 1, 0 }, { 0, 1 }, { -1, 0 }, { 0, -1 },
            { 1, 1 }, { 1, -1 }, { -1, 1 }, { -1, -1 } // Include diagonals
        } };
        
        for (const auto& [dx, dy] : directions) {
            ChunkPos neighbor = nextChunk;
            neighbor.x += dx;
            neighbor.y += dy;
            
            // Calculate priority based on distance to target
            glm::vec2 chunkCenter(neighbor.x * 16 + 8, neighbor.y * 16 + 8);
            glm::vec2 targetPos(mTargetPosition.x, mTargetPosition.z);
            float distance = glm::distance(chunkCenter, targetPos);
            
            // Add to queue if not visited
            if (visitedChunks.find(neighbor) == visitedChunks.end() && 
                glm::distance(glm::vec2(neighbor), glm::vec2(mSearchCenter)) <= mChunkRadius.mValue) {
                chunkQueue.push({distance, neighbor});
            }
        }
    } else {
        // If queue is empty, reset to search center
        mCurrentChunkPos = mSearchCenter;
    }
    
    mSubChunkIndex = 0;
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

    std::vector<BlockPos> blocksToErase;

    BlockUtils::iterateSubChunkElements(blockReader, chunkBaseX, subChunkBaseY, chunkBaseZ, heightLimit,
        [&](const Block* found, const BlockPos& pos) {
            auto it = mFoundBlocks.find(pos);

            if (!found || found->mLegacy->getBlockId() == 0) {
                if (it != mFoundBlocks.end()) blocksToErase.push_back(pos);
                return;
            }

            // Check if this is a walkable block (solid block with air above)
            if (isWalkableBlock(found)) {
                int exposedHeight = BlockUtils::getExposedHeight(pos);
                if (exposedHeight >= 2) { // Need at least 2 blocks of air above for player to walk
                    if (it == mFoundBlocks.end()) {
                        mFoundBlocks.emplace(pos, FoundBlock{ found, AABB(pos, glm::vec3(1.f, 1.f, 1.f)), WALKABLE_COLOR });
                    }
                }
            } else if (it != mFoundBlocks.end()) {
                blocksToErase.push_back(pos);
            }
        }
    );

    for (const auto& pos : blocksToErase) {
        mFoundBlocks.erase(pos);
    }

    return true;
}

void AutoMove::reset() {
    std::lock_guard<std::mutex> lock(pathMutex);

    ClientInstance* ci = ClientInstance::get();
    Actor* player = ci->getLocalPlayer();
    mSearchStart = NOW;
    mFoundBlocks.clear();
    mStepsCount = 0;
    mSteps = 1;
    mDirectionIndex = 0;
    mSubChunkIndex = 0;
    mPath.clear();
    mPathFound = false;
    
    if (!player) return;
    
    // Set target position from settings
    mTargetPosition = glm::vec3(mTargetX.mValue, mTargetY.mValue, mTargetZ.mValue);
    
    BlockSource* blockSource = ci->getBlockSource();
    mSearchCenter = ChunkPos(*player->getPos());
    mCurrentChunkPos = mSearchCenter;
}

void AutoMove::onEnable() {
    gFeatureManager->mDispatcher->listen<RenderEvent, &AutoMove::onRenderEvent, nes::event_priority::VERY_FIRST>(this);
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoMove::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<BlockChangedEvent, &AutoMove::onBlockChangedEvent>(this);
    gFeatureManager->mDispatcher->listen<PacketInEvent, &AutoMove::onPacketInEvent>(this);
    reset();
    
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;
    
    // Initialize target position from settings
    mTargetPosition = glm::vec3(mTargetX.mValue, mTargetY.mValue, mTargetZ.mValue);
    
    // Initialize search center based on player position
    glm::vec3 playerPos = *player->getPos();
    mSearchCenter = ChunkPos(static_cast<int>(playerPos.x) >> 4, static_cast<int>(playerPos.z) >> 4);
    mCurrentChunkPos = mSearchCenter;
    
    // Initialize checkpoint system
    mCheckpoints.clear();
    mCurrentCheckpointIndex = 0;
    mReachedCheckpoint = false;
    generateCheckpoints(playerPos, mTargetPosition);
}

void AutoMove::generateCheckpoints(const glm::vec3& start, const glm::vec3& target) {
    mCheckpoints.clear();
    mCurrentCheckpointIndex = 0;
    mReachedCheckpoint = false;
    
    // Always add start and target as checkpoints
    mCheckpoints.push_back(start);
    
    // Calculate the total distance
    float totalDistance = glm::distance(start, target);
    
    // If the distance is less than the checkpoint distance, just add the target
    if (totalDistance <= mCheckpointDistance.mValue) {
        mCheckpoints.push_back(target);
        return;
    }
    
    // Calculate how many checkpoints we need
    int numCheckpoints = static_cast<int>(totalDistance / mCheckpointDistance.mValue);
    
    // Create intermediate checkpoints along the straight line from start to target
    for (int i = 1; i < numCheckpoints; i++) {
        float t = static_cast<float>(i) / numCheckpoints;
        glm::vec3 checkpoint = start + t * (target - start);
        mCheckpoints.push_back(checkpoint);
    }
    
    // Add the target as the final checkpoint
    mCheckpoints.push_back(target);
}

void AutoMove::smoothPath() {
    if (!mSmoothPath.mValue || mPath.size() < 3) return;
    
    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource) return;
    
    std::vector<glm::vec3> smoothedPath;
    smoothedPath.push_back(mPath[0]); // Always keep the start point
    
    // Use line-of-sight checks to remove unnecessary waypoints
    size_t i = 0;
    while (i < mPath.size() - 1) {
        size_t j = i + 2; // Try to connect to points further ahead
        size_t furthestVisible = i + 1;
        
        // Find the furthest point that has line of sight from current point
        while (j < mPath.size()) {
            if (hasLineOfSight(mPath[i], mPath[j])) {
                furthestVisible = j;
            }
            j++;
        }
        
        // Add the furthest visible point to the smoothed path
        smoothedPath.push_back(mPath[furthestVisible]);
        i = furthestVisible;
    }
    
    // Replace the original path with the smoothed path
    mPath = smoothedPath;
}

bool AutoMove::hasLineOfSight(const glm::vec3& start, const glm::vec3& end) const {
    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource) return false;
    
    // Calculate direction vector
    glm::vec3 direction = end - start;
    float distance = glm::length(direction);
    direction = glm::normalize(direction);
    
    // Ray-trace from start to end
    for (float t = 0.0f; t < distance; t += 0.5f) {
        glm::vec3 checkPos = start + direction * t;
        glm::ivec3 blockPos = glm::floor(checkPos);
        
        // Check if block is solid
        Block* block = blockSource->getBlock(blockPos);
        if (block && block->mLegacy->getBlockId() != 0 && block->mLegacy->mMaterial->mIsBlockingMotion) {
            return false; // Line of sight blocked
        }
    }
    
    return true; // Clear line of sight
}

bool AutoMove::isPathVisible() const {
    if (mPath.empty()) return false;
    
    auto* player = ClientInstance::get()->getLocalPlayer();
    if (!player) return false;
    
    // Get player position
    glm::vec3 playerPos = *player->getPos();
    
    // Check if any part of the path is visible to the player
    for (const auto& pathPos : mPath) {
        // Calculate distance to player
        float distance = glm::distance(playerPos, pathPos);
        
        // If within render distance, consider it visible
        // Using a conservative estimate of render distance
        if (distance < 100.0f) {
            return true;
        }
    }
    
    return false;
}

void AutoMove::onDisable() {
    gFeatureManager->mDispatcher->deafen<RenderEvent, &AutoMove::onRenderEvent>(this);
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoMove::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<BlockChangedEvent, &AutoMove::onBlockChangedEvent>(this);
    gFeatureManager->mDispatcher->deafen<PacketInEvent, &AutoMove::onPacketInEvent>(this);
    reset();
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

    // If the block is walkable, add it to our found blocks
    if (isWalkableBlock(event.mNewBlock)) {
        int exposedHeight = BlockUtils::getExposedHeight(event.mBlockPos);
        if (exposedHeight >= 2) {
            mFoundBlocks[event.mBlockPos] = { event.mNewBlock, AABB(event.mBlockPos, glm::vec3(1.f, 1.f, 1.f)), WALKABLE_COLOR };
        }
    } else {
        mFoundBlocks.erase(event.mBlockPos);
    }
    
    // Recalculate path if a block changed
    auto player = ClientInstance::get()->getLocalPlayer();
    if (player) {
        mPathFound = findPath(*player->getPos(), mTargetPosition);
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
    auto blockSource = ci->getBlockSource();

    if (glm::distance(glm::vec2(mCurrentChunkPos), glm::vec2(mSearchCenter)) > mChunkRadius.mValue) {
        mSearchStart = NOW;
        mSearchCenter = ChunkPos(*player->getPos());
        mCurrentChunkPos = mSearchCenter;
        mStepsCount = 0;
        mSteps = 1;
        mDirectionIndex = 0;
        mSubChunkIndex = 0;
    }

    for (int i = 0; i < mChunkUpdatesPerTick.mValue; i++) {
        bool processed = false;
        tryProcessSub(processed, mCurrentChunkPos, mSubChunkIndex);
        if (!processed) {
            spdlog::critical("Failed to process subchunk [scIndex: {}/{}, chunkPos: ({}, {})].", mSubChunkIndex, 
                (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16, 
                mCurrentChunkPos.x, mCurrentChunkPos.y);
        }
        moveToNext();
    }

    BlockPos playerPos = *player->getPos();
    glm::vec3 playerPosition = *player->getPos();

    int subChunk = (playerPos.y - ClientInstance::get()->getBlockSource()->getBuildDepth()) >> 4;
    bool result = false;
    tryProcessSub(result, ChunkPos(playerPos), subChunk);

    if (!result) {
        spdlog::critical("Failed to process subchunk [scIndex: {}/{}, chunkPos: ({}, {})].", subChunk, 
            (blockSource->getBuildHeight() - blockSource->getBuildDepth()) / 16, 
            playerPos.x, playerPos.z);
    }
    
    // Update target position from settings
    mTargetPosition = glm::vec3(mTargetX.mValue, mTargetY.mValue, mTargetZ.mValue);
    
    // Check if we need to recalculate the path based on checkpoints
    if (mPathFound && mAutoRecalculate.mValue) {
        // Check if we've reached the current checkpoint
        if (mCurrentCheckpointIndex < mCheckpoints.size() - 1) {
            float distanceToCheckpoint = glm::distance(playerPosition, mCheckpoints[mCurrentCheckpointIndex + 1]);
            
            // If we're close enough to the checkpoint, move to the next one
            if (distanceToCheckpoint < 5.0f) {
                mCurrentCheckpointIndex++;
                
                // If we've reached the final checkpoint, we're done
                if (mCurrentCheckpointIndex >= mCheckpoints.size() - 1) {
                    mReachedCheckpoint = true;
                } else {
                    // Recalculate path to the next checkpoint
                    mPath.clear();
                    mPathFound = findPath(playerPosition, mCheckpoints[mCurrentCheckpointIndex + 1]);
                }
            }
        }
        
        // Check if the path is no longer visible and needs recalculation
        if (!isPathVisible() && !mReachedCheckpoint) {
            // Recalculate checkpoints from current position
            generateCheckpoints(playerPosition, mTargetPosition);
            mPath.clear();
            mPathFound = findPath(playerPosition, mCheckpoints[mCurrentCheckpointIndex + 1]);
        }
    }
    
    // Try to find a path to the target position
    if (!mPathFound) {
        // Generate checkpoints first if needed
        if (mCheckpoints.empty()) {
            generateCheckpoints(playerPosition, mTargetPosition);
        }
        mPathFound = findPath(playerPosition, mCheckpoints[mCurrentCheckpointIndex + 1]);
    }
}

void AutoMove::onPacketInEvent(PacketInEvent& event) {
    if (!ClientInstance::get()->getLevelRenderer()) {
        reset();
        return;
    }

    if (event.mPacket->getId() == PacketID::ChangeDimension) {
        reset(); // Reset the search when changing dimensions
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

    glm::ivec3 playerPos = *player->getPos();

    if (mRenderCurrentChunk.mValue) {
        ChunkPos currentChunkPos = ChunkPos(mCurrentChunkPos);
        glm::vec3 pos = glm::vec3(currentChunkPos.x * 16, 0, currentChunkPos.y * 16);

        // Render the current chunk being processed
        AABB chunkAABB = AABB(pos, glm::vec3(16.f, 1.f, 16.f));
        std::vector<ImVec2> chunkPoints = MathUtils::getImBoxPoints(chunkAABB);

        // Use a distinct color for the current chunk being processed
        ImColor chunkColor = ImColor(1.f, 0.5f, 0.f, 1.f); // Orange color

        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
            drawList->AddPolyline(chunkPoints.data(), chunkPoints.size(), chunkColor, 0, 2.0f);
        }
        if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {
            drawList->AddConvexPolyFilled(chunkPoints.data(), chunkPoints.size(), ImColor(chunkColor.Value.x, chunkColor.Value.y, chunkColor.Value.z, 0.25f));
        }
    }

    // Render checkpoints if available
    if (mRenderPath.mValue && !mCheckpoints.empty()) {
        for (size_t i = 0; i < mCheckpoints.size(); i++) {
            const auto& checkpointPos = mCheckpoints[i];
            
            // Convert world position to screen position
            ImVec2 screenPos;
            if (RenderUtils::worldToScreen(checkpointPos, screenPos)) {
                // Draw checkpoint as a larger marker
                float checkpointSize = 8.0f;
                ImColor checkpointColor = (i == mCurrentCheckpointIndex) ? 
                    ImColor(1.0f, 0.5f, 0.0f, 1.0f) : // Current checkpoint (orange)
                    ImColor(1.0f, 1.0f, 0.0f, 1.0f);  // Other checkpoints (yellow)
                
                drawList->AddCircleFilled(screenPos, checkpointSize, checkpointColor);
                
                // Draw lines connecting checkpoints
                if (i < mCheckpoints.size() - 1) {
                    const auto& nextCheckpoint = mCheckpoints[i + 1];
                    ImVec2 screenNext;
                    if (RenderUtils::worldToScreen(nextCheckpoint, screenNext)) {
                        drawList->AddLine(screenPos, screenNext, ImColor(1.0f, 1.0f, 0.0f, 0.5f), 1.0f);
                    }
                }
            }
        }
    }

    // Only render path blocks if path is found and rendering is enabled
    if (mPathFound && mRenderPath.mValue && !mPath.empty()) {
        // Render each node in the path
        for (size_t i = 0; i < mPath.size(); i++) {
            const auto& pathPos = mPath[i];
            AABB pathAABB = AABB(pathPos, glm::vec3(1.f, 1.f, 1.f));
            std::vector<ImVec2> pathPoints = MathUtils::getImBoxPoints(pathAABB);

            if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Outline) {
                drawList->AddPolyline(pathPoints.data(), pathPoints.size(), PATH_COLOR, 0, 2.0f);
            }
            if (mRenderMode.mValue == BlockRenderMode::Both || mRenderMode.mValue == BlockRenderMode::Filled) {
                drawList->AddConvexPolyFilled(pathPoints.data(), pathPoints.size(), ImColor(PATH_COLOR.Value.x, PATH_COLOR.Value.y, PATH_COLOR.Value.z, 0.25f));
            }

            // Draw lines connecting path nodes
            if (i < mPath.size() - 1) {
                const auto& nextPos = mPath[i + 1];
                glm::vec3 currentCenter = pathPos + glm::vec3(0.5f, 0.5f, 0.5f);
                glm::vec3 nextCenter = nextPos + glm::vec3(0.5f, 0.5f, 0.5f);
                
                ImVec2 screenCurrent, screenNext;
                if (RenderUtils::worldToScreen(currentCenter, screenCurrent) && RenderUtils::worldToScreen(nextCenter, screenNext)) {
                    drawList->AddLine(screenCurrent, screenNext, PATH_COLOR, 2.0f);
                }
            }
        }
    }
}

bool AutoMove::isWalkableBlock(const Block* block) const {
    if (!block) return false;
    
    int blockId = block->mLegacy->getBlockId();
    
    // Check if the block is solid and can be walked on
    // Exclude liquids (IDs 8-11) and air (ID 0)
    bool isLiquid = (blockId >= 8 && blockId <= 11);
    
    return blockId != 0 && !isLiquid && block->mLegacy->mMaterial->mIsBlockingMotion;
}

float AutoMove::calculateHeuristic(const glm::vec3& a, const glm::vec3& b) const {
    // Improved heuristic function using a combination of Manhattan distance and Euclidean distance
    // This provides better pathfinding for longer distances
    float dx = std::abs(a.x - b.x);
    float dy = std::abs(a.y - b.y);
    float dz = std::abs(a.z - b.z);
    
    // Manhattan distance for horizontal movement
    float manhattan = dx + dz;
    
    // Euclidean distance for overall direction
    float euclidean = std::sqrt(dx*dx + dy*dy + dz*dz);
    
    // Weight vertical movement more to prefer paths that stay at similar heights
    // Use the configurable vertical penalty to allow users to adjust how much the algorithm avoids vertical movement
    float verticalPenalty = dy * mVerticalPenalty.mValue;
    
    // Combine the heuristics with a weight factor
    return 0.6f * manhattan + 0.4f * euclidean + verticalPenalty;
}

std::vector<glm::vec3> AutoMove::getNeighbors(const glm::vec3& position) const {
    std::vector<glm::vec3> neighbors;
    auto* blockSource = ClientInstance::get()->getBlockSource();
    if (!blockSource) return neighbors;
    
    // Check horizontal neighbors (same Y level)
    static const std::vector<glm::vec3> horizontalOffsets = {
        {1, 0, 0}, {-1, 0, 0}, {0, 0, 1}, {0, 0, -1},
        {1, 0, 1}, {1, 0, -1}, {-1, 0, 1}, {-1, 0, -1} // Diagonals
    };
    
    // Check for neighbors at the same level
    for (const auto& offset : horizontalOffsets) {
        glm::vec3 neighborPos = position + offset;
        glm::ivec3 floorPos = glm::floor(neighborPos);
        
        // Check if the block below is solid and walkable
        glm::ivec3 blockBelowPos = floorPos - glm::ivec3(0, 1, 0);
        Block* blockBelow = blockSource->getBlock(blockBelowPos);
        
        // Check if there's enough headroom (2 blocks)
        if (isWalkableBlock(blockBelow) && 
            BlockUtils::isAirBlock(floorPos) && 
            BlockUtils::isAirBlock(floorPos + glm::ivec3(0, 1, 0))) {
            
            // Check for collisions with blocks if collision avoidance is enabled
            if (mAvoidCollisions.mValue) {
                // Check if there's a clear path from current position to neighbor
                if (hasLineOfSight(position, neighborPos)) {
                    neighbors.push_back(floorPos);
                }
            } else {
                neighbors.push_back(floorPos);
            }
        }
    }
    
    // Check for step up (1 block up) - limit vertical movement to reduce excessive up/down
    for (const auto& offset : horizontalOffsets) {
        glm::vec3 neighborPos = position + offset + glm::vec3(0, 1, 0);
        glm::ivec3 floorPos = glm::floor(neighborPos);
        
        // Check if the block below is solid and walkable
        glm::ivec3 blockBelowPos = floorPos - glm::ivec3(0, 1, 0);
        Block* blockBelow = blockSource->getBlock(blockBelowPos);
        
        // Check if there's enough headroom (2 blocks)
        if (isWalkableBlock(blockBelow) && 
            BlockUtils::isAirBlock(floorPos) && 
            BlockUtils::isAirBlock(floorPos + glm::ivec3(0, 1, 0))) {
            
            // Check for collisions with blocks if collision avoidance is enabled
            if (mAvoidCollisions.mValue) {
                if (hasLineOfSight(position, neighborPos)) {
                    neighbors.push_back(floorPos);
                }
            } else {
                neighbors.push_back(floorPos);
            }
        }
    }
    
    // Check for step down (1 block down)
    for (const auto& offset : horizontalOffsets) {
        glm::vec3 neighborPos = position + offset - glm::vec3(0, 1, 0);
        glm::ivec3 floorPos = glm::floor(neighborPos);
        
        // Check if the block below is solid and walkable
        glm::ivec3 blockBelowPos = floorPos - glm::ivec3(0, 1, 0);
        Block* blockBelow = blockSource->getBlock(blockBelowPos);
        
        // Check if there's enough headroom (2 blocks)
        if (isWalkableBlock(blockBelow) && 
            BlockUtils::isAirBlock(floorPos) && 
            BlockUtils::isAirBlock(floorPos + glm::ivec3(0, 1, 0))) {
            
            // Check for collisions with blocks if collision avoidance is enabled
            if (mAvoidCollisions.mValue) {
                if (hasLineOfSight(position, neighborPos)) {
                    neighbors.push_back(floorPos);
                }
            } else {
                neighbors.push_back(floorPos);
            }
        }
    }
    
    return neighbors;
}

std::vector<glm::vec3> AutoMove::reconstructPath(PathNode* endNode) {
    std::vector<glm::vec3> path;
    PathNode* current = endNode;
    
    while (current != nullptr) {
        path.push_back(current->position);
        current = current->parent;
    }
    
    std::reverse(path.begin(), path.end());
    return path;
}

bool AutoMove::findPath(const glm::vec3& start, const glm::vec3& target) {
    // Generate checkpoints if needed
    if (mCheckpoints.empty()) {
        generateCheckpoints(start, target);
    }
    
    // Determine the current target based on checkpoints
    glm::vec3 currentTarget;
    if (mCurrentCheckpointIndex < mCheckpoints.size() - 1) {
        currentTarget = mCheckpoints[mCurrentCheckpointIndex + 1];
    } else {
        currentTarget = target;
    }
    
    // Improved A* pathfinding algorithm implementation
    // Use a priority queue for better performance with large open sets
    struct NodeCompare {
        bool operator()(const PathNode* a, const PathNode* b) const {
            return a->getFCost() > b->getFCost(); // Min-heap
        }
    };
    
    std::priority_queue<PathNode*, std::vector<PathNode*>, NodeCompare> openSet;
    std::unordered_map<glm::vec3, PathNode*> openMap; // For quick lookup
    std::unordered_set<glm::vec3> closedSet;
    
    // Create start node
    PathNode* startNode = new PathNode{
        glm::floor(start),
        0.0f,
        calculateHeuristic(start, currentTarget),
        nullptr
    };
    
    openSet.push(startNode);
    openMap[startNode->position] = startNode;
    
    // Set a maximum search distance to prevent searching too far
    float maxSearchDistance = glm::distance(start, currentTarget) * 2.0f;
    maxSearchDistance = std::max(maxSearchDistance, 100.0f); // At least 100 blocks
    maxSearchDistance = std::min(maxSearchDistance, MAX_SEARCH_DISTANCE); // Cap at MAX_SEARCH_DISTANCE
    
    // Track the closest node to target for partial paths
    PathNode* closestNode = startNode;
    float closestDistance = glm::distance(startNode->position, currentTarget);
    
    int iterations = 0;
    const int MAX_ITERATIONS = 5000; // Prevent infinite loops
    
    while (!openSet.empty() && iterations < MAX_ITERATIONS) {
        iterations++;
        
        // Get the node with the lowest F cost
        PathNode* currentNode = openSet.top();
        openSet.pop();
        
        // Remove from open map
        openMap.erase(currentNode->position);
        
        // If we reached the target (or close enough)
        float distanceToTarget = glm::distance(currentNode->position, currentTarget);
        if (distanceToTarget < 2.0f) {
            mPath = reconstructPath(currentNode);
            
            // Apply path smoothing if enabled
            if (mSmoothPath.mValue) {
                smoothPath();
            }
            
            // Clean up memory
            for (auto& pair : openMap) {
                delete pair.second;
            }
            
            // Don't delete currentNode as it's part of the path
            mPathFound = true;
            return true;
        }
        
        // Track closest node to target for partial paths
        if (distanceToTarget < closestDistance) {
            closestDistance = distanceToTarget;
            closestNode = currentNode;
        }
        
        // Move current node to closed set
        closedSet.insert(currentNode->position);
        
        // Check all neighbors
        for (const auto& neighborPos : getNeighbors(currentNode->position)) {
            // Skip if already evaluated or too far from start
            if (closedSet.find(neighborPos) != closedSet.end() ||
                glm::distance(start, neighborPos) > maxSearchDistance) {
                continue;
            }
            
            // Calculate new path cost
            float newGCost = currentNode->gCost + glm::distance(currentNode->position, neighborPos);
            
            // Check if neighbor is in open set
            auto neighborIt = openMap.find(neighborPos);
            bool inOpenSet = neighborIt != openMap.end();
            
            // If not in open set or better path found
            if (!inOpenSet || newGCost < neighborIt->second->gCost) {
                float hCost = calculateHeuristic(neighborPos, currentTarget);
                
                // If not in open set, create new node
                if (!inOpenSet) {
                    PathNode* newNode = new PathNode{
                        neighborPos,
                        newGCost,
                        hCost,
                        currentNode
                    };
                    openSet.push(newNode);
                    openMap[neighborPos] = newNode;
                } else {
                    // Update existing node
                    PathNode* existingNode = neighborIt->second;
                    existingNode->gCost = newGCost;
                    existingNode->hCost = hCost;
                    existingNode->parent = currentNode;
                    
                    // Need to reinsert into priority queue to update position
                    // This is a workaround since we can't update priority directly
                    openSet.push(existingNode);
                }
            }
        }
    }
    
    // If we couldn't find a complete path, use the closest node we found
    if (closestNode != nullptr && closestNode != startNode) {
        mPath = reconstructPath(closestNode);
        
        // Apply path smoothing if enabled
        if (mSmoothPath.mValue) {
            smoothPath();
        }
        
        // Clean up memory
        for (auto& pair : openMap) {
            delete pair.second;
        }
        
        mPathFound = true;
        return true; // Return true for partial path
    }
    
    // Clean up memory if no path found
    for (auto& pair : openMap) {
        delete pair.second;
    }
    
    // No path found
    mPathFound = false;
    return false;
}