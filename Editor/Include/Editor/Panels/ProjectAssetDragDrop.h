#pragma once

#include <array>
#include <cstdint>
#include <string>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    enum class ProjectAssetPayloadKind : uint8_t
    {
        File,
        Directory,
        Scene,
        Prefab
    };

    struct ProjectAssetDragPayload
    {
        ProjectAssetPayloadKind Kind = ProjectAssetPayloadKind::File;
        std::array<char, 1024> RelativePath{};
    };

    inline constexpr const char* kProjectAssetDragPayloadType = "EditorProjectAsset";

#if __has_include(<imgui.h>)
    inline int g_ProjectAssetDropConsumedFrame = -1;
    inline ProjectAssetPayloadKind g_ProjectAssetDropConsumedKind = ProjectAssetPayloadKind::File;
    inline std::string g_ProjectAssetDropConsumedPath;

    inline bool TryConsumeProjectAssetDropDelivery(const ProjectAssetDragPayload& payload)
    {
        constexpr int kDuplicateDropCooldownFrames = 12;
        const int frame = ImGui::GetFrameCount();
        const std::string path(payload.RelativePath.data());
        if (g_ProjectAssetDropConsumedKind == payload.Kind &&
            g_ProjectAssetDropConsumedPath == path &&
            frame - g_ProjectAssetDropConsumedFrame <= kDuplicateDropCooldownFrames)
        {
            return false;
        }

        g_ProjectAssetDropConsumedFrame = frame;
        g_ProjectAssetDropConsumedKind = payload.Kind;
        g_ProjectAssetDropConsumedPath = path;
        return true;
    }

    inline bool TryConsumeProjectAssetDropDelivery()
    {
        const int frame = ImGui::GetFrameCount();
        if (g_ProjectAssetDropConsumedFrame == frame)
            return false;

        g_ProjectAssetDropConsumedFrame = frame;
        return true;
    }
#else
    inline bool TryConsumeProjectAssetDropDelivery(const ProjectAssetDragPayload&)
    {
        return true;
    }

    inline bool TryConsumeProjectAssetDropDelivery()
    {
        return true;
    }
#endif
}
