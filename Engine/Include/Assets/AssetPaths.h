#pragma once

#include "Core/Error.h"

#include <filesystem>
#include <string>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetPaths
    // Resolves Unity-style asset keys (e.g. "Assets/Textures/player.png") to
    // absolute filesystem paths.
    //
    // Project root discovery:
    // 1. Explicit override via SetAssetRootDirectory()
    // 2. LIFE_ASSET_ROOT environment variable
    // 3. Walk upward from CWD looking for Project/Project.json marker
    // 4. Walk upward from CWD looking for any Assets/ directory
    // -----------------------------------------------------------------------------

    void SetAssetRootDirectory(const std::filesystem::path& rootDirectory);

    [[nodiscard]] Result<std::filesystem::path> FindProjectRootFromWorkingDirectory();

    [[nodiscard]] Result<std::filesystem::path> ResolveAssetKeyToPath(const std::string& assetKey);
}
