#include "TestSupport.h"
 
using namespace Life::Tests;
 
TEST_CASE("Platform detection initializes and exposes basic metadata")
{
    Life::Log::Init();
 
    Life::PlatformDetection::Initialize();
    CHECK(Life::PlatformDetection::IsInitialized());
 
    const Life::PlatformInfo& platformInfo = Life::PlatformDetection::GetPlatformInfo();
    CHECK_FALSE(platformInfo.platformName.empty());
    CHECK_FALSE(platformInfo.architectureName.empty());
    CHECK_FALSE(platformInfo.compilerName.empty());
    CHECK_FALSE(platformInfo.buildDate.empty());
    CHECK_FALSE(platformInfo.buildTime.empty());
    CHECK_FALSE(platformInfo.buildType.empty());
}
 
TEST_CASE("Platform utilities surface missing environment variables and dynamic library failures")
{
    const std::string missingEnvironmentVariable = "LIFE_TEST_MISSING_ENVIRONMENT_VARIABLE_4A91F4E4";
    CHECK_FALSE(Life::PlatformUtils::GetEnvironmentVariable(missingEnvironmentVariable).has_value());
 
#if defined(LIFE_PLATFORM_WINDOWS)
    const std::string validLibraryPath = "kernel32.dll";
    const std::string invalidLibraryPath = "LifeDefinitelyMissingLibrary_123456.dll";
#elif defined(LIFE_PLATFORM_MACOS)
    const std::string validLibraryPath = "/usr/lib/libSystem.B.dylib";
    const std::string invalidLibraryPath = "/usr/lib/libLifeDefinitelyMissingLibrary_123456.dylib";
#else
    const std::string validLibraryPath = "libc.so.6";
    const std::string invalidLibraryPath = "libLifeDefinitelyMissingLibrary_123456.so";
#endif
 
    void* library = Life::PlatformUtils::LoadLibrary(validLibraryPath);
    REQUIRE(library != nullptr);
    CHECK(Life::PlatformUtils::GetProcAddress(library, "LifeDefinitelyMissingSymbol_123456") == nullptr);
    Life::PlatformUtils::FreeLibrary(library);
 
    CHECK(Life::PlatformUtils::LoadLibrary(invalidLibraryPath) == nullptr);
}
 
TEST_CASE("Error handling captures engine error details in Result")
{
    Life::PlatformDetection::Initialize();
 
    Life::Result<int> result = Life::ErrorHandling::Try([]() -> int
    {
        LIFE_THROW_ERROR(Life::ErrorCode::InvalidState, "result failure");
        return 0;
    });
 
    REQUIRE(result.IsFailure());
    CHECK(result.GetError().GetCode() == Life::ErrorCode::InvalidState);
    CHECK(result.GetError().GetErrorMessage() == "result failure");
    CHECK_FALSE(result.GetError().GetContext().threadId.empty());
    CHECK(result.GetError().GetContext().timestamp > 0);
    CHECK(result.GetError().ToString().find("InvalidState") != std::string::npos);
}
 
TEST_CASE("Log configuration updates logger names and levels")
{
    const Life::LogSpecification originalSpecification = Life::Log::GetSpecification();
 
    Life::LogSpecification testSpecification = originalSpecification;
    testSpecification.CoreLoggerName = "TEST_CORE";
    testSpecification.ClientLoggerName = "TEST_CLIENT";
    testSpecification.CoreLevel = spdlog::level::warn;
    testSpecification.ClientLevel = spdlog::level::err;
    testSpecification.EnableFile = false;
 
    Life::Log::Configure(testSpecification);
 
    const std::shared_ptr<spdlog::logger> coreLogger = Life::Log::GetCoreLogger();
    const std::shared_ptr<spdlog::logger> clientLogger = Life::Log::GetClientLogger();
 
    REQUIRE(coreLogger != nullptr);
    REQUIRE(clientLogger != nullptr);
    CHECK(coreLogger->name() == testSpecification.CoreLoggerName);
    CHECK(clientLogger->name() == testSpecification.ClientLoggerName);
    CHECK(coreLogger->level() == testSpecification.CoreLevel);
    CHECK(clientLogger->level() == testSpecification.ClientLevel);
 
    Life::Log::Configure(originalSpecification);
}
 
