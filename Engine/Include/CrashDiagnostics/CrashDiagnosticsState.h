#pragma once

#include "Core/CrashDiagnostics.h"

#include <atomic>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if defined(LIFE_PLATFORM_WINDOWS)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#elif defined(LIFE_PLATFORM_LINUX) || defined(LIFE_PLATFORM_MACOS)
    #include <csignal>
#endif

namespace Life::CrashDiagnosticsDetail
{
    template<typename T>
    class SharedPtrStorage final
    {
    public:
        SharedPtrStorage() = default;

        explicit SharedPtrStorage(std::shared_ptr<T> value)
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
            : m_Value(std::move(value))
#else
            : m_Value(std::move(value))
#endif
        {
        }

        std::shared_ptr<T> Load() const
        {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
            return m_Value.load(std::memory_order_acquire);
#else
            return std::atomic_load_explicit(&m_Value, std::memory_order_acquire);
#endif
        }

        void Store(std::shared_ptr<T> value)
        {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
            m_Value.store(std::move(value), std::memory_order_release);
#else
            std::atomic_store_explicit(&m_Value, std::move(value), std::memory_order_release);
#endif
        }

    private:
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        std::atomic<std::shared_ptr<T>> m_Value;
#else
        std::shared_ptr<T> m_Value;
#endif
    };

    struct CrashDiagnosticsConfigurationSnapshot
    {
        CrashReportingSpecification Specification;
        std::string ApplicationName = "Life Application";
        std::vector<std::string> CommandLine;
    };

    struct CrashDiagnosticsEvent
    {
        std::string Category;
        std::string Reason;
        std::string Phase;
        std::string Details;
        std::vector<std::string> StackTrace;
        int SignalNumber = 0;
        std::uintptr_t FaultAddress = 0;
        std::uint32_t WindowsExceptionCode = 0;
    };

    struct CrashDiagnosticsState
    {
        std::mutex Mutex;
        SharedPtrStorage<CrashDiagnosticsConfigurationSnapshot> Snapshot{ std::make_shared<CrashDiagnosticsConfigurationSnapshot>() };
        std::filesystem::path LastReportPath;
        bool Installed = false;
        std::terminate_handler PreviousTerminateHandler = nullptr;
        std::atomic<bool> HandlingCrash = false;
#if defined(LIFE_PLATFORM_WINDOWS)
        LPTOP_LEVEL_EXCEPTION_FILTER PreviousUnhandledExceptionFilter = nullptr;
#elif defined(LIFE_PLATFORM_LINUX) || defined(LIFE_PLATFORM_MACOS)
        struct SignalRegistration
        {
            int SignalNumber = 0;
            struct sigaction PreviousAction{};
        };

        std::vector<SignalRegistration> SignalRegistrations;
#endif
    };

    CrashDiagnosticsState& GetState();
    std::shared_ptr<CrashDiagnosticsConfigurationSnapshot> LoadConfigurationSnapshot();
    void StoreConfigurationSnapshot(std::shared_ptr<CrashDiagnosticsConfigurationSnapshot> snapshot);
    void StoreLastReportPath(const std::filesystem::path& reportPath);
}
