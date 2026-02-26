//
// Created by Tozic on 7/15/2024.
//

#include "ModernDropdown.hpp"
#include <Features/GUI/ScreenManager/Core/ScreenBuilder.hpp>
#include <Features/GUI/ScreenManager/Core/ScreenContext.hpp>
#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/GUI/ScreenManager/Screens/ColorPickerScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/ListChooserScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/ProgressOverlayScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/Settings/BoolSettingScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/Settings/ButtonSettingScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/Settings/ColorSettingScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/Settings/EnumSettingScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/Settings/ListSettingScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/Settings/NumberSettingScreen.hpp>
#include <Features/GUI/ScreenManager/Screens/Settings/SettingRenderContext.hpp>
#include <Features/Modules/ModuleCategory.hpp>
#include <Features/Modules/Visual/ClickGui.hpp>
#include <Utils/FontHelper.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/Modules/Visual/Interface.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>
#include <Utils/StringUtils.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>
#include <algorithm>
#include <ranges>

namespace
{
    GuiScreen::ScreenManager& ensureScreenManager(ModernGui& gui)
    {
        if (!gui.mScreenManager)
        {
            gui.mScreenManager = std::make_unique<GuiScreen::ScreenManager>();
        }

        if (!gui.mScreenBuilder)
        {
            gui.mScreenBuilder = std::make_unique<GuiScreen::ScreenBuilder>(*gui.mScreenManager);
        }

        return *gui.mScreenManager;
    }
}

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

void ModernGui::beginModuleBinding(const std::shared_ptr<Module>& module)
{
    ensureScreenManager(*this).beginModuleBinding(module);
    lastMod = module;
    isBinding = (module != nullptr);
    isBoolSettingBinding = false;
    lastBoolSetting = nullptr;
}

void ModernGui::beginBoolSettingBinding(BoolSetting* setting)
{
    ensureScreenManager(*this).beginBoolSettingBinding(setting);
    lastBoolSetting = setting;
    isBoolSettingBinding = (setting != nullptr);
    isBinding = false;
    lastMod = nullptr;
}

bool ModernGui::commitBindingKey(int key)
{
    const bool committed = ensureScreenManager(*this).commitBindingKey(key);
    if (committed)
    {
        cancelBindings();
    }
    return committed;
}

void ModernGui::cancelBindings()
{
    ensureScreenManager(*this).cancelBindings();
    isBinding = false;
    isBoolSettingBinding = false;
    lastMod = nullptr;
    lastBoolSetting = nullptr;
}

std::string ModernGui::getSearchQuery() const
{
    return std::string(StringUtils::trim(mSearchBuffer));
}

bool ModernGui::hasSearchQuery() const
{
    return !getSearchQuery().empty();
}

bool ModernGui::isSearchActive() const
{
    return mSearchFocused;
}

bool ModernGui::closeOverlayOnEscape()
{
    if (!mScreenManager)
    {
        return false;
    }

    if (mScreenManager->isListChooserOpen())
    {
        mScreenManager->closeListChooser();
        return true;
    }

    if (mScreenManager->isColorPickerOpen())
    {
        mScreenManager->closeColorPicker();
        return true;
    }

    return false;
}

void ModernGui::openProgressOverlay(const std::string& ownerId, const std::string& title, const std::string& status)
{
    ensureScreenManager(*this).openProgressOverlay(ownerId, title, status);
}

void ModernGui::updateProgressOverlay(const std::string& ownerId, const float progress, const std::string& status)
{
    ensureScreenManager(*this).updateProgressOverlay(ownerId, progress, status);
}

void ModernGui::closeProgressOverlay(const std::string& ownerId)
{
    if (!mScreenManager)
    {
        return;
    }

    mScreenManager->closeProgressOverlay(ownerId);
}

void ModernGui::clearSearchQuery()
{
    mSearchBuffer[0] = '\0';
    if (mScreenManager)
    {
        mScreenManager->clearSearchQuery();
    }
}

