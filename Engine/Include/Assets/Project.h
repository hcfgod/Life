#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace Life::Assets
{
    inline constexpr uint32_t ProjectDescriptorCurrentVersion = 1;
    inline constexpr std::string_view ProjectDescriptorFileExtension = ".lifeproject";
    inline constexpr std::string_view ProjectDefaultEngineVersion = "0.1.0";

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
        uint32_t Version = ProjectDescriptorCurrentVersion;
        std::string Name;
        std::string EngineVersion = std::string(ProjectDefaultEngineVersion);
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
        std::string DescriptorFileName;
        std::string AssetsDirectory = "Assets";
        std::string SettingsDirectory = "Settings";
        std::string StartupScene;
    };
}
