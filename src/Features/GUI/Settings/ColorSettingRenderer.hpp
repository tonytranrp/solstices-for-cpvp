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

class ColorSettingRenderer {
public:
    /**
     * @brief Renders a color setting UI component.
     *
     * @param colorSetting The ColorSetting instance to render.
     * @param modRect The rectangle for the current module.
     * @param catRect The rectangle for the current category.
     * @param baseY The base Y coordinate (typically catPositions[i].y + catHeight).
     * @param modHeight Height of one module row.
     * @param setPadding Additional vertical padding for the setting.
     * @param animation Current animation factor.
     * @param inScale Current UI scale factor.
     * @param screenHalfY Half the screen height (screen.y / 2).
     * @param textHeight Height of the text.
     * @param textSize Size of the text.
     * @param isEnabled Whether the GUI is enabled.
     * @param lowercase Whether to render setting names in lowercase.
     * @param moduleCAnim The current module animation factor (e.g. mod->cAnim).
     * @param moduleY In/out parameter: the current Y offset for this module (will be updated).
     * @param showSettings Whether settings are currently shown.
     * @param isExtended Whether the current category is extended.
     * @param tooltip Out parameter: will be set if the mouse hovers over the setting.
     * @param displayColorPicker In/out parameter: controls whether the color picker is displayed.
     * @param lastColorSetting In/out parameter: will be updated if the user clicks to open the picker.
     */
    static void render(ColorSetting* colorSetting,
        const ImVec4& modRect,
        const ImVec4& catRect,
        float baseY,
        float modHeight,
        float setPadding,
        float animation,
        float inScale,
        float screenHalfY,
        float textHeight,
        float textSize,
        bool isEnabled,
        bool lowercase,
        float moduleCAnim,
        float& moduleY,
        bool showSettings,
        bool isExtended,
        std::string& tooltip,
        bool& displayColorPicker,
        ColorSetting*& lastColorSetting)
    {
        // Retrieve the current color and set up the display name.
        ImColor color = colorSetting->getAsImColor();
        ImVec4 rgb = color.Value;
        std::string setName = lowercase ? StringUtils::toLower(colorSetting->mName) : colorSetting->mName;

        // Animate the module's vertical offset.
        moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, moduleCAnim);

        // Build the rectangle for the color setting row.
        ImVec4 rect = ImVec4(
            modRect.x,
            baseY + moduleY + setPadding,
            modRect.z,
            baseY + moduleY + modHeight
        ).scaleToPoint(ImVec4(modRect.x, screenHalfY, modRect.z, screenHalfY), inScale);
        rect.y = std::floor(rect.y);
        if (rect.y < modRect.y)
            rect.y = modRect.y;

        // Only render if the computed rect is sufficiently low in the category.
        if (rect.y > catRect.y + 0.5f)
        {
            // Draw the background for the color setting.
            ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), animation);

            // If the mouse is over this setting, show its tooltip.
            // Also, if clicked and settings are visible, toggle the color picker.
            if (ImRenderUtils::isMouseOver(rect) && isEnabled && isExtended)
            {
                tooltip = colorSetting->mDescription;
                if (ImGui::IsMouseClicked(0) && !displayColorPicker && showSettings)
                {
                    displayColorPicker = !displayColorPicker;
                    lastColorSetting = colorSetting;
                }
            }

            // Center the setting name vertically within the rectangle.
            float cSetRectCentreY = rect.y + ((rect.w - rect.y) - textHeight) / 2.0f;
            ImRenderUtils::drawText(ImVec2(rect.x + 5.f, cSetRectCentreY), setName,
                ImColor(255, 255, 255), textSize, animation, true);

            // Draw a small rectangle (color swatch) on the right side.
            ImVec2 colorRect = ImVec2(rect.z - 20, rect.y + 5);
            ImRenderUtils::fillRectangle(ImVec4(rect.z - 20, rect.y + 5, rect.z - 5, rect.w - 5),
                colorSetting->getAsImColor(), animation);
        }
    }
};