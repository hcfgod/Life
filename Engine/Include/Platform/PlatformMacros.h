#pragma once

#if defined(_WIN32)
    #define LIFE_PLATFORM_WINDOWS 1
    #define LIFE_PLATFORM_NAME "Windows"
#elif defined(__APPLE__)
    #define LIFE_PLATFORM_MACOS 1
    #define LIFE_PLATFORM_NAME "macOS"
#elif defined(__linux__)
    #define LIFE_PLATFORM_LINUX 1
    #define LIFE_PLATFORM_NAME "Linux"
#else
    #define LIFE_PLATFORM_UNKNOWN 1
    #define LIFE_PLATFORM_NAME "Unknown"
#endif

#if defined(_MSC_VER)
    #define LIFE_COMPILER_MSVC 1
    #define LIFE_COMPILER_NAME "MSVC"
#elif defined(__clang__)
    #if defined(__apple_build_version__) || defined(__APPLE__)
        #define LIFE_COMPILER_APPLE_CLANG 1
        #define LIFE_COMPILER_NAME "AppleClang"
    #else
        #define LIFE_COMPILER_CLANG 1
        #define LIFE_COMPILER_NAME "Clang"
    #endif
#elif defined(__GNUC__)
    #define LIFE_COMPILER_GCC 1
    #define LIFE_COMPILER_NAME "GCC"
#else
    #define LIFE_COMPILER_UNKNOWN 1
    #define LIFE_COMPILER_NAME "Unknown"
#endif

#if defined(_M_X64) || defined(__x86_64__)
    #define LIFE_ARCHITECTURE_X64 1
    #define LIFE_ARCHITECTURE_NAME "x64"
#elif defined(_M_IX86) || defined(__i386__)
    #define LIFE_ARCHITECTURE_X86 1
    #define LIFE_ARCHITECTURE_NAME "x86"
#elif defined(__aarch64__) || defined(_M_ARM64)
    #define LIFE_ARCHITECTURE_ARM64 1
    #define LIFE_ARCHITECTURE_NAME "ARM64"
#elif defined(__arm__) || defined(_M_ARM)
    #define LIFE_ARCHITECTURE_ARM32 1
    #define LIFE_ARCHITECTURE_NAME "ARM32"
#elif defined(__riscv)
    #define LIFE_ARCHITECTURE_RISC_V 1
    #define LIFE_ARCHITECTURE_NAME "RISC-V"
#else
    #define LIFE_ARCHITECTURE_UNKNOWN 1
    #define LIFE_ARCHITECTURE_NAME "Unknown"
#endif

#if defined(_DEBUG)
    #define LIFE_CONFIG_DEBUG 1
    #define LIFE_BUILD_TYPE "Debug"
#elif defined(NDEBUG)
    #define LIFE_CONFIG_RELEASE 1
    #define LIFE_BUILD_TYPE "Release"
#else
    #define LIFE_BUILD_TYPE "Unknown"
#endif
