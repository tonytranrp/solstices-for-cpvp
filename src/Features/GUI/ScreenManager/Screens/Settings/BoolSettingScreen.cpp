#include "BoolSettingScreen.hpp"

#include "SettingRenderContext.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/StringUtils.hpp>

namespace GuiScreen::Settings
{
    void renderBoolSetting(Setting* setting, BoolSetting* boolSetting, SettingRenderContext& context)
    {
        context.moduleY = MathUtils::lerp(context.moduleY, context.moduleY + context.modHeight, context.module->cAnim);

        ImVec4 rect = ImVec4(
            context.modRect.x,
            context.categoryY + context.categoryHeaderHeight + context.moduleY + context.setPadding,
            context.modRect.z,
            context.categoryY + context.categoryHeaderHeight + context.moduleY + context.modHeight)
            .scaleToPoint(ImVec4(context.modRect.x, context.screen.y / 2, context.modRect.z, context.screen.y / 2), context.inScale);
        rect.y = std::floor(rect.y);
        if (rect.y < context.modRect.y)
        {
            rect.y = context.modRect.y;
        }

        if (rect.y <= context.catRect.y + 0.5f)
        {
            return;
        }

        const std::string setName = context.lowercase ? StringUtils::toLower(setting->mName) : setting->mName;
        ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), context.animation);

        const bool hovered = ImRenderUtils::isMouseOver(rect) && context.isEnabled && context.categoryExtended;
        if (hovered)
        {
            context.tooltip = setting->mDescription;
            if (ImGui::IsMouseClicked(0) && !context.manager.isOverlayOpen() && context.module->showSettings)
            {
                boolSetting->mValue = !boolSetting->mValue;
            }

            if (ImGui::IsMouseClicked(2) && !context.manager.isOverlayOpen() && context.categoryExtended)
            {
                context.manager.beginBoolSettingBinding(boolSetting);
                ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
            }
        }

        auto& boolState = context.manager.boolState(setting);
        boolState.scale = MathUtils::animate(
            boolSetting->mValue ? 1.f : 0.f, boolState.scale, ImRenderUtils::getDeltaTime() * 10.f);

        const float cSetRectCentreY = rect.y + ((rect.w - rect.y) - context.textHeight) / 2;
        ImColor targetShadowCol = ImColor(15, 15, 15);
        ImColor themedColor = context.themedColor;
        ImColor shadowCol = MathUtils::lerpImColor(targetShadowCol, themedColor, boolState.scale);

        ImVec4 booleanRect = ImVec4(rect.z - 23.5f, cSetRectCentreY - 2.5f, rect.z - 5.f, cSetRectCentreY + 17.5f);
        booleanRect = booleanRect.scaleToPoint(ImVec4(rect.z, rect.y, rect.z, rect.w), context.animation);

        ImRenderUtils::fillShadowCircle(ImVec2(booleanRect.getCenter().x, booleanRect.getCenter().y), 5, shadowCol, context.animation * context.module->cAnim, 40, 0);

        if (boolState.scale > 0.01f)
        {
            if (booleanRect.y < context.modRect.w)
            {
                booleanRect.y = context.modRect.w;
            }

            const float rectXDiff = booleanRect.z - booleanRect.x;
            ImGui::GetForegroundDrawList()->PushClipRect(
                ImVec2(booleanRect.x, booleanRect.y),
                ImVec2(booleanRect.x + rectXDiff * boolState.scale, booleanRect.w),
                true);

            ImRenderUtils::drawCheckMark(
                ImVec2(booleanRect.getCenter().x - (4 * context.animation), booleanRect.getCenter().y - (1 * context.animation)),
                1.3f * context.animation,
                context.themedColor,
                context.module->cAnim * context.animation);
            ImGui::GetForegroundDrawList()->PopClipRect();
        }

        ImRenderUtils::drawText(
            ImVec2(rect.x + 5.f, cSetRectCentreY),
            setName,
            ImColor(255, 255, 255),
            context.textSize,
            context.animation,
            true);
    }
}
