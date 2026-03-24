#include "Core/CrashDiagnostics/CrashDiagnosticsHandlers.h"
#include "Core/CrashDiagnostics/CrashDiagnosticsState.h"

namespace Life
{
    void CrashDiagnostics::Install()
    {
        CrashDiagnosticsDetail::CrashDiagnosticsState& state = CrashDiagnosticsDetail::GetState();
        std::scoped_lock lock(state.Mutex);
        if (state.Installed)
            return;

        CrashDiagnosticsDetail::InstallHandlersLocked(state, CrashDiagnosticsDetail::LoadConfigurationSnapshot()->Specification);
    }

    void CrashDiagnostics::Shutdown()
    {
        CrashDiagnosticsDetail::CrashDiagnosticsState& state = CrashDiagnosticsDetail::GetState();
        std::scoped_lock lock(state.Mutex);
        CrashDiagnosticsDetail::ShutdownHandlersLocked(state);
    }

    void CrashDiagnostics::Configure(const CrashReportingSpecification& specification)
    {
        CrashDiagnosticsDetail::CrashDiagnosticsState& state = CrashDiagnosticsDetail::GetState();
        std::scoped_lock lock(state.Mutex);

        const std::shared_ptr<CrashDiagnosticsDetail::CrashDiagnosticsConfigurationSnapshot> currentSnapshot = CrashDiagnosticsDetail::LoadConfigurationSnapshot();
        std::shared_ptr<CrashDiagnosticsDetail::CrashDiagnosticsConfigurationSnapshot> updatedSnapshot = std::make_shared<CrashDiagnosticsDetail::CrashDiagnosticsConfigurationSnapshot>(*currentSnapshot);
        updatedSnapshot->Specification = specification;
        CrashDiagnosticsDetail::StoreConfigurationSnapshot(std::move(updatedSnapshot));

        if (state.Installed)
        {
            CrashDiagnosticsDetail::ShutdownHandlersLocked(state);
            CrashDiagnosticsDetail::InstallHandlersLocked(state, specification);
        }
    }

    CrashReportingSpecification CrashDiagnostics::GetSpecification()
    {
        return CrashDiagnosticsDetail::LoadConfigurationSnapshot()->Specification;
    }

    bool CrashDiagnostics::IsInstalled()
    {
        std::scoped_lock lock(CrashDiagnosticsDetail::GetState().Mutex);
        return CrashDiagnosticsDetail::GetState().Installed;
    }

    void CrashDiagnostics::SetApplicationInfo(std::string applicationName, std::vector<std::string> commandLine)
    {
        CrashDiagnosticsDetail::CrashDiagnosticsState& state = CrashDiagnosticsDetail::GetState();
        std::scoped_lock lock(state.Mutex);

        const std::shared_ptr<CrashDiagnosticsDetail::CrashDiagnosticsConfigurationSnapshot> currentSnapshot = CrashDiagnosticsDetail::LoadConfigurationSnapshot();
        std::shared_ptr<CrashDiagnosticsDetail::CrashDiagnosticsConfigurationSnapshot> updatedSnapshot = std::make_shared<CrashDiagnosticsDetail::CrashDiagnosticsConfigurationSnapshot>(*currentSnapshot);
        updatedSnapshot->ApplicationName = applicationName.empty() ? "Life Application" : std::move(applicationName);
        updatedSnapshot->CommandLine = std::move(commandLine);
        CrashDiagnosticsDetail::StoreConfigurationSnapshot(std::move(updatedSnapshot));
    }

    std::filesystem::path CrashDiagnostics::GetLastReportPath()
    {
        std::scoped_lock lock(CrashDiagnosticsDetail::GetState().Mutex);
        return CrashDiagnosticsDetail::GetState().LastReportPath;
    }
}
