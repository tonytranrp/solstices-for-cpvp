#pragma once

#include <memory>
#include <vector>

class Module;

namespace GuiScreen
{
    class ScreenManager;

    class ScreenBuilder
    {
    public:
        explicit ScreenBuilder(ScreenManager& screenManager);
        void sync(const std::vector<std::shared_ptr<Module>>& modules);

    private:
        ScreenManager& mScreenManager;
    };
}

