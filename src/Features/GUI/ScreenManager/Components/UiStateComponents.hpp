#pragma once

#include <array>
#include <memory>
#include <string>

class Module;
class Setting;
class BoolSetting;
class ColorSetting;
class ListSetting;

namespace GuiScreen::Components
{
    struct SettingRefComponent
    {
        Setting* setting = nullptr;
    };

    struct BoolSettingUiState
    {
        float scale = 0.f;
    };

    struct NumberSettingUiState
    {
        float sliderEase = 0.f;
        bool isDragging = false;
    };

    struct EnumSettingUiState
    {
        bool extended = false;
        float slide = 0.f;
    };

    struct ColorSettingUiState
    {
        bool extended = false;
        float slide = 0.f;
    };

    struct ListSettingUiState
    {
        bool initialized = false;
    };

    struct BindingStateComponent
    {
        std::shared_ptr<Module> module = nullptr;
        BoolSetting* boolSetting = nullptr;
        bool moduleBindingActive = false;
        bool boolBindingActive = false;
    };

    struct SearchStateComponent
    {
        std::array<char, 96> queryBuffer{};
        bool focused = false;
    };

    struct ColorPickerStateComponent
    {
        bool open = false;
        ColorSetting* setting = nullptr;
    };

    struct ListChooserStateComponent
    {
        bool open = false;
        ListSetting* setting = nullptr;
        std::array<char, 96> searchBuffer{};
        std::string activeAvailableOption;
        std::string activeSelectedOption;
    };

    struct ProgressOverlayStateComponent
    {
        bool open = false;
        std::string ownerId;
        std::string title;
        std::string status;
        float progress = 0.f;
    };
}
