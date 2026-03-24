#include "Platform/PlatformUtils.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>

#ifdef LIFE_PLATFORM_WINDOWS
    #include <Windows.h>
    #include <malloc.h>
#elif defined(LIFE_PLATFORM_MACOS)
    #include <dlfcn.h>
    #include <mach/mach_time.h>
    #include <pthread.h>
    #include <signal.h>
    #include <unistd.h>
#elif defined(LIFE_PLATFORM_LINUX)
    #include <dlfcn.h>
    #include <pthread.h>
    #include <signal.h>
    #include <unistd.h>
#endif

namespace Life
{
    namespace PlatformUtils
    {
        std::string GetPathSeparator()
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                return "\\";
            #else
                return "/";
            #endif
        }

        std::string NormalizePath(const std::string& path)
        {
            std::filesystem::path fsPath(path);
            return fsPath.lexically_normal().string();
        }

        std::string JoinPath(const std::string& path1, const std::string& path2)
        {
            std::filesystem::path fsPath1(path1);
            std::filesystem::path fsPath2(path2);
            return (fsPath1 / fsPath2).string();
        }

        std::string GetDirectoryName(const std::string& path)
        {
            std::filesystem::path fsPath(path);
            return fsPath.parent_path().string();
        }

        std::string GetFileName(const std::string& path)
        {
            std::filesystem::path fsPath(path);
            return fsPath.filename().string();
        }

        std::string GetFileExtension(const std::string& path)
        {
            std::filesystem::path fsPath(path);
            return fsPath.extension().string();
        }

        std::optional<std::string> GetEnvironmentVariable(const std::string& name)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                char* value = nullptr;
                size_t size = 0;
                if (_dupenv_s(&value, &size, name.c_str()) == 0 && value != nullptr)
                {
                    std::string result(value);
                    free(value);
                    return result;
                }
            #else
                const char* value = getenv(name.c_str());
                if (value != nullptr)
                {
                    return std::string(value);
                }
            #endif
            return std::nullopt;
        }

        bool SetEnvironmentVariable(const std::string& name, const std::string& value)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                return _putenv_s(name.c_str(), value.c_str()) == 0;
            #else
                return setenv(name.c_str(), value.c_str(), 1) == 0;
            #endif
        }

        uint32_t GetCurrentProcessId()
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                return static_cast<uint32_t>(::GetCurrentProcessId());
            #else
                return static_cast<uint32_t>(getpid());
            #endif
        }

        uint32_t GetCurrentThreadId()
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                return static_cast<uint32_t>(::GetCurrentThreadId());
            #elif defined(LIFE_PLATFORM_MACOS)
                uint64_t threadId;
                pthread_threadid_np(pthread_self(), &threadId);
                return static_cast<uint32_t>(threadId);
            #else
                return static_cast<uint32_t>(pthread_self());
            #endif
        }

        void Sleep(uint32_t milliseconds)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                ::Sleep(milliseconds);
            #else
                usleep(milliseconds * 1000);
            #endif
        }

        void* LoadLibrary(const std::string& path)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                return LoadLibraryA(path.c_str());
            #else
                return dlopen(path.c_str(), RTLD_LAZY);
            #endif
        }

        void* GetProcAddress(void* library, const std::string& name)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                return ::GetProcAddress(static_cast<HMODULE>(library), name.c_str());
            #else
                return dlsym(library, name.c_str());
            #endif
        }

        void FreeLibrary(void* library)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                ::FreeLibrary(static_cast<HMODULE>(library));
            #else
                dlclose(library);
            #endif
        }

        void* AllocateAligned(size_t size, size_t alignment)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                return _aligned_malloc(size, alignment);
            #else
                void* ptr = nullptr;
                if (posix_memalign(&ptr, alignment, size) == 0)
                {
                    return ptr;
                }
                return nullptr;
            #endif
        }

        void FreeAligned(void* ptr)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                _aligned_free(ptr);
            #else
                free(ptr);
            #endif
        }

        uint64_t GetHighResolutionTime()
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                LARGE_INTEGER frequency, counter;
                QueryPerformanceFrequency(&frequency);
                QueryPerformanceCounter(&counter);
                return static_cast<uint64_t>((counter.QuadPart * 1000000) / frequency.QuadPart);
            #elif defined(LIFE_PLATFORM_MACOS)
                return static_cast<uint64_t>(mach_absolute_time() / 1000);
            #else
                auto now = std::chrono::high_resolution_clock::now();
                auto duration = now.time_since_epoch();
                return std::chrono::duration_cast<std::chrono::microseconds>(duration).count();
            #endif
        }

        uint64_t GetSystemTime()
        {
            auto now = std::chrono::system_clock::now();
            auto duration = now.time_since_epoch();
            return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        }

        void SetConsoleColor(uint32_t color)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(hConsole, static_cast<WORD>(color));
            #else
                #ifdef LIFE_CONSOLE_LOGGING_ENABLED
                std::cout << "\033[" << color << "m";
                #endif
            #endif
        }

        void ResetConsoleColor()
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
                SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);
            #else
                #ifdef LIFE_CONSOLE_LOGGING_ENABLED
                std::cout << "\033[0m";
                #endif
            #endif
        }

        bool IsConsoleAvailable()
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                return GetConsoleWindow() != nullptr;
            #else
                return isatty(STDOUT_FILENO) != 0;
            #endif
        }

        void BreakIntoDebugger()
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                __debugbreak();
            #elif defined(LIFE_PLATFORM_MACOS)
                __builtin_trap();
            #else
                raise(SIGTRAP);
            #endif
        }

        void OutputDebugString(const std::string& message)
        {
            #ifdef LIFE_PLATFORM_WINDOWS
                OutputDebugStringA(message.c_str());
            #else
                #ifdef LIFE_CONSOLE_LOGGING_ENABLED
                std::cerr << "[DEBUG] " << message << std::endl;
                #endif
            #endif
        }
    }
}