TEST_CASE("Log configuration creates file sink directories")
{
    const Life::LogSpecification originalSpecification = Life::Log::GetSpecification();
    const std::filesystem::path logDirectory = std::filesystem::temp_directory_path() / "LifeTests" / "LoggingDirectoryCreation";
    const std::filesystem::path logFilePath = logDirectory / "life.log";
 
    std::filesystem::remove_all(logDirectory);
 
    Life::LogSpecification fileSpecification = originalSpecification;
    fileSpecification.CoreLoggerName = "TEST_FILE_CORE";
    fileSpecification.ClientLoggerName = "TEST_FILE_CLIENT";
    fileSpecification.EnableConsole = false;
    fileSpecification.EnableFile = true;
    fileSpecification.FilePath = logFilePath.string();
 
    Life::Log::Configure(fileSpecification);
 
    std::shared_ptr<spdlog::logger> coreLogger = Life::Log::GetCoreLogger();
    REQUIRE(coreLogger != nullptr);
    coreLogger->info("file sink initialization");
    coreLogger->flush();
 
    CHECK(std::filesystem::exists(logDirectory));
    CHECK(std::filesystem::exists(logFilePath));
 
    coreLogger.reset();
    Life::Log::Configure(originalSpecification);
    std::filesystem::remove_all(logDirectory);
}
 
TEST_CASE("Log configuration failure preserves the current logger state")
{
    const Life::LogSpecification originalSpecification = Life::Log::GetSpecification();
 
    Life::LogSpecification validSpecification = originalSpecification;
    validSpecification.CoreLoggerName = "TEST_STABLE_CORE";
    validSpecification.ClientLoggerName = "TEST_STABLE_CLIENT";
    validSpecification.EnableFile = false;
 
    Life::Log::Configure(validSpecification);
 
    Life::LogSpecification invalidSpecification = validSpecification;
    invalidSpecification.EnableFile = true;
    invalidSpecification.FilePath.clear();
 
    CHECK_THROWS_AS(Life::Log::Configure(invalidSpecification), std::runtime_error);
 
    const std::shared_ptr<spdlog::logger> coreLogger = Life::Log::GetCoreLogger();
    const std::shared_ptr<spdlog::logger> clientLogger = Life::Log::GetClientLogger();
    const Life::LogSpecification activeSpecification = Life::Log::GetSpecification();
 
    REQUIRE(coreLogger != nullptr);
    REQUIRE(clientLogger != nullptr);
    CHECK(coreLogger->name() == validSpecification.CoreLoggerName);
    CHECK(clientLogger->name() == validSpecification.ClientLoggerName);
    CHECK(activeSpecification.CoreLoggerName == validSpecification.CoreLoggerName);
    CHECK(activeSpecification.ClientLoggerName == validSpecification.ClientLoggerName);
    CHECK(activeSpecification.EnableFile == validSpecification.EnableFile);
 
    Life::Log::Configure(originalSpecification);
}
 
