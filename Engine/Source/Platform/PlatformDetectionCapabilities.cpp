#include "Platform/PlatformDetection.h"

#ifdef LIFE_PLATFORM_WINDOWS
    #include <Windows.h>
    #include <intrin.h>
    #include <sysinfoapi.h>
#elif defined(LIFE_PLATFORM_MACOS)
    #include <mach/mach.h>
    #include <mach/mach_host.h>
    #include <sys/sysctl.h>
    #include <sys/types.h>
    #include <unistd.h>
#elif defined(LIFE_PLATFORM_LINUX)
    #include <cpuid.h>
    #include <sys/sysinfo.h>
    #include <unistd.h>
#endif

namespace Life
{
    void PlatformDetection::DetectCapabilities()
    {
        // CPU count
        #ifdef LIFE_PLATFORM_WINDOWS
            SYSTEM_INFO sysInfo;
            GetSystemInfo(&sysInfo);
            s_PlatformInfo.capabilities.cpuCount = sysInfo.dwNumberOfProcessors;
        #elif defined(LIFE_PLATFORM_MACOS)
            int cpuCount;
            size_t cpuSize = sizeof(cpuCount);
            if (sysctlbyname("hw.ncpu", &cpuCount, &cpuSize, nullptr, 0) == 0)
            {
                s_PlatformInfo.capabilities.cpuCount = static_cast<uint32_t>(cpuCount);
            }
        #elif defined(LIFE_PLATFORM_LINUX)
            s_PlatformInfo.capabilities.cpuCount = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));
        #endif

        // Memory information
        #ifdef LIFE_PLATFORM_WINDOWS
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            if (GlobalMemoryStatusEx(&memInfo))
            {
                s_PlatformInfo.capabilities.totalMemory = memInfo.ullTotalPhys;
                s_PlatformInfo.capabilities.availableMemory = memInfo.ullAvailPhys;
            }
        #elif defined(LIFE_PLATFORM_MACOS)
            int64_t totalMem;
            size_t memSize = sizeof(totalMem);
            if (sysctlbyname("hw.memsize", &totalMem, &memSize, nullptr, 0) == 0)
            {
                s_PlatformInfo.capabilities.totalMemory = static_cast<uint64_t>(totalMem);

                // Get available memory
                vm_size_t pageSize;
                host_page_size(mach_host_self(), &pageSize);

                vm_statistics64_data_t vmStats;
                mach_msg_type_number_t infoCount = sizeof(vmStats) / sizeof(natural_t);
                if (host_statistics64(mach_host_self(), HOST_VM_INFO64, (host_info64_t)&vmStats, &infoCount) == KERN_SUCCESS)
                {
                    uint64_t freeMem = static_cast<uint64_t>(vmStats.free_count) * pageSize;
                    s_PlatformInfo.capabilities.availableMemory = freeMem;
                }
            }
        #elif defined(LIFE_PLATFORM_LINUX)
            struct sysinfo si;
            if (sysinfo(&si) == 0)
            {
                s_PlatformInfo.capabilities.totalMemory = static_cast<uint64_t>(si.totalram) * si.mem_unit;
                s_PlatformInfo.capabilities.availableMemory = static_cast<uint64_t>(si.freeram) * si.mem_unit;
            }
        #endif

        // CPU instruction set detection
        #if defined(LIFE_ARCHITECTURE_X64) || defined(LIFE_ARCHITECTURE_X86)
            // Correct AVX-family detection requires BOTH:
            // - CPU support bits (CPUID)
            // - OS support for saving YMM/ZMM state (OSXSAVE + XGETBV(XCR0))
            //
            // References:
            // - AVX: CPUID.(EAX=1):ECX.AVX[bit 28] + OSXSAVE[bit 27] + XCR0[1:2] == 11b
            // - AVX2: CPUID.(EAX=7,ECX=0):EBX.AVX2[bit 5] + AVX usable
            // - AVX-512F: CPUID.(EAX=7,ECX=0):EBX.AVX512F[bit 16] + XCR0 enables Opmask/ZMM state
            #if defined(LIFE_PLATFORM_WINDOWS)
                int cpuInfo[4] = {};
                __cpuid(cpuInfo, 1);

                s_PlatformInfo.capabilities.hasSSE2 = (cpuInfo[3] & (1 << 26)) != 0;
                s_PlatformInfo.capabilities.hasSSE3 = (cpuInfo[2] & (1 << 0)) != 0;
                s_PlatformInfo.capabilities.hasSSE4_1 = (cpuInfo[2] & (1 << 19)) != 0;
                s_PlatformInfo.capabilities.hasSSE4_2 = (cpuInfo[2] & (1 << 20)) != 0;

                const bool cpuHasXSAVE   = (cpuInfo[2] & (1 << 26)) != 0;
                const bool cpuHasOSXSAVE = (cpuInfo[2] & (1 << 27)) != 0;
                const bool cpuHasAVX     = (cpuInfo[2] & (1 << 28)) != 0;

                uint64_t xcr0 = 0;
                if (cpuHasXSAVE && cpuHasOSXSAVE)
                {
                    xcr0 = _xgetbv(0);
                }

                // XCR0 bit 1 = XMM state, bit 2 = YMM state
                const bool osHasAVXState = (xcr0 & 0x6) == 0x6;
                s_PlatformInfo.capabilities.hasAVX = cpuHasAVX && cpuHasXSAVE && cpuHasOSXSAVE && osHasAVXState;

                // Leaf 7 feature bits
                __cpuidex(cpuInfo, 7, 0);
                const bool cpuHasAVX2    = (cpuInfo[1] & (1 << 5)) != 0;
                const bool cpuHasAVX512F = (cpuInfo[1] & (1 << 16)) != 0;

                s_PlatformInfo.capabilities.hasAVX2 = s_PlatformInfo.capabilities.hasAVX && cpuHasAVX2;

                // XCR0 bits required for AVX-512: XMM(1), YMM(2), Opmask(5), ZMM_hi256(6), Hi16_ZMM(7)
                const bool osHasAVX512State = (xcr0 & 0xE6) == 0xE6;
                s_PlatformInfo.capabilities.hasAVX512 = s_PlatformInfo.capabilities.hasAVX && cpuHasAVX512F && osHasAVX512State;

            #elif defined(LIFE_PLATFORM_LINUX)
                // Local helper: XGETBV is required to validate OS support for AVX/AVX-512 state.
                auto XGetBV = [](uint32_t index) -> uint64_t
                {
                    uint32_t eax = 0;
                    uint32_t edx = 0;
                    __asm__ volatile("xgetbv" : "=a"(eax), "=d"(edx) : "c"(index));
                    return (static_cast<uint64_t>(edx) << 32) | eax;
                };

                unsigned int eax = 0, ebx = 0, ecx = 0, edx = 0;
                if (__get_cpuid(1, &eax, &ebx, &ecx, &edx))
                {
                    s_PlatformInfo.capabilities.hasSSE2 = (edx & (1 << 26)) != 0;
                    s_PlatformInfo.capabilities.hasSSE3 = (ecx & (1 << 0)) != 0;
                    s_PlatformInfo.capabilities.hasSSE4_1 = (ecx & (1 << 19)) != 0;
                    s_PlatformInfo.capabilities.hasSSE4_2 = (ecx & (1 << 20)) != 0;

                    const bool cpuHasXSAVE   = (ecx & (1 << 26)) != 0;
                    const bool cpuHasOSXSAVE = (ecx & (1 << 27)) != 0;
                    const bool cpuHasAVX     = (ecx & (1 << 28)) != 0;

                    uint64_t xcr0 = 0;
                    if (cpuHasXSAVE && cpuHasOSXSAVE)
                    {
                        xcr0 = XGetBV(0);
                    }

                    const bool osHasAVXState = (xcr0 & 0x6) == 0x6;
                    s_PlatformInfo.capabilities.hasAVX = cpuHasAVX && cpuHasXSAVE && cpuHasOSXSAVE && osHasAVXState;

                    // CPUID leaf 7 is queried via __get_cpuid_count(7, 0, ...)
                    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx))
                    {
                        const bool cpuHasAVX2    = (ebx & (1 << 5)) != 0;
                        const bool cpuHasAVX512F = (ebx & (1 << 16)) != 0;

                        s_PlatformInfo.capabilities.hasAVX2 = s_PlatformInfo.capabilities.hasAVX && cpuHasAVX2;

                        const bool osHasAVX512State = (xcr0 & 0xE6) == 0xE6;
                        s_PlatformInfo.capabilities.hasAVX512 = s_PlatformInfo.capabilities.hasAVX && cpuHasAVX512F && osHasAVX512State;
                    }
                }
            #endif
        #endif // x86/x64

        // ARM-specific capabilities
        #if defined(LIFE_ARCHITECTURE_ARM64) || defined(LIFE_ARCHITECTURE_ARM32)
            s_PlatformInfo.capabilities.hasNEON = true;
        #endif

        // PowerPC capabilities (if applicable)
        #ifdef __powerpc__
            s_PlatformInfo.capabilities.hasAltiVec = true;
        #endif
    }

    void PlatformDetection::DetectGraphicsAPIs()
    {
        // This would typically involve checking for available graphics APIs
        // For now, we'll set basic defaults based on platform

        #ifdef LIFE_PLATFORM_WINDOWS
            s_PlatformInfo.capabilities.hasDirectX = true;
            s_PlatformInfo.capabilities.hasOpenGL = true;
            s_PlatformInfo.capabilities.hasVulkan = true;
        #elif defined(LIFE_PLATFORM_MACOS)
            s_PlatformInfo.capabilities.hasMetal = true;
            s_PlatformInfo.capabilities.hasOpenGL = true;
            s_PlatformInfo.capabilities.hasVulkan = false;
        #elif defined(LIFE_PLATFORM_LINUX)
            s_PlatformInfo.capabilities.hasOpenGL = true;
            s_PlatformInfo.capabilities.hasVulkan = true;
        #endif
    }
}
