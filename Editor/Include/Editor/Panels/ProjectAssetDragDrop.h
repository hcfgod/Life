#pragma once

#include <array>
#include <cstdint>

namespace EditorApp
{
    enum class ProjectAssetPayloadKind : uint8_t
    {
        File,
        Directory,
        Scene
    };

    struct ProjectAssetDragPayload
    {
        ProjectAssetPayloadKind Kind = ProjectAssetPayloadKind::File;
        std::array<char, 1024> RelativePath{};
    };

    inline constexpr const char* kProjectAssetDragPayloadType = "EditorProjectAsset";
}
