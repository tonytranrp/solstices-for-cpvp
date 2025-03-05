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

                    // Animate the setting-animation percentage for this module.
                    float targetAnim = mod->showSettings ? 1.f : 0.f;
                    mod->cAnim = MathUtils::animate(targetAnim, mod->cAnim, ImRenderUtils::getDeltaTime() * 12.5f);
                    mod->cAnim = MathUtils::clamp(mod->cAnim, 0.f, 1.f);

                    // Only render settings if cAnim is greater than a small threshold (to avoid 'sticking').
                    if (mod->cAnim > 0.001f)
                    {
                        static bool wasDragging = false;
                        Setting* lastDraggedSetting = nullptr;

                        int sIndex = 0;
                        for (auto& setting : mod->mSettings)
                        {
                            // If the setting is hidden, reset its animations and skip it.
                            if (!setting->mIsVisible()) {
                                setting->sliderEase = 0.f;
                                setting->enumSlide = 0.f;
                                continue;
                            }

                            // Example corner rounding logic
                            float radius = 0.f;
                            bool isLastSetting = (sIndex == (mod->mSettings.size() - 1));
                            if (endMod && isLastSetting) {
                                // If it’s the last setting in the entire category
                                radius = 15.f;
                            }
                            else if (endMod) {
                                radius = 15.f * (1.f - mod->cAnim);
                            }

                            // Possibly add or remove some bottom padding on the final setting row
                            bool endSetting = (sIndex == mod->mSettings.size() - 1);
                            float setPadding = endSetting ? (-2.f * animation) : 0.f;

                            // Example for color alpha = module’s cAnim * GUI animation
                            ImColor rgb = ColorUtils::getThemedColor(moduleY * 2);
                            rgb.Value.w = animation * mod->cAnim;

                            // Normal switch by SettingType
                            switch (setting->mType)
                            {
                            case SettingType::Bool: {
                                auto boolSetting = reinterpret_cast<BoolSetting*>(setting);
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
                                    mod->cAnim,
                                    ColorUtils::getThemedColor(modRect.y),
                                    lastBoolSetting,
                                    isBoolSettingBinding
                                );
                                break;
                            }
                            case SettingType::Keybind:
                            {
                                auto keybindSetting = reinterpret_cast<KeybindSetting*>(setting);
                                KeybindSettingRenderer::render(
                                    keybindSetting,
                                    modRect,
                                    catPositions[i].y,
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
                                    mod->mKey
                                );
                                break;
                            }
                            case SettingType::Enum:
                            {
                                auto enumSetting = reinterpret_cast<EnumSetting*>(setting);
                                EnumSettingRenderer::render(
                                    enumSetting,
                                    modRect,
                                    catRect,
                                    catPositions[i].y,
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
                                    mod->cAnim,
                                    tooltip,
                                    radius,
                                    displayColorPicker,
                                    mod->showSettings
                                );
                                break;
                            }
                            case SettingType::MultiSelect: {
                                auto multiSetting = reinterpret_cast<MultiSelectSetting*>(setting);
                                MultiSelectSettingRenderer::render(
                                    multiSetting,
                                    modRect,
                                    catRect,
                                    catPositions[i].y,
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
                                    mod->cAnim,
                                    tooltip,
                                    radius,
                                    displayColorPicker,
                                    mod->showSettings
                                );
                                break;
                            }
                            case SettingType::Number:
                            {
                                auto numSetting = reinterpret_cast<NumberSetting*>(setting);
                                NumberSettingRenderer::render(
                                    numSetting,
                                    modRect,
                                    catRect,
                                    catPositions[i].y + catHeight,
                                    modHeight,
                                    setPadding,
                                    animation,
                                    inScale,
                                    screen.y / 2,
                                    textHeight,
                                    textSize,
                                    isEnabled,
                                    lowercase,
                                    mod->cAnim,
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
                                auto colorSetting = reinterpret_cast<ColorSetting*>(setting);
                                ColorSettingRenderer::render(
                                    colorSetting,
                                    modRect,
                                    catRect,
                                    catPositions[i].y + catHeight,
                                    modHeight,
                                    setPadding,
                                    animation,
                                    inScale,
                                    screen.y / 2,
                                    textHeight,
                                    textSize,
                                    isEnabled,
                                    lowercase,
                                    mod->cAnim,
                                    moduleY,
                                    mod->showSettings,
                                    catPositions[i].isExtended,
                                    tooltip,
                                    displayColorPicker,
                                    lastColorSetting
                                );
                                break;
                            }
                            } // end switch

                            sIndex++;
                        } // end for each setting
                    } // end if (mod->cAnim > 0.001f)



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
        // 1) Animation states:
        static float searchAnim = 0.0f;  // 0 => fully "down", 1 => fully "up"
        static float idleTimer = 3.0f;   // Timer before we begin sliding it down
        static const float openAnimValue = 1.0f;    // Where the bar is fully visible
        static const float closedAnimValue = 0.3f;  // “Lowered” position so it remains partially visible
        static bool  searchingPreviously = false;

        // We'll assume you have these external variables somewhere:
        // bool isSearching;
        // std::string searchingModule;   // The user's typed text
        // float textSize;                // Possibly 18.f or something
        // ImVec2 screen;                 // The size of the window/screen

        ImGuiIO& io = ImGui::GetIO();

        // --- A) Process typed characters if user is in search mode ---
        if (isSearching) {
            for (unsigned int c : io.InputQueueCharacters) {
                // Append only ASCII‐printable characters.
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

        ImVec2 mousePos = ImRenderUtils::getMousePos();
        const float searchWidth = 400.f;
        const float searchHeight = 40.f;

        // The rectangle in which we consider "hovering" to open the search
        ImVec4 searchRegion(
            screen.x / 2.f - 275.f,
            screen.y / 1.25f,
            screen.x / 2.f + 275.f,
            screen.y
        );

        // If the user's mouse is inside that region, we consider it "hovered".
        bool isOverSearchRegion =
            (mousePos.x >= searchRegion.x && mousePos.x <= searchRegion.z &&
                mousePos.y >= searchRegion.y && mousePos.y <= searchRegion.w);

        // --- B) Update the searchAnim based on whether we want it "up" or "down" ---
        if (isSearching || isOverSearchRegion) {
            // Animate toward 'openAnimValue'
            searchAnim = MathUtils::animate(openAnimValue, searchAnim, io.DeltaTime * 6.f);
            idleTimer = 3.0f;  // reset idle timer
        }
        else {
            // If no text is typed and we’re not searching, eventually slide down
            if (searchingModule.empty()) {
                idleTimer -= io.DeltaTime;
                if (idleTimer <= 0.f) {
                    // Slide it down but do NOT hide it completely:
                    searchAnim = MathUtils::animate(closedAnimValue, searchAnim, io.DeltaTime * 6.f);
                }
            }
            // Otherwise, if there *is* typed text, you might keep it fully up, etc.
        }

        // --- C) Position the bar using 'searchAnim' only for vertical sliding. ---
        float verticalSlide = 50.f * searchAnim;
        ImVec4 searchRectPos(
            screen.x / 2.f - searchWidth * 0.5f,
            (screen.y - searchHeight * 0.5f + 10.f) - verticalSlide,
            screen.x / 2.f + searchWidth * 0.5f,
            (screen.y + searchHeight * 0.5f + 10.f) - verticalSlide
        );

        // We'll keep a constant barAlpha = 1.0f
        float barAlpha = 1.0f;


        ImVec4 typeRectPos(
            searchRectPos.x + 5.f,
            searchRectPos.y + 5.f,
            searchRectPos.z, // reserve space for the icon
            searchRectPos.w - 5.f
        );

        // E) Mouse detection
        bool isInTypeArea = ModernGui::isMouseOver(typeRectPos);
        if (isInTypeArea && ImGui::IsMouseClicked(0)) {
            isSearching = true;
        }
        else if (!isInTypeArea && ImGui::IsMouseClicked(0)) {
            isSearching = false;
        }

        // --- F) Always draw the bar
        {
            // 1) If you want to keep calling blur logic:
            ImVec4 blurRect = searchRectPos;
            blurRect.x -= 10.f;
            blurRect.y -= 10.f;
            blurRect.z += 10.f;
            blurRect.w += 10.f;

            float blurFactor = 25.f * barAlpha;
            ImRenderUtils::addBlur(blurRect, blurFactor, 0);

            // 2) Overdraw a fully opaque rectangle
            ImColor backgroundColor(29, 29, 29, 255);
            ImRenderUtils::fillRectangle(
                searchRectPos,
                backgroundColor,
                1.0f,    // corner scale factor
                7.5f,    // corner radius
                ImGui::GetBackgroundDrawList(),
                0
            );

            // Sub-rectangle for typing
            ImColor typeRectColor(21, 21, 21, 255);
            ImRenderUtils::fillRectangle(
                typeRectPos,
                typeRectColor,
                1.0f,
                7.5f,
                ImGui::GetBackgroundDrawList(),
                0
            );
            // Render the user’s typed text
            float currentTextWidth = ImRenderUtils::getTextWidth(&searchingModule, textSize);
            if (!searchingModule.empty())
            {
                ImDrawList* fgDraw = ImGui::GetForegroundDrawList();
                fgDraw->PushClipRect(
                    ImVec2(typeRectPos.x + 5.f, typeRectPos.y),
                    ImVec2(typeRectPos.z - 5.f, typeRectPos.w),
                    true
                );

                ImVec2 typeTextPos(typeRectPos.x + 5.f, typeRectPos.y + 5.f);
                float rightLimit = typeRectPos.z - 15.f;
                if (typeTextPos.x + currentTextWidth > rightLimit) {
                    float overflow = (typeTextPos.x + currentTextWidth) - rightLimit;
                    typeTextPos.x -= overflow;
                }

                ImColor textCol(255, 255, 255, 255);
                ImRenderUtils::drawText(
                    typeTextPos,
                    searchingModule,
                    textCol,
                    textSize,
                    1.0f,
                    false,
                    0,
                    fgDraw
                );
                fgDraw->PopClipRect();
            }
            else if (!isSearching)
            {
                // Show placeholder if not searching
                std::string placeholder = "Search for Module :)";
                ImColor placeholderCol(125, 125, 125, 255);
                ImRenderUtils::drawText(
                    ImVec2(typeRectPos.x + 5.f, typeRectPos.y + 5.f),
                    placeholder,
                    placeholderCol,
                    textSize,
                    1.0f,
                    false,
                    0,
                    ImGui::GetForegroundDrawList()
                );
            }

            // G) Caret blinking if isSearching
            if (isSearching)
            {
                static float caretOpacity = 1.f;
                static bool  caretIncreasing = false;
                float caretSpeed = 2.f;
                if (!caretIncreasing) {
                    caretOpacity -= io.DeltaTime * caretSpeed;
                    if (caretOpacity < 0.f) {
                        caretOpacity = 0.f;
                        caretIncreasing = true;
                    }
                }
                else {
                    caretOpacity += io.DeltaTime * caretSpeed;
                    if (caretOpacity > 1.f) {
                        caretOpacity = 1.f;
                        caretIncreasing = false;
                    }
                }

                // Draw caret at the end of typed text
                ImVec2 caretPos(typeRectPos.x + 5.f + currentTextWidth, typeRectPos.y + 5.f);
                ImColor caretColor(255, 255, 255, (int)(caretOpacity * 255));
                ImRenderUtils::fillRectangle(
                    ImVec4(caretPos.x, caretPos.y, caretPos.x + 2.f, caretPos.y + textSize),
                    caretColor,
                    1.0f,
                    0,
                    ImGui::GetForegroundDrawList(),
                    0
                );
            }
        }
}
    // --- End Search Bar Section ---




    ImGui::PopFont();



}

void ModernGui::onWindowResizeEvent(WindowResizeEvent& event)
{
    resetPosition = true;
    lastReset = NOW;
}
