#include "ScreenManager.hpp"

#include <Features/Modules/Module.hpp>
#include <Utils/StringUtils.hpp>

#include <algorithm>
#include <unordered_set>

namespace GuiScreen
{
    ScreenManager::ScreenManager()
    {
        mRootEntity = mRegistry.create();
        mRegistry.emplace<Components::BindingStateComponent>(mRootEntity);
        mRegistry.emplace<Components::SearchStateComponent>(mRootEntity);
        mRegistry.emplace<Components::ColorPickerStateComponent>(mRootEntity);
        mRegistry.emplace<Components::ListChooserStateComponent>(mRootEntity);
        mRegistry.emplace<Components::ProgressOverlayStateComponent>(mRootEntity);
    }

    std::string ScreenManager::getSearchQuery() const
    {
        const auto& searchState = mRegistry.get<Components::SearchStateComponent>(mRootEntity);
        return std::string(StringUtils::trim(searchState.queryBuffer.data()));
    }

    bool ScreenManager::hasSearchQuery() const
    {
        return !getSearchQuery().empty();
    }

    bool ScreenManager::isSearchActive() const
    {
        return mRegistry.get<Components::SearchStateComponent>(mRootEntity).focused;
    }

    void ScreenManager::clearSearchQuery()
    {
        auto& searchState = mRegistry.get<Components::SearchStateComponent>(mRootEntity);
        searchState.queryBuffer.fill('\0');
    }

    char* ScreenManager::searchBuffer()
    {
        return mRegistry.get<Components::SearchStateComponent>(mRootEntity).queryBuffer.data();
    }

    void ScreenManager::setSearchFocused(const bool focused)
    {
        mRegistry.get<Components::SearchStateComponent>(mRootEntity).focused = focused;
    }

    void ScreenManager::beginModuleBinding(const std::shared_ptr<Module>& module)
    {
        auto& bindingState = mRegistry.get<Components::BindingStateComponent>(mRootEntity);
        bindingState.module = module;
        bindingState.moduleBindingActive = (module != nullptr);
        bindingState.boolBindingActive = false;
        bindingState.boolSetting = nullptr;
    }

    void ScreenManager::beginBoolSettingBinding(BoolSetting* setting)
    {
        auto& bindingState = mRegistry.get<Components::BindingStateComponent>(mRootEntity);
        bindingState.boolSetting = setting;
        bindingState.boolBindingActive = (setting != nullptr);
        bindingState.moduleBindingActive = false;
        bindingState.module.reset();
    }

    bool ScreenManager::commitBindingKey(const int key)
    {
        const int bindingKey = (key == VK_ESCAPE) ? 0 : key;
        auto& bindingState = mRegistry.get<Components::BindingStateComponent>(mRootEntity);

        if (bindingState.moduleBindingActive && bindingState.module)
        {
            bindingState.module->setKeybind(bindingKey);
            cancelBindings();
            return true;
        }

        if (bindingState.boolBindingActive && bindingState.boolSetting)
        {
            bindingState.boolSetting->mKey = bindingKey;
            cancelBindings();
            return true;
        }

        return false;
    }

    void ScreenManager::cancelBindings()
    {
        auto& bindingState = mRegistry.get<Components::BindingStateComponent>(mRootEntity);
        bindingState.moduleBindingActive = false;
        bindingState.boolBindingActive = false;
        bindingState.module.reset();
        bindingState.boolSetting = nullptr;
    }

    bool ScreenManager::isBindingModule(const std::shared_ptr<Module>& module) const
    {
        const auto& bindingState = mRegistry.get<Components::BindingStateComponent>(mRootEntity);
        return bindingState.moduleBindingActive && bindingState.module == module;
    }

    bool ScreenManager::isBindingBoolSetting(const BoolSetting* setting) const
    {
        const auto& bindingState = mRegistry.get<Components::BindingStateComponent>(mRootEntity);
        return bindingState.boolBindingActive && bindingState.boolSetting == setting;
    }

    bool ScreenManager::isColorPickerOpen() const
    {
        return mRegistry.get<Components::ColorPickerStateComponent>(mRootEntity).open;
    }

    bool ScreenManager::isListChooserOpen() const
    {
        return mRegistry.get<Components::ListChooserStateComponent>(mRootEntity).open;
    }

    bool ScreenManager::isOverlayOpen() const
    {
        return isColorPickerOpen() || isListChooserOpen();
    }

    bool ScreenManager::isProgressOverlayOpen() const
    {
        return mRegistry.get<Components::ProgressOverlayStateComponent>(mRootEntity).open;
    }

    void ScreenManager::toggleColorPicker(ColorSetting* setting)
    {
        auto& picker = mRegistry.get<Components::ColorPickerStateComponent>(mRootEntity);
        if (picker.open && picker.setting == setting)
        {
            picker.open = false;
            picker.setting = nullptr;
            return;
        }

        picker.setting = setting;
        picker.open = (setting != nullptr);
    }

    void ScreenManager::closeColorPicker()
    {
        auto& picker = mRegistry.get<Components::ColorPickerStateComponent>(mRootEntity);
        picker.open = false;
        picker.setting = nullptr;
    }

    void ScreenManager::openListChooser(ListSetting* setting)
    {
        auto& chooser = mRegistry.get<Components::ListChooserStateComponent>(mRootEntity);
        if (chooser.setting != setting)
        {
            chooser.searchBuffer.fill('\0');
            chooser.activeAvailableOption.clear();
            chooser.activeSelectedOption.clear();
        }

        chooser.setting = setting;
        chooser.open = (setting != nullptr);
    }