void ModernGui::render(float animation, float inScale, int& scrollDirection, char* h, float blur, float midclickRounding, bool isPressingShift)
{
    auto& screenManager = ensureScreenManager(*this);
    if (mScreenBuilder)
    {
        mScreenBuilder->sync(gFeatureManager->mModuleManager->mModules);
    }

    static auto interfaceMod = gFeatureManager->mModuleManager->getModule<Interface>();
    bool lowercase = interfaceMod->mNamingStyle.mValue == NamingStyle::Lowercase || interfaceMod->mNamingStyle.mValue == NamingStyle::LowercaseSpaced;

    FontHelper::pushPrefFont(true, false, true);
    ImVec2 screen = ImRenderUtils::getScreenSize();
    float deltaTime = ImGui::GetIO().DeltaTime;
    auto drawList = ImGui::GetBackgroundDrawList();
    GuiScreen::ScreenContext screenContext{
        .screenSize = screen,
        .animation = animation,
        .inScale = inScale,
        .isEnabled = false,
        .lowercase = lowercase,
    };

    // If the reset position bool is true and lastReset was more than 100ms ago, reset the position
    if (resetPosition && NOW - lastReset > 100)
    {
        catPositions.clear();
        ImVec2 screen = ImRenderUtils::getScreenSize();
        auto categories = ModuleCategoryNames;
        if (catPositions.empty())
        {
            float centerX = screen.x / 2.f;
            float xPos = centerX - (categories.size() * (catWidth + catGap) / 2);
            for (std::string& category : categories)
            {
                CategoryPosition pos;
                pos.x = xPos;
                pos.y = catGap * 2;
                pos.x = std::round(pos.x / 2) * 2;
                pos.y = std::round(pos.y / 2) * 2;

                xPos += catWidth + catGap;
                catPositions.push_back(pos);
            }
        }
        resetPosition = false;
    }

    /*ImRenderUtils::fillRectangle(
            ImVec4(0, 0, screen.x, screen.y),
            ImColor(0, 0, 0), animation * 0.38f);*/
    drawList->AddRectFilled(ImVec2(0, 0), ImVec2(screen.x, screen.y), IM_COL32(0, 0, 0, 255 * animation * 0.38f));
    ImRenderUtils::addBlur(ImVec4(0.f, 0.f, screen.x, screen.y),
                           animation * blur, 0);

    // Draw a glow rect on the bottom 1/3 of the screen
    ImColor shadowRectColor = ColorUtils::getThemedColor(0);
    shadowRectColor.Value.w = 0.5f * animation;

    // Draw a gradient rect on the bottom 1/3 of the screen with the shadow color, and making the alpha taper off as it goes up
    float firstheight = (screen.y - screen.y / 3);
    // As the animation reaches 0, the height should go below the screen height
    firstheight = MathUtils::lerp(screen.y, firstheight, inScale);
    ImRenderUtils::fillGradientOpaqueRectangle(
        ImVec4(0, firstheight, screen.x, screen.y),
        shadowRectColor, shadowRectColor, 0.4f * inScale, 0.0f);

    static std::vector<std::string> categories = ModuleCategoryNames;

    static auto* clickGui = gFeatureManager->mModuleManager->getModule<ClickGui>();
    bool isEnabled = clickGui->mEnabled;
    screenContext.isEnabled = isEnabled;
    std::string tooltip = "";

    float textSize = inScale;
    //float textHeight = ImRenderUtils::getTextHeight(textSize);
    float textHeight = ImGui::GetFont()->CalcTextSizeA(textSize * 18, FLT_MAX, -1, "").y;

    if (!isEnabled)
    {
        screenManager.closeColorPicker();
        screenManager.closeListChooser();
    }

    displayColorPicker = screenManager.isColorPickerOpen();
    displayListChooser = screenManager.isListChooserOpen();
    lastColorSetting = screenManager.activeColorSetting();
    lastListSetting = screenManager.activeListSetting();

    const bool overlayOpen = screenManager.isOverlayOpen();
    static float popupPushOffsetX = 0.f;
    const float popupPushTargetX = overlayOpen ? -(screen.x + catWidth + catGap) : 0.f;
    popupPushOffsetX = MathUtils::animate(popupPushTargetX, popupPushOffsetX, ImRenderUtils::getDeltaTime() * 10.f);

    const float searchPadding = 10.f;
    const float searchWidth = 320.f;
    const float searchHeight = 26.f;
    const float searchY = MathUtils::clamp(screen.y - searchHeight - 12.f, 0.f, screen.y - searchHeight);
    const ImVec2 searchPos = ImVec2(searchPadding + popupPushOffsetX, searchY);
    const ImVec2 searchSize = ImVec2(searchWidth, searchHeight);

    const std::string searchQuery = getSearchQuery();

    if (catPositions.empty() && isEnabled)
    {
        float centerX = screen.x / 2.f;
        float xPos = centerX - (categories.size() * (catWidth + catGap) / 2);
        for (std::string& category : categories)
        {
            CategoryPosition pos;
            pos.x = xPos;
            pos.y = catGap * 2;
            pos.x = std::round(pos.x / 2) * 2;
            pos.y = std::round(pos.y / 2) * 2;
            xPos += catWidth + catGap;
            catPositions.push_back(pos);
        }
    }

    if (!catPositions.empty())
    {
        for (size_t i = 0; i < categories.size(); i++)
        {
            const float categoryRenderX = catPositions[i].x + popupPushOffsetX;
            // Mod math stuff
            const float modWidth = catWidth;
            const float modHeight = catHeight;
            float moduleY = -catPositions[i].yOffset;

            // Get all the modules and populate our vector
            const auto& modsInCategory = gFeatureManager->mModuleManager->getModulesInCategory(i);
            std::vector<std::shared_ptr<Module>> visibleModsInCategory;
            visibleModsInCategory.reserve(modsInCategory.size());

            for (const auto& mod : modsInCategory)
            {
                if (searchQuery.empty() || StringUtils::containsIgnoreCase(mod->getName(), searchQuery))
                {
                    visibleModsInCategory.push_back(mod);
                }
            }

            // Calculate the catRect pos
            ImVec4 catRect = ImVec4(categoryRenderX, catPositions[i].y,
                                                    categoryRenderX + catWidth, catPositions[i].y + catHeight)
                .scaleToPoint(ImVec4(screen.x / 2,
                                             screen.y / 2,
                                             screen.x / 2,
                                             screen.y / 2), inScale);

            /* Calculate the height of the catWindow including the settings */
            float settingsHeight = 0;

            for (const auto& mod : visibleModsInCategory)
            {
                std::string modLower = mod->getName();

                std::transform(modLower.begin(), modLower.end(), modLower.begin(), [](unsigned char c)
                {
                    return std::tolower(c);
                });

                for (const auto& setting : mod->mSettings)
                {
                    switch (setting->mType)
                    {
                    case SettingType::Bool:
                        {
                            settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, mod->cAnim);
                            break;
                        }
                    case SettingType::Enum:
                        {
                            EnumSetting* enumSetting = reinterpret_cast<EnumSetting*>(setting);
                            std::vector<std::string> enumValues = enumSetting->mValues;
                            int numValues = static_cast<int>(enumValues.size());
                            auto& enumState = screenManager.enumState(setting);

                            settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, mod->cAnim);
                            if (enumState.slide > 0.01f)
                            {
                                for (int j = 0; j < numValues; j++)
                                    settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight,
                                                                enumState.slide);
                            }
                            break;
                        }
                    case SettingType::Number:
                        {
                            settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, mod->cAnim);
                            break;
                        }
                    case SettingType::Color:
                        {
                            settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, mod->cAnim);
                            break;
                        }
                    case SettingType::List:
                        {
                            settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, mod->cAnim);
                            break;
                        }
                    case SettingType::Button:
                        {
                            settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, mod->cAnim);
                            break;
                        }
                    }
                }

                settingsHeight = MathUtils::lerp(settingsHeight, settingsHeight + modHeight, mod->cAnim);
            }

            float catWindowHeight = catHeight + modHeight * visibleModsInCategory.size() + settingsHeight;
            if (visibleModsInCategory.empty())
            {
                catWindowHeight += modHeight;
            }
            ImVec4 catWindow = ImVec4(categoryRenderX, catPositions[i].y,
                                                      categoryRenderX + catWidth,
                                                      catPositions[i].y + moduleY + catWindowHeight)
                .scaleToPoint(ImVec4(screen.x / 2,
                                             screen.y / 2,
                                             screen.x / 2,
                                             screen.y / 2), inScale);
            ImColor rgb = ColorUtils::getThemedColor(i * 20);

            // Can we scroll?
            if (ImRenderUtils::isMouseOver(catWindow) && catPositions[i].isExtended && !screenManager.isOverlayOpen())
            {
                if (scrollDirection > 0)
                {
                    catPositions[i].scrollEase += scrollDirection * catHeight;
                    if (catPositions[i].scrollEase > catWindowHeight - modHeight * 2)
                        catPositions[i].scrollEase = catWindowHeight - modHeight * 2;
                }
                else if (scrollDirection < 0)
                {
                    catPositions[i].scrollEase += scrollDirection * catHeight;
                    if (catPositions[i].scrollEase < 0)
                        catPositions[i].scrollEase = 0;
                }
                scrollDirection = 0;
            }

            // Lerp the category extending
            if (!catPositions[i].isExtended)
            {
                catPositions[i].scrollEase = catWindowHeight - catHeight;
                catPositions[i].wasExtended = false;
            }
            else if (!catPositions[i].wasExtended)
            {
                catPositions[i].scrollEase = 0;
                catPositions[i].wasExtended = true;
            }

            // Lerp the scrolling cuz smooth
            catPositions[i].yOffset = MathUtils::animate(catPositions[i].scrollEase, catPositions[i].yOffset,
                                                    ImRenderUtils::getDeltaTime() * 10.5);

            ImVec4 clipRect = ImVec4(catRect.x, catRect.w, catRect.z, screen.y);
            drawList->PushClipRect(ImVec2(clipRect.x, clipRect.y), ImVec2(clipRect.z, clipRect.w), true);

            int modIndex = 0;
            bool endMod = false;
            bool moduleToggled = false;
            for (const auto& mod : visibleModsInCategory)
            {
                ImDrawFlags flags = ImDrawFlags_RoundCornersBottom;
                float radius = 0.f;
                if (modIndex == visibleModsInCategory.size() - 1) {
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
                    ImVec4 modRect = ImVec4(categoryRenderX,
                                                            catPositions[i].y + catHeight + moduleY,
                                                            categoryRenderX + modWidth,
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
                        bool hasVisibleSettings = false;
                        for (const auto& setting : mod->mSettings)
                        {
                            if (setting->mIsVisible())
                            {
                                hasVisibleSettings = true;
                                break;
                            }
                        }

                        moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, mod->cAnim);
                        ImVec4 bindRect = ImVec4(
                                modRect.x, catPositions[i].y + catHeight + moduleY, modRect.z,
                                catPositions[i].y + catHeight + moduleY + modHeight)
                            .scaleToPoint(
                                ImVec4(modRect.x, screen.y / 2, modRect.z, screen.y / 2),
                                inScale);
                        bindRect.y = std::floor(bindRect.y);
                        if (bindRect.y < modRect.y)
                        {
                            bindRect.y = modRect.y;
                        }

                        if (bindRect.y > catRect.y + 0.5f)
                        {
                            const bool isBindingThisModule = isBinding && lastMod == mod;
                            std::string bindSettingName = lowercase ? "keybind" : "Keybind";
                            std::string bindValue = isBindingThisModule ? "PRESS A KEY..." : mod->getKeybindName();
                            if (lowercase)
                            {
                                bindValue = StringUtils::toLower(bindValue);
                            }

                            float bindRadius = 0.f;
                            if (endMod && !hasVisibleSettings)
                            {
                                bindRadius = 15.f;
                            }
                            else if (endMod)
                            {
                                bindRadius = 15.f * (1.f - mod->cAnim);
                            }

                            ImRenderUtils::fillRectangle(bindRect, ImColor(30, 30, 30), animation, bindRadius, ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

                            if (ImRenderUtils::isMouseOver(bindRect) && isEnabled && catPositions[i].isExtended)
                            {
                                tooltip = isBindingThisModule
                                    ? "Press any key. ESC clears the bind."
                                    : "Left click to set keybind. Right click to clear.";

                                if (ImGui::IsMouseClicked(0) && !screenManager.isOverlayOpen() && mod->showSettings)
                                {
                                    beginModuleBinding(mod);
                                    ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
                                }
                                else if (ImGui::IsMouseClicked(1) && !screenManager.isOverlayOpen() && mod->showSettings)
                                {
                                    mod->setKeybind(0);
                                    cancelBindings();
                                    ClientInstance::get()->playUi("random.break", 0.75f, 1.0f);
                                }
                            }

                            float cBindRectCentreY = bindRect.y + ((bindRect.w - bindRect.y) - textHeight) / 2;
                            auto bindValueLen = ImRenderUtils::getTextWidth(&bindValue, textSize);
                            ImRenderUtils::drawText(ImVec2(bindRect.x + 5.f, cBindRectCentreY), bindSettingName,
                                                   ImColor(255, 255, 255), textSize, animation, true);
                            ImRenderUtils::drawText(ImVec2((bindRect.z - 5.f) - bindValueLen, cBindRectCentreY), bindValue,
                                                   ImColor(170, 170, 170), textSize, animation, true);
                        }

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
                            GuiScreen::Settings::SettingRenderContext settingContext{
                                .manager = screenManager,
                                .module = mod,
                                .moduleY = moduleY,
                                .modRect = modRect,
                                .catRect = catRect,
                                .screen = screen,
                                .categoryY = catPositions[i].y,
                                .categoryHeaderHeight = catHeight,
                                .setPadding = setPadding,
                                .modHeight = modHeight,
                                .textHeight = textHeight,
                                .textSize = textSize,
                                .inScale = inScale,
                                .animation = animation,
                                .midclickRounding = midclickRounding,
                                .isEnabled = isEnabled,
                                .categoryExtended = catPositions[i].isExtended,
                                .lowercase = lowercase,
                                .themedColor = rgb,
                                .tooltip = tooltip,
                                .lastDraggedSetting = lastDraggedSetting,
                            };
                            switch (setting->mType)

                            {
                            case SettingType::Bool:
                                {
                                    BoolSetting* boolSetting = reinterpret_cast<BoolSetting*>(setting);
                                    GuiScreen::Settings::renderBoolSetting(setting, boolSetting, settingContext);
                                    break;
                                    moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, mod->cAnim);

                                    ImVec4 rect = ImVec4(
                                            modRect.x, catPositions[i].y + catHeight + moduleY + setPadding, modRect.z,
                                            catPositions[i].y + catHeight + moduleY + modHeight)
                                        .scaleToPoint(
                                            ImVec4(modRect.x, screen.y / 2,
                                                           modRect.z, screen.y / 2),
                                            inScale);
                                    rect.y = std::floor(rect.y);
                                    // Clamp rect start Y to top of the modRect
                                    if (rect.y < modRect.y)
                                    {
                                        rect.y = modRect.y;
                                    }

                                    if (rect.y > catRect.y + 0.5f)
                                    {
                                        std::string setName = lowercase ? StringUtils::toLower(setting->mName) : setting->mName;
                                        ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), animation, radius, ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

                                        if (ImRenderUtils::isMouseOver(rect) && isEnabled && catPositions[i].isExtended)
                                        {
                                            tooltip = setting->mDescription;
                                            if (ImGui::IsMouseClicked(0) && !screenManager.isOverlayOpen() && mod->showSettings)
                                            {
                                                //*(bool*)setting->getValue() = !*(bool*)setting->getValue();
                                                boolSetting->mValue = !boolSetting->mValue;
                                            }

                                            if (ImGui::IsMouseClicked(2) && !screenManager.isOverlayOpen() && catPositions[i].isExtended)
                                            {
                                                beginBoolSettingBinding(boolSetting);
                                                ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
                                            }
                                        }

                                        setting->boolScale = MathUtils::animate(
                                            boolSetting->mValue ? 1 : 0, setting->boolScale,
                                            ImRenderUtils::getDeltaTime() * 10);

                                        float scaledWidth = rect.getWidth();
                                        float scaledHeight = rect.getHeight();

                                        ImVec2 center = ImVec2(rect.x + rect.getWidth() / 2.f, rect.y + rect.getHeight() / 2.f);
                                        ImVec4 scaledRect = ImVec4(center.x - scaledWidth / 2.f, center.y - scaledHeight / 2.f, center.x + scaledWidth / 2.f, center.y + scaledHeight / 2.f);

                                        float cSetRectCentreX = rect.x + ((rect.z - rect.x) - ImRenderUtils::getTextWidth(&setName, textSize)) / 2;
                                        float cSetRectCentreY = rect.y + ((rect.w - rect.y) - textHeight) / 2;

                                        ImVec4 smoothScaledRect = ImVec4(scaledRect.z - 19, scaledRect.y + 5, scaledRect.z - 5, scaledRect.w - 5);//
                                        ImVec2 circleRect = ImVec2(smoothScaledRect.getCenter().x, smoothScaledRect.getCenter().y);

                                        // Lerp shadow color using boolScale
                                        ImColor targetShadowCol = ImColor(15, 15, 15);
                                        ImColor shadowCol = MathUtils::lerpImColor(targetShadowCol, rgb, setting->boolScale);

                                        ImRenderUtils::fillShadowCircle(circleRect, 5, shadowCol, animation * mod->cAnim, 40, 0);

                                        ImVec4 booleanRect = ImVec4(rect.z - 23.5f, cSetRectCentreY - 2.5f, rect.z - 5, cSetRectCentreY + 17.5f);
                                        booleanRect = booleanRect.scaleToPoint(ImVec4(rect.z, rect.y, rect.z, rect.w), animation);

                                        float rectXDiff = booleanRect.z - booleanRect.x;

                                        if (setting->boolScale > 0.01) {
                                            // Make the min y of the boolean rect the top of the setting rect
                                            if (booleanRect.y < modRect.w) {
                                                booleanRect.y = modRect.w;
                                            }
                                            ImGui::GetForegroundDrawList()->PushClipRect(ImVec2(booleanRect.x, booleanRect.y), ImVec2(booleanRect.x + rectXDiff * setting->boolScale, booleanRect.w), true);

                                            ImRenderUtils::drawCheckMark(ImVec2(booleanRect.getCenter().x - (4 * animation),
                                                                                booleanRect.getCenter().y - (1 * animation)), 1.3 * animation,
                                                                         rgb, mod->cAnim * animation);
                                            ImRenderUtils::drawCheckMark(ImVec2(booleanRect.getCenter().x - (4 * animation),
                                                                                booleanRect.getCenter().y - (1 * animation)), 1.3 * animation,
                                                                         rgb, mod->cAnim * animation);

                                            ImGui::GetForegroundDrawList()->PopClipRect();
                                        }

                                        ImRenderUtils::drawText(ImVec2(rect.x + 5.f, cSetRectCentreY), setName,
                                                               ImColor(255, 255, 255), textSize, animation, true);
                                    }
                                    break;
                                }
                            case SettingType::Enum:
                                {
                                    EnumSetting* enumSetting = reinterpret_cast<EnumSetting*>(setting);
                                    GuiScreen::Settings::renderEnumSetting(setting, enumSetting, settingContext);
                                    break;
                                    std::string setName = lowercase ? StringUtils::toLower(setting->mName) : setting->mName;
                                    std::vector<std::string> enumValues = enumSetting->mValues;
                                    if (lowercase)
                                    {
                                        for (std::string& value : enumValues)
                                        {
                                            value = StringUtils::toLower(value);
                                        }
                                    }
                                    int* iterator = &enumSetting->mValue;
                                    int numValues = static_cast<int>(enumValues.size());

                                    moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, mod->cAnim);

                                    ImVec4 rect = ImVec4(
                                            modRect.x, catPositions[i].y + catHeight + moduleY + setPadding, modRect.z,
                                            catPositions[i].y + catHeight + moduleY + modHeight)
                                        .scaleToPoint(
                                            ImVec4(modRect.x, screen.y / 2,
                                                           modRect.z, screen.y / 2),
                                            inScale);
                                    rect.y = std::floor(rect.y);
                                    if (rect.y < modRect.y)
                                    {
                                        rect.y = modRect.y;
                                    }

                                    float targetAnim = setting->enumExtended && mod->showSettings ? 1.f : 0.f;
                                    setting->enumSlide = MathUtils::animate(
                                        targetAnim, setting->enumSlide, ImRenderUtils::getDeltaTime() * 10);
                                    setting->enumSlide = MathUtils::clamp(setting->enumSlide, 0.f, 1.f);

                                    if (setting->enumSlide > 0.001)
                                    {
                                        for (int j = 0; j < numValues; j++)
                                        {
                                            std::string enumValue = enumValues[j];

                                            moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, setting->enumSlide);

                                            ImVec4 rect2 = ImVec4(
                                                    modRect.x, catPositions[i].y + catHeight + moduleY + setPadding, modRect.z,
                                                    catPositions[i].y + catHeight + moduleY + modHeight)
                                                .scaleToPoint(
                                                    ImVec4(modRect.x, screen.y / 2,
                                                                   modRect.z, screen.y / 2),
                                                    inScale);

                                            if (rect2.y > catRect.y + 0.5f)
                                            {
                                                float cSetRectCentreY = rect2.y + ((rect2.w - rect2.y) - textHeight)
                                                    / 2;

                                                ImRenderUtils::fillRectangle(rect2, ImColor(20, 20, 20), animation, radius, ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

                                                if (*iterator == j)
                                                    ImRenderUtils::fillRectangle(
                                                        ImVec4(rect2.x, rect2.y, rect2.x + 1.5f, rect2.w),
                                                        rgb, animation);

                                                if (ImRenderUtils::isMouseOver(rect2) && ImGui::IsMouseClicked(0) &&
                                                    isEnabled && !screenManager.isOverlayOpen() && mod->showSettings)
                                                {
                                                    *iterator = j;
                                                }

                                                ImRenderUtils::drawText(
                                                    ImVec2(rect2.x + 5.f, cSetRectCentreY), enumValue,
                                                    ImColor(255, 255, 255), textSize, animation, true);
                                            }
                                        }
                                    }

                                    if (rect.y > catRect.y + 0.5f)
                                    {
                                        ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), animation, radius, ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

                                        if (ImRenderUtils::isMouseOver(rect) && isEnabled && catPositions[i].isExtended)
                                        {
                                            tooltip = setting->mDescription;
                                            if (ImGui::IsMouseClicked(0) && !screenManager.isOverlayOpen() && mod->showSettings)
                                            {
                                                *iterator = (*iterator + 1) % enumValues.size();
                                            }
                                            else if (ImGui::IsMouseClicked(1) && mod->showSettings && !screenManager.isOverlayOpen() && mod->showSettings)
                                            {
                                                setting->enumExtended = !setting->enumExtended;
                                            }
                                        }

                                        float cSetRectCentreY = rect.y + ((rect.w - rect.y) - textHeight) / 2;

                                        std::string enumValue = enumValues[*iterator];
                                        std::string settingName = setName;
                                        std::string settingString = enumValue;
                                        auto ValueLen = ImRenderUtils::getTextWidth(&settingString, textSize);

                                        ImRenderUtils::drawText(ImVec2(rect.x + 5.f, cSetRectCentreY),
                                                               settingName, ImColor(255, 255, 255), textSize,
                                                               animation, true);
                                        ImRenderUtils::drawText(
                                            ImVec2((rect.z - 5.f) - ValueLen, cSetRectCentreY),
                                            settingString, ImColor(170, 170, 170), textSize, animation, true);
                                    }
                                    if (rect.y > catRect.y - modHeight)
                                    {
                                        ImRenderUtils::fillGradientOpaqueRectangle(
                                            ImVec4(rect.x, rect.w, rect.z,
                                                           rect.w + 10.f * setting->enumSlide * animation),
                                            ImColor(0, 0, 0), ImColor(0, 0, 0), 0.F * animation, 0.55F * animation);
                                    }
                                    break;
                                }
                            case SettingType::Number:
                                {
                                    NumberSetting* numSetting = reinterpret_cast<NumberSetting*>(setting);
                                    GuiScreen::Settings::renderNumberSetting(setting, numSetting, settingContext);
                                    break;
                                    const float value = numSetting->mValue;
                                    const float min = numSetting->mMin;
                                    const float max = numSetting->mMax;

                                    char str[10];
                                    sprintf_s(str, 10, "%.2f", value);
                                    std::string rVal = str;

                                    std::string setName = lowercase ? StringUtils::toLower(setting->mName) : setting->mName;
                                    std::string valueName = rVal;

                                    moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, mod->cAnim);

                                    ImVec4 backGroundRect = ImVec4(
                                            modRect.x, (catPositions[i].y + catHeight + moduleY), modRect.z,
                                            catPositions[i].y + catHeight + moduleY + modHeight)
                                        .scaleToPoint(
                                            ImVec4(modRect.x, screen.y / 2,
                                                           modRect.z, screen.y / 2),
                                            inScale);

                                    backGroundRect.y = std::floor(backGroundRect.y);
                                    if (backGroundRect.y < modRect.y)
                                    {
                                        backGroundRect.y = modRect.y;
                                    }

                                    ImVec4 rect = ImVec4(
                                            modRect.x + 7, (catPositions[i].y + catHeight + moduleY + setPadding), modRect.z - 7,
                                            catPositions[i].y + catHeight + moduleY + modHeight)
                                        .scaleToPoint(
                                            ImVec4(modRect.x, screen.y / 2,
                                                           modRect.z, screen.y / 2),
                                            inScale);
                                    rect.y = std::floor(rect.y);
                                    if (rect.y < modRect.y)
                                    {
                                        rect.y = modRect.y;
                                    }

                                    static float clickAnimation = 1.f;

                                    // If left click is down, lerp the alpha to 0.60f;
                                    if (ImGui::IsMouseDown(0) && ImRenderUtils::isMouseOver(rect))
                                    {
                                        clickAnimation = MathUtils::animate(0.60f, clickAnimation, ImRenderUtils::getDeltaTime() * 10);
                                    }
                                    else
                                    {
                                        clickAnimation = MathUtils::animate(1.f, clickAnimation, ImRenderUtils::getDeltaTime() * 10);
                                    }

                                    if (backGroundRect.y > catRect.y + 0.5f)
                                    {
                                        ImRenderUtils::fillRectangle(backGroundRect, ImColor(30, 30, 30), animation, radius, ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

                                        const float sliderPos = (value - min) / (max - min) * (rect.z - rect.x);

                                        setting->sliderEase = MathUtils::animate(
                                            sliderPos, setting->sliderEase, ImRenderUtils::getDeltaTime() * 10);
                                        setting->sliderEase = std::clamp(setting->sliderEase, 0.f, rect.getWidth());

#pragma region Slider dragging
                                       if (ImRenderUtils::isMouseOver(rect) && isEnabled && catPositions[i].isExtended)
                                        {
                                            tooltip = setting->mDescription;
                                            if (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(2))
                                            {
                                                setting->isDragging = true;
                                                lastDraggedSetting = setting;
                                            }
                                        }

                                        if (ImGui::IsMouseDown(0) && setting->isDragging && isEnabled)
                                        {
                                            if (lastDraggedSetting != setting)
                                            {
                                                setting->isDragging = false;
                                            } else
                                            {
                                                const float newValue = std::fmax(
                                                    std::fmin(
                                                        (ImRenderUtils::getMousePos().x - rect.x) / (rect.z - rect.x) * (
                                                            max - min) + min, max), min);
                                                numSetting->setValue(newValue);
                                            }
                                        }
                                        else if (ImGui::IsMouseDown(2) && setting->isDragging && isEnabled)
                                        {
                                            if (lastDraggedSetting != setting)
                                            {
                                                setting->isDragging = false;
                                            } else
                                            {
                                                float newValue = std::fmax(
                                                    std::fmin(
                                                        (ImRenderUtils::getMousePos().x - rect.x) / (rect.z - rect.x) * (
                                                            max - min) + min, max), min);
                                                // Round the value to the nearest value specified by midclickRounding
                                                newValue = std::round(newValue / midclickRounding) * midclickRounding;
                                                numSetting->mValue = newValue;
                                            }
                                        }
                                        else
                                        {
                                            setting->isDragging = false;
                                        }
#pragma endregion

                                        // "doesn't animate down when animating out" cuz its made by a 11 yr old smart nerd :yum:
                                        /* Original code (doesn't animate down when animating out)
                                        ImRenderUtils::fillRectangle(ImVec4(rect.x, (catPositions[i].y + catHeight + moduleY + modHeight) - 3, rect.x + setting->sliderEase, rect.w), rgb, animation);
                                        ImRenderUtils::fillShadowRectangle(ImVec4(rect.x, (catPositions[i].y + catHeight + moduleY + modHeight) - 3, rect.x + setting->sliderEase, rect.w), rgb, animation, 50.f, 0);*/

                                        float ySize = rect.w - rect.y;

                                        ImVec2 sliderBarMin = ImVec2(rect.x, rect.w - ySize / 8);
                                        ImVec2 sliderBarMax = ImVec2(rect.x + (setting->sliderEase * inScale), rect.w);
                                        sliderBarMin.y = sliderBarMax.y - 4 * inScale;

                                        ImVec4 sliderRect = ImVec4(sliderBarMin.x, sliderBarMin.y - 4.5f, sliderBarMax.x, sliderBarMax.y - 6.5f);

                                        // The slider bar
                                        ImRenderUtils::fillRectangle(sliderRect, rgb, animation, 15);

                                        // Circle (I am not sure)
                                        ImVec2 circlePos = ImVec2(sliderRect.z - 2.25f, sliderRect.getCenter().y);

                                        if (value <= min + 0.83f)
                                        {
                                            circlePos.x = sliderRect.z + 2.25f;
                                        }


                                        ImRenderUtils::fillCircle(circlePos, 5.5f * clickAnimation * animation, rgb, animation, 12);

                                        // Push a clip rect to prevent the shadow from going outside the slider bar
                                        ImGui::GetBackgroundDrawList()->PushClipRect(ImVec2(sliderRect.x, sliderRect.y), ImVec2(sliderRect.z, sliderRect.w), true);

                                        ImRenderUtils::fillShadowRectangle(sliderRect, rgb, animation * 0.75f, 15.f, 0);

                                        ImGui::GetBackgroundDrawList()->PopClipRect();

                                        auto ValueLen = ImRenderUtils::getTextWidth(&valueName, textSize);
                                        ImRenderUtils::drawText(
                                            ImVec2((backGroundRect.z - 5.f) - ValueLen, backGroundRect.y + 2.5f), valueName,
                                            ImColor(170, 170, 170), textSize, animation, true);
                                        ImRenderUtils::drawText(ImVec2(backGroundRect.x + 5.f, backGroundRect.y + 2.5f),
                                                               setName, ImColor(255, 255, 255), textSize,
                                                               animation, true);
                                    }
                                    break;
                                }
                            case SettingType::Color:
                                {
                                    ColorSetting* colorSetting = reinterpret_cast<ColorSetting*>(setting);
                                    GuiScreen::Settings::renderColorSetting(setting, colorSetting, settingContext);
                                    break;
                                    ImColor color = colorSetting->getAsImColor();
                                    ImVec4 rgb = color.Value;
                                    std::string setName = lowercase ? StringUtils::toLower(setting->mName) : setting->mName;

                                    moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, mod->cAnim);

                                    ImVec4 rect = ImVec4(
                                            modRect.x, catPositions[i].y + catHeight + moduleY + setPadding, modRect.z,
                                            catPositions[i].y + catHeight + moduleY + modHeight)
                                        .scaleToPoint(
                                            ImVec4(modRect.x, screen.y / 2,
                                                           modRect.z, screen.y / 2),
                                            inScale);
                                    rect.y = std::floor(rect.y);
                                    if (rect.y < modRect.y)
                                    {
                                        rect.y = modRect.y;
                                    }

                                    if (rect.y > catRect.y + 0.5f)
                                    {
                                        ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), animation);

                                        if (ImRenderUtils::isMouseOver(rect) && isEnabled && catPositions[i].isExtended)
                                        {
                                            tooltip = setting->mDescription;
                                            if (ImGui::IsMouseClicked(0) && !screenManager.isOverlayOpen() && mod->showSettings)
                                            {
                                                displayColorPicker = !displayColorPicker;
                                                lastColorSetting = colorSetting;
                                            }
                                        }

                                        float cSetRectCentreY = rect.y + ((rect.w - rect.y) - textHeight) / 2;
                                        ImRenderUtils::drawText(ImVec2(rect.x + 5.f, cSetRectCentreY), setName,
                                                               ImColor(255, 255, 255), textSize, animation, true);

                                        ImVec2 colorRect = ImVec2(rect.z - 20, rect.y + 5);
                                        ImRenderUtils::fillRectangle(ImVec4(rect.z - 20, rect.y + 5, rect.z - 5, rect.w - 5),
                                                                     colorSetting->getAsImColor(), animation);
                                    }
                                    break;
                                }
                            case SettingType::List:
                                {
                                    ListSetting* listSetting = reinterpret_cast<ListSetting*>(setting);
                                    GuiScreen::Settings::renderListSetting(setting, listSetting, settingContext);
                                    break;
                                    std::string setName = lowercase ? StringUtils::toLower(setting->mName) : setting->mName;

                                    moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, mod->cAnim);

                                    ImVec4 rect = ImVec4(
                                            modRect.x, catPositions[i].y + catHeight + moduleY + setPadding, modRect.z,
                                            catPositions[i].y + catHeight + moduleY + modHeight)
                                        .scaleToPoint(
                                            ImVec4(modRect.x, screen.y / 2,
                                                           modRect.z, screen.y / 2),
                                            inScale);
                                    rect.y = std::floor(rect.y);
                                    if (rect.y < modRect.y)
                                    {
                                        rect.y = modRect.y;
                                    }

                                    if (rect.y > catRect.y + 0.5f)
                                    {
                                        ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), animation);

                                        const bool hovered = ImRenderUtils::isMouseOver(rect) && isEnabled && catPositions[i].isExtended;
                                        if (hovered)
                                        {
                                            tooltip = setting->mDescription;
                                        }

                                        const float cSetRectCentreY = rect.y + ((rect.w - rect.y) - textHeight) / 2;
                                        ImRenderUtils::drawText(ImVec2(rect.x + 5.f, cSetRectCentreY), setName,
                                                               ImColor(255, 255, 255), textSize, animation, true);

                                        std::string selectedText = std::to_string(listSetting->mSelectedValues.size()) + "/" +
                                            std::to_string(listSetting->mOptions.size());
                                        const float selectedTextWidth = ImRenderUtils::getTextWidth(&selectedText, textSize);
                                        ImRenderUtils::drawText(
                                            ImVec2(rect.z - 72.f - selectedTextWidth, cSetRectCentreY),
                                            selectedText,
                                            ImColor(170, 170, 170),
                                            textSize,
                                            animation,
                                            true
                                        );

                                        const ImVec4 chooseButtonRect(
                                            rect.z - 64.f,
                                            rect.y + 4.f,
                                            rect.z - 6.f,
                                            rect.w - 4.f
                                        );
                                        ImRenderUtils::fillRectangle(chooseButtonRect, ImColor(22, 22, 22), animation, 4.f);
                                        std::string chooseText = lowercase ? "choose" : "Choose";
                                        const float chooseTextWidth = ImRenderUtils::getTextWidth(&chooseText, textSize * 0.9f);
                                        const float chooseTextY = chooseButtonRect.y + ((chooseButtonRect.w - chooseButtonRect.y) - textHeight) * 0.5f;
                                        ImRenderUtils::drawText(
                                            ImVec2(chooseButtonRect.x + ((chooseButtonRect.z - chooseButtonRect.x) - chooseTextWidth) * 0.5f, chooseTextY),
                                            chooseText,
                                            ImColor(255, 255, 255),
                                            textSize * 0.9f,
                                            animation,
                                            true
                                        );

                                        const bool chooseHovered = hovered && ImRenderUtils::isMouseOver(chooseButtonRect);
                                        if ((hovered || chooseHovered) && ImGui::IsMouseClicked(0) && !screenManager.isOverlayOpen() && mod->showSettings)
                                        {
                                            lastListSetting = listSetting;
                                            displayListChooser = true;
                                            mListSearchBuffer[0] = '\0';
                                            mActiveAvailableOption.clear();
                                            mActiveSelectedOption.clear();
                                        }
                                        else if (hovered && ImGui::IsMouseClicked(1) && !screenManager.isOverlayOpen() && mod->showSettings)
                                        {
                                            listSetting->clearSelection();
                                        }
                                    }
                                    break;
                                }
                            case SettingType::Button:
                                {
                                    ButtonSetting* buttonSetting = reinterpret_cast<ButtonSetting*>(setting);
                                    GuiScreen::Settings::renderButtonSetting(setting, buttonSetting, settingContext);
                                    break;
                                }
                            }

                            sIndex++;
                        }

                    }


                    if (modRect.y > catRect.y + 0.5f)
                    {
                        // Draw the rect
                        if (mod->cScale <= 1)
                        {
                            if (mod->mEnabled)
                                ImRenderUtils::fillRectangle(modRect, rgb, animation, radius, ImGui::GetBackgroundDrawList(), ImDrawCornerFlags_BotRight | ImDrawCornerFlags_BotLeft);
                            else
                                ImRenderUtils::fillRectangle(modRect, ImColor(30, 30, 30), animation, radius, ImGui::GetBackgroundDrawList(), ImDrawCornerFlags_BotRight | ImDrawCornerFlags_BotLeft);
                            ImRenderUtils::fillRectangle(modRect, grayColor, animation, radius, ImGui::GetBackgroundDrawList(), ImDrawCornerFlags_BotRight | ImDrawCornerFlags_BotLeft);
                        }

                        std::string modName = mod->getName();

                        // Calculate the centre of the rect
                        ImVec2 center = ImVec2(modRect.x + modRect.getWidth() / 2.f,
                                                               modRect.y + modRect.getHeight() / 2.f);

                        mod->cScale = MathUtils::animate(mod->mEnabled ? 1 : 0, mod->cScale,
                                                    ImRenderUtils::getDeltaTime() * 10);

                        // Calculate scaled size based on cScale
                        float scaledWidth = modRect.getWidth();
                        float scaledHeight = modRect.getHeight();

                        // Calculate new rectangle based on scaled size and center point
                        ImVec4 scaledRect = ImVec4(center.x - scaledWidth / 2.f,
                                                                   center.y - scaledHeight / 2.f,
                                                                   center.x + scaledWidth / 2.f,
                                                                   center.y + scaledHeight / 2.f);

                        // Interpolate between original rectangle and scaled rectangle
                        if (mod->cScale > 0)
                        {

                            //ImRenderUtils::fillRectangle(scaledRect, rgb, animation * mod->cScale + 0.01f);
                            ImColor rgb1 = rgb;
                            float modIndexY = moduleY + (scaledRect.y - scaledRect.w);

                            ImColor rgb2 = ColorUtils::getThemedColor(scaledRect.y + ((scaledRect.z - scaledRect.x)));
                            //ImRenderUtils::fillGradientOpaqueRectangle(scaledRect, rgb1, rgb2, animation * mod->cScale, animation * mod->cScale);
                            // Round only if we are rendering the last module and the settings aren't expanded
                            //ImRenderUtils::fillRectangle(scaledRect, rgb1, animation * mod->cScale, radius, ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);
                            ImRenderUtils::fillRoundedGradientRectangle(scaledRect, rgb1, rgb2, radius, animation * mod->cScale, animation * mod->cScale, flags);
                        }

                        float cRectCentreX = modRect.x + ((modRect.z - modRect.x) - ImRenderUtils::getTextWidth(
                            &modName, textSize)) / 2;
                        float cRectCentreY = modRect.y + ((modRect.w - modRect.y) - textHeight) / 2;

                        // cRectCentreX. vRectCentreY
                        //.lerp(ImVec2(modRect.x + 5, cRectCentreY), mod->cAnim) // if we want lerp to left on extend
                        ImVec2 modPosLerped = ImVec2(cRectCentreX, cRectCentreY);

                        ImRenderUtils::drawText(modPosLerped, modName,
                                               ImColor(mod->mEnabled
                                                           ? ImColor(255, 255, 255)
                                                           : ImColor(180, 180, 180)).Lerp(
                                                           mod->mEnabled
                                                           ? ImColor(255, 255, 255)
                                                           : ImColor(180, 180, 180), mod->cAnim), textSize, animation, true);

                        if (ImRenderUtils::isMouseOver(modRect) && catPositions[i].isExtended && isEnabled && catPositions[i].isExtended)
                        {
                            if (ImRenderUtils::isMouseOver(catWindow) && catPositions[i].isExtended && catPositions[i].isExtended)
                            {
                                tooltip = mod->mDescription;
                            }

                            if (ImGui::IsMouseClicked(0) && !screenManager.isOverlayOpen() && catPositions[i].isExtended)
                            {
                                if (!moduleToggled) mod->toggle();
                                ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
                                moduleToggled = true;
                            }
                            else if (ImGui::IsMouseClicked(1) && !screenManager.isOverlayOpen() && catPositions[i].isExtended)
                            {
                                mod->showSettings = !mod->showSettings;
                            }
                            else if (ImGui::IsMouseClicked(2)  && !screenManager.isOverlayOpen() && catPositions[i].isExtended)
                            {
                                beginModuleBinding(mod);
                                ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
                            }
                        }
                    }
                    if (modRect.y > catRect.y - modHeight)
                    {
                        // Render a slight glow effect
                        ImRenderUtils::fillGradientOpaqueRectangle(
                            ImVec4(modRect.x, modRect.w, modRect.z,
                                           modRect.w + 10.f * mod->cAnim * animation), ImColor(0, 0, 0),
                            ImColor(0, 0, 0), 0.F * animation, 0.55F * animation);
                    }
                    moduleY += modHeight;

                    modIndex++;
                }
            }

            if (visibleModsInCategory.empty() && catPositions[i].isExtended)
            {
                std::string emptyText = hasSearchQuery()
                    ? (lowercase ? "no matching modules" : "No matching modules")
                    : (lowercase ? "no modules" : "No modules");
                auto textWidth = ImRenderUtils::getTextWidth(&emptyText, textSize);
                const float textX = catRect.x + ((catRect.z - catRect.x) - textWidth) * 0.5f;
                const float textY = catRect.w + ((modHeight - textHeight) * 0.5f);
                ImRenderUtils::drawText(ImVec2(textX, textY), emptyText, ImColor(170, 170, 170), textSize, animation, true);
            }
            drawList->PopClipRect();

            if (isBinding && lastMod)
            {
                tooltip = "Currently binding " + lastMod->getName() + "... Press ESC to clear.";
            }

            if (isBoolSettingBinding && lastBoolSetting)
            {
                tooltip = "Currently binding " + lastBoolSetting->mName + "... Press ESC to clear.";
            }

            std::string catName = lowercase ? StringUtils::toLower(categories[i]) : categories[i];

            if (ImRenderUtils::isMouseOver(catRect) && ImGui::IsMouseClicked(1) && !screenManager.isOverlayOpen())
                catPositions[i].isExtended = !catPositions[i].isExtended;

            catRect.w += 1.5f;
            ImRenderUtils::fillRectangle(catRect, darkBlack, animation, 15, ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersTop);

            ImVec4 lineRect = ImVec4(catRect.x, catRect.w - 0.75f, catRect.z, catRect.w + 0.75f);

            //ImRenderUtils::fillGradientOpaqueRectangle(lineRect, rgb, ColorUtils::getThemedColor(catRect.y + ((catRect.z - catRect.x))), animation, animation);



            FontHelper::pushPrefFont(true, true, true);
            // Calculate the centre of the rect
            float textHeight = ImGui::GetFont()->CalcTextSizeA(textSize * 18, FLT_MAX, -1, catName.c_str()).y;
            float cRectCentreX = catRect.x + ((catRect.z - catRect.x) - ImRenderUtils::getTextWidth(
                &catName, textSize * 1.15)) / 2;
            float cRectCentreY = catRect.y + ((catRect.w - catRect.y) - textHeight) / 2;

            std::string IconStr = "B";
            // TODO: please for the love of god make icon fkery like this into FontHelper.......
            // (also don't forget to check for case u idiot!!!!!!!111!!!!1)
            if (StringUtils::equalsIgnoreCase(catName, "Combat")) IconStr = "c";
            else if (StringUtils::equalsIgnoreCase(catName, "Movement")) IconStr = "f";
            else if (StringUtils::equalsIgnoreCase(catName, "Visual")) IconStr = "d";
            else if (StringUtils::equalsIgnoreCase(catName, "Player")) IconStr = "e";
            else if (StringUtils::equalsIgnoreCase(catName, "Misc")) IconStr = "a";


            ImGui::PushFont(FontHelper::Fonts["tenacity_icons_large"]);
            // Draw the icon
            ImRenderUtils::drawText(ImVec2(catRect.x + 10, cRectCentreY), IconStr, ImColor(255, 255, 255),
                                   textSize * 1.15, animation, true);
            ImGui::PopFont();

            // Draw the string
            ImRenderUtils::drawText(ImVec2(cRectCentreX, cRectCentreY), catName, ImColor(255, 255, 255),
                                   textSize * 1.15, animation, true);
            ImGui::PopFont();

            catPositions[i].x = std::clamp(catPositions[i].x, 0.f, screen.x - catWidth);
            catPositions[i].y = std::clamp(catPositions[i].y, 0.f, screen.y - catHeight);

