#pragma once

#include "Editor/Scene/EditorSceneState.h"
#include "Engine.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace EditorApp
{
    struct EditorServices;

    class ProjectAssetsPanel
    {
    public:
        void Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState);
        void QueueExternalFileDrop(std::filesystem::path absolutePath, float x, float y);

    private:
        struct PendingExternalDrop
        {
            std::filesystem::path AbsolutePath;
            float X = 0.0f;
            float Y = 0.0f;
        };

        enum class PendingPopup
        {
            None,
            CreateFolder,
            CreateScene,
            Rename
        };

        std::filesystem::path m_ActiveFolderRelativePath;
        std::filesystem::path m_SelectedRelativePath;
        std::filesystem::path m_PopupTargetRelativePath;
        std::unordered_map<std::string, bool> m_ExpandedFolders;
        std::vector<PendingExternalDrop> m_PendingExternalDrops;
        std::string m_SearchFilter;
        std::string m_PopupName = "New Folder";
        float m_GridScale = 1.0f;
        PendingPopup m_PendingPopup = PendingPopup::None;
        bool m_OpenPendingPopup = false;
    };
}
