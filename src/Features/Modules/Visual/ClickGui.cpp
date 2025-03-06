//
// ClickGui.cpp (enhanced & sharpened style)
//

#include "ClickGui.hpp"
#include <Features/Events/MouseEvent.hpp>
#include <Features/Events/KeyEvent.hpp>
#include <Features/GUI/ModernDropdown.hpp>
#include <Features/GUI/ScriptingGui.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>
#include <Utils/FontHelper.hpp>

static bool lastMouseState = false;
static bool isPressingShift = false;
static ModernGui modernGui = ModernGui(); // Our fancy dropdown-based GUI

void ClickGui::onEnable()
{
    auto ci = ClientInstance::get();
    lastMouseState = !ci->getMouseGrabbed(); // remember old mouse state
    ci->releaseMouse();

    // Start listening for input events (so we can consume them)
    gFeatureManager->mDispatcher->listen<MouseEvent, &ClickGui::onMouseEvent>(this);
    gFeatureManager->mDispatcher->listen<KeyEvent, &ClickGui::onKeyEvent, nes::event_priority::FIRST>(this);
}

void ClickGui::onDisable()
{
    // Stop listening for input events
    gFeatureManager->mDispatcher->deafen<MouseEvent, &ClickGui::onMouseEvent>(this);
    gFeatureManager->mDispatcher->deafen<KeyEvent, &ClickGui::onKeyEvent>(this);

    // Restore mouse if it was grabbed before
    if (lastMouseState) {
        ClientInstance::get()->grabMouse();
    }
}

void ClickGui::onWindowResizeEvent(WindowResizeEvent& event)
{
    // Let the ModernGui handle a window resize gracefully
    modernGui.onWindowResizeEvent(event);
}

void ClickGui::onMouseEvent(MouseEvent& event)
{
    // Block the default game from handling any mouse events
    event.mCancelled = true;
}

void ClickGui::onKeyEvent(KeyEvent& event)
{
    // If they press ESC, close the GUI (unless binding a module key)
    if (event.mKey == VK_ESCAPE) {
        if (!modernGui.isBinding && event.mPressed) this->toggle();
        event.mCancelled = true;
    }

    // Also block other keys from toggling modules
    if (modernGui.isBinding) {
        event.mCancelled = true;
        return;
    }

    // Track SHIFT
    if (event.mKey == VK_SHIFT && event.mPressed) {
        isPressingShift = true;
        event.mCancelled = true;
    }
    else {
        isPressingShift = false;
    }
}

/**
 * @brief A small utility to get the ease factor from a given EasingUtil state + mode
 */
float ClickGui::getEaseAnim(EasingUtil ease, int mode) {
    switch (mode) {
    case 0: // e.g. "Zoom" style
        return ease.easeOutExpo();
    case 1: // e.g. "Bounce" style
        return mEnabled ? ease.easeOutElastic() : ease.easeOutBack();
    default:
        return ease.easeOutExpo();
    }
}

enum class Tab
{
    ClickGui,
    HudEditor,
    Scripting
};

