#pragma once

#include "Assets/TextureSpecificationJson.h"
#include "Core/Error.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace Life::Assets::Cooking
{
    struct CookedTexture2DMipLevelView
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        const uint8_t* PixelsRGBA8 = nullptr;
        uint32_t SizeBytes = 0;
    };

    struct CookedTexture2DView
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        TextureSpecification Specification{};
        std::vector<CookedTexture2DMipLevelView> MipLevels;
    };

    Result<std::vector<uint8_t>> CookTexture2DFromRGBA8(
        uint32_t width,
        uint32_t height,
        const uint8_t* rgbaPixels,
        const TextureSpecification& specification);

    Result<CookedTexture2DView> ParseCookedTexture2DView(const uint8_t* bytes, size_t byteCount);
}
