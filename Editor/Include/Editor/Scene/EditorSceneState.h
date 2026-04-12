#pragma once

#include "Engine.h"

#include <string>
#include <utility>

namespace EditorApp
{
    struct EditorSceneState
    {
        void SelectEntity(const Life::Entity& entity)
        {
            SelectedEntityId = entity.IsValid() ? entity.GetId() : std::string{};
        }

        void ClearSelection() noexcept
        {
            SelectedEntityId.clear();
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
        std::string StatusMessage;
        bool StatusIsError = false;
    };
}
