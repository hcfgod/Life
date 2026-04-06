#include "Assets/ShaderStageParsing.h"

#include "Core/Log.h"

namespace Life::Assets
{
    Result<ParsedShaderStages> ParseCombinedGlsl(const ParseCombinedGlslInput& input)
    {
        const std::string_view key = input.Key;
        const std::string_view resolvedPath = input.ResolvedPath;
        const std::string_view fileText = input.FileText;
        const std::string_view nameOverride = input.NameOverride;

        // Format:
        //   #type vertex
        //   ...
        //   #type fragment
        //   ...
        auto findStage = [&](const std::string& stage) -> size_t {
            const std::string tag = "#type " + stage;
            return fileText.find(tag);
        };

        const size_t vPos = findStage("vertex");
        const size_t fPos = findStage("fragment");
        if (vPos == std::string::npos || fPos == std::string::npos)
        {
            return Result<ParsedShaderStages>(ErrorCode::ResourceFormatNotSupported,
                "Shader file must contain '#type vertex' and '#type fragment': " + std::string(resolvedPath));
        }

        auto readStageBody = [&](const size_t tagPos, const size_t nextTagPos) -> std::string {
            const size_t lineEnd = fileText.find('\n', tagPos);
            const size_t bodyStart = (lineEnd == std::string::npos) ? tagPos : (lineEnd + 1);
            const size_t bodyEnd = (nextTagPos == std::string::npos) ? fileText.size() : nextTagPos;
            if (bodyStart >= bodyEnd)
            {
                return {};
            }
            return std::string(fileText.substr(bodyStart, bodyEnd - bodyStart));
        };

        ParsedShaderStages out;
        if (!nameOverride.empty())
        {
            out.Name.assign(nameOverride.begin(), nameOverride.end());
        }
        else
        {
            const auto slash = key.find_last_of("/\\");
            const std::string fileName = (slash == std::string::npos)
                ? std::string(key)
                : std::string(key.substr(slash + 1));
            const auto dot = fileName.find_last_of('.');
            out.Name = (dot == std::string::npos) ? fileName : fileName.substr(0, dot);
        }

        if (vPos < fPos)
        {
            out.Vertex = readStageBody(vPos, fPos);
            out.Fragment = readStageBody(fPos, std::string::npos);
        }
        else
        {
            out.Fragment = readStageBody(fPos, vPos);
            out.Vertex = readStageBody(vPos, std::string::npos);
        }

        if (out.Vertex.empty() || out.Fragment.empty())
        {
            return Result<ParsedShaderStages>(ErrorCode::ResourceCorrupted, "Shader stage source was empty: " + std::string(resolvedPath));
        }

        return out;
    }

    Result<ParsedShaderStages> PrepareShaderStagesForActiveGraphicsAPI(ParsedShaderStages parsed, const std::string& debugPath)
    {
        // Life currently uses pre-compiled SPIR-V via nvrhi/Vulkan.
        // Runtime GLSL-to-SPIRV compilation (shaderc) can be added here later.
        (void)debugPath;
        return parsed;
    }
}
