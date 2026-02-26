#include "ListChooserScreen.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenContext.hpp>
#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/Modules/Setting.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/StringUtils.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <ranges>
#include <string>
#include <vector>

namespace
{
    struct ListColumnInteraction
    {
        std::string clickedValue;
        std::string doubleClickedValue;
    };

    void appendPrintableCharacters(char* buffer, const std::size_t capacity)
    {
        ImGuiIO& io = ImGui::GetIO();
        std::size_t length = std::strlen(buffer);

        for (int i = 0; i < io.InputQueueCharacters.Size; ++i)
        {
            const ImWchar character = io.InputQueueCharacters[i];
            if (character < 32 || character > 126)
            {
                continue;
            }

            if (length + 1 >= capacity)
            {
                break;
            }

            buffer[length++] = static_cast<char>(character);
            buffer[length] = '\0';
        }
    }

    bool renderButton(const ImVec4& rect, const std::string& text, const ImColor& accentColor, const float alpha, const bool enabled)
    {
        auto* drawList = ImGui::GetForegroundDrawList();
        const bool hovered = enabled && ImRenderUtils::isMouseOver(rect);
        const bool clicked = hovered && ImGui::IsMouseClicked(0);

        const float fillAlpha = enabled ? (hovered ? 0.28f : 0.20f) : 0.12f;
        const float borderAlpha = enabled ? (hovered ? 0.75f : 0.45f) : 0.22f;
        ImRenderUtils::fillRectangle(rect, ImColor(15, 15, 15), fillAlpha * alpha, 5.f, drawList);
        ImRenderUtils::drawRoundRect(rect, ImDrawFlags_RoundCornersAll, 5.f, accentColor, borderAlpha * alpha, 1.f);

        std::string drawTextValue = text;
        const float textSize = 0.85f;
        const float textWidth = ImRenderUtils::getTextWidth(&drawTextValue, textSize);
        const float textHeight = ImRenderUtils::getTextHeight(textSize);
        const ImColor textColor = enabled ? ImColor(255, 255, 255) : ImColor(148, 148, 148);
        ImRenderUtils::drawText(
            ImVec2(rect.x + ((rect.z - rect.x) - textWidth) * 0.5f, rect.y + ((rect.w - rect.y) - textHeight) * 0.5f),
            drawTextValue,
            textColor,
            textSize,
            alpha,
            true,
            0,
            drawList);

        return clicked;
    }

