#include "Scene/SceneService.h"

#include "Assets/AssetManager.h"
#include "Assets/AssetPaths.h"
#include "Scene/SceneSerializer.h"

#include <stdexcept>
#include <utility>

namespace Life
{
    namespace
    {
        std::filesystem::path ResolveScenePath(const std::filesystem::path& sourcePath)
        {
            if (sourcePath.empty())
                return {};

            if (sourcePath.is_absolute())
            {
                std::error_code ec;
                return std::filesystem::absolute(sourcePath, ec).lexically_normal();
            }

            const auto resolvedAssetPath = Assets::ResolveAssetKeyToPath(sourcePath.generic_string());
            if (resolvedAssetPath.IsSuccess())
                return resolvedAssetPath.GetValue();

            if (const auto projectRoot = Assets::TryGetActiveProjectRootDirectory(); projectRoot.has_value())
                return (projectRoot.value() / sourcePath).lexically_normal();

            std::error_code ec;
            return std::filesystem::absolute(sourcePath, ec).lexically_normal();
        }
    }

    void SceneService::BindAssetManager(Assets::AssetManager& assetManager) noexcept
    {
        m_AssetManager = &assetManager;
        if (m_ActiveScene)
            ResolveSceneAssetReferences(*m_ActiveScene);
    }

    void SceneService::UnbindAssetManager() noexcept
    {
        m_AssetManager = nullptr;
    }

    Scene& SceneService::CreateScene(std::string name)
    {
        auto scene = CreateScope<Scene>(std::move(name));
        scene->SetState(Scene::State::Ready);
        m_ActiveScene = std::move(scene);
        m_IsActiveSceneDirty = false;
        return *m_ActiveScene;
    }

    Result<void> SceneService::LoadScene(const std::filesystem::path& sourcePath)
    {
        return OpenSceneInternal(sourcePath, false);
    }

    Scene& SceneService::OpenScene(const std::filesystem::path& sourcePath)
    {
        (void)OpenSceneInternal(sourcePath, true);
        return *m_ActiveScene;
    }

    void SceneService::SetActiveScene(Scope<Scene> scene)
    {
        m_ActiveScene = std::move(scene);
        if (m_ActiveScene)
        {
            ResolveSceneAssetReferences(*m_ActiveScene);
            m_IsActiveSceneDirty = false;
        }
        else
        {
            m_IsActiveSceneDirty = false;
        }
    }

    bool SceneService::CloseScene() noexcept
    {
        const bool hadScene = static_cast<bool>(m_ActiveScene);
        m_ActiveScene.reset();
        m_IsActiveSceneDirty = false;
        return hadScene;
    }

    Result<void> SceneService::SaveActiveScene()
    {
        if (!m_ActiveScene)
        {
            return Result<void>(ErrorCode::InvalidState,
                                "No active scene is available to save.");
        }

        if (m_ActiveScene->GetSourcePath().empty())
        {
            return Result<void>(ErrorCode::InvalidArgument,
                                "Active scene does not have a source path. Use SaveActiveSceneAs first.");
        }

        auto saveResult = SceneSerializer::Save(*m_ActiveScene, m_ActiveScene->GetSourcePath());
        if (saveResult.IsSuccess())
            m_IsActiveSceneDirty = false;
        return saveResult;
    }

    Result<void> SceneService::SaveActiveSceneAs(const std::filesystem::path& sourcePath)
    {
        if (!m_ActiveScene)
        {
            return Result<void>(ErrorCode::InvalidState,
                                "No active scene is available to save.");
        }

        const std::filesystem::path resolvedPath = ResolveScenePath(sourcePath);
        if (resolvedPath.empty())
        {
            return Result<void>(ErrorCode::InvalidArgument,
                                "Scene save path must not be empty.");
        }

        m_ActiveScene->SetSourcePath(resolvedPath);
        auto saveResult = SceneSerializer::Save(*m_ActiveScene, resolvedPath);
        if (saveResult.IsSuccess())
            m_IsActiveSceneDirty = false;
        return saveResult;
    }

