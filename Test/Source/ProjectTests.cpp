#include "TestSupport.h"

#include <random>

namespace
{
    std::filesystem::path MakeUniqueTestDirectory(const std::string& prefix)
    {
        std::random_device device;
        std::mt19937_64 generator(device());
        std::uniform_int_distribution<unsigned long long> distribution;

        return std::filesystem::temp_directory_path() /
               (prefix + "-" + std::to_string(distribution(generator)));
    }

    struct TemporaryDirectoryScope final
    {
        explicit TemporaryDirectoryScope(std::filesystem::path root)
            : Root(std::move(root))
        {
        }

        ~TemporaryDirectoryScope()
        {
            std::error_code ec;
            std::filesystem::remove_all(Root, ec);
        }

        std::filesystem::path Root;
    };

    class ProjectCacheTestAsset final : public Life::Asset
    {
    public:
        ProjectCacheTestAsset(std::string key, std::string guid)
            : Life::Asset(std::move(key), std::move(guid))
        {
        }
    };
}

TEST_CASE("ProjectSerializer creates and loads project descriptors")
{
    const std::filesystem::path projectRoot = MakeUniqueTestDirectory("life-project-serializer");
    TemporaryDirectoryScope cleanup(projectRoot);

    Life::Assets::ProjectCreateOptions options;
    options.RootDirectory = projectRoot;
    options.Name = "SerializerProject";
    options.StartupScene = "Assets/Scenes/Bootstrap.scene";

    const auto createResult = Life::Assets::ProjectSerializer::CreateOnDisk(options);
    REQUIRE(createResult.IsSuccess());

    const Life::Assets::Project& project = createResult.GetValue();
    CHECK(project.Paths.RootDirectory == std::filesystem::absolute(projectRoot).lexically_normal());
    CHECK(project.Paths.DescriptorPath.filename() == "SerializerProject.lifeproject");
    CHECK(project.Paths.AssetsDirectory == project.Paths.RootDirectory / "Assets");
    CHECK(project.Paths.SettingsDirectory == project.Paths.RootDirectory / "Settings");
    CHECK(std::filesystem::exists(project.Paths.DescriptorPath));
    CHECK(std::filesystem::is_directory(project.Paths.AssetsDirectory));
    CHECK(std::filesystem::is_directory(project.Paths.SettingsDirectory));

    const auto loadResult = Life::Assets::ProjectSerializer::Load(project.Paths.DescriptorPath);
    REQUIRE(loadResult.IsSuccess());

    const Life::Assets::Project& loadedProject = loadResult.GetValue();
    CHECK(loadedProject.Descriptor.Name == "SerializerProject");
    CHECK(loadedProject.Descriptor.Startup.Scene == "Assets/Scenes/Bootstrap.scene");
    CHECK(loadedProject.Paths.DescriptorPath == project.Paths.DescriptorPath);
}

