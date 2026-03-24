#include "Platform/PlatformDetection.h"

#include <cstdlib>
#include <string>

#ifdef LIFE_PLATFORM_WINDOWS
    #include <Windows.h>
    #include <direct.h>
    #include <shlobj.h>
#elif defined(LIFE_PLATFORM_MACOS)
    #include <limits.h>
    #include <mach-o/dyld.h>
    #include <unistd.h>
#elif defined(LIFE_PLATFORM_LINUX)
    #include <limits.h>
    #include <unistd.h>
#endif

namespace Life
{
    void PlatformDetection::DetectPaths()
    {
        // Executable path
        #ifdef LIFE_PLATFORM_WINDOWS
            char path[MAX_PATH];
            GetModuleFileNameA(nullptr, path, MAX_PATH);
            s_PlatformInfo.executablePath = path;
        #elif defined(LIFE_PLATFORM_MACOS)
            // macOS does not provide /proc/self/exe. Use the dyld API to locate the current executable.
            char path[PATH_MAX];
            uint32_t size = sizeof(path);
            if (_NSGetExecutablePath(path, &size) == 0)
            {
                // _NSGetExecutablePath may return a path containing symlinks or relative components.
                // Resolve it to an absolute canonical path when possible.
                char resolved[PATH_MAX];
                if (realpath(path, resolved) != nullptr)
                {
                    s_PlatformInfo.executablePath = resolved;
                }
                else
                {
                    // Fall back to the dyld path if realpath fails (should be rare).
                    s_PlatformInfo.executablePath = path;
                }
            }
            else if (size > 0)
            {
                // Buffer was too small; allocate the requested size and retry.
                std::string tmp;
                tmp.resize(size);
                if (_NSGetExecutablePath(tmp.data(), &size) == 0)
                {
                    char resolved[PATH_MAX];
                    if (realpath(tmp.c_str(), resolved) != nullptr)
                    {
                        s_PlatformInfo.executablePath = resolved;
                    }
                    else
                    {
                        s_PlatformInfo.executablePath = tmp.c_str();
                    }
                }
            }
        #elif defined(LIFE_PLATFORM_LINUX)
            // Linux: /proc/self/exe points to the running executable.
            char path[PATH_MAX];
            const ssize_t len = readlink("/proc/self/exe", path, sizeof(path) - 1);
            if (len > 0)
            {
                path[len] = '\0';
                s_PlatformInfo.executablePath = path;
            }
        #endif

        // Working directory
        #ifdef LIFE_PLATFORM_WINDOWS
            char cwd[MAX_PATH];
            if (_getcwd(cwd, MAX_PATH) != nullptr)
            {
                s_PlatformInfo.workingDirectory = cwd;
            }
        #elif defined(LIFE_PLATFORM_MACOS) || defined(LIFE_PLATFORM_LINUX)
            char cwd[PATH_MAX];
            if (getcwd(cwd, PATH_MAX) != nullptr)
            {
                s_PlatformInfo.workingDirectory = cwd;
            }
        #endif

        // User data path
        #ifdef LIFE_PLATFORM_WINDOWS
            char appData[MAX_PATH];
            if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appData)))
            {
                s_PlatformInfo.userDataPath = std::string(appData) + "\\Life";
            }
        #elif defined(LIFE_PLATFORM_MACOS)
            const char* home = getenv("HOME");
            if (home)
            {
                s_PlatformInfo.userDataPath = std::string(home) + "/Library/Application Support/Life";
            }
        #elif defined(LIFE_PLATFORM_LINUX)
            const char* home = getenv("HOME");
            if (home)
            {
                s_PlatformInfo.userDataPath = std::string(home) + "/.local/share/Life";
            }
        #endif

        // Temp path
        #ifdef LIFE_PLATFORM_WINDOWS
            char tempPath[MAX_PATH];
            if (GetTempPathA(MAX_PATH, tempPath) != 0)
            {
                s_PlatformInfo.tempPath = tempPath;
            }
        #elif defined(LIFE_PLATFORM_MACOS) || defined(LIFE_PLATFORM_LINUX)
            const char* temp = getenv("TMPDIR");
            if (temp)
            {
                s_PlatformInfo.tempPath = temp;
            }
            else
            {
                s_PlatformInfo.tempPath = "/tmp";
            }
        #endif

        // System path
        #ifdef LIFE_PLATFORM_WINDOWS
            char systemPath[MAX_PATH];
            if (GetSystemDirectoryA(systemPath, MAX_PATH) != 0)
            {
                s_PlatformInfo.systemPath = systemPath;
            }
        #elif defined(LIFE_PLATFORM_MACOS)
            s_PlatformInfo.systemPath = "/System";
        #elif defined(LIFE_PLATFORM_LINUX)
            s_PlatformInfo.systemPath = "/usr";
        #endif
    }
}
