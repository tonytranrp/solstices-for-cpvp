//
// Created by vastrakai on 7/7/2024.
//

#include "ExceptionHandler.hpp"

#include <build_info.h>

#include <DbgHelp.h>
#include <Shellapi.h>
#include <Windows.h>
#include <commctrl.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <vector>

#include <Features/FeatureManager.hpp>
#include <Utils/FileUtils.hpp>
#include <Utils/ProcUtils.hpp>
#include <spdlog/spdlog.h>

#include "StackWalker.hpp"

namespace
{
    constexpr int kTaskButtonCopy = 1001;
    constexpr int kTaskButtonOpenFolder = 1002;
    constexpr int kTaskButtonDebugBreak = 1003;

    std::string toHex(const std::uint64_t value, const int width = 0)
    {
        std::ostringstream stream;
        stream << "0x" << std::uppercase << std::hex;
        if (width > 0)
        {
            stream << std::setw(width) << std::setfill('0');
        }
        stream << value;
        return stream.str();
    }

    std::string currentTimestamp()
    {
        const auto now = std::chrono::system_clock::now();
        const auto nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
        localtime_s(&localTime, &nowTime);

        std::ostringstream stream;
        stream << std::put_time(&localTime, "%Y-%m-%d %H:%M:%S");
        return stream.str();
    }

    std::string currentTimestampForFilename()
    {
        const auto now = std::chrono::system_clock::now();
        const auto nowTime = std::chrono::system_clock::to_time_t(now);
        std::tm localTime{};
        localtime_s(&localTime, &nowTime);

        std::ostringstream stream;
        stream << std::put_time(&localTime, "%Y%m%d_%H%M%S");
        return stream.str();
    }