void ClickGui::onRenderEvent(RenderEvent& event)
{
    // Keep or release mouse if GUI is active
    if (mEnabled)
        ClientInstance::get()->releaseMouse();

    // Local static variables for animation
    static float animation = 0.0f;
    static int scrollDirection = 0;
    static char h[2] = { 0 };
    static EasingUtil inEase = EasingUtil();

    // We animate "inEase" to open or close the entire GUI
    float delta = ImGui::GetIO().DeltaTime;
    if (mEnabled) {
        inEase.incrementPercentage(delta * mEaseSpeed.mValue / 10);
    }
    else {
        inEase.decrementPercentage(delta * 2.f * mEaseSpeed.mValue / 10);
    }

    float inScale = getEaseAnim(inEase, mAnimation.as<int>());
    if (inEase.isPercentageMax()) {
        // Slight clamp for "Zoom"
        inScale = 0.996f;
    }
    if (mAnimation.mValue == ClickGuiAnimation::Zoom) {
        inScale = MathUtils::clamp(inScale, 0.0f, 0.996f);
    }

    // "animation" can be used for alpha or other effects
    animation = MathUtils::lerp(0.f, 1.f, inEase.easeOutExpo());
    if (animation < 0.0001f) {
        return; // if basically fully closed, skip
    }

    // Determine scroll direction from the mouse wheel
    float mouseWheel = ImGui::GetIO().MouseWheel;
    if (mouseWheel > 0) scrollDirection = -1;
    else if (mouseWheel < 0) scrollDirection = 1;
    else scrollDirection = 0;

    // We'll show different tabs, but you only have "Modern" currently
    static Tab currentTab = Tab::ClickGui;

    auto drawList = ImGui::GetForegroundDrawList();
    // Render the main interface based on the current tab
    switch (currentTab)
    {
    case Tab::ClickGui:
    {
        if (mStyle.mValue == ClickGuiStyle::Modern) {
            // Our ModernGui is basically the fancy, sharper GUI
            modernGui.render(animation, inScale, scrollDirection, h, mBlurStrength.mValue, mMidclickRounding.mValue, isPressingShift);
        }
        break;
    }
    case Tab::HudEditor:
    {
        // Example placeholder background for the "HUD Editor"
        ImVec2 screen = ImRenderUtils::getScreenSize();
        drawList->AddRectFilled(ImVec2(0, 0), ImVec2(screen.x, screen.y),
            IM_COL32(0, 0, 0, (int)(255 * animation * 0.38f)));

        ImRenderUtils::addBlur(ImVec4(0.f, 0.f, screen.x, screen.y),
            animation * mBlurStrength.mValue, 0);

        ImColor shadowRectColor = ColorUtils::getThemedColor(0);
        shadowRectColor.Value.w = 0.5f * animation;
        float firstheight = screen.y - screen.y / 3;
        firstheight = MathUtils::lerp(screen.y, firstheight, inScale);

        ImRenderUtils::fillGradientOpaqueRectangle(
            ImVec4(0, firstheight, screen.x, screen.y),
            shadowRectColor, shadowRectColor,
            0.4f * inScale, 0.0f
        );

        // Just draw text in the center
        FontHelper::pushPrefFont(true, false, true);
        float fontSize = 25.f;
        std::string hudText = "HudEditor not implemented yet!";
        float fontX = ImGui::GetFont()->CalcTextSizeA(fontSize, FLT_MAX, 0, hudText.c_str()).x;
        drawList->AddText(ImVec2(screen.x / 2 - (fontX), screen.y / 2),
            IM_COL32(255, 0, 0, 255),
            hudText.c_str());
        FontHelper::popPrefFont();

        break;
    }
    case Tab::Scripting:
    {
        // Your Scripting GUI code
        ScriptingGui::render(inScale, animation, ImRenderUtils::getScreenSize(), mBlurStrength.mValue);
        break;
    }
    }

    // Below is the "tab bar" at top center
    FontHelper::pushPrefFont(false, false, true);

    // List of possible tabs
    std::vector<std::pair<Tab, std::string>> tabs = {
        {Tab::ClickGui, "ClickGui"},
        {Tab::HudEditor, "HudEditor"},
        {Tab::Scripting, "Scripting"}
    };

    // We do some fancy spacing & underline animation
    float paddingBetween = 20.f; // space between tabs
    float topFontSize = 25.f;
    static ImVec2 underlinePos = ImVec2(0, 0);
    static ImVec2 underlineSize = ImVec2(0, 0);

    // measure text size for each tab
    std::map<Tab, ImVec2> tabTextSizes;
    for (auto& t : tabs) {
        ImVec2 textSz = ImGui::GetFont()->CalcTextSizeA(topFontSize, FLT_MAX, 0, t.second.c_str());
        tabTextSizes[t.first] = textSz;
    }

    // sum total widths
    auto windowSize = ImGui::GetIO().DisplaySize;
    float totalWidth = 0.f;
    for (auto& t : tabs) {
        totalWidth += tabTextSizes[t.first].x + paddingBetween;
    }
    totalWidth -= paddingBetween; // no trailing padding

    // compute start coords for the tab bar
    float x = (windowSize.x - totalWidth) / 2;
    float startY = -35.f;  // off screen
    float targetY = 10.f;  // actual position
    float y = MathUtils::lerp(startY, targetY, inScale);

    // Render a background behind the tab bar
    // A slight gradient to make it pop
    ImVec4 barRect(x - paddingBetween, y,
        x + totalWidth + paddingBetween, y + tabTextSizes[tabs[0].first].y + 10.f);
    {
        // Top bar background
        // Let's do a small gradient from dark to slightly lighter
        ImColor topBarColor1 = ImColor(40, 40, 40, (int)(180 * animation));
        ImColor topBarColor2 = ImColor(55, 55, 55, (int)(180 * animation));
        ImRenderUtils::fillRoundedGradientRectangle(
            barRect, topBarColor1, topBarColor2,
            5.f,      // radius
            animation,
            animation,
            ImDrawFlags_RoundCornersAll
        );
    }

    std::map<Tab, ImVec2> tabPositions;
    float currentX = x;

    // Render each tab text
    for (auto& t : tabs)
    {
        Tab tabId = t.first;
        std::string tabLabel = t.second;

        ImVec2 textSize = tabTextSizes[tabId];
        ImVec2 textPos(currentX, y);
        // center vertically
        textPos.y += (barRect.w - barRect.y - textSize.y) * 0.5f;

        // This is so the text is horizontally aligned in its own region
        // by default, we just place it at "currentX"
        tabPositions[tabId] = textPos;

        // Check hover
        ImVec4 tabHitbox(textPos.x, y, textPos.x + textSize.x, y + textSize.y + 10.f);
        if (ImRenderUtils::isMouseOver(tabHitbox)) {
            // highlight if hovered
            // If left-click, change tab
            if (ImGui::IsMouseClicked(0)) {
                currentTab = tabId;
            }
        }

        // Actually draw tab text
        // If hovered or active, we can lighten it
        bool isActive = (tabId == currentTab);
        ImColor textCol = isActive
            ? ImColor(255, 255, 255, (int)(255 * animation))
            : ImColor(210, 210, 210, (int)(220 * animation));

        drawList->AddText(textPos, textCol, tabLabel.c_str());

        // Move to next tab position
        currentX += textSize.x + paddingBetween;
    }

    // Animate the underline below the active tab
    auto activeSize = tabTextSizes[currentTab];
    // A bit bigger than the text for a nice effect
    ImVec2 underlineTargetPos(
        tabPositions[currentTab].x,
        tabPositions[currentTab].y + activeSize.y + 3.f
    );
    underlinePos.x = MathUtils::lerp(underlinePos.x, underlineTargetPos.x, delta * 10.f);
    underlinePos.y = MathUtils::lerp(underlinePos.y, underlineTargetPos.y, delta * 10.f);

    float underlineWidth = activeSize.x;
    underlineSize.x = MathUtils::lerp(underlineSize.x, underlineWidth, delta * 10.f);
    underlineSize.y = 2.f; // thickness

    // Let's also add a small glow behind the underline if we like
    {
        ImVec2 glowStart(underlinePos.x, underlinePos.y);
        ImVec2 glowEnd(underlinePos.x + underlineSize.x, underlinePos.y + underlineSize.y);

        // draw the line
        ImColor underlineColor(255, 255, 255, (int)(255 * animation));
        drawList->AddLine(
            glowStart,
            ImVec2(glowEnd.x, glowStart.y),
            underlineColor,
            underlineSize.y
        );

        // Glow effect: subtle highlight around the underline
        // We can do an AddRect or fillRect with alpha
        ImRenderUtils::fillGradientOpaqueRectangle(
            ImVec4(glowStart.x, glowStart.y - 3.f, glowEnd.x, glowStart.y + 3.f),
            underlineColor, underlineColor, 0.4f * animation, 0.4f * animation
        );
    }

    FontHelper::popPrefFont();
}
