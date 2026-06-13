#pragma once

#include "Editor/Shell/EditorShellTypes.h"
#include "Engine.h"

#include <filesystem>
#include <algorithm>
#include <string>
#include <utility>
#include <vector>

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
            SelectedEntityIds.clear();
            ActiveSelectedEntityId.clear();
            if (entity.IsValid())
            {
                SelectedEntityIds.push_back(entity.GetId());
                ActiveSelectedEntityId = entity.GetId();
            }
            SelectedProjectAssetRelativePath.clear();
        }

        void SetEntitySelection(std::vector<std::string> entityIds, std::string activeEntityId = {})
        {
            SelectedEntityIds.clear();
            for (std::string& entityId : entityIds)
            {
                if (entityId.empty())
                    continue;
                if (std::find(SelectedEntityIds.begin(), SelectedEntityIds.end(), entityId) == SelectedEntityIds.end())
                    SelectedEntityIds.push_back(std::move(entityId));
            }

            if (!activeEntityId.empty() &&
                std::find(SelectedEntityIds.begin(), SelectedEntityIds.end(), activeEntityId) != SelectedEntityIds.end())
            {
                ActiveSelectedEntityId = std::move(activeEntityId);
            }
            else
            {
                ActiveSelectedEntityId = SelectedEntityIds.empty() ? std::string{} : SelectedEntityIds.back();
            }
            SelectedProjectAssetRelativePath.clear();
        }

        void SetEntitySelection(const std::vector<Life::Entity>& entities, const Life::Entity& activeEntity = {})
        {
            std::vector<std::string> entityIds;
            entityIds.reserve(entities.size());
            for (const Life::Entity& entity : entities)
            {
                if (entity.IsValid())
                    entityIds.push_back(entity.GetId());
            }
            SetEntitySelection(std::move(entityIds), activeEntity.IsValid() ? activeEntity.GetId() : std::string{});
        }

        void AddEntityToSelection(const Life::Entity& entity)
        {
            if (!entity.IsValid())
                return;

            const std::string& entityId = entity.GetId();
            if (std::find(SelectedEntityIds.begin(), SelectedEntityIds.end(), entityId) == SelectedEntityIds.end())
                SelectedEntityIds.push_back(entityId);
            ActiveSelectedEntityId = entityId;
            SelectedProjectAssetRelativePath.clear();
        }

        void RemoveEntityFromSelection(const Life::Entity& entity)
        {
            if (!entity.IsValid())
                return;

            const std::string entityId = entity.GetId();
            SelectedEntityIds.erase(
                std::remove(SelectedEntityIds.begin(), SelectedEntityIds.end(), entityId),
                SelectedEntityIds.end());
            if (ActiveSelectedEntityId == entityId)
                ActiveSelectedEntityId = SelectedEntityIds.empty() ? std::string{} : SelectedEntityIds.back();
        }

        void ToggleEntitySelection(const Life::Entity& entity)
        {
            if (!entity.IsValid())
                return;

            if (IsEntitySelected(entity))
                RemoveEntityFromSelection(entity);
            else
                AddEntityToSelection(entity);
            SelectedProjectAssetRelativePath.clear();
        }

        bool IsEntitySelected(const Life::Entity& entity) const
        {
            if (!entity.IsValid())
                return false;

            const std::string& entityId = entity.GetId();
            return std::find(SelectedEntityIds.begin(), SelectedEntityIds.end(), entityId) != SelectedEntityIds.end();
        }

        void ClearEntitySelection() noexcept
        {
            SelectedEntityIds.clear();
            ActiveSelectedEntityId.clear();
        }

        void SelectProjectAsset(const std::filesystem::path& relativePath)
        {
            ClearEntitySelection();
            SelectedProjectAssetRelativePath = relativePath.lexically_normal().generic_string();
        }

        void ClearSelection() noexcept
        {
            ClearEntitySelection();
            SelectedProjectAssetRelativePath.clear();
        }

        Life::Entity GetSelectedEntity(const Life::SceneService& sceneService) const
        {
            if (ActiveSelectedEntityId.empty() || !sceneService.HasActiveScene())
                return {};

            return sceneService.GetActiveScene().FindEntityById(ActiveSelectedEntityId);
        }

        Life::Entity GetSelectedEntity(Life::Scene& scene) const
        {
            if (ActiveSelectedEntityId.empty())
                return {};

            return scene.FindEntityById(ActiveSelectedEntityId);
        }

        Life::Entity GetSelectedEntity(const Life::Scene& scene) const
        {
            if (ActiveSelectedEntityId.empty())
                return {};

            return scene.FindEntityById(ActiveSelectedEntityId);
        }

        Life::Entity GetActiveSelectedEntity(Life::Scene& scene) const
        {
            return GetSelectedEntity(scene);
        }

        Life::Entity GetActiveSelectedEntity(const Life::Scene& scene) const
        {
            return GetSelectedEntity(scene);
        }

        std::vector<Life::Entity> GetSelectedEntities(Life::Scene& scene) const
        {
            std::vector<Life::Entity> entities;
            entities.reserve(SelectedEntityIds.size());
            for (const std::string& entityId : SelectedEntityIds)
            {
                Life::Entity entity = scene.FindEntityById(entityId);
                if (entity.IsValid())
                    entities.push_back(entity);
            }
            return entities;
        }

        std::vector<Life::Entity> GetSelectedEntities(const Life::Scene& scene) const
        {
            std::vector<Life::Entity> entities;
            entities.reserve(SelectedEntityIds.size());
            for (const std::string& entityId : SelectedEntityIds)
            {
                Life::Entity entity = scene.FindEntityById(entityId);
                if (entity.IsValid())
                    entities.push_back(entity);
            }
            return entities;
        }

        void ValidateEntitySelection(const Life::Scene& scene)
        {
            std::vector<std::string> validIds;
            validIds.reserve(SelectedEntityIds.size());
            for (const std::string& entityId : SelectedEntityIds)
            {
                if (!entityId.empty() &&
                    scene.FindEntityById(entityId).IsValid() &&
                    std::find(validIds.begin(), validIds.end(), entityId) == validIds.end())
                {
                    validIds.push_back(entityId);
                }
            }

            SelectedEntityIds = std::move(validIds);
            if (ActiveSelectedEntityId.empty() ||
                std::find(SelectedEntityIds.begin(), SelectedEntityIds.end(), ActiveSelectedEntityId) == SelectedEntityIds.end())
            {
                ActiveSelectedEntityId = SelectedEntityIds.empty() ? std::string{} : SelectedEntityIds.back();
            }
        }

        bool HasSelection(const Life::SceneService& sceneService) const
        {
            return GetSelectedEntity(sceneService).IsValid();
        }

        bool HasSelection(const Life::Scene& scene) const
        {
            return GetSelectedEntity(scene).IsValid();
        }

        bool HasEntitySelection() const noexcept
        {
            return !SelectedEntityIds.empty();
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

        std::vector<std::string> SelectedEntityIds;
        std::string ActiveSelectedEntityId;
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
