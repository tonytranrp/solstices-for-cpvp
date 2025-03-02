#pragma once
#include <cmath>
#include <string>
#include <Features/FeatureManager.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/Modules/ModuleCategory.hpp>
#include <Features/Modules/Visual/ClickGui.hpp>
#include <Utils/FontHelper.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Features/Modules/Visual/Interface.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>
#include <Utils/Keyboard.hpp>
#include <Utils/StringUtils.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>

/**
 * @brief Renders a keybind-setting UI component.
 *
 * This class encapsulates the logic for drawing a single keybind row:
 * - The background rectangle,
 * - The text label on the left,
 * - The "key box" on the right showing the bound key or binding status,
 * - Mouse interaction for entering key-binding mode.
 *
 * Improvements compared to the original:
 * 1. More consistent and clear naming conventions for parameters and variables.
 * 2. Use references where appropriate.
 * 3. Clear in-code documentation to explain each step.
 * 4. Slightly more consistent style for animations and positioning.
 * 5. Reduced “magic numbers” in favor of named variables where useful.
 */
class KeybindSettingRenderer {
public:
    /**
     * @brief Draw the UI row for a single keybind setting.
     *
     * @param keybindSetting       The KeybindSetting being rendered.
     * @param moduleRect           Rectangle describing the module’s overall bounds.
     * @param categoryY            Y coordinate where the category header starts.
     * @param categoryHeight       Height of the category header.
     * @param currentY             [In/Out] Tracks the cumulative Y offset for settings in this module.
     * @param rowPadding           Extra vertical spacing to place before the row.
     * @param rowHeight            The height allocated for each setting row.
     * @param animation            A general animation factor (0..1) controlling fade/scale.
     * @param uiScale              Global UI scale factor.
     * @param centerScreenY        Typically screenHeight / 2, for scaling from the center if needed.
     * @param fontHeight           Precomputed height of the font (for vertical centering).
     * @param fontSize             The size to use for text rendering.
     * @param guiEnabled           Whether the GUI is interactive (mouse, clicks).
     * @param lastKeybindSetting   [In/Out] Points to the KeybindSetting currently being bound (if any).
     * @param isKeybindBinding     [In/Out] Global flag indicating if user is in key-binding mode.
     * @param outTooltip           [Out] Set to the setting’s description if hovered.
     * @param cornerRadius         Corner radius for the background’s rounded edges.
     * @param currentBoundKey      The actual integer keycode set on the module (0 if unbound).
     */
    static void render(
        KeybindSetting* keybindSetting,
        const ImVec4& moduleRect,
        float           categoryY,
        float           categoryHeight,
        float& currentY,
        float           rowPadding,
        float           rowHeight,
        float           animation,
        float           uiScale,
        float           centerScreenY,
        float           fontHeight,
        float           fontSize,
        bool            guiEnabled,
        KeybindSetting*& lastKeybindSetting,
        bool& isKeybindBinding,
        std::string& outTooltip,
        float           cornerRadius,
        int             currentBoundKey
    )
    {
        // 1) Compute the new vertical offset for this row.
        //    If you have a proper “module animation factor,” replace 1.0f with that.
        currentY = MathUtils::lerp(currentY, currentY + rowHeight, 1.0f);

        // 2) Build the rectangle for the entire keybind row (background + text + key box).
        ImVec4 rowRect(
            moduleRect.x,
            categoryY + categoryHeight + currentY + rowPadding,
            moduleRect.z,
            categoryY + categoryHeight + currentY + rowHeight
        );

        // 3) Scale it around the vertical center if your UI uses ImRenderUtils::scaleToPoint.
        rowRect = rowRect.scaleToPoint(
            ImVec4(moduleRect.x, centerScreenY, moduleRect.z, centerScreenY),
            uiScale
        );

        // Snap Y to an integer to avoid sub-pixel blurriness on text.
        rowRect.y = std::floor(rowRect.y);

        // Make sure the row doesn’t start above the module rect
        if (rowRect.y < moduleRect.y)
            rowRect.y = moduleRect.y;

        // 4) Background fill for this row.
        //    Using ImColor(30, 30, 30) as a placeholder “dark gray” color.
        ImRenderUtils::fillRectangle(
            rowRect,
            ImColor(30, 30, 30),
            animation,
            cornerRadius,
            ImGui::GetBackgroundDrawList(),
            ImDrawFlags_RoundCornersBottom
        );

        // 5) Render the setting name on the left.
        const float horizontalPadding = 5.0f;
        const float textY = rowRect.y + (rowHeight - fontHeight) / 2.0f;
        ImRenderUtils::drawText(
            ImVec2(rowRect.x + horizontalPadding, textY),
            keybindSetting->mName,
            ImColor(255, 255, 255),
            fontSize,
            animation,
            true
        );

        // 6) Calculate and draw the “key box” on the right side.
        const float keyBoxSize = rowHeight - 4.0f; // Slight smaller than row height.
        ImVec4 keyBoxRect(
            rowRect.z - keyBoxSize - horizontalPadding,
            rowRect.y + (rowHeight - keyBoxSize) * 0.5f,
            rowRect.z - horizontalPadding,
            rowRect.y + (rowHeight - keyBoxSize) * 0.5f + keyBoxSize
        );

        // Key box background (slightly darker than row background).
        ImRenderUtils::fillRectangle(
            keyBoxRect,
            ImColor(29, 29, 29),
            animation,
            cornerRadius,
            ImGui::GetBackgroundDrawList(),
            ImDrawFlags_RoundCornersAll
        );

        // 7) Decide what text to display in that box.
        //    If we’re currently binding this setting, animate dots; otherwise show the bound key or "None".
        std::string displayKeyText;
        if (isKeybindBinding && lastKeybindSetting == keybindSetting) {
            // Animate a small set of dots to indicate we’re in “binding mode.”
            static const char* bindingStates[3] = { ".", "..", "..." };
            int dotsIndex = static_cast<int>(ImGui::GetTime() * 2) % 3;
            displayKeyText = bindingStates[dotsIndex];
        }
        else {
            // If no key is bound, show “None,” else show the actual key name.
            displayKeyText = (currentBoundKey == 0)
                ? "None"
                : Keyboard::getKey(currentBoundKey);
        }

        // Center the text inside the key box.
        const float keyTextWidth = ImRenderUtils::getTextWidth(&displayKeyText, fontSize);
        const float keyTextX = keyBoxRect.x + ((keyBoxRect.z - keyBoxRect.x) - keyTextWidth) * 0.5f;
        const float keyTextY = keyBoxRect.y + ((keyBoxRect.w - keyBoxRect.y) - fontHeight) * 0.5f;

        ImRenderUtils::drawText(
            ImVec2(keyTextX, keyTextY),
            displayKeyText,
            ImColor(255, 255, 255),
            fontSize,
            animation,
            true
        );

        // 8) Handle mouse input for key-binding:
        //    If hovered and clicked with left or middle mouse, enter binding mode.
        if (ImRenderUtils::isMouseOver(keyBoxRect) && guiEnabled) {
            // Show the tooltip (description) if hovered.
            outTooltip = keybindSetting->mDescription;

            // Left-click or middle-click sets us in binding mode.
            if (ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(2)) {
                lastKeybindSetting = keybindSetting;
                isKeybindBinding = true;
                // Play a small UI sound feedback.
                ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
            }
        }
    }
};