    ListColumnInteraction renderListColumn(
        const ImVec4& rect,
        const std::string& label,
        const std::vector<std::string>& values,
        std::string& activeValue,
        float& scrollOffset,
        const ImColor& accentColor,
        const float alpha,
        const bool lowercase)
    {
        constexpr float kHeaderHeight = 28.f;
        constexpr float kRowHeight = 22.f;
        constexpr float kRowSpacing = 2.f;
        constexpr float kContentPadding = 6.f;

        auto* drawList = ImGui::GetForegroundDrawList();
        ListColumnInteraction interaction{};

        ImRenderUtils::fillRectangle(rect, ImColor(12, 12, 12), 0.96f * alpha, 7.f, drawList);
        ImRenderUtils::drawRoundRect(rect, ImDrawFlags_RoundCornersAll, 7.f, accentColor, 0.25f * alpha, 1.f);

        std::string drawLabel = lowercase ? StringUtils::toLower(label) : label;
        ImRenderUtils::drawText(ImVec2(rect.x + 8.f, rect.y + 6.f), drawLabel, ImColor(220, 220, 220), 0.85f, alpha, true, 0, drawList);
        ImRenderUtils::fillRectangle(
            ImVec4(rect.x + 6.f, rect.y + kHeaderHeight - 1.f, rect.z - 6.f, rect.y + kHeaderHeight),
            accentColor,
            0.32f * alpha,
            0.f,
            drawList);

        const ImVec4 contentRect(
            rect.x + kContentPadding,
            rect.y + kHeaderHeight + 4.f,
            rect.z - kContentPadding,
            rect.w - 6.f);
        const float contentHeight = std::max(0.f, contentRect.w - contentRect.y);
        const float totalHeight = values.size() * (kRowHeight + kRowSpacing);
        const float maxScroll = std::max(0.f, totalHeight - contentHeight);

        const bool hovered = ImRenderUtils::isMouseOver(contentRect);
        if (hovered)
        {
            const float wheelDelta = ImGui::GetIO().MouseWheel;
            if (wheelDelta != 0.f)
            {
                scrollOffset -= wheelDelta * 34.f;
            }
        }
        scrollOffset = MathUtils::clamp(scrollOffset, 0.f, maxScroll);

        if (!activeValue.empty() && std::ranges::find(values, activeValue) == values.end())
        {
            activeValue.clear();
        }

        drawList->PushClipRect(ImVec2(contentRect.x, contentRect.y), ImVec2(contentRect.z, contentRect.w), true);
        for (std::size_t index = 0; index < values.size(); ++index)
        {
            const std::string& option = values[index];
            const float top = contentRect.y + index * (kRowHeight + kRowSpacing) - scrollOffset;
            const ImVec4 rowRect(contentRect.x, top, contentRect.z, top + kRowHeight);
            if (rowRect.w < contentRect.y || rowRect.y > contentRect.w)
            {
                continue;
            }

            const bool rowHovered = hovered && ImRenderUtils::isMouseOver(rowRect);
            const bool rowActive = activeValue == option;
            if (rowActive)
            {
                ImRenderUtils::fillRectangle(rowRect, accentColor, 0.20f * alpha, 4.f, drawList);
                ImRenderUtils::drawRoundRect(rowRect, ImDrawFlags_RoundCornersAll, 4.f, accentColor, 0.65f * alpha, 1.f);
            }
            else if (rowHovered)
            {
                ImRenderUtils::fillRectangle(rowRect, accentColor, 0.15f * alpha, 4.f, drawList);
            }

            if (rowHovered && ImGui::IsMouseClicked(0))
            {
                activeValue = option;
                interaction.clickedValue = option;
            }

            if (rowHovered && ImGui::IsMouseDoubleClicked(0))
            {
                activeValue = option;
                interaction.doubleClickedValue = option;
            }

            std::string drawOption = lowercase ? StringUtils::toLower(option) : option;
            const ImColor rowTextColor = rowActive ? accentColor : ImColor(235, 235, 235);
            ImRenderUtils::drawText(ImVec2(rowRect.x + 6.f, rowRect.y + 3.f), drawOption, rowTextColor, 0.82f, alpha, true, 0, drawList);
        }
        drawList->PopClipRect();

        return interaction;
    }
}

