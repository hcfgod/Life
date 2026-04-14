#pragma once

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

        bool HasSelection(const Life::SceneService& sceneService) const
        {
            return GetSelectedEntity(sceneService).IsValid();
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

        std::string SelectedEntityId;
        std::string SelectedProjectAssetRelativePath;
        std::string StatusMessage;
        bool StatusIsError = false;
    };
}
