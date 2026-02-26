#pragma once
#include <imgui_internal.h>
#include <imgui.h>
#include <string>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <functional>
#include <ranges>
#include <unordered_set>
#include <vector>

//
// Created by vastrakai on 6/30/2024.
//

enum class SettingType
{
    Bool,
    Number,
    Enum,
    Color,
    String,
    List,
    Button,
};

class Setting
{
public:
    virtual ~Setting() = default;
    bool parse(const std::string& value);

    std::string mName;
    std::string mDescription;
    SettingType mType;
    bool mDisplay = false;
    bool* mHideOnClickGUI = nullptr;
    std::function<bool()> mIsVisible = []() { return true; };

    Setting(std::string name, std::string description, SettingType type)
        : mName(std::move(name)), mDescription(std::move(description)), mType(type)
    {
    }

    // Legacy UI transient state. New ScreenManager ECS state is being introduced,
    // but these remain for compatibility during migration.
    float sliderEase = 0;
    float boolScale = 0;
    bool isDragging = false;

    bool enumExtended = false;
    bool colourExtended = false;
    float enumSlide = 0;
    float colourSlide = 0;

    virtual nlohmann::json serialize()
    {
        nlohmann::json j;
        j["name"] = mName;
        j["type"] = static_cast<int>(mType);
        return j;
    }
};

// Define visible condition
#define VISIBILITY_CONDITION(setting, condition) setting.mIsVisible = std::function<bool()>([&]() { return condition; });

class BoolSetting : public Setting
{
public:
    bool mValue = false;
    int mKey;

    BoolSetting(std::string name, std::string description, bool value, int key = -1)
       : Setting(std::move(name), std::move(description), SettingType::Bool), mValue(value), mKey(key)
    {

    }

    void setValue(bool value)
    {
        mValue = value;
    }

    std::string getName()
    {
        return mName;
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["boolValue"] = mValue;
        j["key"] = mKey;
        return j;
    }

    // Operator overloading for easy access
    explicit operator bool() const
    {
        return mValue;
    }
};

class NumberSetting : public Setting
{
public:
    float mValue = 0.0f;
    float mMin = 0.0f;
    float mMax = 0.0f;
    float mStep = 0.0f;

    NumberSetting(std::string name, std::string description, float value, float min, float max, float step)
        : Setting(std::move(name), std::move(description), SettingType::Number), mValue(value), mMin(min), mMax(max), mStep(step)
    {

    }

    void setValue(float value)
    {
        mValue = std::round(value / mStep) * mStep;
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["numberValue"] = mValue;
        return j;
    }

    template <typename T>
    T as() const
    {
        return static_cast<T>(mValue);
    }
};

class EnumSetting : public Setting
{
public:
    int mValue = 0;
    std::vector<std::string> mValues;

    EnumSetting(std::string name, std::string description, int index, std::vector<std::string> values)
        : Setting(std::move(name), std::move(description), SettingType::Enum), mValue(index), mValues(std::move(values))
    {

    }

    template <typename IndexType, typename... Args>
    EnumSetting(std::string name, std::string description, IndexType index, Args... values)
        : Setting(std::move(name), std::move(description), SettingType::Enum), mValue(static_cast<int>(index))
    {
        mValues = { values... };
    }

    void setValue(int value)
    {
        mValue = value;
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["enumValue"] = mValue;
        return j;
    }

    template <typename T>
    explicit operator T() const
    {
        return static_cast<T>(mValue);
    }

    template <typename T>
    T as() const
    {
        return static_cast<T>(mValue);
    }
};

// EnumSetting, but the mValue is a custom type (should always be an enum)
template <typename T>
class EnumSettingT : public Setting
{
public:
    T mValue;
    std::vector<std::string> mValues;

    EnumSettingT(std::string name, std::string description, T index, std::vector<std::string> values)
        : Setting(std::move(name), std::move(description), SettingType::Enum), mValue(index), mValues(std::move(values))
    {

    }

