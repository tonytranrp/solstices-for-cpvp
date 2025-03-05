#pragma once

#include <Features/Modules/Module.hpp>
#include <Features/FeatureManager.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/Events/PacketOutEvent.hpp>
#include <Features/Events/PacketInEvent.hpp>
#include <Features/Events/PingUpdateEvent.hpp>
#include <Features/Events/SendImmediateEvent.hpp>
#include <Features/Modules/Visual/Interface.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <SDK/Minecraft/Actor/GameMode.hpp>
#include <SDK/Minecraft/Network/PacketID.hpp>
#include <SDK/Minecraft/Network/Packets/InventoryTransactionPacket.hpp>
#include <SDK/Minecraft/Network/Packets/PlayerAuthInputPacket.hpp>
#include <SDK/Minecraft/Network/Packets/MobEquipmentPacket.hpp>
#include <SDK/Minecraft/Network/Packets/LevelEventPacket.hpp>
#include <SDK/Minecraft/Network/Packets/UpdateBlockPacket.hpp>
#include <SDK/Minecraft/World/BlockLegacy.hpp>
#include <SDK/Minecraft/World/Level.hpp>
#include <SDK/Minecraft/World/HitResult.hpp>
#include <Features/Modules/Player/ChestStealer.hpp>
#include <Features/Modules/Player/Scaffold.hpp>
class DestroyProgress : public ModuleBase<DestroyProgress>
{
public:
    enum class ColorMode {
        Default,
        Theme
    };

    EnumSettingT<ColorMode> mColorMode = EnumSettingT<ColorMode>("Color", "The color mode", ColorMode::Default, "Default", "Theme");
    NumberSetting mOpacity = NumberSetting("Opacity", "The opacity of box", 0.40f, 0.f, 1.f, 0.01f);
    BoolSetting mFilled = BoolSetting("Filled", "Fill box", true);
    BoolSetting mShowOthers = BoolSetting("Show Others", "Show other destroy Block Pogress:)", false);

    DestroyProgress() : ModuleBase("DestroyProgress", "Render Destroy Progress", ModuleCategory::Visual, 0, false) {
        addSettings(&mColorMode, &mOpacity, &mFilled,&mShowOthers);

        mNames = {
            {Lowercase, "destroyprogress"},
            {LowercaseSpaced, "destroy progress"},
            {Normal, "DestroyProgress"},
            {NormalSpaced, "Destroy Progress"}
        };
    }

    void onEnable() override;
    void onDisable() override;
    void onRenderEvent(class RenderEvent& event);
    void onBaseTickEvent(class BaseTickEvent& event);
    void onPacketInEvent(class PacketInEvent& event);
};