#pragma region DraggingLogic
            static bool dragging = false;
            static ImVec2 dragOffset;
            if (catPositions[i].isDragging)
            {
                if (ImGui::IsMouseDown(0))
                {
                    if (!dragging)
                    {
                        dragOffset = ImVec2(ImRenderUtils::getMousePos().x - catRect.x,
                                                    ImRenderUtils::getMousePos().y - catRect.y);
                        dragging = true;
                    }
                    ImVec2 newPosition = ImVec2(ImRenderUtils::getMousePos().x - dragOffset.x,
                                                                ImRenderUtils::getMousePos().y - dragOffset.y);
                    newPosition.x = std::clamp(newPosition.x, 0.f,
                                               screen.x - catWidth);
                    newPosition.y = std::clamp(newPosition.y, 0.f,
                                               screen.y - catHeight);
                    // Round the position to an even number
                    newPosition.x = std::round(newPosition.x / 2) * 2;
                    newPosition.y = std::round(newPosition.y / 2) * 2;

                    catPositions[i].x = newPosition.x;
                    catPositions[i].y = newPosition.y;
                }
                else
                {
                    catPositions[i].isDragging = false;
                    dragging = false;
                }
            }
            else if (ImRenderUtils::isMouseOver(catRect) && ImGui::IsMouseClicked(0) && isEnabled && !screenManager.isOverlayOpen())
            {
                catPositions[i].isDragging = true;
                dragOffset = ImVec2(ImRenderUtils::getMousePos().x - catRect.x,
                                            ImRenderUtils::getMousePos().y - catRect.y);
            }