    bool SceneService::HasActiveScene() const noexcept
    {
        return static_cast<bool>(m_ActiveScene);
    }

    bool SceneService::HasActiveSceneSourcePath() const noexcept
    {
        return m_ActiveScene != nullptr && m_ActiveScene->HasSourcePath();
    }

    const std::filesystem::path& SceneService::GetActiveSceneSourcePath() const
    {
        if (!m_ActiveScene)
            throw std::logic_error("No active scene is available.");

        return m_ActiveScene->GetSourcePath();
    }

    bool SceneService::IsActiveSceneDirty() const noexcept
    {
        return m_IsActiveSceneDirty;
    }

    void SceneService::MarkActiveSceneDirty() noexcept
    {
        if (m_ActiveScene)
            m_IsActiveSceneDirty = true;
    }

    void SceneService::ClearDirty() noexcept
    {
        m_IsActiveSceneDirty = false;
    }

    bool SceneService::ActiveSceneHasCamera() const noexcept
    {
        return m_ActiveScene != nullptr && m_ActiveScene->HasCamera();
    }

    Scene& SceneService::GetActiveScene()
    {
        if (!m_ActiveScene)
            throw std::logic_error("No active scene is available.");

        return *m_ActiveScene;
    }

    const Scene& SceneService::GetActiveScene() const
    {
        if (!m_ActiveScene)
            throw std::logic_error("No active scene is available.");

        return *m_ActiveScene;
    }

    Scene* SceneService::TryGetActiveScene() noexcept
    {
        return m_ActiveScene.get();
    }

    const Scene* SceneService::TryGetActiveScene() const noexcept
    {
        return m_ActiveScene.get();
    }

    Result<void> SceneService::OpenSceneInternal(const std::filesystem::path& sourcePath, bool allowBlankFallback)
    {
        const std::filesystem::path resolvedPath = ResolveScenePath(sourcePath);
        if (resolvedPath.empty())
        {
            if (allowBlankFallback)
            {
                auto scene = CreateScope<Scene>("Scene");
                scene->SetState(Scene::State::Ready);
                m_ActiveScene = std::move(scene);
                m_IsActiveSceneDirty = false;
                return Result<void>();
            }

            return Result<void>(ErrorCode::InvalidArgument,
                                "Scene path must not be empty.");
        }

        std::error_code ec;
        const bool sceneExists = std::filesystem::exists(resolvedPath, ec) && std::filesystem::is_regular_file(resolvedPath, ec);
        if (sceneExists)
        {
            auto loadResult = SceneSerializer::Load(resolvedPath, m_AssetManager);
            if (loadResult.IsFailure())
            {
                if (!allowBlankFallback)
                    return Result<void>(loadResult.GetError());
            }
            else
            {
                m_ActiveScene = std::move(loadResult.GetValue());
                m_IsActiveSceneDirty = false;
                return Result<void>();
            }
        }

        if (!allowBlankFallback)
        {
            return Result<void>(ErrorCode::FileNotFound,
                                "Scene file does not exist: " + resolvedPath.string());
        }

        auto scene = CreateScope<Scene>(resolvedPath.stem().string().empty() ? std::string("Scene") : resolvedPath.stem().string());
        scene->SetSourcePath(resolvedPath);
        scene->SetState(Scene::State::Ready);
        m_ActiveScene = std::move(scene);
        ResolveSceneAssetReferences(*m_ActiveScene);
        m_IsActiveSceneDirty = false;
        return Result<void>();
    }

    void SceneService::ResolveSceneAssetReferences(Scene& scene) const
    {
        if (m_AssetManager == nullptr)
            return;

        for (Entity entity : scene.GetEntities())
        {
            if (SpriteComponent* sprite = entity.TryGetComponent<SpriteComponent>())
            {
                if (!sprite->TextureAssetKey.empty() && !sprite->TextureAsset)
                    sprite->TextureAsset = m_AssetManager->GetOrLoad<Assets::TextureAsset>(sprite->TextureAssetKey);
            }
        }
    }
}