TEST_CASE("Crash diagnostics writes handled exception reports")
{
    const Life::CrashReportingSpecification originalSpecification = Life::CrashDiagnostics::GetSpecification();
    const bool wasInstalled = Life::CrashDiagnostics::IsInstalled();
    const std::filesystem::path reportDirectory = std::filesystem::temp_directory_path() / "LifeTests" / "CrashReports";
 
    std::error_code initialCleanupError;
    std::filesystem::remove_all(reportDirectory, initialCleanupError);
    REQUIRE(initialCleanupError.value() == 0);
    Life::CrashDiagnostics::Shutdown();
 
    Life::CrashReportingSpecification specification = originalSpecification;
    specification.Enabled = true;
    specification.InstallHandlers = false;
    specification.WriteReport = true;
    specification.WriteMiniDump = false;
    specification.ReportDirectory = reportDirectory.string();
    specification.MaxStackFrames = 16;
 
    Life::CrashDiagnostics::Configure(specification);
    Life::CrashDiagnostics::SetApplicationInfo("CrashTestApp", { "CrashTestApp", "--synthetic" });
 
    std::filesystem::path reportPath;
    try
    {
        throw std::runtime_error("synthetic crash for crash report test");
    }
    catch (const std::exception& exception)
    {
        reportPath = Life::CrashDiagnostics::ReportHandledException(exception, "EngineTests");
    }
 
    REQUIRE_FALSE(reportPath.empty());
    CHECK(std::filesystem::exists(reportPath));
 
    std::string reportText;
    {
        std::ifstream reportStream(reportPath);
        REQUIRE(reportStream.is_open());
        std::ostringstream reportContents;
        reportContents << reportStream.rdbuf();
        reportText = reportContents.str();
    }
 
    CHECK(reportPath.parent_path() == std::filesystem::absolute(reportDirectory));
    CHECK(reportText.find("CrashTestApp") != std::string::npos);
    CHECK(reportText.find("synthetic crash for crash report test") != std::string::npos);
    CHECK(reportText.find("EngineTests") != std::string::npos);
    CHECK(reportText.find("handled-exception") != std::string::npos);
 
    Life::CrashDiagnostics::Configure(originalSpecification);
    if (wasInstalled)
        Life::CrashDiagnostics::Install();
    else
        Life::CrashDiagnostics::Shutdown();
 
    std::error_code finalCleanupError;
    std::filesystem::remove_all(reportDirectory, finalCleanupError);
    CHECK(finalCleanupError.value() == 0);
}

TEST_CASE("Bootstrap exception reports preserve pre-host application info")
{
    Life::Log::Init();

    CrashDiagnosticsTestScope crashScope(std::filesystem::temp_directory_path() / "LifeTests" / "BootstrapFailureOrdering" / "PreHost");

    Life::CrashDiagnostics::SetApplicationInfo("Bootstrap Placeholder", { "bootstrap.exe", "--before-host" });

    try
    {
        throw std::runtime_error("synthetic bootstrap failure before host construction");
    }
    catch (const std::exception& exception)
    {
        CHECK(Life::HandleApplicationBootstrapException(exception) == 1);
    }

    const std::filesystem::path reportPath = Life::CrashDiagnostics::GetLastReportPath();
    REQUIRE_FALSE(reportPath.empty());
    const std::string reportText = ReadTextFile(reportPath);

    CHECK(reportPath.parent_path() == std::filesystem::absolute(crashScope.ReportDirectory));
    CHECK(reportText.find("Bootstrap Placeholder") != std::string::npos);
    CHECK(reportText.find("bootstrap.exe --before-host") != std::string::npos);
    CHECK(reportText.find("synthetic bootstrap failure before host construction") != std::string::npos);
    CHECK(reportText.find("RunApplicationMain") != std::string::npos);
}

TEST_CASE("Runtime exception reports use the runtime loop phase")
{
    Life::Log::Init();

    CrashDiagnosticsTestScope crashScope(std::filesystem::temp_directory_path() / "LifeTests" / "BootstrapFailureOrdering" / "RuntimeLoopFailure");
    Life::CrashDiagnostics::SetApplicationInfo("Runtime Loop App", { "runtime.exe", "--loop" });

    try
    {
        throw std::runtime_error("synthetic runtime loop failure");
    }
    catch (const std::exception& exception)
    {
        CHECK(Life::HandleApplicationRuntimeException(exception, "RunApplicationLoop") == 1);
    }

    const std::filesystem::path reportPath = Life::CrashDiagnostics::GetLastReportPath();
    REQUIRE_FALSE(reportPath.empty());
    const std::string reportText = ReadTextFile(reportPath);

    CHECK(reportPath.parent_path() == std::filesystem::absolute(crashScope.ReportDirectory));
    CHECK(reportText.find("Runtime Loop App") != std::string::npos);
    CHECK(reportText.find("runtime.exe --loop") != std::string::npos);
    CHECK(reportText.find("synthetic runtime loop failure") != std::string::npos);
    CHECK(reportText.find("RunApplicationLoop") != std::string::npos);
    CHECK(reportText.find("RunApplicationMain") == std::string::npos);
}

