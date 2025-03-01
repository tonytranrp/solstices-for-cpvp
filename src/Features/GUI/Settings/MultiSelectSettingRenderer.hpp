#pragma once
#include <vector>
#include <Features/FeatureManager.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/Modules/ModuleCategory.hpp>
#include <Features/Modules/Visual/ClickGui.hpp>
#include <Utils/FontHelper.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/Modules/Visual/Interface.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>
#include <Utils/Keyboard.hpp>
#include <Utils/StringUtils.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>
/**
 * @brief Renders a multi-select setting UI component.
 */
class MultiSelectSettingRenderer {
public:
    static void render(
        MultiSelectSetting* setting,
        const ImVec4& modRect,
        const ImVec4& catRect,
        float catY,
        float catHeight,
        float& moduleY,
        float setPadding,
        float modHeight,
        float animation,
        float inScale,
        float screenHalfY,
        float textHeight,
        float textSize,
        bool isEnabled,
        bool lowercase,
        float modCAnim,
        std::string& tooltip,
        float radius,
        bool displayColorPicker,
        bool showSettings
    ) {
        if (!setting) return;

        // Setting name text
        std::string setName = lowercase ? StringUtils::toLower(setting->mName) : setting->mName;

        // Animate the vertical offset for smooth rendering
        moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, modCAnim);

        // Compute base rectangle for button
        ImVec4 baseRect(
            modRect.x,
            catY + catHeight + moduleY + setPadding,
            modRect.z,
            catY + catHeight + moduleY + modHeight
        );
        baseRect = baseRect.scaleToPoint(ImVec4(modRect.x, screenHalfY, modRect.z, screenHalfY), inScale);
        baseRect.y = std::floor(baseRect.y);

        if (baseRect.y < modRect.y)
            baseRect.y = modRect.y;

        // Draw background for button
        ImRenderUtils::fillRectangle(baseRect, ImColor(30, 30, 30), animation, radius,
            ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

        // Check for mouse interaction
        if (ImRenderUtils::isMouseOver(baseRect) && isEnabled) {
            tooltip = setting->mDescription;

            if (ImGui::IsMouseClicked(0) && !displayColorPicker && showSettings) {
                setting->menuOpen = !setting->menuOpen;  // Toggle menu
            }
        }

        float baseTextY = baseRect.y + ((baseRect.w - baseRect.y) - textHeight) / 2;

        // Draw setting name (left side)
        ImRenderUtils::drawText(ImVec2(baseRect.x + 5.f, baseTextY),
            setName, ImColor(255, 255, 255), textSize, animation, true);

        // Draw selected items summary (right side)
        std::string selectedSummary = getSelectedItemsSummary(setting);
        float optionWidth = ImRenderUtils::getTextWidth(&selectedSummary, textSize);
        ImRenderUtils::drawText(ImVec2((baseRect.z - 5.f) - optionWidth, baseTextY),
            selectedSummary, ImColor(170, 170, 170), textSize, animation, true);

        // Render the menu if open
        if (setting->menuOpen) {
            renderMultiSelectMenu(setting);
        }
    }

private:
    /**
     * @brief Generates a summary of selected items with ellipsis if too many.
     */
    static std::string getSelectedItemsSummary(MultiSelectSetting* setting) {
        std::string selected;
        int count = 0;
        for (size_t i = 0; i < setting->mValues.size(); i++) {
            if (setting->mSelected[i]) {
                if (count < 2) {
                    if (!selected.empty()) selected += ", ";
                    selected += setting->mValues[i];
                }
                count++;
            }
        }
        if (count > 2) {
            selected += " ...";
        }
        return selected.empty() ? "None" : selected;
    }

    /**
     * @brief Renders the pop-up menu for selecting multiple items.
     */
    static void renderMultiSelectMenu(MultiSelectSetting* setting) {
        ImGui::SetNextWindowSize(ImVec2(300, 250));
        ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);

        if (ImGui::BeginPopup("MultiSelectPopup")) {
            ImGui::Text("Select Items:");
            ImGui::Separator();

            for (size_t i = 0; i < setting->mValues.size(); i++) {
                bool isSelected = setting->mSelected[i];
                if (ImGui::Checkbox(setting->mValues[i].c_str(), &isSelected)) {
                    setting->mSelected[i] = isSelected;
                }
            }

            if (ImGui::Button("Close")) {
                setting->menuOpen = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Open menu when triggered
        if (setting->menuOpen) {
            ImGui::OpenPopup("MultiSelectPopup");
        }
    }
};