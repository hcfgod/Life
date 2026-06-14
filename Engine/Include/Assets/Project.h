#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

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
        ProjectDescriptor(const ProjectDescriptor&) = default;
        ProjectDescriptor(ProjectDescriptor&&) noexcept = default;
        ProjectDescriptor& operator=(const ProjectDescriptor&) = default;
        ProjectDescriptor& operator=(ProjectDescriptor&&) noexcept = default;
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
