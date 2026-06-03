#pragma once

#include "Editor/Shell/EditorShellTypes.h"
#include "Engine.h"

#include <filesystem>
#include <string>
#include <utility>

namespace EditorApp
{
    enum class EditorViewportTool
    {
        Select = 0,
        Translate,
        Rotate,
        Scale
    };

    enum class EditorTransformSpace
    {
        Local = 0,
        World
    };

    enum class EditorViewportGridMode
    {
        Auto = 0,
        WorldPlane,
        Screen
    };

    enum class EditorSceneViewMode
    {
        TwoD = 0,
        ThreeD
    };

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

        bool IsPrefabMode() const noexcept
        {
            return static_cast<bool>(PrefabScene);
        }

        void ResetRuntimeState() noexcept
        {
            ExecutionMode = EditorSceneExecutionMode::Edit;
            Paused = false;
            StepSingleFrame = false;
            SupportsRuntimeTicks = false;
            RuntimeScene.reset();
        }

        void ResetPrefabMode() noexcept
        {
            PrefabScene.reset();
            PrefabAssetKey.clear();
            PrefabAssetPath.clear();
            PrefabDisplayName.clear();
            PrefabDirty = false;
        }

        Life::Scene* GetEffectiveScene(Life::SceneService& sceneService) noexcept
        {
            if (IsPrefabMode())
                return PrefabScene.get();
            if (IsRuntimeMode() && RuntimeScene)
                return RuntimeScene.get();
            return sceneService.TryGetActiveScene();
        }

        const Life::Scene* GetEffectiveScene(const Life::SceneService& sceneService) const noexcept
        {
            if (IsPrefabMode())
                return PrefabScene.get();
            if (IsRuntimeMode() && RuntimeScene)
                return RuntimeScene.get();
            return sceneService.TryGetActiveScene();
        }

        Life::Scene* GetEditableScene(Life::SceneService& sceneService) noexcept
        {
            if (IsPrefabMode())
                return PrefabScene.get();
            if (IsRuntimeMode())
                return nullptr;
            return sceneService.TryGetActiveScene();
        }

        const Life::Scene* GetEditableScene(const Life::SceneService& sceneService) const noexcept
        {
            if (IsPrefabMode())
                return PrefabScene.get();
            if (IsRuntimeMode())
                return nullptr;
            return sceneService.TryGetActiveScene();
        }

        void MarkEditableDocumentDirty(Life::SceneService& sceneService) noexcept
        {
            if (IsPrefabMode())
            {
                PrefabDirty = true;
                return;
            }

            if (!IsRuntimeMode())
                sceneService.MarkActiveSceneDirty();
        }

        std::string SelectedEntityId;
        std::string SelectedProjectAssetRelativePath;
        std::string StatusMessage;
        bool StatusIsError = false;
        EditorSceneExecutionMode ExecutionMode = EditorSceneExecutionMode::Edit;
        bool Paused = false;
        bool StepSingleFrame = false;
        bool SupportsRuntimeTicks = false;
        EditorSceneViewMode SceneViewMode = EditorSceneViewMode::TwoD;
        EditorViewportTool ViewportTool = EditorViewportTool::Translate;
        EditorTransformSpace TransformSpace = EditorTransformSpace::Local;
        bool SnapEnabled = false;
        float TranslationSnap = 0.25f;
        float RotationSnapDegrees = 15.0f;
        float ScaleSnap = 0.1f;
        bool ShowGrid = true;
        bool SnapToGrid = false;
        EditorViewportGridMode GridMode = EditorViewportGridMode::Auto;
        float GridSize = 1.0f;
        Life::Scope<Life::Scene> RuntimeScene;
        Life::Scope<Life::Scene> PrefabScene;
        std::string PrefabAssetKey;
        std::filesystem::path PrefabAssetPath;
        std::string PrefabDisplayName;
        bool PrefabDirty = false;
        std::string RequestedOpenPrefabAssetKey;
    };
}
