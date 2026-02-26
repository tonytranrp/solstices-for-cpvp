#pragma once
//
// Created by vastrakai on 7/7/2024.
//

#include <string>
#include <Windows.h>

class ExceptionHandler {
public:
    static void init();
    static std::string makeCrashLog(const std::string& text, DWORD exceptionCode);
};
