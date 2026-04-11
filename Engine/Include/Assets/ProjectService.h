#pragma once

#include "Assets/Project.h"
#include "Core/Error.h"

namespace Life::Assets
{
    class AssetDatabase;
    class AssetManager;

    class ProjectService final
    {
    public:
        ProjectService() = default;
        ~ProjectService() = default;

        ProjectService(const ProjectService&) = delete;
        ProjectService& operator=(const ProjectService&) = delete;

        void BindAssetSystems(AssetDatabase& assetDatabase, AssetManager& assetManager) noexcept;
        void UnbindAssetSystems() noexcept;

        Result<Project> CreateProject(const ProjectCreateOptions& options, bool makeActive = true);
        Result<Project> OpenProject(const std::filesystem::path& descriptorPath);
        Result<void> SaveProject();
        Result<void> SaveProjectAs(const std::filesystem::path& descriptorPath);
        Result<void> CloseProject();

        bool HasActiveProject() const noexcept;
        const Project& GetActiveProject() const;
        Project& GetActiveProject();
        const Project* TryGetActiveProject() const noexcept;
        Project* TryGetActiveProject() noexcept;

    private:
        Result<void> SetActiveProject(Project project);
        Result<void> RebindProjectRoot(const Project* project);

        Project m_ActiveProject;
        bool m_HasActiveProject = false;
        AssetDatabase* m_AssetDatabase = nullptr;
        AssetManager* m_AssetManager = nullptr;
    };
}
