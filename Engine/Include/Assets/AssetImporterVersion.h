#pragma once

#include "Assets/AssetTypes.h"

#include <cstdint>

namespace Life::Assets
{
    [[nodiscard]] uint32_t GetCurrentAssetImporterVersion(AssetType type);
}