namespace GuiScreen::Screens
{
    void renderListChooserScreen(ScreenManager& manager, const ScreenContext& context)
    {
        static float openAnimation = 0.f;
        static bool searchFocused = false;
        static float availableScroll = 0.f;
        static float selectedScroll = 0.f;
        static ListSetting* openedSetting = nullptr;
        static bool wasOpenRequested = false;

        if (!context.isEnabled)
        {
            manager.closeListChooser();
        }

        const bool openRequested = context.isEnabled && manager.isListChooserOpen();
        const bool justOpened = openRequested && !wasOpenRequested;
        wasOpenRequested = openRequested;
        ListSetting* listSetting = manager.activeListSetting();
        if (openRequested && listSetting == nullptr)
        {
            manager.closeListChooser();
            return;
        }

        if (openRequested && openedSetting != listSetting)
        {
            openedSetting = listSetting;
            availableScroll = 0.f;
            selectedScroll = 0.f;
            searchFocused = true;
            manager.listSearchBuffer()[0] = '\0';
            manager.activeAvailableOption().clear();
            manager.activeSelectedOption().clear();
        }

        openAnimation = MathUtils::animate(openRequested ? 1.f : 0.f, openAnimation, ImRenderUtils::getDeltaTime() * 12.f);
        openAnimation = MathUtils::clamp(openAnimation, 0.f, 1.f);

        if (!openRequested && openAnimation <= 0.001f)
        {
            openedSetting = nullptr;
            searchFocused = false;
            availableScroll = 0.f;
            selectedScroll = 0.f;
            return;
        }

        auto* drawList = ImGui::GetForegroundDrawList();
        const ImColor accentColor = ImColor(0, 255, 127);

        ImRenderUtils::fillRectangle(
            ImVec4(0.f, 0.f, context.screenSize.x, context.screenSize.y),
            ImColor(0, 0, 0),
            0.55f * openAnimation,
            0.f,
            drawList);

        const float chooserWidth = MathUtils::clamp(context.screenSize.x * 0.72f, 700.f, 900.f);
        const float chooserHeight = MathUtils::clamp(context.screenSize.y * 0.74f, 430.f, 560.f);
        const float chooserX = (context.screenSize.x - chooserWidth) * 0.5f;
        const float chooserY = (context.screenSize.y - chooserHeight) * 0.5f + (1.f - openAnimation) * 30.f;
        const ImVec4 chooserRect(chooserX, chooserY, chooserX + chooserWidth, chooserY + chooserHeight);

        ImRenderUtils::fillRectangle(chooserRect, ImColor(10, 10, 10), 0.94f * openAnimation, 8.f, drawList);
        ImRenderUtils::drawRoundRect(chooserRect, ImDrawFlags_RoundCornersAll, 8.f, accentColor, 0.42f * openAnimation, 1.f);
        ImRenderUtils::fillShadowRectangle(chooserRect, accentColor, 0.20f * openAnimation, 30.f, ImDrawFlags_RoundCornersAll, 8.f, drawList);

        if (!openRequested || listSetting == nullptr)
        {
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            manager.closeListChooser();
            return;
        }

        const bool chooserHovered = ImRenderUtils::isMouseOver(chooserRect);
        if (ImGui::IsMouseClicked(0) && !chooserHovered && !justOpened)
        {
            manager.closeListChooser();
            return;
        }

        std::string title = context.lowercase ? StringUtils::toLower(listSetting->mName) : listSetting->mName;
        std::string selectedCount = std::to_string(listSetting->mSelectedValues.size()) + " / " + std::to_string(listSetting->mOptions.size()) + " selected";
        if (context.lowercase)
        {
            selectedCount = StringUtils::toLower(selectedCount);
        }

        ImRenderUtils::drawText(ImVec2(chooserRect.x + 16.f, chooserRect.y + 12.f), title, ImColor(255, 255, 255), 1.0f, openAnimation, true, 0, drawList);
        ImRenderUtils::drawText(ImVec2(chooserRect.x + 16.f, chooserRect.y + 30.f), selectedCount, ImColor(165, 165, 165), 0.78f, openAnimation, true, 0, drawList);

        const ImVec4 closeRect(chooserRect.z - 34.f, chooserRect.y + 10.f, chooserRect.z - 10.f, chooserRect.y + 30.f);
        const bool closeHovered = ImRenderUtils::isMouseOver(closeRect);
        ImRenderUtils::fillRectangle(closeRect, ImColor(16, 16, 16), (closeHovered ? 0.32f : 0.22f) * openAnimation, 4.f, drawList);
        ImRenderUtils::drawRoundRect(closeRect, ImDrawFlags_RoundCornersAll, 4.f, accentColor, (closeHovered ? 0.95f : 0.50f) * openAnimation, 1.f);
        ImRenderUtils::drawText(ImVec2(closeRect.x + 7.f, closeRect.y + 2.f), "x", ImColor(255, 255, 255), 0.85f, openAnimation, true, 0, drawList);
        if (closeHovered && ImGui::IsMouseClicked(0))
        {
            manager.closeListChooser();
            return;
        }

        const ImVec4 searchRect(chooserRect.x + 14.f, chooserRect.y + 50.f, chooserRect.z - 236.f, chooserRect.y + 78.f);
        const ImVec4 refreshRect(chooserRect.z - 214.f, chooserRect.y + 50.f, chooserRect.z - 122.f, chooserRect.y + 78.f);
        const ImVec4 clearRect(chooserRect.z - 116.f, chooserRect.y + 50.f, chooserRect.z - 14.f, chooserRect.y + 78.f);

        const bool searchHovered = ImRenderUtils::isMouseOver(searchRect);
        if (ImGui::IsMouseClicked(0) && !justOpened)
        {
            searchFocused = searchHovered;
        }

        ImRenderUtils::fillRectangle(searchRect, ImColor(12, 12, 12), 0.95f * openAnimation, 6.f, drawList);
        ImRenderUtils::drawRoundRect(searchRect, ImDrawFlags_RoundCornersAll, 6.f, accentColor, (searchFocused ? 1.f : 0.45f) * openAnimation, 1.f);
        if (searchFocused)
        {
            ImRenderUtils::fillShadowRectangle(searchRect, accentColor, 0.16f * openAnimation, 18.f, ImDrawFlags_RoundCornersAll, 6.f, drawList);
        }

        const ImVec2 searchIconCenter(searchRect.x + 12.f, searchRect.y + (searchRect.w - searchRect.y) * 0.5f - 0.5f);
        drawList->AddCircle(searchIconCenter, 4.f, ImColor(0.f, 1.f, 0.498f, openAnimation), 16, 1.2f);
        drawList->AddLine(
            ImVec2(searchIconCenter.x + 2.8f, searchIconCenter.y + 2.8f),
            ImVec2(searchIconCenter.x + 6.5f, searchIconCenter.y + 6.5f),
            ImColor(0.f, 1.f, 0.498f, openAnimation),
            1.2f);

        char* searchBuffer = manager.listSearchBuffer();
        if (searchFocused)
        {
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace))
            {
                const std::size_t length = std::strlen(searchBuffer);
                if (length > 0)
                {
                    searchBuffer[length - 1] = '\0';
                }
            }

            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V))
            {
                if (const char* clipboard = ImGui::GetClipboardText())
                {
                    std::size_t length = std::strlen(searchBuffer);
                    constexpr std::size_t kCapacity = 96;
                    while (*clipboard != '\0' && length + 1 < kCapacity)
                    {
                        const unsigned char c = static_cast<unsigned char>(*clipboard++);
                        if (c >= 32 && c <= 126)
                        {
                            searchBuffer[length++] = static_cast<char>(c);
                        }
                    }
                    searchBuffer[length] = '\0';
                }
            }

            appendPrintableCharacters(searchBuffer, 96);
        }

        if (searchHovered && ImGui::IsMouseClicked(1))
        {
            searchBuffer[0] = '\0';
        }

        std::string searchText(searchBuffer);
        const bool showPlaceholder = searchText.empty();
        if (showPlaceholder)
        {
            searchText = context.lowercase ? "type to filter..." : "Type to filter...";
        }
        else if (searchFocused && static_cast<int>(ImGui::GetTime() * 2.0) % 2 == 0)
        {
            searchText += "_";
        }

        const ImColor searchTextColor = showPlaceholder ? ImColor(140, 140, 140) : ImColor(255, 255, 255);
        ImRenderUtils::drawText(ImVec2(searchRect.x + 24.f, searchRect.y + 5.5f), searchText, searchTextColor, 0.84f, openAnimation, true, 0, drawList);

        if (renderButton(refreshRect, context.lowercase ? "refresh" : "Refresh", accentColor, openAnimation, listSetting->canRefresh()))
        {
            listSetting->refresh();
        }

        if (renderButton(clearRect, context.lowercase ? "clear" : "Clear", accentColor, openAnimation, true))
        {
            searchBuffer[0] = '\0';
        }

        const std::string trimmedFilter(StringUtils::trim(searchBuffer));
        const std::string filter = StringUtils::toLower(trimmedFilter);

        std::vector<std::string> availableValues;
        std::vector<std::string> selectedValues;
        availableValues.reserve(listSetting->mOptions.size());
        selectedValues.reserve(listSetting->mSelectedValues.size());

        for (const auto& option : listSetting->mOptions)
        {
            if (!filter.empty() && !StringUtils::containsIgnoreCase(option, filter))
            {
                continue;
            }

            if (listSetting->isSelected(option))
            {
                selectedValues.push_back(option);
            }
            else
            {
                availableValues.push_back(option);
            }
        }

        std::string& activeAvailableOption = manager.activeAvailableOption();
        std::string& activeSelectedOption = manager.activeSelectedOption();
        if (!activeAvailableOption.empty() &&
            std::ranges::find(availableValues, activeAvailableOption) == availableValues.end())
        {
            activeAvailableOption.clear();
        }

        if (!activeSelectedOption.empty() &&
            std::ranges::find(selectedValues, activeSelectedOption) == selectedValues.end())
        {
            activeSelectedOption.clear();
        }

        const float listTop = chooserRect.y + 90.f;
        const float listBottom = chooserRect.w - 50.f;
        const float listHeight = listBottom - listTop;
        const float sidePadding = 14.f;
        const float middleWidth = 118.f;
        const float columnWidth = ((chooserRect.z - chooserRect.x) - sidePadding * 2.f - middleWidth) * 0.5f;

        const ImVec4 availableRect(
            chooserRect.x + sidePadding,
            listTop,
            chooserRect.x + sidePadding + columnWidth,
            listTop + listHeight);
        const ImVec4 controlsRect(
            availableRect.z,
            listTop,
            availableRect.z + middleWidth,
            listTop + listHeight);
        const ImVec4 selectedRect(
            controlsRect.z,
            listTop,
            controlsRect.z + columnWidth,
            listTop + listHeight);

        const auto availableInteraction = renderListColumn(
            availableRect,
            context.lowercase ? "available" : "Available",
            availableValues,
            activeAvailableOption,
            availableScroll,
            accentColor,
            openAnimation,
            context.lowercase);

        const auto selectedInteraction = renderListColumn(
            selectedRect,
            context.lowercase ? "selected" : "Selected",
            selectedValues,
            activeSelectedOption,
            selectedScroll,
            accentColor,
            openAnimation,
            context.lowercase);

        const bool canAdd = !activeAvailableOption.empty();
        const bool canRemove = !activeSelectedOption.empty();
        const ImVec4 addRect(controlsRect.x + 11.f, controlsRect.y + listHeight * 0.36f, controlsRect.z - 11.f, controlsRect.y + listHeight * 0.36f + 28.f);
        const ImVec4 removeRect(controlsRect.x + 11.f, addRect.w + 10.f, controlsRect.z - 11.f, addRect.w + 38.f);
        const ImVec4 resetRect(controlsRect.x + 11.f, removeRect.w + 10.f, controlsRect.z - 11.f, removeRect.w + 38.f);

        if (renderButton(addRect, context.lowercase ? "add >" : "Add >", accentColor, openAnimation, canAdd))
        {
            listSetting->select(activeAvailableOption);
        }

        if (renderButton(removeRect, context.lowercase ? "< remove" : "< Remove", accentColor, openAnimation, canRemove))
        {
            listSetting->deselect(activeSelectedOption);
        }

        if (renderButton(resetRect, context.lowercase ? "reset" : "Reset", accentColor, openAnimation, true))
        {
            listSetting->clearSelection();
            activeSelectedOption.clear();
        }

        if (!availableInteraction.doubleClickedValue.empty())
        {
            listSetting->select(availableInteraction.doubleClickedValue);
        }
        if (!selectedInteraction.doubleClickedValue.empty())
        {
            listSetting->deselect(selectedInteraction.doubleClickedValue);
        }

        std::string footer = std::to_string(listSetting->mSelectedValues.size()) + " / " + std::to_string(listSetting->mOptions.size()) + " selected values";
        if (context.lowercase)
        {
            footer = StringUtils::toLower(footer);
        }
        ImRenderUtils::drawText(ImVec2(chooserRect.x + 16.f, chooserRect.w - 30.f), footer, ImColor(170, 170, 170), 0.78f, openAnimation, true, 0, drawList);
    }
}
