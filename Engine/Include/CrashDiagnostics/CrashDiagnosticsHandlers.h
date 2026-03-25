#pragma once

#include "CrashDiagnosticsState.h"

namespace Life::CrashDiagnosticsDetail
{
    void ShutdownHandlersLocked(CrashDiagnosticsState& state);
    void InstallHandlersLocked(CrashDiagnosticsState& state, const CrashReportingSpecification& specification);
}
