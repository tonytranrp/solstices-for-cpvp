//
// Created by vastrakai on 6/25/2024.
//

#include "KeyHook.hpp"

#include <Solstice.hpp>
#include <Features/Modules/Visual/ClickGui.hpp>
#include <Features/Events/KeyEvent.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>

std::unique_ptr<Detour> KeyHook::mDetour = nullptr;

ImGuiKey ImGui_ImplWin32_VirtualKeyToImGuiKey(WPARAM wParam)
{
    switch (wParam)
    {
        case VK_TAB: return ImGuiKey_Tab;
        case VK_LEFT: return ImGuiKey_LeftArrow;
        case VK_RIGHT: return ImGuiKey_RightArrow;
        case VK_UP: return ImGuiKey_UpArrow;
        case VK_DOWN: return ImGuiKey_DownArrow;
        case VK_PRIOR: return ImGuiKey_PageUp;
        case VK_NEXT: return ImGuiKey_PageDown;
        case VK_HOME: return ImGuiKey_Home;
        case VK_END: return ImGuiKey_End;
        case VK_INSERT: return ImGuiKey_Insert;
        case VK_DELETE: return ImGuiKey_Delete;
        case VK_BACK: return ImGuiKey_Backspace;
        case VK_SPACE: return ImGuiKey_Space;
        case VK_RETURN: return ImGuiKey_Enter;
        case VK_ESCAPE: return ImGuiKey_Escape;
        case VK_OEM_7: return ImGuiKey_Apostrophe;
        case VK_OEM_COMMA: return ImGuiKey_Comma;
        case VK_OEM_MINUS: return ImGuiKey_Minus;
        case VK_OEM_PERIOD: return ImGuiKey_Period;
        case VK_OEM_2: return ImGuiKey_Slash;
        case VK_OEM_1: return ImGuiKey_Semicolon;
        case VK_OEM_PLUS: return ImGuiKey_Equal;
        case VK_OEM_4: return ImGuiKey_LeftBracket;
        case VK_OEM_5: return ImGuiKey_Backslash;
        case VK_OEM_6: return ImGuiKey_RightBracket;
        case VK_OEM_3: return ImGuiKey_GraveAccent;
        case VK_CAPITAL: return ImGuiKey_CapsLock;
        case VK_SCROLL: return ImGuiKey_ScrollLock;
        case VK_NUMLOCK: return ImGuiKey_NumLock;
        case VK_SNAPSHOT: return ImGuiKey_PrintScreen;
        case VK_PAUSE: return ImGuiKey_Pause;
        case VK_NUMPAD0: return ImGuiKey_Keypad0;
        case VK_NUMPAD1: return ImGuiKey_Keypad1;
        case VK_NUMPAD2: return ImGuiKey_Keypad2;
        case VK_NUMPAD3: return ImGuiKey_Keypad3;
        case VK_NUMPAD4: return ImGuiKey_Keypad4;
        case VK_NUMPAD5: return ImGuiKey_Keypad5;
        case VK_NUMPAD6: return ImGuiKey_Keypad6;
        case VK_NUMPAD7: return ImGuiKey_Keypad7;
        case VK_NUMPAD8: return ImGuiKey_Keypad8;
        case VK_NUMPAD9: return ImGuiKey_Keypad9;
        case VK_DECIMAL: return ImGuiKey_KeypadDecimal;
        case VK_DIVIDE: return ImGuiKey_KeypadDivide;
        case VK_MULTIPLY: return ImGuiKey_KeypadMultiply;
        case VK_SUBTRACT: return ImGuiKey_KeypadSubtract;
        case VK_ADD: return ImGuiKey_KeypadAdd;
        case VK_LSHIFT: return ImGuiKey_LeftShift;
        case VK_LCONTROL: return ImGuiKey_LeftCtrl;
        case VK_LMENU: return ImGuiKey_LeftAlt;
        case VK_LWIN: return ImGuiKey_LeftSuper;
        case VK_RSHIFT: return ImGuiKey_RightShift;
        case VK_RCONTROL: return ImGuiKey_RightCtrl;
        case VK_RMENU: return ImGuiKey_RightAlt;
        case VK_RWIN: return ImGuiKey_RightSuper;
        case VK_APPS: return ImGuiKey_Menu;
        case '0': return ImGuiKey_0;
        case '1': return ImGuiKey_1;
        case '2': return ImGuiKey_2;
        case '3': return ImGuiKey_3;
        case '4': return ImGuiKey_4;
        case '5': return ImGuiKey_5;
        case '6': return ImGuiKey_6;
        case '7': return ImGuiKey_7;
        case '8': return ImGuiKey_8;
        case '9': return ImGuiKey_9;
        case 'A': return ImGuiKey_A;
        case 'B': return ImGuiKey_B;
        case 'C': return ImGuiKey_C;
        case 'D': return ImGuiKey_D;
        case 'E': return ImGuiKey_E;
        case 'F': return ImGuiKey_F;
        case 'G': return ImGuiKey_G;
        case 'H': return ImGuiKey_H;
        case 'I': return ImGuiKey_I;
        case 'J': return ImGuiKey_J;
        case 'K': return ImGuiKey_K;
        case 'L': return ImGuiKey_L;
        case 'M': return ImGuiKey_M;
        case 'N': return ImGuiKey_N;
        case 'O': return ImGuiKey_O;
        case 'P': return ImGuiKey_P;
        case 'Q': return ImGuiKey_Q;
        case 'R': return ImGuiKey_R;
        case 'S': return ImGuiKey_S;
        case 'T': return ImGuiKey_T;
        case 'U': return ImGuiKey_U;
        case 'V': return ImGuiKey_V;
        case 'W': return ImGuiKey_W;
        case 'X': return ImGuiKey_X;
        case 'Y': return ImGuiKey_Y;
        case 'Z': return ImGuiKey_Z;
        case VK_F1: return ImGuiKey_F1;
        case VK_F2: return ImGuiKey_F2;
        case VK_F3: return ImGuiKey_F3;
        case VK_F4: return ImGuiKey_F4;
        case VK_F5: return ImGuiKey_F5;
        case VK_F6: return ImGuiKey_F6;
        case VK_F7: return ImGuiKey_F7;
        case VK_F8: return ImGuiKey_F8;
        case VK_F9: return ImGuiKey_F9;
        case VK_F10: return ImGuiKey_F10;
        case VK_F11: return ImGuiKey_F11;
        case VK_F12: return ImGuiKey_F12;
        default: return ImGuiKey_None;
    }
}

