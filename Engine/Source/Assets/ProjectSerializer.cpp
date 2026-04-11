#include "Assets/ProjectSerializer.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <system_error>

namespace Life::Assets
{
    namespace
    {
        constexpr std::string_view kPathsField = "paths";
        constexpr std::string_view kStartupField = "startup";

        std::string Trim(std::string value)
        {
            const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
                return std::isspace(character) != 0;
            });
            const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
                return std::isspace(character) != 0;
            }).base();

            if (begin >= end)
                return {};

            return std::string(begin, end);
        }

        std::string SanitizeFileStem(std::string value)
        {
            value = Trim(std::move(value));
            if (value.empty())
                return "Project";

            constexpr std::array<char, 9> invalidCharacters{ '<', '>', ':', '"', '/', '\\', '|', '?', '*' };
            for (char& character : value)
            {
                if (std::find(invalidCharacters.begin(), invalidCharacters.end(), character) != invalidCharacters.end())
                    character = '_';
            }

            return value;
        }

        Result<std::string> ResolveDescriptorFileName(const ProjectCreateOptions& options)
        {
            std::filesystem::path descriptorFileName = options.DescriptorFileName;
            if (descriptorFileName.empty())
            {
                descriptorFileName = SanitizeFileStem(options.Name);
            }

            if (descriptorFileName.has_parent_path())
            {
                return Result<std::string>(ErrorCode::InvalidArgument,
                                           "Project descriptor file name must not contain directory segments");
            }

            if (!descriptorFileName.has_extension())
                descriptorFileName += std::string(ProjectDescriptorFileExtension);

            if (descriptorFileName.extension() != std::filesystem::path(ProjectDescriptorFileExtension))
            {
                return Result<std::string>(ErrorCode::InvalidArgument,
                                           "Project descriptor file name must use the .lifeproject extension");
            }

            const std::string resolved = descriptorFileName.string();
            if (resolved.empty())
            {
                return Result<std::string>(ErrorCode::InvalidArgument,
                                           "Project descriptor file name resolved to an empty value");
            }

            return resolved;
        }

        std::filesystem::path NormalizeAbsolutePath(const std::filesystem::path& path)
        {
            if (path.empty())
                return {};

            std::error_code ec;
            std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
            if (ec)
                return path.lexically_normal();

            return absolutePath.lexically_normal();
        }

        nlohmann::json ToJson(const ProjectDescriptor& descriptor)
        {
            nlohmann::json root;
            root["version"] = descriptor.Version;
            root["name"] = descriptor.Name;
            root["engineVersion"] = descriptor.EngineVersion;
            root[std::string(kPathsField)] = {
                { "assets", descriptor.Paths.Assets },
                { "settings", descriptor.Paths.Settings }
            };
            root[std::string(kStartupField)] = {
                { "scene", descriptor.Startup.Scene }
            };
            return root;
        }

        Result<ProjectDescriptor> DescriptorFromJson(const nlohmann::json& root)
        {
            if (!root.is_object())
            {
                return Result<ProjectDescriptor>(ErrorCode::FileCorrupted,
                                                 "Project descriptor root must be a JSON object");
            }

            ProjectDescriptor descriptor;
            if (root.contains("version") && root["version"].is_number_unsigned())
                descriptor.Version = root["version"].get<uint32_t>();

            if (root.contains("name") && root["name"].is_string())
                descriptor.Name = root["name"].get<std::string>();

            if (root.contains("engineVersion") && root["engineVersion"].is_string())
                descriptor.EngineVersion = root["engineVersion"].get<std::string>();

            if (root.contains(std::string(kPathsField)) && root[std::string(kPathsField)].is_object())
            {
                const nlohmann::json& paths = root[std::string(kPathsField)];
                if (paths.contains("assets") && paths["assets"].is_string())
                    descriptor.Paths.Assets = paths["assets"].get<std::string>();
                if (paths.contains("settings") && paths["settings"].is_string())
                    descriptor.Paths.Settings = paths["settings"].get<std::string>();
            }

            if (root.contains(std::string(kStartupField)) && root[std::string(kStartupField)].is_object())
            {
                const nlohmann::json& startup = root[std::string(kStartupField)];
                if (startup.contains("scene") && startup["scene"].is_string())
                    descriptor.Startup.Scene = startup["scene"].get<std::string>();
            }

            return descriptor;
        }
    }

    Result<Project> ProjectSerializer::Load(const std::filesystem::path& descriptorPath)
    {
        if (descriptorPath.empty())
        {
            return Result<Project>(ErrorCode::InvalidArgument,
                                   "ProjectSerializer::Load requires a descriptor path");
        }

        const std::filesystem::path normalizedDescriptorPath = NormalizeAbsolutePath(descriptorPath);
        std::ifstream stream(normalizedDescriptorPath, std::ios::in | std::ios::binary);
        if (!stream.is_open())
        {
            return Result<Project>(ErrorCode::FileNotFound,
                                   "Failed to open project descriptor: " + normalizedDescriptorPath.string());
        }

        try
        {
            nlohmann::json root;
            stream >> root;

            const auto descriptorResult = DescriptorFromJson(root);
            if (descriptorResult.IsFailure())
                return Result<Project>(descriptorResult.GetError());

            Project project;
            project.Descriptor = descriptorResult.GetValue();
            project.Paths.RootDirectory = normalizedDescriptorPath.parent_path();
            project.Paths.DescriptorPath = normalizedDescriptorPath;
            project.Paths.AssetsDirectory = project.Paths.RootDirectory / project.Descriptor.Paths.Assets;
            project.Paths.SettingsDirectory = project.Paths.RootDirectory / project.Descriptor.Paths.Settings;

            const auto validateResult = ValidateProject(project);
            if (validateResult.IsFailure())
                return Result<Project>(validateResult.GetError());

            return project;
        }
        catch (const std::exception& exception)
        {
            return Result<Project>(ErrorCode::FileCorrupted,
                                   std::string("Failed to parse project descriptor JSON: ") + exception.what());
        }
    }

    Result<void> ProjectSerializer::Save(const Project& project)
    {
        const auto validateResult = ValidateProject(project);
        if (validateResult.IsFailure())
            return validateResult;

        const auto ensureDirectoriesResult = EnsureProjectDirectoriesExist(project);
        if (ensureDirectoriesResult.IsFailure())
            return ensureDirectoriesResult;

        try
        {
            const nlohmann::json root = ToJson(project.Descriptor);
            const std::filesystem::path descriptorPath = project.Paths.DescriptorPath;
            const std::filesystem::path tempPath = descriptorPath.string() + ".tmp";

            {
                std::ofstream stream(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
                if (!stream.is_open())
                {
                    return Result<void>(ErrorCode::FileAccessDenied,
                                        "Failed to open temporary project descriptor for writing: " + tempPath.string());
                }

                stream << root.dump(4);
                stream.flush();
                if (!stream.good())
                {
                    return Result<void>(ErrorCode::FileAccessDenied,
                                        "Failed to flush project descriptor to disk: " + tempPath.string());
                }
            }

            std::error_code ec;
            std::filesystem::rename(tempPath, descriptorPath, ec);
            if (ec)
            {
                ec.clear();
                std::filesystem::remove(descriptorPath, ec);
                ec.clear();
                std::filesystem::rename(tempPath, descriptorPath, ec);
                if (ec)
                {
                    return Result<void>(ErrorCode::FileAccessDenied,
                                        "Failed to replace project descriptor: " + ec.message());
                }
            }

            return Result<void>();
        }
        catch (const std::exception& exception)
        {
            return Result<void>(ErrorCode::FileAccessDenied,
                                std::string("Failed to save project descriptor: ") + exception.what());
        }
    }

    Result<Project> ProjectSerializer::CreateInMemory(const ProjectCreateOptions& options)
    {
        return CreateProjectFromOptions(options);
    }

    Result<Project> ProjectSerializer::CreateOnDisk(const ProjectCreateOptions& options)
    {
        const auto projectResult = CreateProjectFromOptions(options);
        if (projectResult.IsFailure())
            return projectResult;

        Project project = projectResult.GetValue();
        const auto ensureDirectoriesResult = EnsureProjectDirectoriesExist(project);
        if (ensureDirectoriesResult.IsFailure())
            return Result<Project>(ensureDirectoriesResult.GetError());

        const auto saveResult = Save(project);
        if (saveResult.IsFailure())
            return Result<Project>(saveResult.GetError());

        return project;
    }

    Result<Project> ProjectSerializer::CreateProjectFromOptions(const ProjectCreateOptions& options)
    {
        if (options.RootDirectory.empty())
        {
            return Result<Project>(ErrorCode::InvalidArgument,
                                   "Project root directory must not be empty");
        }

        Project project;
        project.Descriptor.Name = Trim(options.Name);
        if (project.Descriptor.Name.empty())
        {
            std::filesystem::path inferredName = options.RootDirectory.filename();
            project.Descriptor.Name = inferredName.empty() ? "Project" : inferredName.string();
        }

        project.Descriptor.EngineVersion = Trim(options.EngineVersion);
        if (project.Descriptor.EngineVersion.empty())
            project.Descriptor.EngineVersion = std::string(ProjectDefaultEngineVersion);

        project.Descriptor.Paths.Assets = Trim(options.AssetsDirectory);
        if (project.Descriptor.Paths.Assets.empty())
            project.Descriptor.Paths.Assets = "Assets";

        project.Descriptor.Paths.Settings = Trim(options.SettingsDirectory);
        if (project.Descriptor.Paths.Settings.empty())
            project.Descriptor.Paths.Settings = "Settings";

        project.Descriptor.Startup.Scene = Trim(options.StartupScene);

        const auto descriptorFileNameResult = ResolveDescriptorFileName(options);
        if (descriptorFileNameResult.IsFailure())
            return Result<Project>(descriptorFileNameResult.GetError());

        project.Paths.RootDirectory = NormalizeAbsolutePath(options.RootDirectory);
        project.Paths.DescriptorPath = project.Paths.RootDirectory / descriptorFileNameResult.GetValue();
        project.Paths.AssetsDirectory = project.Paths.RootDirectory / project.Descriptor.Paths.Assets;
        project.Paths.SettingsDirectory = project.Paths.RootDirectory / project.Descriptor.Paths.Settings;

        const auto validateResult = ValidateProject(project);
        if (validateResult.IsFailure())
            return Result<Project>(validateResult.GetError());

        return project;
    }

    Result<void> ProjectSerializer::ValidateDescriptor(const ProjectDescriptor& descriptor)
    {
        if (descriptor.Version != ProjectDescriptorCurrentVersion)
        {
            return Result<void>(ErrorCode::ConfigVersionMismatch,
                                "Unsupported project descriptor version: " + std::to_string(descriptor.Version));
        }

        if (Trim(descriptor.Name).empty())
        {
            return Result<void>(ErrorCode::ConfigValidationError,
                                "Project descriptor name must not be empty");
        }

        if (Trim(descriptor.Paths.Assets).empty() || Trim(descriptor.Paths.Settings).empty())
        {
            return Result<void>(ErrorCode::ConfigValidationError,
                                "Project descriptor paths must not be empty");
        }

        if (std::filesystem::path(descriptor.Paths.Assets).is_absolute() ||
            std::filesystem::path(descriptor.Paths.Settings).is_absolute())
        {
            return Result<void>(ErrorCode::ConfigValidationError,
                                "Project descriptor asset/settings paths must be relative");
        }

        if (!descriptor.Startup.Scene.empty() && std::filesystem::path(descriptor.Startup.Scene).is_absolute())
        {
            return Result<void>(ErrorCode::ConfigValidationError,
                                "Project startup scene path must be relative when specified");
        }

        return Result<void>();
    }

    Result<void> ProjectSerializer::ValidateProject(const Project& project)
    {
        const auto validateDescriptorResult = ValidateDescriptor(project.Descriptor);
        if (validateDescriptorResult.IsFailure())
            return validateDescriptorResult;

        if (project.Paths.RootDirectory.empty() || project.Paths.DescriptorPath.empty())
        {
            return Result<void>(ErrorCode::InvalidArgument,
                                "Project paths must include a root directory and descriptor path");
        }

        if (project.Paths.DescriptorPath.parent_path() != project.Paths.RootDirectory)
        {
            return Result<void>(ErrorCode::InvalidArgument,
                                "Project descriptor path must reside under the project root directory");
        }

        if (project.Paths.AssetsDirectory.empty() || project.Paths.SettingsDirectory.empty())
        {
            return Result<void>(ErrorCode::InvalidArgument,
                                "Project paths must include assets and settings directories");
        }

        return Result<void>();
    }

    Result<void> ProjectSerializer::EnsureProjectDirectoriesExist(const Project& project)
    {
        const auto validateResult = ValidateProject(project);
        if (validateResult.IsFailure())
            return validateResult;

        try
        {
            std::filesystem::create_directories(project.Paths.RootDirectory);
            std::filesystem::create_directories(project.Paths.AssetsDirectory);
            std::filesystem::create_directories(project.Paths.SettingsDirectory);
            return Result<void>();
        }
        catch (const std::exception& exception)
        {
            return Result<void>(ErrorCode::FileAccessDenied,
                                std::string("Failed to create project directory structure: ") + exception.what());
        }
    }
}