TEST_CASE("ProjectService creates, saves, opens, and closes active projects")
{
    const std::filesystem::path firstRoot = MakeUniqueTestDirectory("life-project-service-a");
    const std::filesystem::path secondRoot = MakeUniqueTestDirectory("life-project-service-b");
    TemporaryDirectoryScope cleanupFirst(firstRoot);
    TemporaryDirectoryScope cleanupSecond(secondRoot);

    Life::Assets::AssetDatabase assetDatabase;
    Life::Assets::AssetManager assetManager;
    assetManager.BindDatabase(assetDatabase);

    Life::Assets::ProjectService projectService;
    projectService.BindAssetSystems(assetDatabase, assetManager);

    Life::Assets::ProjectCreateOptions createOptions;
    createOptions.RootDirectory = firstRoot;
    createOptions.Name = "GameplayProject";

    const auto createResult = projectService.CreateProject(createOptions);
    REQUIRE(createResult.IsSuccess());
    CHECK(projectService.HasActiveProject());
    REQUIRE(projectService.TryGetActiveProject() != nullptr);
    CHECK(projectService.GetActiveProject().Descriptor.Name == "GameplayProject");
    CHECK(Life::Assets::TryGetActiveProjectRootDirectory().has_value());
    CHECK(Life::Assets::TryGetActiveProjectRootDirectory().value() == projectService.GetActiveProject().Paths.RootDirectory);

    projectService.GetActiveProject().Descriptor.EngineVersion = "0.2.0-test";
    const auto saveResult = projectService.SaveProject();
    REQUIRE(saveResult.IsSuccess());

    const auto reopenedResult = Life::Assets::ProjectSerializer::Load(projectService.GetActiveProject().Paths.DescriptorPath);
    REQUIRE(reopenedResult.IsSuccess());
    CHECK(reopenedResult.GetValue().Descriptor.EngineVersion == "0.2.0-test");

    Life::Assets::ProjectCreateOptions secondaryOptions;
    secondaryOptions.RootDirectory = secondRoot;
    secondaryOptions.Name = "SecondaryProject";
    secondaryOptions.DescriptorFileName = "SecondaryProject.lifeproject";

    const auto secondaryCreateResult = Life::Assets::ProjectSerializer::CreateOnDisk(secondaryOptions);
    REQUIRE(secondaryCreateResult.IsSuccess());

    assetDatabase.RegisterGeneratedAsset("generated-guid", "Generated/Transient.asset", Life::Assets::AssetType::Texture2D).GetValue();
    CHECK(assetDatabase.GetRecordCount() == 1);
    auto cachedAsset = Life::Ref<Life::Asset>(new ProjectCacheTestAsset("Assets/Test.asset", "asset-guid"));
    assetManager.Cache(cachedAsset->GetKey(), cachedAsset->GetGuid(), cachedAsset);
    CHECK(assetManager.GetCachedByKey<ProjectCacheTestAsset>(cachedAsset->GetKey()) != nullptr);

    const auto openResult = projectService.OpenProject(secondaryCreateResult.GetValue().Paths.DescriptorPath);
    REQUIRE(openResult.IsSuccess());
    CHECK(projectService.GetActiveProject().Descriptor.Name == "SecondaryProject");
    CHECK(assetDatabase.GetRecordCount() == 0);
    CHECK(assetManager.GetCachedByKey<ProjectCacheTestAsset>(cachedAsset->GetKey()) == nullptr);
    CHECK(Life::Assets::TryGetActiveProjectRootDirectory().value() == openResult.GetValue().Paths.RootDirectory);

    const auto closeResult = projectService.CloseProject();
    REQUIRE(closeResult.IsSuccess());
    CHECK_FALSE(projectService.HasActiveProject());
    CHECK(projectService.TryGetActiveProject() == nullptr);
    CHECK_FALSE(Life::Assets::TryGetActiveProjectRootDirectory().has_value());
}

TEST_CASE("ApplicationHost opens configured project descriptor before exposing services")
{
    const std::filesystem::path projectRoot = MakeUniqueTestDirectory("life-project-host");
    TemporaryDirectoryScope cleanup(projectRoot);

    Life::Assets::ProjectCreateOptions options;
    options.RootDirectory = projectRoot;
    options.Name = "HostBackedProject";
    options.StartupScene = "Assets/Scenes/Bootstrap.scene";
    const auto createResult = Life::Assets::ProjectSerializer::CreateOnDisk(options);
    REQUIRE(createResult.IsSuccess());

    Life::ApplicationSpecification specification = Life::Tests::TestApplication::CreateSpecification();
    specification.Name = "Project Host Test";
    specification.ProjectDescriptorPath = createResult.GetValue().Paths.DescriptorPath;

    auto application = Life::CreateScope<Life::Tests::ConfigurableTestApplication>(specification);
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<Life::Tests::TestRuntime>());

    REQUIRE(host->GetServices().Has<Life::Assets::ProjectService>());
    REQUIRE(host->GetServices().Has<Life::SceneService>());
    CHECK(host->GetProjectService().HasActiveProject());
    CHECK(host->GetProjectService().GetActiveProject().Descriptor.Name == "HostBackedProject");
    CHECK(host->GetApplication().GetService<Life::Assets::ProjectService>().HasActiveProject());
    REQUIRE(host->GetSceneService() != nullptr);
    CHECK(host->GetSceneService()->HasActiveScene());
    CHECK(host->GetSceneService()->GetActiveScene().GetSourcePath().filename() == "Bootstrap.scene");
    CHECK(host->GetApplication().GetService<Life::SceneService>().HasActiveScene());
    CHECK(Life::Assets::TryGetActiveProjectRootDirectory().has_value());
    CHECK(Life::Assets::TryGetActiveProjectRootDirectory().value() == createResult.GetValue().Paths.RootDirectory);

    const auto resolvedAssetPathResult = Life::Assets::ResolveAssetKeyToPath("Assets/Textures/Example.png");
    REQUIRE(resolvedAssetPathResult.IsSuccess());
    CHECK(resolvedAssetPathResult.GetValue() == createResult.GetValue().Paths.RootDirectory / "Assets" / "Textures" / "Example.png");

    host.reset();
    CHECK_FALSE(Life::Assets::TryGetActiveProjectRootDirectory().has_value());
}
