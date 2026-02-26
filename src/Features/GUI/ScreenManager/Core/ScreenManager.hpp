#pragma once

#include <entt/entt.hpp>

#include <Features/GUI/ScreenManager/Components/UiStateComponents.hpp>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Module;
class Setting;
class BoolSetting;
class ColorSetting;
class ListSetting;

namespace GuiScreen
{
    class ScreenManager
    {
    public:
        ScreenManager();
        ~ScreenManager() = default;

        ScreenManager(const ScreenManager&) = delete;
        ScreenManager& operator=(const ScreenManager&) = delete;
        ScreenManager(ScreenManager&&) = delete;
        ScreenManager& operator=(ScreenManager&&) = delete;

        [[nodiscard]] std::string getSearchQuery() const;
        [[nodiscard]] bool hasSearchQuery() const;
        [[nodiscard]] bool isSearchActive() const;
        void clearSearchQuery();
        char* searchBuffer();
        void setSearchFocused(bool focused);

        void beginModuleBinding(const std::shared_ptr<Module>& module);
        void beginBoolSettingBinding(BoolSetting* setting);
        [[nodiscard]] bool commitBindingKey(int key);
        void cancelBindings();
        [[nodiscard]] bool isBindingModule(const std::shared_ptr<Module>& module) const;
        [[nodiscard]] bool isBindingBoolSetting(const BoolSetting* setting) const;

        [[nodiscard]] bool isColorPickerOpen() const;
        [[nodiscard]] bool isListChooserOpen() const;
        [[nodiscard]] bool isOverlayOpen() const;
        [[nodiscard]] bool isProgressOverlayOpen() const;
        void toggleColorPicker(ColorSetting* setting);
        void closeColorPicker();
        void openListChooser(ListSetting* setting);
        void closeListChooser();
        void openProgressOverlay(const std::string& ownerId, const std::string& title, const std::string& status);
        void updateProgressOverlay(const std::string& ownerId, float progress, const std::string& status);
        void closeProgressOverlay(const std::string& ownerId);
        [[nodiscard]] const Components::ProgressOverlayStateComponent& progressOverlayState() const;
        [[nodiscard]] ColorSetting* activeColorSetting() const;
        [[nodiscard]] ListSetting* activeListSetting() const;
        char* listSearchBuffer();
        std::string& activeAvailableOption();
        std::string& activeSelectedOption();

        void pruneSettingEntities(const std::vector<std::shared_ptr<Module>>& modules);

        Components::BoolSettingUiState& boolState(Setting* setting);
        Components::NumberSettingUiState& numberState(Setting* setting);
        Components::EnumSettingUiState& enumState(Setting* setting);
        Components::ColorSettingUiState& colorState(Setting* setting);
        Components::ListSettingUiState& listState(Setting* setting);

        entt::registry& registry();
        const entt::registry& registry() const;

    private:
        entt::registry mRegistry;
        entt::entity mRootEntity = entt::null;
        std::unordered_map<Setting*, entt::entity> mSettingEntities;

        entt::entity ensureSettingEntity(Setting* setting);

        template <typename ComponentT>
        ComponentT& ensureRootComponent()
        {
            if (!mRegistry.all_of<ComponentT>(mRootEntity))
            {
                return mRegistry.emplace<ComponentT>(mRootEntity);
            }
            return mRegistry.get<ComponentT>(mRootEntity);
        }

        template <typename ComponentT>
        ComponentT& ensureSettingComponent(Setting* setting)
        {
            if (setting == nullptr)
            {
                return ensureRootComponent<ComponentT>();
            }

            const entt::entity entity = ensureSettingEntity(setting);
            if (entity == entt::null)
            {
                return ensureRootComponent<ComponentT>();
            }

            if (!mRegistry.all_of<ComponentT>(entity))
            {
                return mRegistry.emplace<ComponentT>(entity);
            }
            return mRegistry.get<ComponentT>(entity);
        }
    };
}
