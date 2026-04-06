#pragma once

#include "Core/Error.h"

#include <string>
#include <string_view>

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

    struct ParseCombinedGlslInput
    {
        std::string_view Key;
        std::string_view ResolvedPath;
        std::string_view FileText;
        std::string_view NameOverride;
    };

    // Parse a combined GLSL file with "#type vertex" and "#type fragment" markers.
    [[nodiscard]] Result<ParsedShaderStages> ParseCombinedGlsl(const ParseCombinedGlslInput& input);

    // Optionally validate/reflect stages for the active graphics API.
    [[nodiscard]] Result<ParsedShaderStages> PrepareShaderStagesForActiveGraphicsAPI(
        ParsedShaderStages parsed,
        const std::string& debugPath);
}
