//
// Created by AI Assistant
//

#include "AutoMoveCommand.hpp"

#include <Features/FeatureManager.hpp>
#include <Features/Modules/Movement/AutoMove.hpp>
#include <Utils/GameUtils/ChatUtils.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Actor/Actor.hpp>

void AutoMoveCommand::execute(const std::vector<std::string>& args)
{
    auto autoMove = gFeatureManager->mModuleManager->getModule<AutoMove>();
    if (!autoMove) {
        ChatUtils::displayClientMessage("§cERROR: AutoMove module not found.");
        return;
    }

    if (args.size() != 4) {
        ChatUtils::displayClientMessage("§c" + getUsage());
        return;
    }

    try {
        float x = std::stof(args[1]);
        float y = std::stof(args[2]);
        float z = std::stof(args[3]);

        // Update the target position settings
        autoMove->mTargetX.mValue = x;
        autoMove->mTargetY.mValue = y;
        autoMove->mTargetZ.mValue = z;

        // Enable the module if it's not already enabled
        if (!autoMove->mEnabled) {
            autoMove->setEnabled(true);
        } else {
            // If already enabled, disable and re-enable to reset
            autoMove->setEnabled(false);
            autoMove->setEnabled(true);
        }

        ChatUtils::displayClientMessage("§aAutoMove target set to: §6{}, {}, {}", x, y, z);
        ChatUtils::displayClientMessage("§aAutoMove has been enabled and is calculating path...");
    } catch (const std::exception& e) {
        ChatUtils::displayClientMessage("§cInvalid coordinates! Please enter valid numbers.");
    }
}

std::vector<std::string> AutoMoveCommand::getAliases() const
{
    return {"am"};
}

std::string AutoMoveCommand::getDescription() const
{
    return "Set target coordinates for AutoMove module";
}

std::string AutoMoveCommand::getUsage() const
{
    return "Usage: .automove <x> <y> <z>";
} 