    void ScreenManager::closeListChooser()
    {
        auto& chooser = mRegistry.get<Components::ListChooserStateComponent>(mRootEntity);
        chooser.open = false;
        chooser.setting = nullptr;
        chooser.searchBuffer.fill('\0');
        chooser.activeAvailableOption.clear();
        chooser.activeSelectedOption.clear();
    }

    void ScreenManager::openProgressOverlay(const std::string& ownerId, const std::string& title, const std::string& status)
    {
        auto& overlay = mRegistry.get<Components::ProgressOverlayStateComponent>(mRootEntity);
        overlay.open = true;
        overlay.ownerId = ownerId;
        overlay.title = title;
        overlay.status = status;
        overlay.progress = 0.f;
    }

    void ScreenManager::updateProgressOverlay(const std::string& ownerId, const float progress, const std::string& status)
    {
        auto& overlay = mRegistry.get<Components::ProgressOverlayStateComponent>(mRootEntity);
        if (!overlay.open || overlay.ownerId != ownerId)
        {
            return;
        }

        overlay.progress = std::clamp(progress, 0.f, 1.f);
        overlay.status = status;
    }

    void ScreenManager::closeProgressOverlay(const std::string& ownerId)
    {
        auto& overlay = mRegistry.get<Components::ProgressOverlayStateComponent>(mRootEntity);
        if (!overlay.open || overlay.ownerId != ownerId)
        {
            return;
        }

        overlay.open = false;
        overlay.ownerId.clear();
        overlay.title.clear();
        overlay.status.clear();
        overlay.progress = 0.f;
    }

    const Components::ProgressOverlayStateComponent& ScreenManager::progressOverlayState() const
    {
        return mRegistry.get<Components::ProgressOverlayStateComponent>(mRootEntity);
    }

    ColorSetting* ScreenManager::activeColorSetting() const
    {
        return mRegistry.get<Components::ColorPickerStateComponent>(mRootEntity).setting;
    }

    ListSetting* ScreenManager::activeListSetting() const
    {
        return mRegistry.get<Components::ListChooserStateComponent>(mRootEntity).setting;
    }

    char* ScreenManager::listSearchBuffer()
    {
        return mRegistry.get<Components::ListChooserStateComponent>(mRootEntity).searchBuffer.data();
    }

    std::string& ScreenManager::activeAvailableOption()
    {
        return mRegistry.get<Components::ListChooserStateComponent>(mRootEntity).activeAvailableOption;
    }

    std::string& ScreenManager::activeSelectedOption()
    {
        return mRegistry.get<Components::ListChooserStateComponent>(mRootEntity).activeSelectedOption;
    }

void ScreenManager::pruneSettingEntities(const std::vector<std::shared_ptr<Module>>& modules)
{
    std::unordered_set<Setting*> liveSettings;
        for (const auto& module : modules)
        {
            if (!module)
            {
                continue;
            }

            for (Setting* setting : module->mSettings)
            {
                if (setting)
                {
                    liveSettings.insert(setting);
                }
            }
        }

    for (auto it = mSettingEntities.begin(); it != mSettingEntities.end();)
    {
        if (!liveSettings.contains(it->first))
        {
            auto& bindingState = mRegistry.get<Components::BindingStateComponent>(mRootEntity);
            if (bindingState.boolSetting == it->first)
            {
                bindingState.boolSetting = nullptr;
                bindingState.boolBindingActive = false;
            }

            auto& colorPickerState = mRegistry.get<Components::ColorPickerStateComponent>(mRootEntity);
            if (colorPickerState.setting == it->first)
            {
                colorPickerState.setting = nullptr;
                colorPickerState.open = false;
            }

            auto& listChooserState = mRegistry.get<Components::ListChooserStateComponent>(mRootEntity);
            if (listChooserState.setting == it->first)
            {
                listChooserState.setting = nullptr;
                listChooserState.open = false;
                listChooserState.searchBuffer.fill('\0');
                listChooserState.activeAvailableOption.clear();
                listChooserState.activeSelectedOption.clear();
            }

            const entt::entity entity = it->second;
            if (entity != entt::null && mRegistry.valid(entity))
            {
                mRegistry.destroy(entity);
            }
            it = mSettingEntities.erase(it);
            continue;
        }

            ++it;
        }
    }

    Components::BoolSettingUiState& ScreenManager::boolState(Setting* setting)
    {
        return ensureSettingComponent<Components::BoolSettingUiState>(setting);
    }

    Components::NumberSettingUiState& ScreenManager::numberState(Setting* setting)
    {
        return ensureSettingComponent<Components::NumberSettingUiState>(setting);
    }

    Components::EnumSettingUiState& ScreenManager::enumState(Setting* setting)
    {
        return ensureSettingComponent<Components::EnumSettingUiState>(setting);
    }

    Components::ColorSettingUiState& ScreenManager::colorState(Setting* setting)
    {
        return ensureSettingComponent<Components::ColorSettingUiState>(setting);
    }

    Components::ListSettingUiState& ScreenManager::listState(Setting* setting)
    {
        return ensureSettingComponent<Components::ListSettingUiState>(setting);
    }

    entt::registry& ScreenManager::registry()
    {
        return mRegistry;
    }

    const entt::registry& ScreenManager::registry() const
    {
        return mRegistry;
    }

entt::entity ScreenManager::ensureSettingEntity(Setting* setting)
{
    if (setting == nullptr)
    {
        return entt::null;
    }

    if (const auto it = mSettingEntities.find(setting); it != mSettingEntities.end())
    {
        if (it->second != entt::null && mRegistry.valid(it->second))
        {
            return it->second;
        }

        mSettingEntities.erase(it);
    }

    const entt::entity entity = mRegistry.create();
    mRegistry.emplace<Components::SettingRefComponent>(entity, setting);
    mSettingEntities.emplace(setting, entity);
        return entity;
    }
}