    void setValue(T value)
    {
        mValue = value;
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["enumValue"] = mValue;
        return j;
    }

    template <typename... Args>
    EnumSettingT(std::string name, std::string description, T index, Args... values)
        : Setting(std::move(name), std::move(description), SettingType::Enum), mValue(index)
    {
        mValues = { values... };
    }

    template <typename type>
    type as() const
    {
        return static_cast<type>(mValue);
    }
};

class ColorSetting : public Setting
{
public:
    // Use ImColor to convert this to a color
    float mValue[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    bool mIsExtended = false;
    float mSlide = 0;
    // vertical size of the color picker
    static inline float mColorPickerSize = 400;


    ColorSetting(std::string name, std::string description, float r, float g, float b, float a)
        : Setting(std::move(name), std::move(description), SettingType::Color)
    {
        mValue[0] = r;
        mValue[1] = g;
        mValue[2] = b;
        mValue[3] = a;
    }

    ColorSetting(std::string name, std::string description, uint64_t color)
        : Setting(std::move(name), std::move(description), SettingType::Color)
    {
        mValue[0] = ((color >> 16) & 0xFF) / 255.0f;
        mValue[1] = ((color >> 8) & 0xFF) / 255.0f;
        mValue[2] = (color & 0xFF) / 255.0f;
        mValue[3] = ((color >> 24) & 0xFF) / 255.0f;
    }



    void setValue(float r, float g, float b, float a)
    {
        mValue[0] = r;
        mValue[1] = g;
        mValue[2] = b;
        mValue[3] = a;
    }

    ImColor getAsImColor()
    {
        return { mValue[0], mValue[1], mValue[2], mValue[3] };
    }

    void setFromImColor(const ImColor& color)
    {
        mValue[0] = color.Value.x;
        mValue[1] = color.Value.y;
        mValue[2] = color.Value.z;
        mValue[3] = color.Value.w;
    }

    void setFromImVec4(const ImVec4& color)
    {
        mValue[0] = color.x;
        mValue[1] = color.y;
        mValue[2] = color.z;
        mValue[3] = color.w;
    }

    ImVec4 getAsImVec4()
    {
        return { mValue[0], mValue[1], mValue[2], mValue[3] };
    }

    void setColor(float r, float g, float b, float a)
    {
        mValue[0] = r;
        mValue[1] = g;
        mValue[2] = b;
        mValue[3] = a;
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["colorValue"] = {
            mValue[0],
            mValue[1],
            mValue[2],
            mValue[3]
        };
        return j;
    }

    void setFromHex(unsigned long val) {
        mValue[0] = ((val >> 16) & 0xFF) / 255.0f;
        mValue[1] = ((val >> 8) & 0xFF) / 255.0f;
        mValue[2] = (val & 0xFF) / 255.0f;
        mValue[3] = ((val >> 24) & 0xFF) / 255.0f;
    }
};

class ListSetting : public Setting
{
public:
    std::vector<std::string> mOptions;
    std::vector<std::string> mSelectedValues;
    std::function<void(ListSetting&)> mRefreshCallback = nullptr;

    ListSetting(std::string name, std::string description, std::vector<std::string> options = {}, std::vector<std::string> selectedValues = {})
        : Setting(std::move(name), std::move(description), SettingType::List),
          mOptions(std::move(options)),
          mSelectedValues(std::move(selectedValues))
    {
        dedupeAndNormalize(mOptions);
        dedupeAndNormalize(mSelectedValues);
        pruneSelectionToOptions();
    }

    [[nodiscard]] bool isSelected(const std::string& value) const
    {
        const std::string normalized = normalizeValue(value);
        return std::ranges::find(mSelectedValues, normalized) != mSelectedValues.end();
    }

    bool select(const std::string& value, const bool requireExistingOption = true)
    {
        const std::string normalized = normalizeValue(value);
        if (normalized.empty())
        {
            return false;
        }

        if (requireExistingOption && std::ranges::find(mOptions, normalized) == mOptions.end())
        {
            return false;
        }

        if (isSelected(normalized))
        {
            return false;
        }

        mSelectedValues.push_back(normalized);
        return true;
    }

