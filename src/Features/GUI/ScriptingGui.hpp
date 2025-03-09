#pragma once
//
// ScriptingGui.hpp
// Basic file-loading and text-editing GUI for scripts
//

#include <imgui.h>
#include <string>
#include <vector>

class ScriptingGui {
public:
    // Renders the Scripting GUI.
    // inScale: scaling factor for window size
    // animation: overall animation factor (0..1)
    // screen: current screen dimensions (width, height)
    // blurStrength: for background blur effects (if any)
    static void render(float inScale, float animation, ImVec2 screen, float blurStrength);
};
