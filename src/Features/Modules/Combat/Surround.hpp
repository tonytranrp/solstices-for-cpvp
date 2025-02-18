#pragma once
//
// Surround.hpp
// Automatically places blocks around the player for protection.
// (Adapted to Solstice code style)
//
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/World/BlockSource.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Network/Packets/InventoryTransactionPacket.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>
#include <Utils/GameUtils/ActorUtils.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <Features/Modules/Module.hpp>
#include <vector>
#include <glm/glm.hpp>

// Note: Depending on your project, ensure that types like ColorSetting exist.
// For this example we assume a ColorSetting that can convert to ImColor via .toImColor().
class Surround : public ModuleBase<Surround> {
public:
    // Settings
    BoolSetting mCenter = BoolSetting("Center", "Center the player on the block grid", true);
    BoolSetting mDisableComplete = BoolSetting("Disable Complete", "Disable module when all surround blocks are placed", false);
    // Switch mode: 0 = None, 1 = Full, 2 = Silent
    EnumSettingT<int> mSwitchMode = EnumSettingT<int>("Switch", "Hotbar switching mode for block placement", 0, "None", "Full", "Silent");
    BoolSetting mRender = BoolSetting("Render", "Render surround placement positions", true);
    BoolSetting mPlaceAbove = BoolSetting("Place Above", "Also place a block above your head", false);


    Surround();

    // Module lifecycle methods
    void onEnable() override;
    void onDisable() override;
    void onBaseTickEvent(class BaseTickEvent& event) ;
    void onPacketOutEvent(class PacketOutEvent& event) ;
    void onRenderEvent(class RenderEvent& event) ;

private:
    // List of block positions (in world block coordinates) that we need to place
    std::vector<glm::ivec3> mBlocksToPlace;

    // Calculates positions around the player where blocks should be placed
    void calculateBlockPositions();

    // Attempts to place a block at the given position using proper switching and packet placement
    void placeBlockAt(const glm::ivec3& pos);
};
