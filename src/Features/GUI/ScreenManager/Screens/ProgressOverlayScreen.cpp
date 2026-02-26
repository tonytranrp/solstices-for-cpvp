#include "ProgressOverlayScreen.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenContext.hpp>
#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/StringUtils.hpp>

#include <cmath>

namespace GuiScreen::Screens
{
    void renderProgressOverlayScreen(ScreenManager& manager, const ScreenContext& context)
    {
        static float openAnimation = 0.f;

        const bool openRequested = context.isEnabled && manager.isProgressOverlayOpen();
        openAnimation = MathUtils::animate(openRequested ? 1.f : 0.f, openAnimation, ImRenderUtils::getDeltaTime() * 12.f);
        openAnimation = MathUtils::clamp(openAnimation, 0.f, 1.f);

        if (openAnimation <= 0.001f)
        {
            return;
        }

        const auto& overlayState = manager.progressOverlayState();
        auto* drawList = ImGui::GetForegroundDrawList();
        const ImColor accentColor = ImColor(0, 255, 127);

        ImRenderUtils::fillRectangle(
            ImVec4(0.f, 0.f, context.screenSize.x, context.screenSize.y),
            ImColor(0, 0, 0),
            0.40f * openAnimation,
            0.f,
            drawList);

        const float panelWidth = MathUtils::clamp(context.screenSize.x * 0.42f, 380.f, 620.f);
        const float panelHeight = 120.f;
        const ImVec4 panelRect(
            (context.screenSize.x - panelWidth) * 0.5f,
            (context.screenSize.y - panelHeight) * 0.5f + (1.f - openAnimation) * 16.f,
            (context.screenSize.x + panelWidth) * 0.5f,
            (context.screenSize.y + panelHeight) * 0.5f + (1.f - openAnimation) * 16.f);

        ImRenderUtils::fillRectangle(panelRect, ImColor(10, 10, 10), 0.96f * openAnimation, 8.f, drawList);
        ImRenderUtils::drawRoundRect(panelRect, ImDrawFlags_RoundCornersAll, 8.f, accentColor, 0.55f * openAnimation, 1.f);
        ImRenderUtils::fillShadowRectangle(panelRect, accentColor, 0.20f * openAnimation, 26.f, ImDrawFlags_RoundCornersAll, 8.f, drawList);

        std::string title = overlayState.title.empty() ? "Working..." : overlayState.title;
        if (context.lowercase)
        {
            title = StringUtils::toLower(title);
        }
        ImRenderUtils::drawText(
            ImVec2(panelRect.x + 14.f, panelRect.y + 14.f),
            title,
            ImColor(255, 255, 255),
            0.9f,
            openAnimation,
            true,
            0,
            drawList);

        const float progress = MathUtils::clamp(overlayState.progress, 0.f, 1.f);
        const ImVec4 progressBarRect(panelRect.x + 14.f, panelRect.y + 56.f, panelRect.z - 14.f, panelRect.y + 76.f);
        ImRenderUtils::fillRectangle(progressBarRect, ImColor(24, 24, 24), 1.f * openAnimation, 4.f, drawList);
        ImRenderUtils::drawRoundRect(progressBarRect, ImDrawFlags_RoundCornersAll, 4.f, accentColor, 0.35f * openAnimation, 1.f);

        const float progressWidth = (progressBarRect.z - progressBarRect.x) * progress;
        if (progressWidth > 0.5f)
        {
            const ImVec4 progressFill(progressBarRect.x, progressBarRect.y, progressBarRect.x + progressWidth, progressBarRect.w);
            ImRenderUtils::fillRectangle(progressFill, accentColor, 0.85f * openAnimation, 4.f, drawList);
            ImRenderUtils::fillShadowRectangle(progressFill, accentColor, 0.18f * openAnimation, 16.f, ImDrawFlags_RoundCornersAll, 4.f, drawList);
        }

        std::string status = overlayState.status;
        if (status.empty())
        {
            const int percent = static_cast<int>(std::round(progress * 100.f));
            status = "Progress: " + std::to_string(percent) + "%";
        }
        if (context.lowercase)
        {
            status = StringUtils::toLower(status);
        }

        ImRenderUtils::drawText(
            ImVec2(panelRect.x + 14.f, panelRect.y + 87.f),
            status,
            ImColor(190, 190, 190),
            0.78f,
            openAnimation,
            true,
            0,
            drawList);
    }
}
