#pragma once

#include "Core/Error.h"

#include <nlohmann/json.hpp>

#include <cstdint>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // Texture import specification
    // Lightweight struct describing how a texture should be imported/cooked.
    // Lives in the asset layer (not Graphics) so the import pipeline can reference
    // it without pulling in GPU types.
    // -----------------------------------------------------------------------------
    enum class TextureFilter : uint8_t
    {
        Nearest = 0,
        Linear = 1,
        NearestMipmapNearest = 2,
        LinearMipmapLinear = 3
    };

    enum class TextureWrap : uint8_t
    {
        Repeat = 0,
        ClampToEdge = 1,
        MirroredRepeat = 2
    };

    struct TextureSpecification
    {
        TextureFilter MinFilter = TextureFilter::Linear;
        TextureFilter MagFilter = TextureFilter::Linear;
        TextureWrap WrapU = TextureWrap::Repeat;
        TextureWrap WrapV = TextureWrap::Repeat;
        bool GenerateMipmaps = true;
        bool FlipVerticallyOnLoad = false;
    };

    // Converts importer settings JSON -> TextureSpecification.
    // Missing fields fall back to defaults.
    TextureSpecification TextureSpecificationFromImporterSettingsJson(const nlohmann::json& j);

    // Returns true if `s` is exactly the default-constructed spec.
    bool IsDefaultTextureSpecification(const TextureSpecification& s);
}
