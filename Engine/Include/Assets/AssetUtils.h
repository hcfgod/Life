#pragma once

#include "Core/Error.h"

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AssetUtils
    // GUID generation, .meta file management, and dependency persistence.
    // -----------------------------------------------------------------------------

    [[nodiscard]] std::string GenerateGuid();

    [[nodiscard]] Result<std::string> LoadOrCreateGuid(const std::string& assetPath,
                                                        const nlohmann::json& extraMeta = nlohmann::json::object());

    [[nodiscard]] Result<std::string> ForceRegenerateGuid(const std::string& assetPath,
                                                           const nlohmann::json& extraMeta = nlohmann::json::object());

    [[nodiscard]] Result<void> WriteImporterSettings(const std::string& assetPath,
                                                      const nlohmann::json& importerSettings);

    [[nodiscard]] Result<void> WriteDependencies(const std::string& assetPath,
                                                  const std::vector<std::string>& dependencies);
}