#pragma endregion
        }

        if (!overlayOpen)
        {
            const bool searchWillFocus = mSearchFocused || (ImGui::IsMouseClicked(0) && ImRenderUtils::isMouseOver(ImVec4(searchPos.x, searchPos.y, searchPos.x + searchSize.x, searchPos.y + searchSize.y)));

            ImGui::SetNextWindowPos(searchPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(searchSize, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 6.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(24.f, 4.f));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBgActive, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_TextDisabled, IM_COL32(136, 136, 136, static_cast<int>(255.f * animation)));
            ImGui::PushStyleColor(ImGuiCol_Text, searchWillFocus ? IM_COL32(255, 255, 255, static_cast<int>(255.f * animation)) : IM_COL32(225, 225, 225, static_cast<int>(255.f * animation)));
            ImGui::Begin(
                "##solstice_clickgui_search",
                nullptr,
                ImGuiWindowFlags_NoTitleBar |
                ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse
            );

            auto* searchDrawList = ImGui::GetWindowDrawList();
            const ImVec2 windowMin = ImGui::GetWindowPos();
            const ImVec2 windowMax = ImVec2(windowMin.x + searchSize.x, windowMin.y + searchSize.y);
            searchDrawList->AddRectFilled(windowMin, windowMax, IM_COL32(10, 10, 10, static_cast<int>(191.f * animation)), 6.f);

            ImGui::SetCursorPos(ImVec2(0.f, 0.f));
            ImGui::SetNextItemWidth(searchWidth);
            ImGui::InputTextWithHint("##solstice_clickgui_search_input", "Type to search...", mSearchBuffer, IM_ARRAYSIZE(mSearchBuffer));
            mSearchFocused = ImGui::IsItemActive() || ImGui::IsItemFocused();
            std::ranges::copy(mSearchBuffer, screenManager.searchBuffer());
            screenManager.setSearchFocused(mSearchFocused);
            if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(1))
            {
                clearSearchQuery();
            }

            const bool searchActive = mSearchFocused;
            auto* overlayDrawList = ImGui::GetForegroundDrawList();
            const ImVec2 boxMin = windowMin;
            const ImVec2 boxMax = windowMax;

            const int borderAlpha = searchActive ? static_cast<int>(255.f * animation) : static_cast<int>(77.f * animation);
            overlayDrawList->AddRect(boxMin, boxMax, IM_COL32(0, 255, 127, borderAlpha), 6.f, ImDrawFlags_RoundCornersAll, 1.f);

            if (searchActive)
            {
                for (int glow = 1; glow <= 4; ++glow)
                {
                    const int glowAlpha = static_cast<int>((70.f - glow * 12.f) * animation);
                    overlayDrawList->AddRect(
                        ImVec2(boxMin.x - static_cast<float>(glow), boxMin.y - static_cast<float>(glow)),
                        ImVec2(boxMax.x + static_cast<float>(glow), boxMax.y + static_cast<float>(glow)),
                        IM_COL32(0, 255, 127, std::max(0, glowAlpha)),
                        6.f + static_cast<float>(glow),
                        ImDrawFlags_RoundCornersAll,
                        1.f
                    );
                }
            }

            const ImVec2 iconCenter = ImVec2(boxMin.x + 11.f, (boxMin.y + boxMax.y) * 0.5f - 1.f);
            overlayDrawList->AddCircle(iconCenter, 4.f, IM_COL32(0, 255, 127, static_cast<int>(255.f * animation)), 16, 1.25f);
            overlayDrawList->AddLine(
                ImVec2(iconCenter.x + 3.0f, iconCenter.y + 3.0f),
                ImVec2(iconCenter.x + 7.0f, iconCenter.y + 7.0f),
                IM_COL32(0, 255, 127, static_cast<int>(255.f * animation)),
                1.25f
            );

            ImGui::End();
            ImGui::PopStyleColor(7);
            ImGui::PopStyleVar(6);
        }
        else
        {
            mSearchFocused = false;
            screenManager.setSearchFocused(false);
        }

        GuiScreen::Screens::renderColorPickerScreen(screenManager, screenContext);
        GuiScreen::Screens::renderListChooserScreen(screenManager, screenContext);
        displayColorPicker = screenManager.isColorPickerOpen();
        displayListChooser = screenManager.isListChooserOpen();
        lastColorSetting = screenManager.activeColorSetting();
        lastListSetting = screenManager.activeListSetting();
        GuiScreen::Screens::renderProgressOverlayScreen(screenManager, screenContext);

        if (!screenManager.isOverlayOpen() && !tooltip.empty())
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
    ImGui::PopFont();
}

void ModernGui::onWindowResizeEvent(WindowResizeEvent& event)
{
    resetPosition = true;
    lastReset = NOW;
}