TEST_CASE("ApplicationHost startup failures update crash report application info before bootstrap handling")
{
    Life::Log::Init();

    CrashDiagnosticsTestScope crashScope(std::filesystem::temp_directory_path() / "LifeTests" / "BootstrapFailureOrdering" / "HostStartupFailure");
    Life::CrashDiagnostics::SetApplicationInfo("Bootstrap Placeholder", { "bootstrap.exe", "--before-host" });

    char executable[] = "HostFailureApp.exe";
    char option[] = "--from-host";
    char* commandLineArgs[] = { executable, option };
 
    Life::ApplicationSpecification specification;
    specification.Name = "Host Failure App";
    specification.Width = 800;
    specification.Height = 600;
    specification.CommandLineArgs = { 2, commandLineArgs };
    specification.CrashReporting.ReportDirectory = crashScope.ReportDirectory.string();
 
    try
    {
        auto host = Life::CreateScope<Life::ApplicationHost>(
            Life::CreateScope<ConfigurableTestApplication>(specification),
            Life::CreateScope<ThrowingRuntime>("synthetic window creation failure"));
        FAIL("Expected ApplicationHost construction to throw");
    }
    catch (const std::exception& exception)
    {
        CHECK(Life::HandleApplicationBootstrapException(exception) == 1);
    }
 
    const std::filesystem::path reportPath = Life::CrashDiagnostics::GetLastReportPath();
    REQUIRE_FALSE(reportPath.empty());
    const std::string reportText = ReadTextFile(reportPath);
 
    CHECK(reportPath.parent_path() == std::filesystem::absolute(crashScope.ReportDirectory));
    CHECK(reportText.find("Host Failure App") != std::string::npos);
    CHECK(reportText.find("HostFailureApp.exe --from-host") != std::string::npos);
    CHECK(reportText.find("synthetic window creation failure") != std::string::npos);
    CHECK(reportText.find("Bootstrap Placeholder") == std::string::npos);
}
 
TEST_CASE("ApplicationHost reapplies the application crash reporting specification even when it matches defaults")
{
    Life::Log::Init();
 
    CrashDiagnosticsConfigurationRestorer restoreCrashDiagnostics;
    Life::CrashDiagnostics::Shutdown();
 
    const std::filesystem::path overriddenReportDirectory = std::filesystem::temp_directory_path() / "LifeTests" / "BootstrapFailureOrdering" / "HostDefaultReset";
    Life::CrashReportingSpecification bootstrapOverride;
    bootstrapOverride.Enabled = false;
    bootstrapOverride.InstallHandlers = false;
    bootstrapOverride.CaptureSignals = false;
    bootstrapOverride.CaptureTerminate = false;
    bootstrapOverride.CaptureUnhandledExceptions = false;
    bootstrapOverride.WriteReport = false;
    bootstrapOverride.WriteMiniDump = false;
    bootstrapOverride.ReportDirectory = overriddenReportDirectory.string();
    bootstrapOverride.MaxStackFrames = 8;
    Life::CrashDiagnostics::Configure(bootstrapOverride);
    CHECK(CrashReportingSpecificationsEqual(Life::CrashDiagnostics::GetSpecification(), bootstrapOverride));
 
    char executable[] = "DefaultCrashPolicyApp.exe";
    char option[] = "--host-default";
    char* commandLineArgs[] = { executable, option };
 
    Life::ApplicationSpecification specification;
    specification.Name = "Default Crash Policy App";
    specification.Width = 800;
    specification.Height = 600;
    specification.Concurrency.JobWorkerCount = 1;
    specification.Concurrency.AsyncWorkerCount = 1;
    specification.CommandLineArgs = { 2, commandLineArgs };
    specification.CrashReporting = Life::CrashReportingSpecification{};
 
    auto host = Life::CreateScope<Life::ApplicationHost>(
        Life::CreateScope<ConfigurableTestApplication>(specification),
        Life::CreateScope<TestRuntime>());
 
    const Life::CrashReportingSpecification activeSpecification = Life::CrashDiagnostics::GetSpecification();
    CHECK(CrashReportingSpecificationsEqual(activeSpecification, specification.CrashReporting));
    CHECK_FALSE(CrashReportingSpecificationsEqual(activeSpecification, bootstrapOverride));
 
    host.reset();
    CHECK(CrashReportingSpecificationsEqual(Life::CrashDiagnostics::GetSpecification(), specification.CrashReporting));
}