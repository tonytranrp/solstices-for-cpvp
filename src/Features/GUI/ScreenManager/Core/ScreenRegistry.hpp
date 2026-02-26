#pragma once

#include <functional>
#include <string>
#include <unordered_map>

namespace GuiScreen
{
    class ScreenRegistry
    {
    public:
        using ScreenCallback = std::function<void()>;

        void registerScreen(const std::string& id, ScreenCallback callback);
        [[nodiscard]] bool hasScreen(const std::string& id) const;
        void render(const std::string& id) const;

    private:
        std::unordered_map<std::string, ScreenCallback> mScreens;
    };
}

