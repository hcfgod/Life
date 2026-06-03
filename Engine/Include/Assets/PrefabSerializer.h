#pragma once

#include "Core/Error.h"
#include "Core/Memory.h"

#include <filesystem>

namespace Life
{
    class Entity;
    class Scene;
}

namespace Life::Assets
{
    class AssetManager;

    class PrefabSerializer final
    {
    public:
        static Result<void> SaveEntityAsPrefab(const Scene& sourceScene, Entity rootEntity, const std::filesystem::path& destinationPath);
        static Result<void> SaveSceneAsPrefab(const Scene& prefabScene, const std::filesystem::path& destinationPath);
        static Result<Scope<Scene>> Load(const std::filesystem::path& sourcePath, AssetManager* assetManager = nullptr);
    };
}
