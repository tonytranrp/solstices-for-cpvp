//
// Created by vastrakai on 7/5/2024.
//

#include "PlayerInventory.hpp"

#include <libhat/Access.hpp>
#include <SDK/OffsetProvider.hpp>
#include <SDK/SigManager.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Actor/GameMode.hpp>
#include <SDK/Minecraft/Inventory/Item.hpp>
#include <SDK/Minecraft/Network/LoopbackPacketSender.hpp>
#include <SDK/Minecraft/Network/MinecraftPackets.hpp>
#include <SDK/Minecraft/Network/Packets/InventoryTransactionPacket.hpp>

#include "ContainerManagerModel.hpp"



void Inventory::dropSlot(int slot)
{
    ItemStack* itemStack = getItem(slot);

    if (!itemStack->mItem) return;

    static ItemStack blankStack = ItemStack();

    InventoryAction action = InventoryAction(slot, itemStack, &blankStack);
    InventoryAction action2 = InventoryAction(0, &blankStack, itemStack);
    action.mSource.mContainerId = static_cast<int>(ContainerID::Inventory);
    action2.mSource.mContainerId = static_cast<int>(ContainerID::Inventory);
    action.mSource.mType = InventorySourceType::ContainerInventory;
    action2.mSource.mType = InventorySourceType::WorldInteraction;

    auto pkt = MinecraftPackets::createPacket<InventoryTransactionPacket>();

    auto cit = std::make_unique<ComplexInventoryTransaction>();
    cit->data.addAction(action);
    cit->data.addAction(action2);

    pkt->mTransaction = std::move(cit);
    ClientInstance::get()->getPacketSender()->sendToServer(pkt.get());

    //setItem(slot, &blankStack);
}

void Inventory::swapSlots(int from, int to)
{
    ItemStack* item1 = getItem(from);
    ItemStack* item2 = getItem(to);

    auto action1 = InventoryAction(from, item1, item2);
    auto action2 = InventoryAction(to, item2, item1);

    action1.mSource.mType = InventorySourceType::ContainerInventory;
    action2.mSource.mType = InventorySourceType::ContainerInventory;
    action1.mSource.mContainerId = static_cast<int>(ContainerID::Inventory);
    action2.mSource.mContainerId = static_cast<int>(ContainerID::Inventory);

    auto pkt = MinecraftPackets::createPacket<InventoryTransactionPacket>();

    auto cit = std::make_unique<ComplexInventoryTransaction>();

    cit->data.addAction(action1);
    cit->data.addAction(action2);

    /*setItem(from, item2);
    setItem(to, item1);*/

    pkt->mTransaction = std::move(cit);
    ClientInstance::get()->getPacketSender()->sendToServer(pkt.get());
}
// Inventory.cpp

// Inventory.cpp

// Inventory.cpp