    std::wstring utf8ToWide(const std::string& text)
    {
        if (text.empty())
        {
            return {};
        }

        const int required = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
        if (required <= 0)
        {
            return {};
        }

        std::wstring wide(static_cast<std::size_t>(required), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), wide.data(), required);
        return wide;
    }

    std::string basenameFromPath(const std::string& path)
    {
        std::error_code ec;
        const std::filesystem::path fsPath(path);
        const auto filename = fsPath.filename().string();
        if (!filename.empty())
        {
            return filename;
        }
        return path;
    }

    bool copyToClipboard(const std::string& text)
    {
        const std::wstring wide = utf8ToWide(text);
        if (wide.empty())
        {
            return false;
        }

        if (!OpenClipboard(nullptr))
        {
            return false;
        }

        EmptyClipboard();

        const std::size_t bytes = (wide.size() + 1) * sizeof(wchar_t);
        HGLOBAL handle = GlobalAlloc(GMEM_MOVEABLE, bytes);
        if (handle == nullptr)
        {
            CloseClipboard();
            return false;
        }

        void* memory = GlobalLock(handle);
        if (memory == nullptr)
        {
            GlobalFree(handle);
            CloseClipboard();
            return false;
        }

        memcpy(memory, wide.c_str(), bytes);
        GlobalUnlock(handle);

        if (SetClipboardData(CF_UNICODETEXT, handle) == nullptr)
        {
            GlobalFree(handle);
            CloseClipboard();
            return false;
        }

        CloseClipboard();
        return true;
    }

    std::string exceptionCodeName(const DWORD code)
    {
        switch (code)
        {
        case EXCEPTION_ACCESS_VIOLATION: return "EXCEPTION_ACCESS_VIOLATION";
        case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "EXCEPTION_ARRAY_BOUNDS_EXCEEDED";
        case EXCEPTION_BREAKPOINT: return "EXCEPTION_BREAKPOINT";
        case EXCEPTION_DATATYPE_MISALIGNMENT: return "EXCEPTION_DATATYPE_MISALIGNMENT";
        case EXCEPTION_FLT_DENORMAL_OPERAND: return "EXCEPTION_FLT_DENORMAL_OPERAND";
        case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "EXCEPTION_FLT_DIVIDE_BY_ZERO";
        case EXCEPTION_FLT_INVALID_OPERATION: return "EXCEPTION_FLT_INVALID_OPERATION";
        case EXCEPTION_FLT_OVERFLOW: return "EXCEPTION_FLT_OVERFLOW";
        case EXCEPTION_FLT_STACK_CHECK: return "EXCEPTION_FLT_STACK_CHECK";
        case EXCEPTION_FLT_UNDERFLOW: return "EXCEPTION_FLT_UNDERFLOW";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "EXCEPTION_ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "EXCEPTION_IN_PAGE_ERROR";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "EXCEPTION_INT_DIVIDE_BY_ZERO";
        case EXCEPTION_INT_OVERFLOW: return "EXCEPTION_INT_OVERFLOW";
        case EXCEPTION_INVALID_DISPOSITION: return "EXCEPTION_INVALID_DISPOSITION";
        case EXCEPTION_NONCONTINUABLE_EXCEPTION: return "EXCEPTION_NONCONTINUABLE_EXCEPTION";
        case EXCEPTION_PRIV_INSTRUCTION: return "EXCEPTION_PRIV_INSTRUCTION";
        case EXCEPTION_SINGLE_STEP: return "EXCEPTION_SINGLE_STEP";
        case EXCEPTION_STACK_OVERFLOW: return "EXCEPTION_STACK_OVERFLOW";
        default: return "UNKNOWN_EXCEPTION";
        }
    }

    std::string moduleNameFromAddress(void* address, std::uintptr_t& moduleBase)
    {
        moduleBase = 0;
        if (address == nullptr)
        {
            return "unknown";
        }

        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(address, &info, sizeof(info)) == 0 || info.AllocationBase == nullptr)
        {
            return "unknown";
        }

        moduleBase = reinterpret_cast<std::uintptr_t>(info.AllocationBase);
        char modulePath[MAX_PATH] = {};
        if (GetModuleFileNameA(reinterpret_cast<HMODULE>(info.AllocationBase), modulePath, MAX_PATH) == 0)
        {
            return "unknown";
        }

        return basenameFromPath(modulePath);
    }

    std::string buildRegisterDump(const PCONTEXT context)
    {
        if (context == nullptr)
        {
            return "Registers unavailable (context is null).\n";
        }

        std::ostringstream stream;
        stream
            << "RAX=" << toHex(context->Rax, 16) << "  "
            << "RBX=" << toHex(context->Rbx, 16) << "  "
            << "RCX=" << toHex(context->Rcx, 16) << "  "
            << "RDX=" << toHex(context->Rdx, 16) << "\n"
            << "RSI=" << toHex(context->Rsi, 16) << "  "
            << "RDI=" << toHex(context->Rdi, 16) << "  "
            << "RBP=" << toHex(context->Rbp, 16) << "  "
            << "RSP=" << toHex(context->Rsp, 16) << "\n"
            << "R8 =" << toHex(context->R8, 16) << "  "
            << "R9 =" << toHex(context->R9, 16) << "  "
            << "R10=" << toHex(context->R10, 16) << "  "
            << "R11=" << toHex(context->R11, 16) << "\n"
            << "R12=" << toHex(context->R12, 16) << "  "
            << "R13=" << toHex(context->R13, 16) << "  "
            << "R14=" << toHex(context->R14, 16) << "  "
            << "R15=" << toHex(context->R15, 16) << "\n"
            << "RIP=" << toHex(context->Rip, 16) << "  "
            << "EFlags=" << toHex(context->EFlags, 8) << "\n";
        return stream.str();
    }

    std::string writeMiniDump(const PEXCEPTION_POINTERS info)
    {
        const std::string dumpPath = FileUtils::getSolsticeDir() + "crash_" + currentTimestampForFilename() + ".dmp";

        HANDLE dumpFile = CreateFileA(
            dumpPath.c_str(),
            GENERIC_WRITE,
            FILE_SHARE_READ,
            nullptr,
            CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);

        if (dumpFile == INVALID_HANDLE_VALUE)
        {
            spdlog::error("[crash] Failed creating minidump file: {}", dumpPath);
            return {};
        }

        MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
        exceptionInfo.ThreadId = GetCurrentThreadId();
        exceptionInfo.ExceptionPointers = info;
        exceptionInfo.ClientPointers = FALSE;

        const BOOL success = MiniDumpWriteDump(
            GetCurrentProcess(),
            GetCurrentProcessId(),
            dumpFile,
            static_cast<MINIDUMP_TYPE>(
                MiniDumpWithThreadInfo |
                MiniDumpWithIndirectlyReferencedMemory |
                MiniDumpWithDataSegs |
                MiniDumpWithHandleData),
            info != nullptr ? &exceptionInfo : nullptr,
            nullptr,
            nullptr);

        CloseHandle(dumpFile);

        if (!success)
        {
            spdlog::error("[crash] MiniDumpWriteDump failed (error={})", GetLastError());
            return {};
        }

        return dumpPath;
    }

    void showCrashDialog(const std::string& reportText, const std::string& crashLogPath, const std::string& minidumpPath, const DWORD exceptionCode)
    {
        const std::string mainInstructionUtf8 =
            "Solstice crashed (" + exceptionCodeName(exceptionCode) + ", " + toHex(exceptionCode, 8) + ")";

        std::ostringstream contentStream;
        contentStream << "Crash log: " << crashLogPath << "\n";
        if (!minidumpPath.empty())
        {
            contentStream << "Minidump: " << minidumpPath << "\n";
        }
        contentStream << "\nUse \"Copy Details\" to copy the full report.";

        const std::wstring title = L"Solstice Crash Handler";
        const std::wstring mainInstruction = utf8ToWide(mainInstructionUtf8);
        const std::wstring content = utf8ToWide(contentStream.str());
        const std::wstring expanded = utf8ToWide(reportText);

        HMODULE comctl = LoadLibraryW(L"comctl32.dll");
        if (comctl == nullptr)
        {
            MessageBoxA(
                ProcUtils::getMinecraftWindow(),
                reportText.c_str(),
                "Solstice Crash Handler",
                MB_ICONERROR | MB_OK);
            return;
        }

        using TaskDialogIndirectFn = HRESULT(WINAPI*)(const TASKDIALOGCONFIG*, int*, int*, BOOL*);
        const auto taskDialogIndirect = reinterpret_cast<TaskDialogIndirectFn>(GetProcAddress(comctl, "TaskDialogIndirect"));
        if (taskDialogIndirect == nullptr)
        {
            MessageBoxA(
                ProcUtils::getMinecraftWindow(),
                reportText.c_str(),
                "Solstice Crash Handler",
                MB_ICONERROR | MB_OK);
            FreeLibrary(comctl);
            return;
        }

        const std::wstring copyText = L"Copy Details";
        const std::wstring openFolderText = L"Open Crash Folder";
        const std::wstring closeText = L"Close";
        const std::wstring debugBreakText = L"Debug Break";

        TASKDIALOG_BUTTON buttons[4]{};
        int buttonCount = 0;
        buttons[buttonCount++] = TASKDIALOG_BUTTON{ kTaskButtonCopy, copyText.c_str() };
        buttons[buttonCount++] = TASKDIALOG_BUTTON{ kTaskButtonOpenFolder, openFolderText.c_str() };
        if (IsDebuggerPresent())
        {
            buttons[buttonCount++] = TASKDIALOG_BUTTON{ kTaskButtonDebugBreak, debugBreakText.c_str() };
        }
        buttons[buttonCount++] = TASKDIALOG_BUTTON{ IDOK, closeText.c_str() };

        int pressedButton = IDOK;
        while (true)
        {
            TASKDIALOGCONFIG config{};
            config.cbSize = sizeof(config);
            config.hwndParent = ProcUtils::getMinecraftWindow();
            config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_EXPANDED_BY_DEFAULT | TDF_EXPAND_FOOTER_AREA;
            config.pszWindowTitle = title.c_str();
            config.pszMainInstruction = mainInstruction.c_str();
            config.pszContent = content.c_str();
            config.pszExpandedInformation = expanded.c_str();
            config.pszFooter = L"You can share crash.log and .dmp for debugging.";
            config.pButtons = buttons;
            config.cButtons = buttonCount;
            config.nDefaultButton = IDOK;

            if (FAILED(taskDialogIndirect(&config, &pressedButton, nullptr, nullptr)))
            {
                break;
            }

            if (pressedButton == kTaskButtonCopy)
            {
                copyToClipboard(reportText);
                continue;
            }

            if (pressedButton == kTaskButtonOpenFolder)
            {
                ShellExecuteA(nullptr, "open", FileUtils::getSolsticeDir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
                continue;
            }

            if (pressedButton == kTaskButtonDebugBreak && IsDebuggerPresent())
            {
                __debugbreak();
                continue;
            }

            break;
        }

        FreeLibrary(comctl);
    }

    std::string buildCrashReport(const std::string& reason, const PEXCEPTION_POINTERS info)
    {
        const auto* exceptionRecord = info != nullptr ? info->ExceptionRecord : nullptr;
        const DWORD exceptionCode = exceptionRecord != nullptr ? exceptionRecord->ExceptionCode : 0;
        const void* exceptionAddress = exceptionRecord != nullptr ? exceptionRecord->ExceptionAddress : nullptr;
        const PCONTEXT context = info != nullptr ? info->ContextRecord : nullptr;

        std::uintptr_t moduleBase = 0;
        const std::string moduleName = moduleNameFromAddress(const_cast<void*>(exceptionAddress), moduleBase);
        const std::uintptr_t exceptionOffset = exceptionAddress != nullptr
            ? (reinterpret_cast<std::uintptr_t>(exceptionAddress) - moduleBase)
            : 0;

        StackWalker walker;
        const auto stackFrames = walker.ShowCallstack(GetCurrentThread(), context);

        std::ostringstream report;
        report << "Solstice Crash Report\n";
        report << "Timestamp: " << currentTimestamp() << "\n";
        report << "Reason: " << reason << "\n";
        report << "Exception: " << exceptionCodeName(exceptionCode) << " (" << toHex(exceptionCode, 8) << ")\n";
        report << "Process ID: " << GetCurrentProcessId() << ", Thread ID: " << GetCurrentThreadId() << "\n";
        report << "Exception Address: " << toHex(reinterpret_cast<std::uintptr_t>(exceptionAddress), 16)
               << " (" << moduleName << "+0x" << std::hex << std::uppercase << exceptionOffset << std::dec << ")\n";
        if (exceptionRecord != nullptr)
        {
            report << "Exception Flags: " << toHex(exceptionRecord->ExceptionFlags, 8) << "\n";
            report << "Exception Parameters: " << exceptionRecord->NumberParameters << "\n";
            for (DWORD index = 0; index < exceptionRecord->NumberParameters; ++index)
            {
                report << "  Param[" << index << "]: "
                    << toHex(exceptionRecord->ExceptionInformation[index], 16) << "\n";
            }
        }

        report << "\nRegisters\n";
        report << buildRegisterDump(context);

        report << "\nStack Trace\n";
        for (const auto& frame : stackFrames)
        {
            report << frame << "\n";
        }

        report << "\nRuntime\n";
        report << "Module count: " << ProcUtils::getModuleCount() << "\n";
        report << "Solstice commit: " << SOLSTICE_BUILD_VERSION << "\n";
        report << "Solstice branch: " << SOLSTICE_BUILD_BRANCH << "\n";
        report << "Solstice commit msg: " << SOLSTICE_BUILD_COMMIT_MESSAGE << "\n";
        report << "Minecraft version: " << ProcUtils::getVersion() << "\n";

        if (gFeatureManager != nullptr && gFeatureManager->mModuleManager != nullptr)
        {
            const auto modules = gFeatureManager->mModuleManager->getModules();
            report << "\nModules (" << modules.size() << ")\n";
            for (const auto& module : modules)
            {
                if (module == nullptr)
                {
                    continue;
                }
                report << "  " << module->mName << " - " << (module->mEnabled ? "Enabled" : "Disabled") << "\n";
            }
        }
        else
        {
            report << "\nModules unavailable (FeatureManager not initialized)\n";
        }

        return report.str();
    }

    LONG WINAPI TopLevelExceptionHandler(const PEXCEPTION_POINTERS exceptionInfo)
    {
        static std::atomic_bool isHandlingCrash = false;
        if (isHandlingCrash.exchange(true))
        {
            return EXCEPTION_EXECUTE_HANDLER;
        }

        const DWORD exceptionCode =
            (exceptionInfo != nullptr && exceptionInfo->ExceptionRecord != nullptr)
            ? exceptionInfo->ExceptionRecord->ExceptionCode
            : 0;

        const std::string reason = "Unhandled exception";
        const std::string crashReport = buildCrashReport(reason, exceptionInfo);
        spdlog::critical("\n{}", crashReport);

        const std::string crashLogPath = ExceptionHandler::makeCrashLog(crashReport, exceptionCode);
        const std::string minidumpPath = writeMiniDump(exceptionInfo);
        showCrashDialog(crashReport, crashLogPath, minidumpPath, exceptionCode);

        isHandlingCrash.store(false);
        return EXCEPTION_EXECUTE_HANDLER;
    }
}

void ExceptionHandler::init()
{
    SetUnhandledExceptionFilter(TopLevelExceptionHandler);
}

std::string ExceptionHandler::makeCrashLog(const std::string& text, const DWORD exceptionCode)
{
    const std::string directory = FileUtils::getSolsticeDir();
    const std::string crashLogPath = directory + "crash.log";
    const std::string archivedPath = directory + "crash_" + currentTimestampForFilename() + ".log";

    auto writeLog = [&](const std::string& path, const bool append) {
        std::ofstream file(path, append ? std::ios::app : std::ios::trunc);
        if (!file.is_open())
        {
            return false;
        }

        file << "----------------- Crash at " << currentTimestamp() << "\n";
        file << text << "\n";
        file << "Exception Code: " << toHex(exceptionCode, 8) << "\n";
        file << "----------------------------------------\n";
        file.flush();
        return true;
    };

    writeLog(crashLogPath, true);
    writeLog(archivedPath, false);
    return archivedPath;
}
