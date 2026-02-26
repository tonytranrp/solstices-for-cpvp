//
// Created by vastrakai on 7/7/2024.
//

#include "StackWalker.hpp"

#include <DbgHelp.h>
#include <iomanip>
#include <sstream>

#include <spdlog/spdlog.h>

namespace
{
    std::string formatAddress(const DWORD64 address)
    {
        std::ostringstream stream;
        stream << "0x" << std::uppercase << std::hex << std::setw(16) << std::setfill('0') << address;
        return stream.str();
    }
}

StackWalker::StackWalker()
{
    constexpr DWORD kSymbolOptions =
        SYMOPT_DEFERRED_LOADS |
        SYMOPT_LOAD_LINES |
        SYMOPT_UNDNAME |
        SYMOPT_FAIL_CRITICAL_ERRORS |
        SYMOPT_INCLUDE_32BIT_MODULES;

    SymSetOptions(SymGetOptions() | kSymbolOptions);

    if (!SymInitialize(GetCurrentProcess(), nullptr, TRUE))
    {
        spdlog::error("[crash] SymInitialize failed (error={})", GetLastError());
    }
}

StackWalker::~StackWalker()
{
    SymCleanup(GetCurrentProcess());
}

void StackWalker::LoadModuleSymbols(const std::vector<std::wstring>&)
{
}

void StackWalker::UnloadModuleSymbols()
{
}

std::vector<std::string> StackWalker::ShowCallstack(HANDLE hThread, PCONTEXT pContext)
{
    std::vector<std::string> stackTrace;

    if (hThread == nullptr)
    {
        hThread = GetCurrentThread();
    }

    CONTEXT contextRecord{};
    if (pContext != nullptr)
    {
        contextRecord = *pContext;
    }
    else
    {
        contextRecord.ContextFlags = CONTEXT_FULL;
        RtlCaptureContext(&contextRecord);
    }

    STACKFRAME64 frame{};
#if defined(_M_X64)
    constexpr DWORD machineType = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = contextRecord.Rip;
    frame.AddrFrame.Offset = contextRecord.Rbp;
    frame.AddrStack.Offset = contextRecord.Rsp;
#elif defined(_M_IX86)
    constexpr DWORD machineType = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = contextRecord.Eip;
    frame.AddrFrame.Offset = contextRecord.Ebp;
    frame.AddrStack.Offset = contextRecord.Esp;
#else
    stackTrace.emplace_back("Unsupported architecture for stack walking.");
    return stackTrace;
#endif

    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Mode = AddrModeFlat;

    HANDLE process = GetCurrentProcess();
    DWORD64 previousAddress = 0;
    for (std::size_t frameIndex = 0; frameIndex < 256; ++frameIndex)
    {
        const BOOL walked = StackWalk64(
            machineType,
            process,
            hThread,
            &frame,
            &contextRecord,
            nullptr,
            SymFunctionTableAccess64,
            SymGetModuleBase64,
            nullptr);

        if (!walked || frame.AddrPC.Offset == 0 || frame.AddrPC.Offset == previousAddress)
        {
            break;
        }
        previousAddress = frame.AddrPC.Offset;

        DWORD64 address = frame.AddrPC.Offset;
        DWORD64 symbolDisplacement = 0;
        char symbolBuffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
        auto* symbolInfo = reinterpret_cast<SYMBOL_INFO*>(symbolBuffer);
        symbolInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
        symbolInfo->MaxNameLen = MAX_SYM_NAME;

        std::string symbolName = "??";
        if (SymFromAddr(process, address, &symbolDisplacement, symbolInfo))
        {
            symbolName = symbolInfo->Name;
        }

        IMAGEHLP_MODULE64 moduleInfo{};
        moduleInfo.SizeOfStruct = sizeof(moduleInfo);
        std::string moduleName = "unknown";
        if (SymGetModuleInfo64(process, address, &moduleInfo))
        {
            if (moduleInfo.ModuleName[0] != '\0')
            {
                moduleName = moduleInfo.ModuleName;
            }
            else if (moduleInfo.ImageName != nullptr)
            {
                moduleName = moduleInfo.ImageName;
            }
        }

        IMAGEHLP_LINE64 lineInfo{};
        lineInfo.SizeOfStruct = sizeof(lineInfo);
        DWORD lineDisplacement = 0;
        const bool hasLine = SymGetLineFromAddr64(process, address, &lineDisplacement, &lineInfo) == TRUE;

        std::ostringstream line;
        line << "#" << std::setw(2) << std::setfill('0') << frameIndex
             << " " << formatAddress(address)
             << " " << moduleName << "!" << symbolName;

        if (symbolDisplacement != 0)
        {
            line << "+0x" << std::uppercase << std::hex << symbolDisplacement << std::dec;
        }

        if (hasLine && lineInfo.FileName != nullptr)
        {
            line << " [" << lineInfo.FileName << ":" << lineInfo.LineNumber << "]";
        }

        stackTrace.emplace_back(line.str());
    }

    if (stackTrace.empty())
    {
        stackTrace.emplace_back("No frames captured.");
    }

    return stackTrace;
}
