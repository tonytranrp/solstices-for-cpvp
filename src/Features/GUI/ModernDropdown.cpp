//
// Created by Tozic on 7/15/2024.
//

#include "ModernDropdown.hpp"

bool ModernGui::isKeybindBinding = false;
bool ModernGui::isSearching = false;
KeybindSetting* ModernGui::lastKeybindSetting = nullptr;
std::string ModernGui::searchingModule = "";
ImVec4 ModernGui::scaleToPoint(const ImVec4& _this, const ImVec4& point, float amount)
{
    return {point.x + (_this.x - point.x) * amount, point.y + (_this.y - point.y) * amount,
        point.z + (_this.z - point.z) * amount, point.w + (_this.w - point.w) * amount };
}

bool ModernGui::isMouseOver(const ImVec4& rect)
{
    ImVec2 mousePos = ImGui::GetIO().MousePos;
    return mousePos.x >= rect.x && mousePos.y >= rect.y && mousePos.x < rect.z && mousePos.y < rect.w;
}

ImVec4 ModernGui::getCenter(ImVec4& vec)
{
    float centerX = (vec.x + vec.z) / 2.0f;
    float centerY = (vec.y + vec.w) / 2.0f;
    return { centerX, centerY, centerX, centerY };
}

void ModernGui::render(float animation, float inScale, int& scrollDirection, char* h, float blur, float midclickRounding, bool isPressingShift)
{
    static auto interfaceMod = gFeatureManager->mModuleManager->getModule<Interface>();
    bool lowercase = (interfaceMod->mNamingStyle.mValue == NamingStyle::Lowercase ||
        interfaceMod->mNamingStyle.mValue == NamingStyle::LowercaseSpaced);

    // Push the preferred font.
    FontHelper::pushPrefFont(true, false, true);

    // Get screen and delta time.
    ImVec2 screen = ImRenderUtils::getScreenSize();
    float deltaTime = ImGui::GetIO().DeltaTime;
    auto drawList = ImGui::GetBackgroundDrawList();

    // Reset category positions if needed.
    if (resetPosition && NOW - lastReset > 100) {
        catPositions.clear();
        float centerX = screen.x * 0.5f;
        // Calculate starting X so that categories are centered.
        float xPos = centerX - (ModuleCategoryNames.size() * (catWidth + catGap) * 0.5f);
        for (const std::string& cat : ModuleCategoryNames) {
            CategoryPosition pos;
            // Round positions to even numbers.
            pos.x = std::round(xPos / 2.f) * 2.f;
            pos.y = std::round((catGap * 2.f) / 2.f) * 2.f;
            catPositions.push_back(pos);
            xPos += catWidth + catGap;
        }
        resetPosition = false;
    }

    // Draw background with blur.
    drawList->AddRectFilled(ImVec2(0, 0), screen, IM_COL32(0, 0, 0, 255 * animation * 0.38f));
    ImRenderUtils::addBlur(ImVec4(0.f, 0.f, screen.x, screen.y), animation * blur, 0);

    // Draw a gradient glow at the bottom third.
    ImColor shadowColor = ColorUtils::getThemedColor(0);
    shadowColor.Value.w = 0.5f * animation;
    float baseHeight = screen.y - screen.y / 3;
    float glowHeight = MathUtils::lerp(screen.y, baseHeight, inScale);
    ImRenderUtils::fillGradientOpaqueRectangle(
        ImVec4(0, glowHeight, screen.x, screen.y),
        shadowColor, shadowColor, 0.4f * inScale, 0.0f
    );

    // Retrieve modules and clickGui.
    static const auto& categories = ModuleCategoryNames;
    static auto& modules = gFeatureManager->mModuleManager->getModules();
    static auto* clickGui = gFeatureManager->mModuleManager->getModule<ClickGui>();
    bool isEnabled = clickGui->mEnabled;
    std::string tooltip;

    // Compute text metrics.
    float textSize = inScale;
    float textHeight = ImGui::GetFont()->CalcTextSizeA(textSize * 18, FLT_MAX, -1, "").y;

    // (Optional) Setup a dummy window position.
    int screenWidth = static_cast<int>(screen.x);
    float windowX = (screenWidth - 220.f) * 0.5f; // Example window width 220.f

    // Color Picker display.
    if (displayColorPicker && isEnabled) {
        FontHelper::pushPrefFont(false, false, true);
        ColorSetting* colorSetting = lastColorSetting;
        ImGui::SetNextWindowPos(ImVec2(screen.x / 2 - 200, screen.y / 2));
        ImGui::SetNextWindowSize(ImVec2(400, 400));
        ImGui::Begin("Color Picker", &displayColorPicker,
            ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
        {
            ImVec4 color = colorSetting->getAsImColor().Value;
            ImGui::ColorPicker4("Color", colorSetting->mValue,
                ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha);
            if (ImGui::Button("Close"))
            {
                colorSetting->setFromImColor(ImColor(color));
                displayColorPicker = false;
            }
        }
        ImGui::End();
        ImGui::PopFont();
        if (ImGui::IsMouseClicked(0) && !ImRenderUtils::isMouseOver(
            ImVec4(screen.x / 2 - 200, screen.y / 2, screen.x / 2 + 200, screen.y / 2 + 400)))
        {
            displayColorPicker = false;
        }
    }
    if (!isEnabled)
        displayColorPicker = false;

    // Initialize category positions if not already done.
    if (catPositions.empty() && isEnabled) {
        float centerX = screen.x / 2.f;
        float xPos = centerX - (categories.size() * (catWidth + catGap) / 2);
        for (const std::string& cat : categories) {
            CategoryPosition pos;
            pos.x = std::round(xPos / 2.f) * 2.f;
            pos.y = std::round((catGap * 2.f) / 2.f) * 2.f;
            catPositions.push_back(pos);
            xPos += catWidth + catGap;
        }
    }
    if (!catPositions.empty())
    {
        for (size_t i = 0; i < categories.size(); i++)
        {
            // Mod math stuff
            const float modWidth = catWidth;
            const float modHeight = catHeight;
            float moduleY = -catPositions[i].yOffset;

            // Get all the modules and populate our vector
            const auto& modsInCategory = gFeatureManager->mModuleManager->getModulesInCategory(i);

            // Calculate the catRect pos
            ImVec4 catRect = ImVec4(catPositions[i].x, catPositions[i].y,
                                                    catPositions[i].x + catWidth, catPositions[i].y + catHeight)
                .scaleToPoint(ImVec4(screen.x / 2,
                                             screen.y / 2,
                                             screen.x / 2,
                                             screen.y / 2), inScale);

            /* Calculate the height of the catWindow including the settings */
            // --- Calculate extra settings height for the category ---
            float settingsHeight = 0.f;
            for (const auto& mod : modsInCategory) {
                for (const auto& setting : mod->mSettings) {
                    // Every setting adds one "modHeight" lerp factor
                    settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, mod->cAnim);
                    if (setting->mType == SettingType::Enum && setting->enumSlide > 0.01f) {
                        int numValues = static_cast<int>(reinterpret_cast<EnumSetting*>(setting)->mValues.size());
                        for (int j = 0; j < numValues; ++j) {
                            settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, setting->enumSlide);
                        }
                    }
                }
            }

            // --- Compute category window height and rectangle ---
            float catWindowHeight = catHeight + modHeight * static_cast<float>(modsInCategory.size()) + settingsHeight;
            ImVec4 catWindow = ImVec4(catPositions[i].x, catPositions[i].y,
                catPositions[i].x + catWidth,
                catPositions[i].y + moduleY + catWindowHeight)
                .scaleToPoint(ImVec4(screen.x / 2, screen.y / 2, screen.x / 2, screen.y / 2), inScale);
            ImColor rgb = ColorUtils::getThemedColor(i * 20);

            // --- Handle scrolling ---
            if (ImRenderUtils::isMouseOver(catWindow) && catPositions[i].isExtended) {
                catPositions[i].scrollEase += scrollDirection * catHeight;
                catPositions[i].scrollEase = std::clamp(catPositions[i].scrollEase, 0.f, catWindowHeight - modHeight * 2);
                scrollDirection = 0;
            }
            if (!catPositions[i].isExtended) {
                catPositions[i].scrollEase = catWindowHeight - catHeight;
                catPositions[i].wasExtended = false;
            }
            else if (!catPositions[i].wasExtended) {
                catPositions[i].scrollEase = 0;
                catPositions[i].wasExtended = true;
            }
            catPositions[i].yOffset = MathUtils::animate(catPositions[i].scrollEase, catPositions[i].yOffset,
                ImRenderUtils::getDeltaTime() * 10.5);

            // --- Set clipping for rendering modules ---
            ImVec4 clipRect(catRect.x, catRect.w, catRect.z, screen.y);
            drawList->PushClipRect(ImVec2(clipRect.x, clipRect.y), ImVec2(clipRect.z, clipRect.w), true);

            // Initialize module iteration variables.
            int modIndex = 0;
            bool endMod = false;
            bool moduleToggled = false;

            for (const auto& mod : modsInCategory)
            {
                std::string modNameLower = StringUtils::toLower(mod->getName());
                std::string searchLower = StringUtils::toLower(searchingModule);
                if (!searchingModule.empty() && modNameLower.find(searchLower) == std::string::npos) {
                    continue; // Skip modules that don't match the search query
                }
                ImDrawFlags flags = ImDrawFlags_RoundCornersBottom;
                float radius = 0.f;
                if (modIndex == modsInCategory.size() - 1) {
                    endMod = true;
                    radius = 15.f * (1.f - mod->cAnim);
                }

                std::string modLower = mod->getName();

                std::transform(modLower.begin(), modLower.end(), modLower.begin(), [](unsigned char c)
                {
                    return std::tolower(c);
                });

                ImColor rgb = ColorUtils::getThemedColor(moduleY * 2);

                // If the mod belongs to the category
                if (mod->getCategory() == categories[i])
                {
                    // Calculate the modRect pos
                    ImVec4 modRect = ImVec4(catPositions[i].x,
                                                            catPositions[i].y + catHeight + moduleY,
                                                            catPositions[i].x + modWidth,
                                                            catPositions[i].y + catHeight + moduleY + modHeight)
                        .scaleToPoint(ImVec4(screen.x / 2,
                                                     screen.y / 2,
                                                     screen.x / 2,
                                                     screen.y / 2), inScale);
                    //modRect.y -= 4.f;
                    // floor the y value of the modRect
                    modRect.y = std::floor(modRect.y);
                    modRect.x = std::floor(modRect.x);

                    // Animate the setting animation percentage
                    float targetAnim = mod->showSettings ? 1.f : 0.f;
                    mod->cAnim = MathUtils::animate(targetAnim, mod->cAnim, ImRenderUtils::getDeltaTime() * 12.5);
                    mod->cAnim = MathUtils::clamp(mod->cAnim, 0.f, 1.f);

                    // Settings
                    if (mod->cAnim > 0.001)
                    {
                        static bool wasDragging = false;
                        Setting* lastDraggedSetting = nullptr;
                        int sIndex = 0;
                        for (const auto& setting : mod->mSettings)
                        {
                            if (!setting->mIsVisible())
                            {
                                //Reset the animation if the setting is not visible
                                setting->sliderEase = 0;
                                setting->enumSlide = 0;
                                continue;
                            }

                            float radius = 0.f;
                            if (endMod && sIndex == mod->mSettings.size() - 1)
                                radius = 15.f;
                            else if (endMod)
                                radius = 15.f * (1.f - mod->cAnim);



                            bool endSetting = sIndex == mod->mSettings.size() - 1;
                            float setPadding = endSetting ? (-2.f * animation) : 0.f;

                            ImColor rgb = ColorUtils::getThemedColor(moduleY * 2);
                            // Base the alpha off the animation percentage
                            rgb.Value.w = animation;
                            switch (setting->mType)

                            {
                            case SettingType::Bool: {
                                BoolSetting* boolSetting = reinterpret_cast<BoolSetting*>(setting);
                                // Assume 'lowercase' is already determined.
                                // Pass catPositions[i].y as catY, and catHeight, modRect, etc., accordingly.
                                BoolSettingRenderer::render(
                                    boolSetting,
                                    modRect,
                                    catPositions[i].y,          // catY
                                    catHeight,
                                    moduleY,
                                    setPadding,
                                    modHeight,
                                    animation,
                                    inScale,
                                    screen.y / 2,
                                    textHeight,
                                    textSize,
                                    isEnabled,
                                    catPositions[i].isExtended,
                                    tooltip,
                                    displayColorPicker,
                                    radius,
                                    mod->cAnim,                 // module animation factor
                                    /* indicatorColor */ ColorUtils::getThemedColor(modRect.y), // or another suitable themed color
                                    lastBoolSetting,
                                    isBoolSettingBinding
                                );
                                break;
                            }

                            case SettingType::Keybind:
                            {
                                KeybindSetting* keybindSetting = reinterpret_cast<KeybindSetting*>(setting);
                                KeybindSettingRenderer::render(
                                    keybindSetting,
                                    modRect,
                                    catPositions[i].y,          // catY
                                    catHeight,
                                    moduleY,
                                    setPadding,
                                    modHeight,
                                    animation,
                                    inScale,
                                    screen.y / 2,
                                    textHeight,
                                    textSize,
                                    isEnabled,
                                    lastKeybindSetting,
                                    isKeybindBinding,
                                    tooltip,
                                    radius,
                                    mod->mKey                   // use the module's key
                                );
                                break;
                            }

                            case SettingType::Enum:
                            {
                                EnumSetting* enumSetting = reinterpret_cast<EnumSetting*>(setting);
                                EnumSettingRenderer::render(
                                    enumSetting,
                                    modRect,
                                    catRect,
                                    catPositions[i].y,          // catY
                                    catHeight,
                                    moduleY,
                                    setPadding,
                                    modHeight,
                                    animation,
                                    inScale,
                                    screen.y / 2,
                                    textHeight,
                                    textSize,
                                    isEnabled,
                                    lowercase,
                                    mod->cAnim,                 // module animation factor
                                    tooltip,
                                    radius,
                                    displayColorPicker,
                                    mod->showSettings
                                );
                                break;
                            }

                            case SettingType::Number:
                            {
                                NumberSetting* numSetting = reinterpret_cast<NumberSetting*>(setting);
                                NumberSettingRenderer::render(
                                    numSetting,
                                    modRect,
                                    catRect,
                                    catPositions[i].y + catHeight,  // baseY
                                    modHeight,
                                    setPadding,
                                    animation,
                                    inScale,
                                    screen.y / 2,
                                    textHeight,
                                    textSize,
                                    isEnabled,
                                    lowercase,
                                    mod->cAnim,   // module animation factor
                                    moduleY,
                                    midclickRounding,
                                    mod->showSettings,
                                    catPositions[i].isExtended,
                                    tooltip,
                                    radius,
                                    lastDraggedSetting
                                );
                                break;
                            }


                            case SettingType::Color:
                            {
                                // Assume you have local variables for the parameters.
                                // For example, 'moduleY' is a float holding the current module offset,
                                // 'baseY' is computed from catPositions[i].y + catHeight, etc.
                                ColorSetting* colorSetting = reinterpret_cast<ColorSetting*>(setting);
                                ColorSettingRenderer::render(
                                    colorSetting,
                                    modRect,
                                    catRect,
                                    catPositions[i].y + catHeight, // baseY
                                    modHeight,
                                    setPadding,
                                    animation,
                                    inScale,
                                    screen.y / 2,
                                    textHeight,
                                    textSize,
                                    isEnabled,
                                    lowercase,
                                    mod->cAnim,   // module animation factor
                                    moduleY,
                                    mod->showSettings,
                                    catPositions[i].isExtended,
                                    tooltip,
                                    displayColorPicker,
                                    lastColorSetting
                                );
                                break;
                            }

                            }

                            sIndex++;
                        }

                    }


                    if (modRect.y > catRect.y + 0.5f) {
                        // --- Draw Module Background ---
                        if (mod->cScale <= 1) {
                            if (mod->mEnabled)
                                ImRenderUtils::fillRectangle(modRect, rgb, animation, radius,
                                    ImGui::GetBackgroundDrawList(), ImDrawCornerFlags_BotRight | ImDrawCornerFlags_BotLeft);
                            else
                                ImRenderUtils::fillRectangle(modRect, ImColor(30, 30, 30), animation, radius,
                                    ImGui::GetBackgroundDrawList(), ImDrawCornerFlags_BotRight | ImDrawCornerFlags_BotLeft);
                            ImRenderUtils::fillRectangle(modRect, grayColor, animation, radius,
                                ImGui::GetBackgroundDrawList(), ImDrawCornerFlags_BotRight | ImDrawCornerFlags_BotLeft);
                        }

                        // --- Animate and Render Module Name ---
                        std::string modName = mod->getName();
                        ImVec2 center(modRect.x + modRect.getWidth() / 2.f,
                            modRect.y + modRect.getHeight() / 2.f);
                        // When the category is closing (not extended), force the module scale to animate to 0.
                        float targetScale = (catPositions[i].extensionAnim > 0.95f)
                            ? (mod->mEnabled ? 1.f : 0.f)
                            : 0.f;
                        mod->cScale = MathUtils::animate(targetScale, mod->cScale, ImRenderUtils::getDeltaTime() * 10);

                        ImVec4 scaledRect(center.x - modRect.getWidth() / 2.f,
                            center.y - modRect.getHeight() / 2.f,
                            center.x + modRect.getWidth() / 2.f,
                            center.y + modRect.getHeight() / 2.f);
                        if (mod->cScale > 0) {
                            ImColor rgb1 = rgb;
                            ImColor rgb2 = ColorUtils::getThemedColor(scaledRect.y + (scaledRect.z - scaledRect.x));
                            ImRenderUtils::fillRoundedGradientRectangle(scaledRect, rgb1, rgb2, radius,
                                animation * mod->cScale, animation * mod->cScale, flags);
                        }

                        float textWidth = ImRenderUtils::getTextWidth(&modName, textSize);
                        float cRectCentreX = modRect.x + ((modRect.z - modRect.x) - textWidth) / 2;
                        float cRectCentreY = modRect.y + ((modRect.w - modRect.y) - textHeight) / 2;
                        ImVec2 modPosLerped(cRectCentreX, cRectCentreY);
                        ImRenderUtils::drawText(modPosLerped, modName,
                            mod->mEnabled ? ImColor(255, 255, 255) : ImColor(180, 180, 180),
                            textSize, animation, true);

                        // --- Handle Mouse Interactions for Module Toggling ---
                        // Only process interactions when the category is fully open.
                        if (ImRenderUtils::isMouseOver(modRect) && (catPositions[i].extensionAnim > 0.95f) && isEnabled) {
                            if (ImRenderUtils::isMouseOver(catWindow))
                                tooltip = mod->mDescription;
                            if (ImGui::IsMouseClicked(0) && !displayColorPicker) {
                                if (!moduleToggled)
                                    mod->toggle();
                                ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
                                moduleToggled = true;
                            }
                            else if (ImGui::IsMouseClicked(1) && !displayColorPicker) {
                                if (!mod->mSettings.empty())
                                    mod->showSettings = !mod->showSettings;
                            }
                            else if (ImGui::IsMouseClicked(2) && !displayColorPicker) {
                                lastMod = mod;
                                isBinding = true;
                                ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
                            }
                        }
                    }

                    if (modRect.y > catRect.y - modHeight) {
                        ImRenderUtils::fillGradientOpaqueRectangle(
                            ImVec4(modRect.x, modRect.w, modRect.z,
                                modRect.w + 10.f * mod->cAnim * animation), ImColor(0, 0, 0),
                            ImColor(0, 0, 0), 0.f * animation, 0.55f * animation);
                    }
                    moduleY += modHeight;
                    modIndex++;

                }
            }
            drawList->PopClipRect();
            auto processBinding = [&](auto* setting, bool& bindingFlag, const std::string& message) {
                tooltip = message;
                for (const auto& key : Keyboard::mPressedKeys) {
                    if (key.second && setting) {
                        setting->mKey = (key.first == VK_ESCAPE ? 0 : key.first);
                        bindingFlag = false;
                        ClientInstance::get()->playUi((key.first == VK_ESCAPE) ? "random.break" : "random.orb", 0.75f, 1.0f);
                        break; // Process first key press and exit.
                    }
                }
            };

            if (isBinding && lastMod) {
                processBinding(lastMod.get(), isBinding, "Currently binding " + lastMod->getName() + "... Press ESC to unbind.");
            }
            if (isKeybindBinding && lastKeybindSetting) {
                processBinding(lastKeybindSetting, isKeybindBinding, "Currently binding " + lastKeybindSetting->mName + "... Press ESC to cancel.");
            }
            if (isBoolSettingBinding && lastBoolSetting) {
                processBinding(lastBoolSetting, isBoolSettingBinding, "Currently binding " + lastBoolSetting->mName + "... Press ESC to unbind.");
            }
            // --- Category Header Rendering ---
            // --- Category Header Rendering ---
// Convert the category name to lowercase if needed.
            std::string catName = lowercase ? StringUtils::toLower(categories[i]) : categories[i];

            // Toggle category extension on right-click over the category rectangle.
            if (ImRenderUtils::isMouseOver(catRect) && ImGui::IsMouseClicked(1))
                catPositions[i].isExtended = !catPositions[i].isExtended;

            // Smoothly animate the header's open/close state.
            float targetExtension = catPositions[i].isExtended ? 1.f : 0.f;
            catPositions[i].extensionAnim = MathUtils::animate(targetExtension, catPositions[i].extensionAnim, ImRenderUtils::getDeltaTime() * 10.f);

            // Use extensionAnim to scale the header height.
            float animatedCatHeight = catHeight * catPositions[i].extensionAnim;
            ImVec4 headerRect = ImVec4(catPositions[i].x, catPositions[i].y,
                catPositions[i].x + catWidth, catPositions[i].y + animatedCatHeight)
                .scaleToPoint(ImVec4(screen.x / 2, screen.y / 2, screen.x / 2, screen.y / 2), inScale);

            // Slightly widen the rectangle and fill with a dark background.
            headerRect.w += 1.5f;
            ImRenderUtils::fillRectangle(headerRect, darkBlack, animation, 15, ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersTop);

            // --- Calculate Text Center ---
            FontHelper::pushPrefFont(true, true, true);
            float headerTextHeight = ImGui::GetFont()->CalcTextSizeA(textSize * 18, FLT_MAX, -1, catName.c_str()).y;
            float centerX = headerRect.x + ((headerRect.z - headerRect.x) - ImRenderUtils::getTextWidth(&catName, textSize * 1.15)) / 2;
            float centerY = headerRect.y + ((headerRect.w - headerRect.y) - headerTextHeight) / 2;

            // --- Determine Icon for Category ---
            std::string IconStr = "B";
            if (StringUtils::equalsIgnoreCase(catName, "Combat"))     IconStr = "c";
            else if (StringUtils::equalsIgnoreCase(catName, "Movement")) IconStr = "f";
            else if (StringUtils::equalsIgnoreCase(catName, "Visual"))   IconStr = "d";
            else if (StringUtils::equalsIgnoreCase(catName, "Player"))   IconStr = "e";
            else if (StringUtils::equalsIgnoreCase(catName, "Misc"))     IconStr = "a";

            // --- Render Icon and Category Name ---
            ImGui::PushFont(FontHelper::Fonts["tenacity_icons_large"]);
            // Draw icon slightly inset.
            ImRenderUtils::drawText(ImVec2(headerRect.x + 10, centerY), IconStr, ImColor(255, 255, 255), textSize * 1.15, animation, true);
            ImGui::PopFont();
            ImRenderUtils::drawText(ImVec2(centerX, centerY), catName, ImColor(255, 255, 255), textSize * 1.15, animation, true);
            FontHelper::popPrefFont();  // Pop the header font.

            // --- Clamp Category Position to Screen Bounds ---
            catPositions[i].x = std::clamp(catPositions[i].x, 0.f, screen.x - catWidth);
            catPositions[i].y = std::clamp(catPositions[i].y, 0.f, screen.y - catHeight);

            // --- Handle Dragging of the Category Header ---
            {
                static bool dragging = false;
                static ImVec2 dragOffset;
                if (catPositions[i].isDragging) {
                    if (ImGui::IsMouseDown(0)) {
                        if (!dragging) {
                            dragOffset = ImVec2(ImRenderUtils::getMousePos().x - headerRect.x,
                                ImRenderUtils::getMousePos().y - headerRect.y);
                            dragging = true;
                        }
                        // Calculate new position based on mouse position and offset.
                        ImVec2 newPos = ImRenderUtils::getMousePos() - dragOffset;
                        // Clamp and round the position for smooth movement.
                        newPos.x = std::round(std::clamp(newPos.x, 0.f, screen.x - catWidth) / 2) * 2;
                        newPos.y = std::round(std::clamp(newPos.y, 0.f, screen.y - catHeight) / 2) * 2;
                        catPositions[i].x = newPos.x;
                        catPositions[i].y = newPos.y;
                    }
                    else {
                        catPositions[i].isDragging = false;
                        dragging = false;
                    }
                }
                else if (ImRenderUtils::isMouseOver(headerRect) && ImGui::IsMouseClicked(0) && isEnabled) {
                    catPositions[i].isDragging = true;
                    dragOffset = ImRenderUtils::getMousePos() - ImVec2(headerRect.x, headerRect.y);
                }
            }


#pragma endregion
        }

        if (!tooltip.empty())
        {
            ImVec2 toolTipHeight = ImGui::GetFont()->CalcTextSizeA(textSize * 14.4f, FLT_MAX, 0, tooltip.c_str());
            float textWidth = ImRenderUtils::getTextWidth(&tooltip, textSize * 0.8f);
            float textHeight = toolTipHeight.y;
            float padding = 2.5f;
            float offset = 8.f;

            ImVec4 tooltipRect = ImVec4(
                ImRenderUtils::getMousePos().x + offset - padding,
                ImRenderUtils::getMousePos().y + textHeight / 2 - textHeight - padding,
                ImRenderUtils::getMousePos().x + offset + textWidth + padding * 2,
                ImRenderUtils::getMousePos().y + textHeight / 2 + padding
            ).scaleToPoint(ImVec4(
                               screen.x / 2,
                               screen.y / 2,
                               screen.x / 2,
                               screen.y / 2
                           ), inScale);

            static float alpha = 1.f;

            // If mid or left click is down, lerp the alpha to 0.25f;
            if (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(2))
            {
                alpha = MathUtils::animate(0.0f, alpha, ImRenderUtils::getDeltaTime() * 10);
            }
            else
            {
                alpha = MathUtils::animate(1.f, alpha, ImRenderUtils::getDeltaTime() * 10);
            }

            tooltipRect = tooltipRect.scaleToCenter(alpha);

            ImRenderUtils::fillRectangle(tooltipRect, ImColor(20, 20, 20), animation * alpha, 0.f, ImGui::GetForegroundDrawList());
            ImRenderUtils::drawText(ImVec2(tooltipRect.x + padding, tooltipRect.y + padding), tooltip,
                                   ImColor(255, 255, 255), (textSize * 0.8f) * alpha, animation * alpha, true, 0, ImGui::GetForegroundDrawList());
        }

        if (isEnabled)
        {
            scrollDirection = 0;
        }
    }
    // --- Begin Search Bar Section ---
    {
        // Get ImGui IO once.
        ImGuiIO& io = ImGui::GetIO();

        // If search mode is active, process input characters.
        if (isSearching) {
            for (unsigned int c : io.InputQueueCharacters) {
                // Append only printable characters.
                if (c >= 32 && c < 127) {
                    searchingModule.push_back(static_cast<char>(c));
                }
            }
            io.InputQueueCharacters.clear();

            // Handle backspace and delete keys.
            if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !searchingModule.empty()) {
                searchingModule.pop_back();
            }
            if (ImGui::IsKeyPressed(ImGuiKey_Delete)) {
                searchingModule.clear();
            }
        }

        // Get current mouse position.
        ImVec2 mousePos = ImRenderUtils::getMousePos();

        // Define the search region (bottom center of screen).
        const float searchWidth = 400.f;
        const float searchHeight = 40.f;
        ImVec4 searchRegion(
            screen.x / 2.f - 275.f,
            screen.y / 1.25f,
            screen.x / 2.f + 275.f,
            screen.y
        );

        // Static state for search animations.
        static float searchDuration = 1.f;
        static float closeDuration = 3.f;

        // Adjust appearance based on mouse position.
        if (mousePos.x >= searchRegion.x && mousePos.x <= searchRegion.z &&
            mousePos.y >= searchRegion.y && mousePos.y <= searchRegion.w) {
            // While the mouse is over the search region, animate to fully open (1.0)
            searchDuration = MathUtils::lerp(searchDuration, 1.f, io.DeltaTime * 10.f);
            closeDuration = 3.f;
        }
        else if (!isSearching && searchingModule.empty()) {
            // After a delay (closeDuration), animate to fully closed (0.0)
            if (closeDuration < 0.f)
                searchDuration = MathUtils::lerp(searchDuration, 0.f, io.DeltaTime * 10.f);
            else
                closeDuration -= io.DeltaTime;
        }


        // Compute the search bar rectangle (slides vertically based on searchDuration).
        ImVec4 searchRectPos(
            screen.x / 2.f - searchWidth / 2.f,
            screen.y - searchHeight / 2.f + 10.f - 50.f * searchDuration,
            screen.x / 2.f + searchWidth / 2.f,
            screen.y + searchHeight / 2.f + 10.f - 50.f * searchDuration
        );

        // Compute the inner "type" area (reserve space for the prompt on the right).
        std::string searchPrompt = "Search ";
        float promptWidth = ImRenderUtils::getTextWidth(&searchPrompt, textSize);
        ImVec4 typeRectPos(
            searchRectPos.x + 5.f,
            searchRectPos.y + 5.f,
            searchRectPos.z - promptWidth - 10.f,
            searchRectPos.w - 5.f
        );

        // Activate search mode when the mouse clicks inside the type area.
        if (ModernGui::isMouseOver(typeRectPos) && ImGui::IsMouseClicked(0))
            isSearching = true;
        else if (!ModernGui::isMouseOver(typeRectPos))
            isSearching = false;

        // Render search bar background.
        ImRenderUtils::fillRectangle(searchRectPos, ImColor(29, 29, 29), 1.0f, 7.5f, ImGui::GetBackgroundDrawList(), 0);
        ImRenderUtils::fillRectangle(typeRectPos, ImColor(21, 21, 21), 1.0f, 7.5f, ImGui::GetBackgroundDrawList(), 0);

        // Draw the search prompt on the right side.
        ImRenderUtils::drawText(
            ImVec2(typeRectPos.z + 5.f, typeRectPos.y),
            searchPrompt, ImColor(255, 255, 255), textSize, 1.0f, false, 0, ImGui::GetForegroundDrawList()
        );

        // Render search text if present.
        float currentTextWidth = ImRenderUtils::getTextWidth(&searchingModule, textSize);
        if (!searchingModule.empty()) {
            ImDrawList* d = ImGui::GetForegroundDrawList();
            d->PushClipRect(ImVec2(typeRectPos.x + 5.f, typeRectPos.y), ImVec2(typeRectPos.z - 5.f, typeRectPos.w), true);
            // Shift text to the left if it overflows.
            ImVec2 typeTextPos(typeRectPos.x + 5.f, typeRectPos.y + 5.f);
            if (typeRectPos.x + currentTextWidth > typeRectPos.z - 15.f) {
                typeTextPos.x -= (typeRectPos.x + currentTextWidth) - (typeRectPos.z - 15.f);
            }
            ImRenderUtils::drawText(typeTextPos, searchingModule, ImColor(255, 255, 255), textSize, 1.0f, false, 0, d);
            d->PopClipRect();
        }
        else if (!isSearching) {
            // Show placeholder text when no input.
            ImRenderUtils::drawText(
                ImVec2(typeRectPos.x + 5.f, typeRectPos.y + 5.f),
                "Search for Module :)", ImColor(125, 125, 125), textSize, 1.0f, false, 0, ImGui::GetForegroundDrawList()
            );
        }

        // Blinking caret animation.
        static float caretOpacity = 1.f;
        static bool caretIncreasing = false;
        if (!caretIncreasing) {
            caretOpacity -= io.DeltaTime * 2.f;
            if (caretOpacity < 0.f) { caretOpacity = 0.f; caretIncreasing = true; }
        }
        else {
            caretOpacity += io.DeltaTime * 2.f;
            if (caretOpacity > 1.f) { caretOpacity = 1.f; caretIncreasing = false; }
        }
        ImVec2 caretPos(typeRectPos.x + 5.f + currentTextWidth, typeRectPos.y + 5.f);
        ImRenderUtils::fillRectangle(
            ImVec4(caretPos.x, caretPos.y, caretPos.x + 2.f, caretPos.y + textSize),
            ImColor(255, 255, 255, static_cast<int>(caretOpacity * 255)),
            1.0f, 0, ImGui::GetForegroundDrawList(), 0
        );
    }
    // --- End Search Bar Section ---

    ImGui::PopFont();



}

void ModernGui::onWindowResizeEvent(WindowResizeEvent& event)
{
    resetPosition = true;
    lastReset = NOW;
}
