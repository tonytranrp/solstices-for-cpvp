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
 * @brief Renders a keybind setting UI component.
 *
 * This class encapsulates the logic for drawing the keybind row, including the key box,
 * label and mouse interaction for entering binding mode.
 */
class KeybindSettingRenderer {
public:
    /**
     * @brief Renders a keybind setting.
     *
     * @param keybindSetting Pointer to the KeybindSetting instance.
     * @param modRect The rectangle of the module row.
     * @param catY The Y coordinate of the current category header.
     * @param catHeight Height of the category header.
     * @param moduleY In/out: current vertical offset for the module row.
     * @param setPadding Additional vertical padding for the setting row.
     * @param modHeight Height of the module row.
     * @param animation Current animation factor.
     * @param inScale Current UI scale factor.
     * @param screenHalfY Half the screen height (e.g. screen.y / 2).
     * @param textHeight Height of the text.
     * @param textSize Text size.
     * @param isEnabled Whether the GUI is enabled.
     * @param lastKeybindSetting In/out: pointer to the last keybind setting being bound.
     * @param isKeybindBinding Reference to the global binding flag.
     * @param tooltip Out: will be set if the mouse hovers over the setting.
     * @param radius Corner radius for rounded drawing.
     * @param modKey The key value from the module (used for display).
     */
    static void render(KeybindSetting* keybindSetting,
        const ImVec4& modRect,
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
        KeybindSetting*& lastKeybindSetting,
        bool& isKeybindBinding,
        std::string& tooltip,
        float radius,
        int modKey)
    {
        // Animate vertical offset for the row.
        moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, 1.0f); // Use appropriate animation factor if available.

        // Compute the overall rectangle for the keybind setting row.
        ImVec4 rect = ImVec4(
            modRect.x,
            catY + catHeight + moduleY + setPadding,
            modRect.z,
            catY + catHeight + moduleY + modHeight
        ).scaleToPoint(ImVec4(modRect.x, screenHalfY, modRect.z, screenHalfY), inScale);
        rect.y = std::floor(rect.y);
        if (rect.y < modRect.y)
            rect.y = modRect.y;

        // Define padding and calculate the square key box on the right.
        float padding = 5.f;
        float keyBoxSize = modHeight - 4; // Square box size.
        ImVec4 keyBoxRect = ImVec4(
            rect.z - keyBoxSize - padding,
            rect.y + (modHeight - keyBoxSize) / 2,
            rect.z - padding,
            rect.y + (modHeight - keyBoxSize) / 2 + keyBoxSize
        );

        // Render the background for the keybind setting row.
        ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), animation, radius,
            ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

        // Draw the setting label on the left.
        ImRenderUtils::drawText(ImVec2(rect.x + padding, rect.y + (modHeight - textHeight) / 2),
            keybindSetting->mName, ImColor(255, 255, 255),
            textSize, animation, true);

        // Determine what text to display in the key box.
        std::string keyText;
        if (isKeybindBinding && lastKeybindSetting == keybindSetting) {
            // Animate dots while binding.
            const char* states[3] = { ".", "..", "..." };
            int index = static_cast<int>(ImGui::GetTime() * 2) % 3;
            keyText = states[index];
        }
        else {
            keyText = Keyboard::getKey(modKey);
            if (modKey == 0)
                keyText = "None";
        }

        // Render the key box with a dark background and rounded corners.
        ImRenderUtils::fillRectangle(keyBoxRect, ImColor(29, 29, 29), animation, 4,
            ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersAll);

        // Center the key text within the key box.
        float keyTextWidth = ImRenderUtils::getTextWidth(&keyText, textSize);
        float keyTextX = keyBoxRect.x + ((keyBoxRect.z - keyBoxRect.x) - keyTextWidth) / 2;
        float keyTextY = keyBoxRect.y + ((keyBoxRect.w - keyBoxRect.y) - textHeight) / 2;
        ImRenderUtils::drawText(ImVec2(keyTextX, keyTextY), keyText, ImColor(255, 255, 255),
            textSize, animation, true);

        // If the mouse is over the key box and a click occurs, enter binding mode.
        if (ImRenderUtils::isMouseOver(keyBoxRect) && isEnabled) {
            tooltip = keybindSetting->mDescription;
            if ((ImGui::IsMouseClicked(0) || ImGui::IsMouseClicked(2))) {
                lastKeybindSetting = keybindSetting;
                isKeybindBinding = true;
                ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
            }
        }
    }
};