#include "Assets/ProjectService.h"

#include "Assets/AssetDatabase.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPaths.h"
#include "Assets/ProjectSerializer.h"

#include <stdexcept>
#include <utility>

namespace Life::Assets
{
    void ProjectService::BindAssetSystems(AssetDatabase& assetDatabase, AssetManager& assetManager) noexcept
    {
        m_AssetDatabase = &assetDatabase;
        m_AssetManager = &assetManager;
    }

    void ProjectService::UnbindAssetSystems() noexcept
    {
        m_AssetDatabase = nullptr;
        m_AssetManager = nullptr;
    }

    Result<Project> ProjectService::CreateProject(const ProjectCreateOptions& options, bool makeActive)
    {
        auto projectResult = ProjectSerializer::CreateOnDisk(options);
        if (projectResult.IsFailure())
            return projectResult;

        Project project = projectResult.GetValue();
        if (makeActive)
        {
            auto activateResult = SetActiveProject(project);
            if (activateResult.IsFailure())
                return Result<Project>(activateResult.GetError());
        }

        return project;
    }

    Result<Project> ProjectService::OpenProject(const std::filesystem::path& descriptorPath)
    {
        auto projectResult = ProjectSerializer::Load(descriptorPath);
        if (projectResult.IsFailure())
            return projectResult;

        Project project = projectResult.GetValue();
        auto activateResult = SetActiveProject(project);
        if (activateResult.IsFailure())
            return Result<Project>(activateResult.GetError());

        return project;
    }

    Result<void> ProjectService::SaveProject()
    {
        if (!m_HasActiveProject)
        {
            return Result<void>(ErrorCode::InvalidState,
                                "ProjectService::SaveProject requires an active project");
        }

        return ProjectSerializer::Save(m_ActiveProject);
    }

    Result<void> ProjectService::SaveProjectAs(const std::filesystem::path& descriptorPath)
    {
        if (!m_HasActiveProject)
        {
            return Result<void>(ErrorCode::InvalidState,
                                "ProjectService::SaveProjectAs requires an active project");
        }

        if (descriptorPath.empty())
        {
            return Result<void>(ErrorCode::InvalidArgument,
                                "ProjectService::SaveProjectAs requires a descriptor path");
        }

        Project updatedProject = m_ActiveProject;
        updatedProject.Paths.DescriptorPath = std::filesystem::absolute(descriptorPath).lexically_normal();
        updatedProject.Paths.RootDirectory = updatedProject.Paths.DescriptorPath.parent_path();
        updatedProject.Paths.AssetsDirectory = updatedProject.Paths.RootDirectory / updatedProject.Descriptor.Paths.Assets;
        updatedProject.Paths.SettingsDirectory = updatedProject.Paths.RootDirectory / updatedProject.Descriptor.Paths.Settings;

        auto saveResult = ProjectSerializer::Save(updatedProject);
        if (saveResult.IsFailure())
            return saveResult;

        return SetActiveProject(std::move(updatedProject));
    }

    Result<void> ProjectService::CloseProject()
    {
        if (!m_HasActiveProject)
        {
            auto rebindResult = RebindProjectRoot(nullptr);
            if (rebindResult.IsFailure())
                return rebindResult;
            return Result<void>();
        }

        auto rebindResult = RebindProjectRoot(nullptr);
        if (rebindResult.IsFailure())
            return rebindResult;

        m_ActiveProject = {};
        m_HasActiveProject = false;
        return Result<void>();
    }

    bool ProjectService::HasActiveProject() const noexcept
    {
        return m_HasActiveProject;
    }

    const Project& ProjectService::GetActiveProject() const
    {
        if (!m_HasActiveProject)
            throw std::logic_error("ProjectService does not have an active project.");

        return m_ActiveProject;
    }

    Project& ProjectService::GetActiveProject()
    {
        if (!m_HasActiveProject)
            throw std::logic_error("ProjectService does not have an active project.");

        return m_ActiveProject;
    }

    const Project* ProjectService::TryGetActiveProject() const noexcept
    {
        return m_HasActiveProject ? &m_ActiveProject : nullptr;
    }

    Project* ProjectService::TryGetActiveProject() noexcept
    {
        return m_HasActiveProject ? &m_ActiveProject : nullptr;
    }

    Result<void> ProjectService::SetActiveProject(Project project)
    {
        auto rebindResult = RebindProjectRoot(&project);
        if (rebindResult.IsFailure())
            return rebindResult;

        m_ActiveProject = std::move(project);
        m_HasActiveProject = true;
        return Result<void>();
    }

    Result<void> ProjectService::RebindProjectRoot(const Project* project)
    {
        if (project != nullptr)
            SetActiveProjectRootDirectory(project->Paths.RootDirectory);
        else
            ClearActiveProjectRootDirectory();

        if (m_AssetDatabase != nullptr)
            m_AssetDatabase->Reset();

        if (m_AssetManager != nullptr)
            m_AssetManager->ClearCaches();

        return Result<void>();
    }
}
