#include "TestCommand.hpp"
#include <Utils/GameUtils/ChatUtils.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>
#include <SDK/Minecraft/Inventory/PlayerInventory.hpp>
#include <magic_enum.hpp>

void TestCommand::execute(const std::vector<std::string>& args)
{
    auto player = ClientInstance::get()->getLocalPlayer();
    if (!player) {
        ChatUtils::displayClientMessage("§cNo local player found!");
        return;
    }

    // ✅ **Get the currently held item**
    auto selectedSlot = player->getSupplies()->mSelectedSlot;
    auto itemStack = player->getSupplies()->getContainer()->getItem(selectedSlot);

    if (!itemStack || !itemStack->mItem) {
        ChatUtils::displayClientMessage("§eYou are not holding any item.");
        return;
    }

    auto item = itemStack->getItem();
    std::string itemName = item->mName;
    short itemID = item->mItemId;

    // ✅ **Print item info and player's name**
    ChatUtils::displayClientMessage("§bPlayer: §f" + player->getRawName());
    ChatUtils::displayClientMessage("§aHolding Item: §f" + itemName + " §7(ID: " + std::to_string(itemID) + ")");
}

std::vector<std::string> TestCommand::getAliases() const
{
    return { };
}

std::string TestCommand::getDescription() const
{
    return "Displays the currently held item and player name.";
}

std::string TestCommand::getUsage() const
{
    return "Usage: .test";
}
