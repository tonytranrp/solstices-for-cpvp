//
// Created by vastrakai on 10/1/2024.
//

#include "AutoPath.hpp"
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/World/HitResult.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/RenderEvent.hpp>




BlockSource* AutoPath::cachedSrc = nullptr;
const float AutoPath::SQRT_2 = sqrtf(2.f);

std::vector<Vec3i> AutoPath::sideAdj = {
    {1, 0, 0},
    {0, 0, 1},
    {-1, 0, 0},
    {0, 0, -1}
};

std::vector<Vec3i> AutoPath::directAdj = {
    {1, 0, 0},
    {0, 0, 1},
    {-1, 0, 0},
    {0, 0, -1},
    {0, 1, 0},
    {0, -1, 0}
};

std::vector<Vec3i> AutoPath::diagAdd = {
    {1, 0, 1},
    {-1, 0, 1},
    {-1, 0, -1},
    {1, 0, -1}
};

struct ScoreList {
    float gScore;
    float fScore;

    ScoreList() {
        this->gScore = 694206942069420.f;
        this->fScore = 694206942069420.f;
    }

    ScoreList(const float g, const float f) {
        this->gScore = g;
        this->fScore = f;
    }
};

static Vec3* getPlayerHitboxPathPosOffsets() {
    static Vec3 res[8] = {
        {0.17f, -0.99f, 0.17f},
        {0.83f, -0.99f, 0.17f},
        {0.83f, -0.99f, 0.83f},
        {0.17f, -0.99f, 0.83f},
        {0.17f, 0.9f, 0.17f},
        {0.83f, 0.9f, 0.17f},
        {0.83f, 0.9f, 0.83f},
        {0.17f, 0.9f, 0.83f}
    };

    return res;
}

__forceinline float AutoPath::heuristicEstimation(const Vec3i& node, const Vec3i& target) {
    const auto diff = node.sub(target);
    int dx = abs(diff.x), dz = abs(diff.z);
    float straight = (dx > dz) ? float(dx - dz) : float(dz - dx);
    float diagonal = (dx > dz) ? float(dz) : float(dx);
    diagonal *= SQRT_2;
    return straight + diagonal + float(abs(target.y - node.y));
}

inline bool AutoPath::isCompletelyObstructed(const Vec3i& pos) {
    const auto block = cachedSrc->getBlock(pos.toGlm());
    if (block->toLegacy()->getmMaterial()->getmIsBlockingMotion() ||
        block->toLegacy()->getmSolid() ||
        block->toLegacy()->getmMaterial()->getmIsBlockingPrecipitation() ||
        block->toLegacy()->getBlockId() != 0)
        return true;
    return false;
}

std::vector<std::pair<Vec3i, float>> AutoPath::getAirAdjacentNodes(const Vec3i& node, const Vec3i& start, Vec3i& goal) {
    std::vector<std::pair<Vec3i, float>> res;
    res.reserve(10);
    std::vector<bool> sideWorks(sideAdj.size(), false);

    // Check side (horizontal) movements.
    for (size_t i = 0; i < sideAdj.size(); i++) {
        Vec3i curr = node.add(sideAdj[i]);
        if (curr.dist(start) <= 100 && !isCompletelyObstructed(curr)) {
            res.emplace_back(curr, 1.f);
            sideWorks[i] = true;
        }
    }

    // Top movement: for flight, only check that the space above is clear.
    {
        Vec3i curr = node.add(0, 1, 0);
        if (curr.dist(start) <= 100 && !isCompletelyObstructed(curr))
            res.emplace_back(curr, 1.f);
    }

    // Bottom movement: ensure both the immediate block below and the one further down are clear.
    {
        Vec3i down1 = node.sub(0, 1, 0);
        Vec3i down2 = node.sub(0, 2, 0);
        if (down1.dist(start) <= 100 && !isCompletelyObstructed(down1) && !isCompletelyObstructed(down2))
            res.emplace_back(down1, 1.f);
    }

    // Diagonal movements: allow only if both adjacent sides are clear.
    {
        std::vector<bool> wrappedSideWorks = sideWorks;
        wrappedSideWorks.push_back(sideWorks[0]);
        for (size_t i = 0; i < 4; i++) {
            if (sideWorks[i] && wrappedSideWorks[i + 1]) {
                Vec3i curr = node.add(diagAdd[i]);
                if (curr.dist(start) <= 100 && !isCompletelyObstructed(curr))
                    res.emplace_back(curr, SQRT_2);
            }
        }
    }
    return res;
}

