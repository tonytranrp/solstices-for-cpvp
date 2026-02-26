#pragma once

#include <Features/Command/Command.hpp>
#include <Solstice.hpp>
#include <Utils/GameUtils/ChatUtils.hpp>

class EjectCommand : public Command {
public:
    EjectCommand() : Command("eject") {}

    void execute(const std::vector<std::string>& args) override
    {
        (void)args;

        if (!Solstice::mInitialized.load(std::memory_order_relaxed))
        {
            ChatUtils::displayClientMessage("Client is not initialized.");
            return;
        }

        if (Solstice::mRequestEject.load(std::memory_order_relaxed))
        {
            ChatUtils::displayClientMessage("Eject is already in progress.");
            return;
        }

        Solstice::mRequestEject.store(true, std::memory_order_relaxed);

        if (Solstice::console)
        {
            Solstice::console->warn("Eject requested via command.");
        }

        ChatUtils::displayClientMessage("Eject requested. Shutting down safely...");
    }

    [[nodiscard]] std::vector<std::string> getAliases() const override
    {
        return {"unload", "exit"};
    }

    [[nodiscard]] std::string getDescription() const override
    {
        return "Safely unloads Solstice from the client.";
    }

    [[nodiscard]] std::string getUsage() const override
    {
        return "Usage: .eject";
    }
};
