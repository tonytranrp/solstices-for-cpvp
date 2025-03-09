#pragma once

#include <vector>
#include <glm/glm.hpp>
#include <string>
#include <cfloat>
#include <unordered_set>

#include <Features/Modules/Module.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>

// Surround module – places blocks around the player for protection.
class Surround : public ModuleBase<Surround> {
public:
    // Settings
    BoolSetting mCenter = BoolSetting("Center", "Center the player on the block grid", true);
    BoolSetting mDisableComplete = BoolSetting("Disable Complete", "Disable module when no blocks are left to place", false);
    // 0 = None, 1 = Full, 2 = Silent
    EnumSettingT<int> mSwitchMode = EnumSettingT<int>("Switch", "Hotbar switching mode for block placement", 0, "None", "Full", "Silent");
    BoolSetting mRender = BoolSetting("Render", "Render placement positions", true);
    BoolSetting mPlaceAbove = BoolSetting("Place Above", "Also place a block above your head", false);
    // Dynamic surround: if enabled, only the outer ring of a square of configurable range is used.
    BoolSetting mDynamic = BoolSetting("Dynamic", "Enable dynamic (perimeter-only) surround", false);
    NumberSetting mRange = NumberSetting("Range", "Range for dynamic surround (outer ring)", 1, 1, 4, 1);
    // Rotation: rotate to each placement position.
    BoolSetting mRotate = BoolSetting("Rotate", "Rotate to block placement positions", true);
    // Stability threshold: how much the player can move before recalculating placements
    NumberSetting mStabilityThreshold = NumberSetting("Stability", "Movement threshold before recalculating (blocks)", 0.15f, 0.0f, 1.0f, 0.01f);

    Surround();

    void onEnable() override;
    void onDisable() override;
    void onBaseTickEvent(class BaseTickEvent& event) ;
    void onPacketOutEvent(class PacketOutEvent& event) ;
    void onRenderEvent(class RenderEvent& event) ;

private:
    // A hash functor for glm::ivec3 so we can store it in an unordered_set.
    struct Vec3iHash {
        std::size_t operator()(const glm::ivec3& v) const {
            std::size_t h1 = std::hash<int>()(v.x);
            std::size_t h2 = std::hash<int>()(v.y);
            std::size_t h3 = std::hash<int>()(v.z);
            return h1 ^ (h2 << 1) ^ (h3 << 2);
        }
    };
    // List of world block positions (as ivec3) to place blocks.
    std::vector<glm::ivec3> mBlocksToPlace;
    
    // Last player position used for stability calculations
    glm::vec3 mLastPlayerPos = glm::vec3(0.0f);
    
    // Last calculated base position
    glm::ivec3 mLastBasePos = glm::ivec3(0);
    
    // Places a block at a given world coordinate.
    void placeBlockAt(const glm::ivec3& pos);
    
    // Generates dynamic surround positions based on range
    void generateDynamicPositions(const glm::ivec3& basePos, std::unordered_set<glm::ivec3, Vec3iHash>& blocksSet);
    
    // Checks if the player has moved significantly from last position
    bool hasPlayerMovedSignificantly(const glm::vec3& currentPos);
};

