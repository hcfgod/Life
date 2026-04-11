#pragma once

#include "Assets/Project.h"
#include "Core/Error.h"

#include <filesystem>

namespace Life::Assets
{
    class ProjectSerializer final
    {
    public:
        static Result<Project> Load(const std::filesystem::path& descriptorPath);
        static Result<void> Save(const Project& project);
        static Result<Project> CreateInMemory(const ProjectCreateOptions& options);
        static Result<Project> CreateOnDisk(const ProjectCreateOptions& options);

    private:
        static Result<Project> CreateProjectFromOptions(const ProjectCreateOptions& options);
        static Result<void> ValidateDescriptor(const ProjectDescriptor& descriptor);
        static Result<void> ValidateProject(const Project& project);
        static Result<void> EnsureProjectDirectoriesExist(const Project& project);
    };
}
