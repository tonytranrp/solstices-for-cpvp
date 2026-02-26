#include "ScreenRegistry.hpp"

namespace GuiScreen
{
    void ScreenRegistry::registerScreen(const std::string& id, ScreenCallback callback)
    {
        mScreens[id] = std::move(callback);
    }

    bool ScreenRegistry::hasScreen(const std::string& id) const
    {
        return mScreens.contains(id);
    }

    void ScreenRegistry::render(const std::string& id) const
    {
        if (const auto it = mScreens.find(id); it != mScreens.end())
        {
            it->second();
        }
    }
}

