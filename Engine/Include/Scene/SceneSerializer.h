#pragma once

#include "Core/Error.h"
#include "Core/Memory.h"

#include <filesystem>

namespace Life
{
    class Scene;

    namespace Assets
    {
        class AssetManager;
    }

    inline constexpr uint32_t SceneFileCurrentVersion = 1;

    class SceneSerializer final
    {
    public:
        static Result<Scope<Scene>> Load(const std::filesystem::path& sourcePath, Assets::AssetManager* assetManager = nullptr);
        static Result<void> Save(const Scene& scene, const std::filesystem::path& sourcePath);
    };
}
