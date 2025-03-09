// ScriptingGui.cpp
// A basic scripting GUI with file listing, editing, and an option to import custom script files.
#include "ScriptingGui.hpp"

#include <imgui.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace fs = std::filesystem;

static const std::string SCRIPT_DIRECTORY = "./scripts";
static std::vector<std::string> gScriptFiles;
static int gSelectedFileIndex = -1;
static std::string gFileContents;

// Refresh the file list from SCRIPT_DIRECTORY.
static void refreshScriptFileList() {
    gScriptFiles.clear();
    gSelectedFileIndex = -1;
    gFileContents.clear();
    try {
        for (const auto& entry : fs::directory_iterator(SCRIPT_DIRECTORY)) {
            if (entry.is_regular_file()) {
                // Optionally filter by extension, e.g., only ".py"
                std::string ext = entry.path().extension().string();
                // Uncomment the following line if you only want Python files:
                // if (ext != ".py") continue;
                gScriptFiles.push_back(entry.path().filename().string());
            }
        }
        std::sort(gScriptFiles.begin(), gScriptFiles.end());
    }
    catch (const std::exception& e) {
        gScriptFiles.push_back("Error: " + std::string(e.what()));
    }
}

// Load file contents from the given filename.
static void loadFileContents(const std::string& filename) {
    fs::path filepath = fs::path(SCRIPT_DIRECTORY) / filename;
    std::ifstream inFile(filepath);
    if (!inFile) {
        gFileContents = "// Failed to load file: " + filename;
        return;
    }
    std::stringstream ss;
    ss << inFile.rdbuf();
    gFileContents = ss.str();
}

// Save gFileContents to the given filename.
static void saveFileContents(const std::string& filename) {
    fs::path filepath = fs::path(SCRIPT_DIRECTORY) / filename;
    std::ofstream outFile(filepath);
    if (!outFile) {
        // Optionally show an error message.
        return;
    }
    outFile << gFileContents;
}

// Helper: call an external Python file dialog and return the selected file path.
static std::string runFileDialog() {
    std::string result;
#ifdef _WIN32
    FILE* pipe = _popen("python file_dialog.py", "r");
#else
    FILE* pipe = popen("python3 file_dialog.py", "r");
#endif
    if (!pipe) return "";
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    // Remove trailing newline characters.
    result.erase(std::remove(result.begin(), result.end(), '\n'), result.end());
    return result;
}

// In your render function, add an "Import Custom Script" button.
void ScriptingGui::render(float inScale, float animation, ImVec2 screen, float blurStrength)
{
    FontHelper::pushPrefFont();

    auto drawList = ImGui::GetBackgroundDrawList();
    drawList->AddRectFilled(ImVec2(0, 0), ImVec2(screen.x, screen.y),
        IM_COL32(0, 0, 0, static_cast<int>(255 * animation * 0.38f)));
    ImRenderUtils::addBlur(ImVec4(0.f, 0.f, screen.x, screen.y), animation * blurStrength, 0);

    ImGui::SetNextWindowSize(ImVec2(800 * inScale, 600 * inScale), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2((screen.x - 800 * inScale) * 0.5f, (screen.y - 600 * inScale) * 0.5f), ImGuiCond_Always);
    ImGui::Begin("Scripting");

    // Header: Refresh and Import buttons.
    if (ImGui::Button("Refresh")) {
        refreshScriptFileList();
    }
    ImGui::SameLine();
    if (ImGui::Button("Import Custom Script")) {
        std::string chosenFile = runFileDialog();
        if (!chosenFile.empty()) {
            try {
                // Copy the chosen file to the SCRIPT_DIRECTORY.
                fs::path srcPath(chosenFile);
                fs::path dstPath = fs::path(SCRIPT_DIRECTORY) / srcPath.filename();
                fs::copy_file(srcPath, dstPath, fs::copy_options::overwrite_existing);
                refreshScriptFileList();
            }
            catch (const std::exception& e) {
                // Optionally log error.
                std::cerr << "Error copying file: " << e.what() << "\n";
            }
        }
    }
    ImGui::SameLine();
    ImGui::Text("Script Files in \"%s\"", SCRIPT_DIRECTORY.c_str());

    // Display list of script files.
    ImGui::BeginChild("File List", ImVec2(0, 150), true);
    for (size_t i = 0; i < gScriptFiles.size(); ++i) {
        const std::string& file = gScriptFiles[i];
        bool selected = (static_cast<int>(i) == gSelectedFileIndex);
        if (ImGui::Selectable(file.c_str(), selected)) {
            gSelectedFileIndex = static_cast<int>(i);
            loadFileContents(file);
        }
    }
    ImGui::EndChild();

    // If a file is selected, show its contents in a multiline editor.
    if (gSelectedFileIndex != -1) {
        ImGui::Separator();
        ImGui::Text("Editing: %s", gScriptFiles[gSelectedFileIndex].c_str());
        // Use InputTextMultiline with std::string overload from imgui_stdlib.h.
        ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput | ImGuiInputTextFlags_CallbackResize;
        char buffer[4096];
        std::strncpy(buffer, gFileContents.c_str(), sizeof(buffer));
        buffer[sizeof(buffer) - 1] = '\0';

        if (ImGui::InputTextMultiline("##ScriptContent", buffer, sizeof(buffer), ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16), flags)) {
            gFileContents = buffer;
        }
        if (ImGui::Button("Save")) {
            saveFileContents(gScriptFiles[gSelectedFileIndex]);
        }
    }

    ImGui::End();
    FontHelper::popPrefFont();
}
