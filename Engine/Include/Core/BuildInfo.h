#pragma once

#include <string>

namespace Life
{
    struct BuildInfo
    {
        std::string Version;
        std::string Commit;
        std::string BuildDate;
        std::string Platform;
        std::string Architecture;
        std::string Configuration;
        std::string Compiler;
    };

    const BuildInfo& GetBuildInfo();
    std::string FormatBuildVersionLine();
    std::string FormatBuildDiagnostics();
}