void Inventory::setOffhandItem(int slot)
{
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) return;

    // 1) Get the stack from this container's slot (the main inventory).
    ItemStack* mainInventoryStack = getItem(slot);
    if (!mainInventoryStack || !mainInventoryStack->mItem)
        return; // no valid item to move

    // 2) Get the current offhand stack (if any).
    //    We’ll assume offhand slot index = 0. Adjust if your reversing says 1, etc.
    int offhandSlotIndex = 0;
    ItemStack* offhandStack = nullptr;

    if (auto offhandContainer = player->getOffhandContainer())
        offhandStack = offhandContainer->getItem(offhandSlotIndex);

    //
    // We’ll build two InventoryActions to form a normal “swap” transaction:
    //   - from main slot -> offhand slot
    //   - from offhand slot -> main slot
    // If offhand is empty, this effectively moves the item to offhand.
    // If offhand has an item, it swaps them.
    //

    // Action #1: mainInventoryStack moves from the main inventory (slot) to the offhand slot.
    InventoryAction actionFromMain(
        /*slot*/             slot,
        /*fromItem*/         mainInventoryStack,
        /*toItem*/           offhandStack // what was in offhand (might be nullptr)
    );
    actionFromMain.mSource.mType = InventorySourceType::ContainerInventory;
    actionFromMain.mSource.mContainerId = static_cast<char>(ContainerID::Inventory); // 0

    // Action #2: offhandStack goes from offhand slot back to the main slot.
    // If offhandStack is empty, this effectively just removes mainInventoryStack from that slot.
    InventoryAction actionToOffhand(
        /*slot*/             offhandSlotIndex,
        /*fromItem*/         offhandStack,
        /*toItem*/           mainInventoryStack
    );
    actionToOffhand.mSource.mType = InventorySourceType::ContainerInventory;
    actionToOffhand.mSource.mContainerId = static_cast<char>(ContainerID::Offhand); // 119

    // 3) Build a ComplexInventoryTransaction with these two actions
    auto transaction = std::make_unique<ComplexInventoryTransaction>();
    transaction->data.addAction(actionFromMain);
    transaction->data.addAction(actionToOffhand);

    // 4) Create the InventoryTransactionPacket and attach your transaction
    auto pkt = MinecraftPackets::createPacket<InventoryTransactionPacket>();
    pkt->mTransaction = std::move(transaction);

    // 5) Send to server
    ClientInstance::get()->getPacketSender()->sendToServer(pkt.get());

    //
    // 6) (Optional) Locally update the item stacks so the client sees the result immediately
    //    Without waiting for server → client sync
    //
    //   if (auto offhandContainer = player->getOffhandContainer())
    //   {
    //       // Put old offhand item in the main slot
    //       setItem(slot, offhandStack);
    //       // Put the new item in offhand
    //       offhandContainer->setItem(offhandSlotIndex, mainInventoryStack);
    //   }
}


void Inventory::equipArmor(int slot)
{
    auto player = ClientInstance::get()->getLocalPlayer();

    ItemStack* itemStack = getItem(slot);

    if (!itemStack->mItem) return;

    static ItemStack blankStack = ItemStack();

    Item* item = *itemStack->mItem;
    // Get the current item stack in the armor slot
    ItemStack* armorStack = player->getArmorContainer()->getItem(item->getArmorSlot());

    InventoryAction action = InventoryAction(slot, itemStack, armorStack);
    action.mSource.mType = InventorySourceType::ContainerInventory;
    action.mSource.mContainerId = static_cast<int>(ContainerID::Inventory);

    InventoryAction action2 = InventoryAction(item->getArmorSlot(), armorStack, itemStack);
    action2.mSource.mType = InventorySourceType::ContainerInventory;
    action2.mSource.mContainerId = static_cast<int>(ContainerID::Armor);


    auto pkt = MinecraftPackets::createPacket<InventoryTransactionPacket>();

    auto cit = std::make_unique<ComplexInventoryTransaction>();
    cit->data.addAction(action);
    cit->data.addAction(action2);

    pkt->mTransaction = std::move(cit);
    ClientInstance::get()->getPacketSender()->sendToServer(pkt.get());

    /*setItem(slot, armorStack);
    player->getArmorContainer()->setItem(item->getArmorSlot(), itemStack);*/
}

void Inventory::startUsingItem(int slot) {
    auto player = ClientInstance::get()->getLocalPlayer();

    auto pkt = MinecraftPackets::createPacket<InventoryTransactionPacket>();

    auto cit = std::make_unique<ItemUseInventoryTransaction>();
    cit->mActionType = ItemUseInventoryTransaction::ActionType::Use;
    cit->mSlot = slot;
    cit->mItemInHand = *player->getSupplies()->getContainer()->getItem(slot);


    pkt->mTransaction = std::move(cit);

    ClientInstance::get()->getPacketSender()->sendToServer(pkt.get());
}

void Inventory::releaseUsingItem(int slot)
{
    auto player = ClientInstance::get()->getLocalPlayer();

    int oldSlot = player->getSupplies()->mSelectedSlot;
    player->getSupplies()->mSelectedSlot = slot;
    player->getGameMode()->releaseUsingItem();
    player->getSupplies()->mSelectedSlot = oldSlot;
}

Inventory* PlayerInventory::getContainer()
{
    return hat::member_at<Inventory*>(this, OffsetProvider::PlayerInventory_mContainer);
}
