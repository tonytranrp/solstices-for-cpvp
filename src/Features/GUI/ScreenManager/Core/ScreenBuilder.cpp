#include "ScreenBuilder.hpp"

#include "ScreenManager.hpp"

namespace GuiScreen
{
    ScreenBuilder::ScreenBuilder(ScreenManager& screenManager)
        : mScreenManager(screenManager)
    {
    }

    void ScreenBuilder::sync(const std::vector<std::shared_ptr<Module>>& modules)
    {
        mScreenManager.pruneSettingEntities(modules);
    }
}

