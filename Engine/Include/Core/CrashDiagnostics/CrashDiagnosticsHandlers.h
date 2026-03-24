#pragma once

#include "Core/CrashDiagnostics/CrashDiagnosticsState.h"

namespace Life::CrashDiagnosticsDetail
{
    void ShutdownHandlersLocked(CrashDiagnosticsState& state);
    void InstallHandlersLocked(CrashDiagnosticsState& state, const CrashReportingSpecification& specification);
}