    bool deselect(const std::string& value)
    {
        const std::string normalized = normalizeValue(value);
        const auto it = std::ranges::find(mSelectedValues, normalized);
        if (it == mSelectedValues.end())
        {
            return false;
        }

        mSelectedValues.erase(it);
        return true;
    }

    void toggle(const std::string& value)
    {
        if (!deselect(value))
        {
            select(value);
        }
    }

    void clearSelection()
    {
        mSelectedValues.clear();
    }

    void setOptions(std::vector<std::string> options, const bool preserveSelection = true)
    {
        dedupeAndNormalize(options);
        mOptions = std::move(options);

        if (!preserveSelection)
        {
            mSelectedValues.clear();
            return;
        }

        pruneSelectionToOptions();
    }

    bool addOption(const std::string& option)
    {
        const std::string normalized = normalizeValue(option);
        if (normalized.empty())
        {
            return false;
        }

        if (std::ranges::find(mOptions, normalized) != mOptions.end())
        {
            return false;
        }

        mOptions.push_back(normalized);
        std::ranges::sort(mOptions);
        return true;
    }

    [[nodiscard]] bool canRefresh() const
    {
        return static_cast<bool>(mRefreshCallback);
    }

    void setRefreshCallback(std::function<void(ListSetting&)> callback)
    {
        mRefreshCallback = std::move(callback);
    }

    void refresh()
    {
        if (mRefreshCallback)
        {
            mRefreshCallback(*this);
        }
    }

    nlohmann::json serialize() override
    {
        nlohmann::json j = Setting::serialize();
        j["listValues"] = mSelectedValues;
        return j;
    }

private:
    static std::string normalizeValue(std::string value)
    {
        auto isNotSpace = [](unsigned char ch) { return !std::isspace(ch); };

        const auto begin = std::find_if(value.begin(), value.end(), isNotSpace);
        const auto end = std::find_if(value.rbegin(), value.rend(), isNotSpace).base();
        if (begin >= end)
        {
            return {};
        }

        value = std::string(begin, end);
        std::ranges::transform(value, value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    static void dedupeAndNormalize(std::vector<std::string>& values)
    {
        std::vector<std::string> normalizedValues;
        normalizedValues.reserve(values.size());
        std::unordered_set<std::string> seen;

        for (const auto& value : values)
        {
            std::string normalized = normalizeValue(value);
            if (normalized.empty())
            {
                continue;
            }

            if (!seen.insert(normalized).second)
            {
                continue;
            }

            normalizedValues.push_back(std::move(normalized));
        }

        std::ranges::sort(normalizedValues);
        values = std::move(normalizedValues);
    }

    void pruneSelectionToOptions()
    {
        dedupeAndNormalize(mSelectedValues);

        mSelectedValues.erase(
            std::remove_if(
                mSelectedValues.begin(),
                mSelectedValues.end(),
                [&](const std::string& selectedValue) {
                    return std::ranges::find(mOptions, selectedValue) == mOptions.end();
                }),
            mSelectedValues.end());
    }
};

class ButtonSetting : public Setting
{
public:
    std::string mButtonText;
    std::function<void()> mOnClick = nullptr;

    ButtonSetting(
        std::string name,
        std::string description,
        std::string buttonText = "Run",
        std::function<void()> onClick = nullptr)
        : Setting(std::move(name), std::move(description), SettingType::Button),
          mButtonText(std::move(buttonText)),
          mOnClick(std::move(onClick))
    {
    }

    void setOnClick(std::function<void()> onClick)
    {
        mOnClick = std::move(onClick);
    }

    [[nodiscard]] bool canClick() const
    {
        return static_cast<bool>(mOnClick);
    }

    void click() const
    {
        if (mOnClick)
        {
            mOnClick();
        }
    }
};
