#include "Editor/PathSafety.h"

#include <system_error>

namespace EditorApp::PathSafety
{
    namespace
    {
        bool StartsWithDotDot(const std::filesystem::path& path)
        {
            const std::string text = path.generic_string();
            return text == ".." || text.rfind("../", 0) == 0;
        }
    }

    std::filesystem::path NormalizePath(const std::filesystem::path& path)
    {
        if (path.empty())
            return {};

        std::error_code ec;
        std::filesystem::path normalized = std::filesystem::weakly_canonical(path, ec);
        if (!ec)
            return normalized.lexically_normal();

        ec.clear();
        normalized = std::filesystem::absolute(path, ec);
        if (!ec)
            return normalized.lexically_normal();

        return path.lexically_normal();
    }

    bool IsPathInside(const std::filesystem::path& root, const std::filesystem::path& candidate, bool allowRoot)
    {
        const std::filesystem::path normalizedRoot = NormalizePath(root);
        const std::filesystem::path normalizedCandidate = NormalizePath(candidate);
        if (normalizedRoot.empty() || normalizedCandidate.empty())
            return false;

        if (normalizedRoot == normalizedCandidate)
            return allowRoot;

        const std::filesystem::path relative = normalizedCandidate.lexically_relative(normalizedRoot);
        return !relative.empty() && !StartsWithDotDot(relative);
    }

    bool IsSafeProjectDeleteTarget(const std::filesystem::path& rootDirectory, const std::filesystem::path& descriptorPath)
    {
        const std::filesystem::path normalizedRoot = NormalizePath(rootDirectory);
        const std::filesystem::path normalizedDescriptor = NormalizePath(descriptorPath);
        if (normalizedRoot.empty() || normalizedDescriptor.empty())
            return false;

        if (normalizedDescriptor.extension() != ".lifeproject")
            return false;

        if (!IsPathInside(normalizedRoot, normalizedDescriptor, false))
            return false;

        std::error_code ec;
        return std::filesystem::exists(normalizedDescriptor, ec) &&
            std::filesystem::is_regular_file(normalizedDescriptor, ec);
    }

    bool IsSafeAssetDeleteTarget(const std::filesystem::path& assetsDirectory, const std::filesystem::path& targetPath)
    {
        std::error_code ec;
        const std::filesystem::path normalizedAssets = NormalizePath(assetsDirectory);
        const std::filesystem::path normalizedTarget = NormalizePath(targetPath);
        if (normalizedAssets.empty() || normalizedTarget.empty())
            return false;

        if (!std::filesystem::exists(normalizedAssets, ec) || !std::filesystem::is_directory(normalizedAssets, ec))
            return false;

        return IsPathInside(normalizedAssets, normalizedTarget, false);
    }
}
