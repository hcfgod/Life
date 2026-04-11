#pragma once

#include "Engine.h"

#include <filesystem>
#include <string>
#include <vector>

namespace EditorApp
{
    class EditorProjectHub
    {
    public:
        struct RecentProjectEntry
        {
            std::string Name;
            std::filesystem::path DescriptorPath;
            std::filesystem::path RootDirectory;
            bool Exists = false;
        };

        void Attach();
        void Detach();
        void RefreshRecentProjects();
        void SetStatusMessage(std::string message, bool isError);

        bool Render(Life::Assets::ProjectService& projectService);

    private:
        void LoadRecentProjects();
        void SaveRecentProjects() const;
        void RecordRecentProject(const Life::Assets::Project& project);
        void RemoveRecentProject(const std::filesystem::path& descriptorPath);
        void QueueDeleteProject(const RecentProjectEntry& project);
        bool TryCreateProject(Life::Assets::ProjectService& projectService);
        bool TryOpenProject(Life::Assets::ProjectService& projectService);
        bool TryDeleteQueuedProject(Life::Assets::ProjectService& projectService);
        void RenderHeader();
        void RenderCreateProjectCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace);
        void RenderOpenProjectCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace);
        void RenderRecentProjectsCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace);
        void RenderDeletePopup(Life::Assets::ProjectService& projectService);
        std::filesystem::path GetRecentProjectsFilePath() const;
        std::filesystem::path NormalizePath(const std::filesystem::path& path) const;
        std::filesystem::path ResolveDescriptorPath(const std::filesystem::path& inputPath) const;
        RecentProjectEntry BuildRecentProjectEntry(const std::filesystem::path& descriptorPath) const;

        std::vector<RecentProjectEntry> m_RecentProjects;
        std::string m_CreateProjectName;
        std::string m_CreateProjectRoot;
        std::string m_OpenProjectPath;
        std::string m_StatusMessage;
        bool m_StatusIsError = false;
        bool m_DeleteProjectFiles = true;
        bool m_OpenDeletePopup = false;
        RecentProjectEntry m_QueuedDeleteProject;
    };
}
