#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>

namespace Life::Assets
{
    inline constexpr uint32_t ProjectDescriptorCurrentVersion = 2;
    inline constexpr std::string_view ProjectDescriptorFileExtension = ".lifeproject";
    inline constexpr std::string_view ProjectDefaultEngineVersion = "0.1.0";

    enum class ProjectDimension : uint8_t
    {
        TwoD = 0,
        ThreeD
    };

    struct ProjectDescriptorPaths
    {
        std::string Assets = "Assets";
        std::string Settings = "Settings";
    };

    struct ProjectDescriptorStartup
    {
        std::string Scene;
    };

    struct ProjectDescriptor
    {
        ProjectDescriptor() = default;

        ProjectDescriptor(const ProjectDescriptor& other)
            : Version(other.Version)
            , Name(other.Name)
            , EngineVersion(other.EngineVersion)
            , Dimension(other.Dimension)
            , Paths(other.Paths)
            , Startup(other.Startup)
        {
        }

        ProjectDescriptor(ProjectDescriptor&& other) noexcept
            : Version(other.Version)
            , Name(std::move(other.Name))
            , EngineVersion(std::move(other.EngineVersion))
            , Dimension(other.Dimension)
            , Paths(std::move(other.Paths))
            , Startup(std::move(other.Startup))
        {
        }

        ProjectDescriptor& operator=(const ProjectDescriptor& other)
        {
            if (this == &other)
                return *this;

            Version = other.Version;
            Name = other.Name;
            EngineVersion = other.EngineVersion;
            Dimension = other.Dimension;
            Paths = other.Paths;
            Startup = other.Startup;
            return *this;
        }

        ProjectDescriptor& operator=(ProjectDescriptor&& other) noexcept
        {
            if (this == &other)
                return *this;

            Version = other.Version;
            Name = std::move(other.Name);
            EngineVersion = std::move(other.EngineVersion);
            Dimension = other.Dimension;
            Paths = std::move(other.Paths);
            Startup = std::move(other.Startup);
            return *this;
        }

        ~ProjectDescriptor() = default;

        uint32_t Version = ProjectDescriptorCurrentVersion;
        std::string Name;
        std::string EngineVersion = std::string(ProjectDefaultEngineVersion);
        ProjectDimension Dimension = ProjectDimension::TwoD;
        ProjectDescriptorPaths Paths;
        ProjectDescriptorStartup Startup;
    };

    struct ProjectPaths
    {
        std::filesystem::path RootDirectory;
        std::filesystem::path DescriptorPath;
        std::filesystem::path AssetsDirectory;
        std::filesystem::path SettingsDirectory;
    };

    struct Project
    {
        ProjectDescriptor Descriptor;
        ProjectPaths Paths;
    };

    struct ProjectCreateOptions
    {
        std::filesystem::path RootDirectory;
        std::string Name;
        std::string EngineVersion = std::string(ProjectDefaultEngineVersion);
        ProjectDimension Dimension = ProjectDimension::TwoD;
        std::string DescriptorFileName;
        std::string AssetsDirectory = "Assets";
        std::string SettingsDirectory = "Settings";
        std::string StartupScene;
    };
}
