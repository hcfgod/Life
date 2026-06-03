#pragma once

#include <filesystem>

namespace EditorApp::PathSafety
{
    [[nodiscard]] std::filesystem::path NormalizePath(const std::filesystem::path& path);
    [[nodiscard]] bool IsPathInside(const std::filesystem::path& root, const std::filesystem::path& candidate, bool allowRoot = true);
    [[nodiscard]] bool IsSafeProjectDeleteTarget(const std::filesystem::path& rootDirectory, const std::filesystem::path& descriptorPath);
    [[nodiscard]] bool IsSafeAssetDeleteTarget(const std::filesystem::path& assetsDirectory, const std::filesystem::path& targetPath);
}
