#pragma once

#include <Features/Modules/Module.hpp>


class AutoTotem : public ModuleBase<AutoTotem> {
public:
    AutoTotem();

    void onEnable() override;
    void onDisable() override;
    // Called once per tick
    void onBaseTickEvent(class BaseTickEvent& event);

private:
    // Used to store the player's previous selected slot
    int mOldSlot = -1;
    // NumberSetting for choosing which hotbar slot to use (e.g. to drop its contents)
    NumberSetting mSlotIndex = NumberSetting("Slot index", "The hotbar slot index to use for desync", 8, 0, 8, 1);
};

#include "AutoTotem.hpp"

#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <Features/Events/BaseTickEvent.hpp>
#include <Features/FeatureManager.hpp>


AutoTotem::AutoTotem()
    : ModuleBase<AutoTotem>("AutoTotem",
        "Automatically switch Totems to offhand",
        ModuleCategory::Player,
        0,
        false),
    mOldSlot(-1)
{
    mNames = {
        {Lowercase,       "AutoTotem"},
        {LowercaseSpaced, "auto offhand"},
        {Normal,          "AutoTotem"},
        {NormalSpaced,    "Auto Offhand"}
    };

    // Add the NumberSetting to your module settings.
    addSetting(&mSlotIndex);
}

void AutoTotem::onEnable() {
    gFeatureManager->mDispatcher->listen<BaseTickEvent, &AutoTotem::onBaseTickEvent>(this);
}

void AutoTotem::onDisable() {
    gFeatureManager->mDispatcher->deafen<BaseTickEvent, &AutoTotem::onBaseTickEvent>(this);

    // Restore the old selected slot on disable if needed.
    auto localPlayer = ClientInstance::get()->getLocalPlayer();
    if (localPlayer && mOldSlot != -1) {
        localPlayer->getSupplies()->mSelectedSlot = mOldSlot;
        mOldSlot = -1;
    }
}

void AutoTotem::onBaseTickEvent(BaseTickEvent& event)
{
    auto localPlayer = ClientInstance::get()->getLocalPlayer();
    if (!localPlayer || localPlayer->isDead())
    {
        // If we can't act, restore the previous slot.
        if (mOldSlot != -1) {
            localPlayer->getSupplies()->mSelectedSlot = mOldSlot;
            mOldSlot = -1;
        }
        return;
    }

    // 1) Check if the offhand already has a Totem.
    {
        auto offhandContainer = localPlayer->getOffhandContainer();
        if (offhandContainer)
        {
            // Assume offhand slot is index 0 (adjust if needed).
            int offhandSlotIndex = 0;
            auto offhandStack = offhandContainer->getItem(offhandSlotIndex);
            if (offhandStack && offhandStack->mItem)
            {
                // mItem is an Item**; dereference it.
                Item** offhandItemPtr = offhandStack->mItem;
                if (offhandItemPtr && *offhandItemPtr)
                {
                    Item* realOffhandItem = *offhandItemPtr;
                    if (realOffhandItem->mName.find("totem_of_undying") != std::string::npos)
                    {
                        // Totem is present in offhand.
                        if (mOldSlot != -1) {
                            localPlayer->getSupplies()->mSelectedSlot = mOldSlot;
                            mOldSlot = -1;
                        }
                        return;
                    }
                }
            }
        }
    }

    // 2) No Totem in offhand; proceed.
    auto mainInventory = localPlayer->getSupplies()->getContainer();
    if (!mainInventory)
        return;

    // 3) Save the current slot if not already saved, then switch to the slot defined by our setting.
    int dropSlotIndex = mSlotIndex.mValue;
    if (mOldSlot == -1)
        mOldSlot = localPlayer->getSupplies()->mSelectedSlot;

    // Only switch if not already using the desired drop slot.
    if (localPlayer->getSupplies()->mSelectedSlot != dropSlotIndex)
        localPlayer->getSupplies()->mSelectedSlot = dropSlotIndex;

    // 4) Drop any items from the drop slot unconditionally.
    {
        ItemStack* dropStack = mainInventory->getItem(dropSlotIndex);
        if (dropStack && dropStack->mItem)
        {
            mainInventory->dropSlot(dropSlotIndex);
           // ChatUtils::displayClientMessage("Dropped items from slot #" + std::to_string(dropSlotIndex));
        }
    }

    // 5) Search the main inventory (slots 0..35) for a Totem.
    std::string totemName = "totem_of_undying";
    for (int i = 0; i < 36; i++)
    {
        auto stack = mainInventory->getItem(i);
        if (!stack || !stack->mItem)
            continue;

        Item** itemPtr = stack->mItem;
        if (itemPtr && *itemPtr)
        {
            Item* realItem = *itemPtr;
            // (Optional debug message)
            //ChatUtils::displayClientMessage("Checking slot " + std::to_string(i) + ": " + realItem->mName);
            if (realItem->mName.find(totemName) != std::string::npos)
            {
                // Totem found in slot i. Move it to offhand.
                mainInventory->setOffhandItem(i);
               // ChatUtils::displayClientMessage("AutoTotem: Moved Totem from slot " + std::to_string(i) + " to offhand!");
                // Restore the player's old selected slot.
                localPlayer->getSupplies()->mSelectedSlot = mOldSlot;
                mOldSlot = -1;
                break;
            }
        }
    }
}
