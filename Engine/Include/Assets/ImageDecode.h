#pragma once

#include "Core/Error.h"

#include <cstdint>
#include <string>
#include <vector>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // DecodedImageRGBA8
    // CPU-side decoded image in RGBA8 format.
    // -----------------------------------------------------------------------------
    struct DecodedImageRGBA8
    {
        uint32_t Width = 0;
        uint32_t Height = 0;
        std::vector<uint8_t> Pixels;
    };

    // Decode from disk file path to RGBA8.
    [[nodiscard]] Result<DecodedImageRGBA8> DecodeToRGBA8(const std::string& path, bool flipVerticallyOnLoad = false);

    // Decode from in-memory bytes to RGBA8.
    [[nodiscard]] Result<DecodedImageRGBA8> DecodeToRGBA8FromMemory(const uint8_t* bytes, size_t byteCount,
                                                                     const std::string& debugName,
                                                                     bool flipVerticallyOnLoad = false);

    // Minimal ASCII PPM (P3) decoder — fallback for stb_image.
    [[nodiscard]] Result<DecodedImageRGBA8> TryDecodePpmP3ToRGBA8(const std::string& path);
    [[nodiscard]] Result<DecodedImageRGBA8> TryDecodePpmP3ToRGBA8FromMemory(const uint8_t* bytes, size_t byteCount,
                                                                             const std::string& debugName);

    // Flip an RGBA8 image vertically in-place.
    void FlipVerticalRGBA8(DecodedImageRGBA8& img);
}