std::vector<Vec3> AutoPath::findFlightPath(Vec3i start, Vec3i goal, BlockSource* src, float howClose, bool optimizePath, int64_t timeout, bool debugMsgs) {
    cachedSrc = src;
    std::map<Vec3i, Vec3i> cameFrom;
    std::map<Vec3i, ScoreList> scores;

    // Priority queue element: pair<fScore, Vec3i>
    typedef std::pair<float, Vec3i> PQElement;
    struct ComparePQ {
        bool operator()(const PQElement& a, const PQElement& b) const {
            return a.first > b.first; // min-heap: lower fScore has higher priority
        }
    };
    std::priority_queue<PQElement, std::vector<PQElement>, ComparePQ> openSet;

    float startH = heuristicEstimation(start, goal);
    scores[start] = ScoreList(0.f, startH);
    openSet.push({ startH, start });

    Vec3 closestPoint = start.toVec3t();
    float bestHeuristic = startH;
    int64_t startTime = NOW;
    bool pathFound = false;
    Vec3i pathEnd = start;

    while (!openSet.empty()) {
        auto [currentF, currNode] = openSet.top();
        openSet.pop();

        // Skip outdated entries.
        if (currentF > scores[currNode].fScore)
            continue;

        // Current node.
        Vec3i current = currNode;

        // Check if we reached the goal.
        if (heuristicEstimation(current, goal) <= howClose) {
            pathFound = true;
            pathEnd = current;
            break;
        }

        // Unified timeout check.
        if (NOW - startTime > timeout) {
            if (debugMsgs)
                ChatUtils::displayClientMessage("§cPathfinding timed out!");
            break;
        }

        // Process each neighbor.
        for (auto& [neighbor, cost] : getAirAdjacentNodes(current, start, goal)) {
            float tentative_gScore = scores[current].gScore + cost;
            if (scores.find(neighbor) == scores.end() || tentative_gScore < scores[neighbor].gScore) {
                cameFrom[neighbor] = current;
                float h = heuristicEstimation(neighbor, goal);
                scores[neighbor] = ScoreList(tentative_gScore, tentative_gScore + h);
                openSet.push({ tentative_gScore + h, neighbor });
                if (h < bestHeuristic) {
                    bestHeuristic = h;
                    closestPoint = neighbor.toVec3t();
                }
            }
        }
    }

    // Reconstruct path: if a full path was found, reconstruct from the goal;
    // otherwise, use the closest point reached.
    std::vector<Vec3> path;
    Vec3i currentReconstruction = pathFound ? pathEnd : Vec3i(closestPoint);
    path.push_back(currentReconstruction.toVec3t());
    while (cameFrom.find(currentReconstruction) != cameFrom.end()) {
        currentReconstruction = cameFrom[currentReconstruction];
        path.push_back(currentReconstruction.toVec3t());
    }
    std::reverse(path.begin(), path.end());

    // Optionally optimize path by removing unnecessary waypoints using line-of-sight checks.
    if (optimizePath && path.size() >= 2) {
        int startIdx = 0;
        int endIdx = path.size() - 1;
        while (startIdx < static_cast<int>(path.size()) - 1) {
            while (endIdx - startIdx > 1) {
                bool hasLineOfSight = true;
                Vec3* offsets = getPlayerHitboxPathPosOffsets();
                for (int li = 0; li < 8; li++) {
                    Vec3 startCheck = path[startIdx].add(offsets[li]);
                    Vec3 endCheck = path[endIdx].add(offsets[li]);
                    HitResult rt = src->checkRayTrace(startCheck.toGlm(), endCheck.toGlm());
                    if (rt.mType == HitType::BLOCK) {
                        hasLineOfSight = false;
                        break;
                    }
                    rt = src->checkRayTrace(startCheck.sub(0, 1, 0).toGlm(), endCheck.sub(0, 1, 0).toGlm());
                    if (rt.mType == HitType::BLOCK) {
                        hasLineOfSight = false;
                        break;
                    }
                }
                if (hasLineOfSight) {
                    startIdx++;
                    path.erase(path.begin() + startIdx, path.begin() + endIdx);
                    endIdx = path.size() - 1;
                }
                else {
                    endIdx--;
                }
            }
            startIdx++;
            endIdx = path.size() - 1;
        }
    }

    // Center x and z coordinates.
    for (auto& pos : path) {
        pos.x = floorf(pos.x) + 0.5f;
        pos.z = floorf(pos.z) + 0.5f;
    }

    if (debugMsgs) {
        if (pathFound)
            ChatUtils::displayClientMessage("§aFound path!");
        else
            ChatUtils::displayClientMessage("§6Found partial path!");
    }

    return path;
}

