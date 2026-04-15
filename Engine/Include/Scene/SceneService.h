#pragma once

#include "Core/Error.h"
#include "Core/Memory.h"
#include "Scene/Scene.h"

#include <filesystem>
#include <string>

namespace Life
{
    namespace Assets
    {
        class AssetManager;
    }

    class SceneService final
    {
    public:
        SceneService() = default;
        ~SceneService() = default;

        SceneService(const SceneService&) = delete;
        SceneService& operator=(const SceneService&) = delete;

        void BindAssetManager(Assets::AssetManager& assetManager) noexcept;
        void UnbindAssetManager() noexcept;

        Scene& CreateScene(std::string name = "Untitled");
        Result<void> LoadScene(const std::filesystem::path& sourcePath);
        Scene& OpenScene(const std::filesystem::path& sourcePath);
        void SetActiveScene(Scope<Scene> scene);
        bool CloseScene() noexcept;

        Result<void> SaveActiveScene();
        Result<void> SaveActiveSceneAs(const std::filesystem::path& sourcePath);

        bool HasActiveScene() const noexcept;
        bool HasActiveSceneSourcePath() const noexcept;
        const std::filesystem::path& GetActiveSceneSourcePath() const;
        bool IsActiveSceneDirty() const noexcept;
        void MarkActiveSceneDirty() noexcept;
        void ClearDirty() noexcept;
        bool ActiveSceneHasCamera() const noexcept;
        bool EnsureActiveSceneHasCamera();
        Scene& GetActiveScene();
        const Scene& GetActiveScene() const;
        Scene* TryGetActiveScene() noexcept;
        const Scene* TryGetActiveScene() const noexcept;

    private:
        Result<void> OpenSceneInternal(const std::filesystem::path& sourcePath, bool allowBlankFallback);
        void ResolveSceneAssetReferences(Scene& scene) const;

        Scope<Scene> m_ActiveScene;
        Assets::AssetManager* m_AssetManager = nullptr;
        bool m_IsActiveSceneDirty = false;
    };
}
