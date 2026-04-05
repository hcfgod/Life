#pragma once

#include "Core/Error.h"

#include <string>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // ParsedShaderStages
    // Result of parsing a combined GLSL file into vertex and fragment stages.
    // -----------------------------------------------------------------------------
    struct ParsedShaderStages
    {
        std::string Name;
        std::string Vertex;
        std::string Fragment;
    };

    // Parse a combined GLSL file with "#type vertex" and "#type fragment" markers.
    [[nodiscard]] Result<ParsedShaderStages> ParseCombinedGlsl(
        const std::string& key,
        const std::string& resolvedPath,
        const std::string& fileText,
        const std::string& nameOverride = {});

    // Optionally validate/reflect stages for the active graphics API.
    [[nodiscard]] Result<ParsedShaderStages> PrepareShaderStagesForActiveGraphicsAPI(
        ParsedShaderStages parsed,
        const std::string& debugPath);
}
