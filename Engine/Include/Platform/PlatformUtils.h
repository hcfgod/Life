#pragma once

#include "Platform/PlatformMacros.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace Life
{
    namespace PlatformUtils
    {
        std::string GetPathSeparator();
        std::string NormalizePath(const std::string& path);
        std::string JoinPath(const std::string& path1, const std::string& path2);
        std::string GetDirectoryName(const std::string& path);
        std::string GetFileName(const std::string& path);
        std::string GetFileExtension(const std::string& path);

        std::optional<std::string> GetEnvironmentVariable(const std::string& name);
        bool SetEnvironmentVariable(const std::string& name, const std::string& value);

        uint32_t GetCurrentProcessId();
        uint32_t GetCurrentThreadId();
        void Sleep(uint32_t milliseconds);

        void* LoadLibrary(const std::string& path);
        void* GetProcAddress(void* library, const std::string& name);
        void FreeLibrary(void* library);

        void* AllocateAligned(size_t size, size_t alignment);
        void FreeAligned(void* ptr);

        uint64_t GetHighResolutionTime();
        uint64_t GetSystemTime();

        void SetConsoleColor(uint32_t color);
        void ResetConsoleColor();
        bool IsConsoleAvailable();

        void BreakIntoDebugger();
        void OutputDebugString(const std::string& message);
    }
}
