#pragma once

#include "Editor/Shell/EditorShellTypes.h"
#include "Engine.h"

#include <filesystem>
#include <string>
#include <utility>

namespace EditorApp
{
    struct EditorSceneState
    {
        void SelectEntity(const Life::Entity& entity)
        {
            SelectedEntityId = entity.IsValid() ? entity.GetId() : std::string{};
            SelectedProjectAssetRelativePath.clear();
        }

        void SelectProjectAsset(const std::filesystem::path& relativePath)
        {
            SelectedEntityId.clear();
            SelectedProjectAssetRelativePath = relativePath.lexically_normal().generic_string();
        }

        void ClearSelection() noexcept
        {
            SelectedEntityId.clear();
            SelectedProjectAssetRelativePath.clear();
        }

        Life::Entity GetSelectedEntity(const Life::SceneService& sceneService) const
        {
            if (SelectedEntityId.empty() || !sceneService.HasActiveScene())
                return {};

            return sceneService.GetActiveScene().FindEntityById(SelectedEntityId);
        }

        Life::Entity GetSelectedEntity(Life::Scene& scene) const
        {
            if (SelectedEntityId.empty())
                return {};

            return scene.FindEntityById(SelectedEntityId);
        }

        Life::Entity GetSelectedEntity(const Life::Scene& scene) const
        {
            if (SelectedEntityId.empty())
                return {};

            return scene.FindEntityById(SelectedEntityId);
        }

        bool HasSelection(const Life::SceneService& sceneService) const
        {
            return GetSelectedEntity(sceneService).IsValid();
        }

        bool HasSelection(const Life::Scene& scene) const
        {
            return GetSelectedEntity(scene).IsValid();
        }

        std::filesystem::path GetSelectedProjectAssetRelativePath() const
        {
            return SelectedProjectAssetRelativePath.empty() ? std::filesystem::path{} : std::filesystem::path(SelectedProjectAssetRelativePath);
        }

        bool HasSelectedProjectAsset() const noexcept
        {
            return !SelectedProjectAssetRelativePath.empty();
        }

        void SetStatusMessage(std::string message, bool isError)
        {
            StatusMessage = std::move(message);
            StatusIsError = isError;
        }

        void ClearStatus() noexcept
        {
            StatusMessage.clear();
            StatusIsError = false;
        }

        bool IsRuntimeMode() const noexcept
        {
            return ExecutionMode != EditorSceneExecutionMode::Edit;
        }

        void ResetRuntimeState() noexcept
        {
            ExecutionMode = EditorSceneExecutionMode::Edit;
            Paused = false;
            StepSingleFrame = false;
            SupportsRuntimeTicks = false;
            RuntimeScene.reset();
        }

        Life::Scene* GetEffectiveScene(Life::SceneService& sceneService) noexcept
        {
            if (IsRuntimeMode() && RuntimeScene)
                return RuntimeScene.get();
            return sceneService.TryGetActiveScene();
        }

        const Life::Scene* GetEffectiveScene(const Life::SceneService& sceneService) const noexcept
        {
            if (IsRuntimeMode() && RuntimeScene)
                return RuntimeScene.get();
            return sceneService.TryGetActiveScene();
        }

        std::string SelectedEntityId;
        std::string SelectedProjectAssetRelativePath;
        std::string StatusMessage;
        bool StatusIsError = false;
        EditorSceneExecutionMode ExecutionMode = EditorSceneExecutionMode::Edit;
        bool Paused = false;
        bool StepSingleFrame = false;
        bool SupportsRuntimeTicks = false;
        Life::Scope<Life::Scene> RuntimeScene;
    };
}
