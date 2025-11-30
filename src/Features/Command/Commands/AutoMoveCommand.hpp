#pragma once
//
// Created by AI Assistant
//

#include <Features/Command/Command.hpp>

class AutoMoveCommand : public Command {
public:
    AutoMoveCommand() : Command("automove") {}
    void execute(const std::vector<std::string>& args) override;
    [[nodiscard]] std::vector<std::string> getAliases() const override;
    [[nodiscard]] std::string getDescription() const override;
    [[nodiscard]] std::string getUsage() const override;
}; 