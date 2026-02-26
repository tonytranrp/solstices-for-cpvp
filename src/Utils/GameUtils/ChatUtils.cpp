//
// Created by vastrakai on 6/25/2024.
//

#include "ChatUtils.hpp"

#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>

void ChatUtils::displayClientMessage(const std::string& msg)
{
    auto* clientInstance = ClientInstance::get();
    if (!clientInstance)
    {
        return;
    }

    auto* guiData = clientInstance->getGuiData();
    if (!guiData)
    {
        return;
    }

    if (!msg.contains("\n"))
    {
        guiData->displayClientMessageQueued("Â§asolsticeÂ§7 Â» Â§r" + msg);
        return;
    }

    std::string formattedMsg = "Â§asolsticeÂ§7 Â» Â§r";
    for (const auto& c : msg)
    {
        if (c == '\n')
        {
            formattedMsg += "\nÂ§asolsticeÂ§7 Â» Â§r";
        }
        else
        {
            formattedMsg += c;
        }
    }

    guiData->displayClientMessageQueued(formattedMsg);
}

void ChatUtils::displayClientMessageSub(const std::string& subcaption, const std::string& msg)
{
    auto* clientInstance = ClientInstance::get();
    if (!clientInstance)
    {
        return;
    }

    auto* guiData = clientInstance->getGuiData();
    if (!guiData)
    {
        return;
    }

    guiData->displayClientMessageQueued("Â§asolsticeÂ§7 Â» Â§7[" + subcaption + "Â§7] Â§r" + msg);
}

void ChatUtils::displayClientMessageRaw(const std::string& msg)
{
    auto* clientInstance = ClientInstance::get();
    if (!clientInstance)
    {
        return;
    }

    auto* guiData = clientInstance->getGuiData();
    if (!guiData)
    {
        return;
    }

    guiData->displayClientMessageQueued(msg);
}
