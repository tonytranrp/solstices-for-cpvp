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

    Surround();

    void onEnable() override;
    void onDisable() override;
    void onBaseTickEvent(class BaseTickEvent& event) ;
    void onPacketOutEvent(class PacketOutEvent& event) ;
    void onRenderEvent(class RenderEvent& event) ;

private:
    // List of world block positions (as ivec3) to place blocks.
    std::vector<glm::ivec3> mBlocksToPlace;

    // Places a block at a given world coordinate.
    void placeBlockAt(const glm::ivec3& pos);
};

