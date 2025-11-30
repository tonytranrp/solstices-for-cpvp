#pragma once
//
// Created by AI Assistant on 12/19/2024.
//

#include <Features/Modules/Module.hpp>
#include <Features/FeatureManager.hpp>
#include <Features/Events/RenderEvent.hpp>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

struct VectorPoint {
    float x;
    float y;
};

struct VectorStroke {
    std::string color;
    float size;
    std::vector<VectorPoint> points;
};

struct VectorData {
    struct {
        float width;
        float height;
    } canvasDimensions;
    std::vector<VectorStroke> strokes;
};

class VectorRenderer : public ModuleBase<VectorRenderer> {
public:
    NumberSetting mXOffset = NumberSetting("X Offset", "Horizontal offset of the drawing", 0.f, -1000.f, 1000.f, 1.f);
    NumberSetting mYOffset = NumberSetting("Y Offset", "Vertical offset of the drawing", 0.f, -1000.f, 1000.f, 1.f);
    NumberSetting mScale = NumberSetting("Scale", "Scale of the drawing", 1.f, 0.1f, 5.f, 0.1f);
    BoolSetting mAutoCenter = BoolSetting("Auto Center", "Automatically center the drawing on screen", true);
    BoolSetting mShowBackground = BoolSetting("Show Background", "Show a background behind the drawing", false);
    NumberSetting mBackgroundOpacity = NumberSetting("Background Opacity", "Opacity of the background", 0.1f, 0.f, 1.f, 0.01f);
    BoolSetting mReloadOnEnable = BoolSetting("Reload on Enable", "Reload the vector data when module is enabled", true);

    VectorRenderer() : ModuleBase("VectorRenderer", "Renders vector drawings from JSON data", ModuleCategory::Visual, 0, false) {
        addSettings(
            &mXOffset,
            &mYOffset,
            &mScale,
            &mAutoCenter,
            &mShowBackground,
            &mBackgroundOpacity,
            &mReloadOnEnable
        );

        mNames = {
            {Lowercase, "vectorrenderer"},
            {LowercaseSpaced, "vector renderer"},
            {Normal, "VectorRenderer"},
            {NormalSpaced, "Vector Renderer"}
        };
    }

    VectorData mVectorData;
    bool mDataLoaded = false;
    std::string mLastError = "";
    std::string mFileName = "Vector.json";

    void onEnable() override;
    void onDisable() override;
    void onRenderEvent(class RenderEvent& event);
    
    bool loadVectorData();
    void renderVectorDrawing(ImDrawList* drawList);
    ImVec2 calculateCenterPosition();
    ImColor hexToImColor(const std::string& hexColor);
    std::string getVectorFilePath();
}; 