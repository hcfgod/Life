#pragma once

#include "Assets/AssetBundle.h"
#include "Core/Error.h"

#include <filesystem>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetBundleBuilder
    // Creates runtime asset bundles from project assets.
    // Discovers all known asset types under Assets/, imports them into the
    // AssetDatabase, then packs them into a single data file + manifest.
    // -----------------------------------------------------------------------------
    class AssetBundleBuilder final
    {
    public:
        struct Settings
        {
            AssetBundleCompression Compression = AssetBundleCompression::None;
            int ZstdCompressionLevel = 3;
        };

        static Result<void> BuildProjectAssetBundle();
        static Result<void> BuildProjectAssetBundle(Settings settings);

        static Result<void> BuildAssetBundleToDirectory(const std::filesystem::path& outputDirectory);
        static Result<void> BuildAssetBundleToDirectory(const std::filesystem::path& outputDirectory, Settings settings);

    private:
        static Result<void> BuildAtOutputDirectory(const std::filesystem::path& outputDirectory, Settings settings);
    };
}