void KeyHook::onKey(uint32_t key, bool isDown)
{
    auto oFunc = mDetour->getOriginal<&onKey>();

    Keyboard::mPressedKeys[key] = isDown;

    bool imguiWantsKeyboard = false;
    if (ImGui::GetCurrentContext())
    {
        ImGuiIO& io = ImGui::GetIO();

        ImGuiKey imKey = ImGui_ImplWin32_VirtualKeyToImGuiKey(key);
        if (imKey != ImGuiKey_None)
        {
            io.AddKeyEvent(imKey, isDown);
        }

        if (isDown)
        {
            const int scanCode = MapVirtualKeyA(key, MAPVK_VK_TO_VSC);
            BYTE translation[2];
            BYTE keyState[256] = { 0 };
            GetKeyboardState(keyState);
            const int result = ToAscii(key, scanCode, keyState, reinterpret_cast<LPWORD>(translation), 0);

            if (result == 1)
            {
                io.AddInputCharacter(static_cast<char>(translation[0]));
            }
            else if (result == 2)
            {
                io.AddInputCharacter(static_cast<char>(translation[1]));
            }
        }

        imguiWantsKeyboard = io.WantCaptureKeyboard || io.WantTextInput;
    }

    bool keyEventCancelled = false;
    if (gFeatureManager && gFeatureManager->mDispatcher)
    {
        auto holder = nes::make_holder<KeyEvent>(key, isDown);
        gFeatureManager->mDispatcher->trigger<KeyEvent>(holder);
        keyEventCancelled = holder->mCancelled;
    }
    else
    {
        if (!gFeatureManager)
        {
            spdlog::critical("FeatureManager is null");
        }
        else if (!gFeatureManager->mDispatcher)
        {
            spdlog::critical("Dispatcher is null");
        }
    }

    auto* clientInstance = ClientInstance::get();
    bool clickGuiEnabled = false;
    if (gFeatureManager && gFeatureManager->mModuleManager)
    {
        if (const auto* clickGui = gFeatureManager->mModuleManager->getModule<ClickGui>())
        {
            clickGuiEnabled = clickGui->mEnabled;
        }
    }

    const bool isChatOpen = clientInstance && clientInstance->getScreenName() == "chat_screen";
    if (key == VK_END && isDown && !isChatOpen && !imguiWantsKeyboard && !clickGuiEnabled)
    {
        Solstice::mRequestEject.store(true);
    }

    const bool shouldBlockGameInput = keyEventCancelled || (isDown && imguiWantsKeyboard) || (clickGuiEnabled && isDown);
    if (shouldBlockGameInput)
    {
        return;
    }

    oFunc(key, isDown);

    if (gFeatureManager && gFeatureManager->mModuleManager)
    {
        gFeatureManager->mModuleManager->handleModuleKeybinds(key, isDown);
    }
}

void KeyHook::init()
{
    mDetour = std::make_unique<Detour>("Keyboard::feed", reinterpret_cast<void*>(SigManager::Keyboard_feed), &KeyHook::onKey);
}

void KeyHook::shutdown()
{
    if (mDetour)
    {
        mDetour->restore();
        mDetour.reset();
    }
}
