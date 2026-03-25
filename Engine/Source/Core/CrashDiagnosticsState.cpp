#include "CrashDiagnostics/CrashDiagnosticsState.h"

namespace Life::CrashDiagnosticsDetail
{
    CrashDiagnosticsState& GetState()
    {
        static CrashDiagnosticsState state;
        return state;
    }

    std::shared_ptr<CrashDiagnosticsConfigurationSnapshot> LoadConfigurationSnapshot()
    {
        return GetState().Snapshot.Load();
    }

    void StoreConfigurationSnapshot(std::shared_ptr<CrashDiagnosticsConfigurationSnapshot> snapshot)
    {
        GetState().Snapshot.Store(std::move(snapshot));
    }

    void StoreLastReportPath(const std::filesystem::path& reportPath)
    {
        CrashDiagnosticsState& state = GetState();
        std::scoped_lock lock(state.Mutex);
        state.LastReportPath = reportPath;
    }
}
