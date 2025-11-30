//
// Created by AI Assistant on 12/19/2024.
//

#include "VectorRenderer.hpp"

#include <Features/Events/RenderEvent.hpp>
#include <Utils/FileUtils.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <spdlog/spdlog.h>
#include <fstream>
#include <sstream>
#include <sstream>
#include <iomanip>

void VectorRenderer::onEnable()
{
    gFeatureManager->mDispatcher->listen<RenderEvent, &VectorRenderer::onRenderEvent>(this);
    
    if (mReloadOnEnable.mValue) {
        loadVectorData();
    }
}

void VectorRenderer::onDisable()
{
    gFeatureManager->mDispatcher->deafen<RenderEvent, &VectorRenderer::onRenderEvent>(this);
}

std::string VectorRenderer::getVectorFilePath()
{
    return FileUtils::getSolsticeDir() + "Vector\\";
}

bool VectorRenderer::loadVectorData()
{
    try {
        std::string filePath = getVectorFilePath() + mFileName;
        
        // Create directory if it doesn't exist
        std::string dirPath = getVectorFilePath();
        if (!FileUtils::fileExists(dirPath)) {
            FileUtils::createDirectory(dirPath);
        }
        
        if (!FileUtils::fileExists(filePath)) {
            mLastError = "Vector data file not found: " + filePath;
            spdlog::error("[VectorRenderer] {}", mLastError);
            return false;
        }

        std::ifstream file(filePath);
        if (!file.is_open()) {
            mLastError = "Failed to open vector data file";
            spdlog::error("[VectorRenderer] {}", mLastError);
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        file.close();

        nlohmann::json json = nlohmann::json::parse(buffer.str());

        // Parse canvas dimensions
        if (json.contains("canvasDimensions")) {
            auto& canvas = json["canvasDimensions"];
            mVectorData.canvasDimensions.width = canvas["width"];
            mVectorData.canvasDimensions.height = canvas["height"];
        } else {
            mLastError = "Missing canvasDimensions in JSON";
            spdlog::error("[VectorRenderer] {}", mLastError);
            return false;
        }

        // Parse strokes
        mVectorData.strokes.clear();
        if (json.contains("strokes")) {
            for (const auto& strokeJson : json["strokes"]) {
                VectorStroke stroke;
                stroke.color = strokeJson["color"];
                stroke.size = strokeJson["size"];

                for (const auto& pointJson : strokeJson["points"]) {
                    VectorPoint point;
                    point.x = pointJson["x"];
                    point.y = pointJson["y"];
                    stroke.points.push_back(point);
                }

                mVectorData.strokes.push_back(stroke);
            }
        }

        mDataLoaded = true;
        mLastError = "";
        spdlog::info("[VectorRenderer] Successfully loaded vector data with {} strokes", mVectorData.strokes.size());
        return true;

    } catch (const nlohmann::json::exception& e) {
        mLastError = "JSON parsing error: " + std::string(e.what());
        spdlog::error("[VectorRenderer] {}", mLastError);
        return false;
    } catch (const std::exception& e) {
        mLastError = "Error loading vector data: " + std::string(e.what());
        spdlog::error("[VectorRenderer] {}", mLastError);
        return false;
    }
}

void VectorRenderer::onRenderEvent(RenderEvent& event)
{
    if (!mDataLoaded) {
        if (!loadVectorData()) {
            return;
        }
    }

    if (mVectorData.strokes.empty()) {
        return;
    }

    auto drawList = ImGui::GetBackgroundDrawList();
    if (!drawList) {
        return;
    }

    renderVectorDrawing(drawList);
}

void VectorRenderer::renderVectorDrawing(ImDrawList* drawList)
{
    ImVec2 centerPos = calculateCenterPosition();
    float scale = mScale.mValue;

    // Draw background if enabled
    if (mShowBackground.mValue) {
        ImVec2 bgSize = ImVec2(
            mVectorData.canvasDimensions.width * scale,
            mVectorData.canvasDimensions.height * scale
        );
        ImVec2 bgPos = ImVec2(centerPos.x - bgSize.x / 2, centerPos.y - bgSize.y / 2);
        
        drawList->AddRectFilled(
            bgPos,
            ImVec2(bgPos.x + bgSize.x, bgPos.y + bgSize.y),
            ImColor(0.f, 0.f, 0.f, mBackgroundOpacity.mValue),
            5.0f
        );
    }

    // Render each stroke
    for (const auto& stroke : mVectorData.strokes) {
        if (stroke.points.size() < 2) {
            continue; // Skip strokes with less than 2 points
        }

        ImColor strokeColor = hexToImColor(stroke.color);
        float strokeWidth = stroke.size * scale;

        // Convert points to screen coordinates
        std::vector<ImVec2> screenPoints;
        screenPoints.reserve(stroke.points.size());

        for (const auto& point : stroke.points) {
            ImVec2 screenPoint = ImVec2(
                centerPos.x + (point.x - mVectorData.canvasDimensions.width / 2) * scale + mXOffset.mValue,
                centerPos.y + (point.y - mVectorData.canvasDimensions.height / 2) * scale + mYOffset.mValue
            );
            screenPoints.push_back(screenPoint);
        }

        // Draw the stroke as a polyline
        if (screenPoints.size() >= 2) {
            drawList->AddPolyline(
                screenPoints.data(),
                static_cast<int>(screenPoints.size()),
                strokeColor,
                false, // not closed
                strokeWidth
            );
        }
    }
}

ImVec2 VectorRenderer::calculateCenterPosition()
{
    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    
    if (mAutoCenter.mValue) {
        return ImVec2(displaySize.x / 2, displaySize.y / 2);
    } else {
        // Use manual positioning based on canvas dimensions
        return ImVec2(
            displaySize.x / 2,
            displaySize.y / 2
        );
    }
}

ImColor VectorRenderer::hexToImColor(const std::string& hexColor)
{
    // Remove # if present
    std::string hex = hexColor;
    if (hex[0] == '#') {
        hex = hex.substr(1);
    }

    // Parse hex color
    unsigned int colorValue;
    std::stringstream ss;
    ss << std::hex << hex;
    ss >> colorValue;

    // Extract RGB components
    float r = ((colorValue >> 16) & 0xFF) / 255.0f;
    float g = ((colorValue >> 8) & 0xFF) / 255.0f;
    float b = (colorValue & 0xFF) / 255.0f;

    return ImColor(r, g, b, 1.0f);
} 