std::vector<glm::vec3> AutoPath::findFlightPathGlm(glm::vec3 start, glm::vec3 goal, BlockSource* src, float howClose, bool optimizePath, int64_t timeout, bool debugMsgs) {
    auto path = findFlightPath(Vec3i(start), Vec3i(goal), src, howClose, optimizePath, timeout, debugMsgs);
    std::vector<glm::vec3> pathGlm;
    for (const auto& pos : path)
        pathGlm.push_back(pos.toGlm());
    return pathGlm;
}



void AutoPath::onEnable()
{
    if (ClientInstance::get()->getLocalPlayer() == nullptr)
        setEnabled(false);

    mPosList.clear();
    mTicks = 0;

    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoPath::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->listen<RenderEvent, &AutoPath::onRenderEvent>(this);
}

void AutoPath::onDisable()
{
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoPath::onBaseTickEvent>(this);
    gFeatureManager->mDispatcher->deafen<RenderEvent, &AutoPath::onRenderEvent>(this);
    mPosList.clear();
    mTicks = 0;
}

void AutoPath::onBaseTickEvent(BaseTickEvent& event)
{
    /*for (const auto target : targetList) {
        const auto targetPos = *target->getPos();
        const auto path = findFlightPath(pos, targetPos, region, howClose, true);
        this->posList = path;
        break;
    }*/
    std::lock_guard<std::mutex> lock(mMutex);

    auto player = ClientInstance::get()->getLocalPlayer();
    if (player == nullptr)
        return;

    auto actors = ActorUtils::getActorList(false, false);
    std::erase(actors, player);

    std::ranges::sort(actors, [&](Actor* a, Actor* b) -> bool
    {
        return glm::distance(*a->getPos(), *player->getPos()) < glm::distance(*b->getPos(), *player->getPos());
    });

    if (actors.empty()) return;

    auto target = actors.at(0);
    if (target == nullptr) return;

    Vec3i targetng = Vec3i(target->getPos()->x, target->getPos()->y, target->getPos()->z);
    Vec3i srcng = Vec3i(player->getPos()->x, player->getPos()->y, player->getPos()->z);
    const auto targetPos = *target->getPos();
    const auto path = findFlightPath(srcng, targetng, ClientInstance::get()->getBlockSource(), mHowClose.mValue, true, mPathTimeout.mValue * 1000, mDebug.mValue);
    // Convert Vec3i to glm::vec3
    std::vector<glm::vec3> pathGlm;
    for (const auto pos : path) pathGlm.push_back(glm::vec3(pos.x, pos.y, pos.z));
    this->mPosList = pathGlm;
    spdlog::info("Added {} nodes to path", path.size());

}

void AutoPath::onRenderEvent(RenderEvent& event)
{
    std::lock_guard<std::mutex> lock(mMutex);

    if (mPosList.empty())
        return;

    auto player = ClientInstance::get()->getLocalPlayer();
    if (player == nullptr)
        return;

    auto pos = *player->getPos();
    auto region = ClientInstance::get()->getBlockSource();

    auto drawList = ImGui::GetBackgroundDrawList();

    std::vector<ImVec2> points;

    for (auto pos : mPosList)
    {
        ImVec2 point;
        if (!RenderUtils::worldToScreen(pos, point)) continue;
        points.emplace_back(point);
    }

    // Connect each point to form a line
    if (!points.empty())
        for (int i = 0; i < points.size() - 1; i++)
        {
            drawList->AddLine(points[i], points[i + 1], IM_COL32(0, 255, 0, 255), 2.0f);
        }

    for (auto pos : mPosList)
    {
        auto drawList = ImGui::GetBackgroundDrawList();

        AABB aabb = AABB(pos, glm::vec3(0.2f, 0.2f, 0.2f));
        auto points = MathUtils::getImBoxPoints(aabb);

        drawList->AddConvexPolyFilled(points.data(), points.size(), IM_COL32(255, 0, 0, 100));
        drawList->AddPolyline(points.data(), points.size(), IM_COL32(255, 0, 0, 255), 0, 2.f);
    }
}