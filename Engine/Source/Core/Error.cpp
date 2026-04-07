#include "Core/Error.h"
#include "Core/Log.h"
#include "Platform/PlatformDetection.h"
#include <sstream>
#include <chrono>
#include <thread>

namespace Life
{
    Error::Error(ErrorCode code, std::string message, const std::source_location& location, ErrorSeverity severity)
        : m_Code(code), m_Message(std::move(message)), m_Severity(severity), m_SystemErrorCode(0)
    {
        // Helper function to clean up function names
        auto CleanFunctionName = [](const std::string& funcName) -> std::string {
            std::string cleaned = funcName;
            
            // Remove __cdecl calling convention
            size_t pos = cleaned.find("__cdecl ");
            if (pos != std::string::npos) {
                cleaned.erase(pos, 8); // Remove "__cdecl "
            }
            
            // Remove __stdcall calling convention
            pos = cleaned.find("__stdcall ");
            if (pos != std::string::npos) {
                cleaned.erase(pos, 10); // Remove "__stdcall "
            }
            
            // Remove __fastcall calling convention
            pos = cleaned.find("__fastcall ");
            if (pos != std::string::npos) {
                cleaned.erase(pos, 11); // Remove "__fastcall "
            }
            
            return cleaned;
        };
        
        // Format location string
        std::ostringstream oss;
        oss << location.file_name() << ":" << location.line() << ":" << location.column() 
            << " in " << CleanFunctionName(location.function_name());
        m_Location = oss.str();

        // Build context information
        BuildContext();
    }

    Error::Error(const Error& other)
        : std::exception()
        , m_Code(other.m_Code)
        , m_Message(other.m_Message)
        , m_Location(other.m_Location)
        , m_Severity(other.m_Severity)
        , m_Context(other.m_Context)
        , m_SystemErrorCode(other.m_SystemErrorCode)
        , m_WhatBuffer(other.m_WhatBuffer)
    {
    }

    Error& Error::operator=(const Error& other)
    {
        if (this == &other)
            return *this;

        m_Code = other.m_Code;
        m_Message = other.m_Message;
        m_Location = other.m_Location;
        m_Severity = other.m_Severity;
        m_Context = other.m_Context;
        m_SystemErrorCode = other.m_SystemErrorCode;
        m_WhatBuffer = other.m_WhatBuffer;
        return *this;
    }

    const char* Error::what() const noexcept
    {
        try
        {
            if (m_WhatBuffer.empty())
            {
                m_WhatBuffer = ToString();
            }
            return m_WhatBuffer.c_str();
        }
        catch (...)
        {
            return "Life::Error";
        }
    }

    std::string Error::ToString() const
    {
        std::ostringstream oss;
        oss << "[" << GetSeverityString() << "] " << GetErrorCodeString() << ": " << m_Message;
        if (!m_Location.empty())
        {
            oss << " at " << m_Location;
        }
        return oss.str();
    }

    std::string Error::ToDetailedString() const
    {
        std::ostringstream oss;
        oss << "=== Error Details ===\n";
        oss << "Severity: " << GetSeverityString() << '\n';
        oss << "Code: " << GetErrorCodeString() << " (" << static_cast<int>(m_Code) << ")\n";
        oss << "Message: " << m_Message << '\n';
        oss << "Location: " << m_Location << '\n';

        if (m_SystemErrorCode != 0)
        {
            oss << "System Error: " << m_SystemErrorCode << " (" << ErrorHandling::GetSystemErrorString(m_SystemErrorCode) << ")\n";
        }

        if (!m_Context.functionName.empty())
        {
            oss << "Function: " << m_Context.functionName << '\n';
        }

        if (!m_Context.className.empty())
        {
            oss << "Class: " << m_Context.className << '\n';
        }

        if (!m_Context.moduleName.empty())
        {
            oss << "Module: " << m_Context.moduleName << '\n';
        }

        if (!m_Context.threadId.empty())
        {
            oss << "Thread: " << m_Context.threadId << '\n';
        }

        if (!m_Context.platformInfo.empty())
        {
            oss << "Platform: " << m_Context.platformInfo << '\n';
        }

        if (!m_Context.systemInfo.empty())
        {
            oss << "System: " << m_Context.systemInfo << '\n';
        }

        if (!m_Context.additionalData.empty())
        {
            oss << "Additional Data:\n";
            for (const auto& [key, value] : m_Context.additionalData)
            {
                oss << "  " << key << ": " << value << '\n';
            }
        }

        oss << "Timestamp: " << m_Context.timestamp << '\n';
        oss << "===================";

        return oss.str();
    }

    void Error::LogError(const Error& error)
    {
        switch (error.GetSeverity())
        {
            case ErrorSeverity::Info:
                LOG_CORE_INFO("{}", error.ToString());
                break;
            case ErrorSeverity::Warning:
                LOG_CORE_WARN("{}", error.ToString());
                break;
            case ErrorSeverity::Error:
                LOG_CORE_ERROR("{}", error.ToString());
                break;
            case ErrorSeverity::Critical:
                LOG_CORE_CRITICAL("{}", error.ToString());
                break;
            case ErrorSeverity::Fatal:
                LOG_CORE_CRITICAL("{}", error.ToString());
                break;
        }

        if (error.IsCritical())
        {
            LOG_CORE_CRITICAL("Detailed error information:\n{}", error.ToDetailedString());
        }
    }

    void Error::AddContext(const std::string& key, const std::string& value)
    {
        m_Context.additionalData[key] = value;
    }

    void Error::SetFunctionName(const std::string& functionName)
    {
        m_Context.functionName = functionName;
    }

    void Error::SetClassName(const std::string& className)
    {
        m_Context.className = className;
    }

    void Error::SetModuleName(const std::string& moduleName)
    {
        m_Context.moduleName = moduleName;
    }

    void Error::SetPlatformInfo(const PlatformInfo& platformInfo)
    {
        std::ostringstream oss;
        oss << platformInfo.platformName << " " << platformInfo.osVersion
            << " (" << platformInfo.architectureName << ")";
        m_Context.platformInfo = oss.str();
    }

    void Error::SetSystemErrorCode(int systemErrorCode)
    {
        m_SystemErrorCode = systemErrorCode;
    }

    std::string Error::GetContextValue(const std::string& key) const
    {
        auto it = m_Context.additionalData.find(key);
        return (it != m_Context.additionalData.end()) ? it->second : "";
    }

    void Error::BuildContext()
    {
        // Set timestamp
        auto now = std::chrono::system_clock::now();
        m_Context.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();

        // Set thread ID
        std::ostringstream oss;
        oss << std::this_thread::get_id();
        m_Context.threadId = oss.str();

        // Set platform info if available
        if (PlatformDetection::IsInitialized())
        {
            const auto& platformInfo = PlatformDetection::GetPlatformInfo();
            SetPlatformInfo(platformInfo);

            std::ostringstream sysInfo;
            sysInfo << "CPU Cores: " << platformInfo.capabilities.cpuCount
                    << ", Memory: " << (platformInfo.capabilities.totalMemory / (static_cast<uint64_t>(1024) * static_cast<uint64_t>(1024))) << " MB";
            m_Context.systemInfo = sysInfo.str();
        }
    }

    std::string Error::GetSeverityString() const
    {
        switch (m_Severity)
        {
            case ErrorSeverity::Info: return "INFO";
            case ErrorSeverity::Warning: return "WARNING";
            case ErrorSeverity::Error: return "ERROR";
            case ErrorSeverity::Critical: return "CRITICAL";
            case ErrorSeverity::Fatal: return "FATAL";
            default: return "UNKNOWN";
        }
    }

    std::string Error::GetErrorCodeString() const
    {
        return ErrorHandling::GetErrorCodeString(m_Code);
    }
}