#include "TestSupport.h"

using namespace Life::Tests;

namespace
{
    std::filesystem::path FindRepositoryRoot()
    {
        std::vector<std::filesystem::path> candidates;
        candidates.emplace_back(std::filesystem::current_path());
        candidates.emplace_back(std::filesystem::absolute(std::filesystem::path(__FILE__)));

        const std::string executablePath = Life::PlatformDetection::GetExecutablePath();
        if (!executablePath.empty())
            candidates.emplace_back(std::filesystem::path(executablePath));

        for (std::filesystem::path candidate : candidates)
        {
            if (candidate.empty())
                continue;

            std::error_code statusError;
            if (std::filesystem::is_regular_file(candidate, statusError))
                candidate = candidate.parent_path();

            while (!candidate.empty())
            {
                if (std::filesystem::exists(candidate / "premake5.lua")
                    && std::filesystem::exists(candidate / "Engine")
                    && std::filesystem::exists(candidate / "Runtime")
                    && std::filesystem::exists(candidate / "Test"))
                {
                    return candidate;
                }

                const std::filesystem::path parent = candidate.parent_path();
                if (parent == candidate)
                    break;

                candidate = parent;
            }
        }

        throw std::runtime_error("Failed to locate the Life repository root.");
    }
}

TEST_CASE("CameraManager supports explicit primary selection and priority fallback")
{
    Life::CameraManager cameraManager;

    Life::CameraSpecification backgroundCameraSpec;
    backgroundCameraSpec.Name = "Background";
    backgroundCameraSpec.Priority = 1;
    backgroundCameraSpec.AspectRatio = 4.0f / 3.0f;

    Life::CameraSpecification gameplayCameraSpec;
    gameplayCameraSpec.Name = "Gameplay";
    gameplayCameraSpec.Priority = 10;
    gameplayCameraSpec.AspectRatio = 16.0f / 9.0f;

    Life::Camera* backgroundCamera = cameraManager.CreateCamera(backgroundCameraSpec);
    Life::Camera* gameplayCamera = cameraManager.CreateCamera(gameplayCameraSpec);

    REQUIRE(backgroundCamera != nullptr);
    REQUIRE(gameplayCamera != nullptr);
    REQUIRE(cameraManager.GetCameraCount() == 2);

    CHECK(cameraManager.GetPrimaryCamera() == backgroundCamera);
    CHECK(cameraManager.SetPrimaryCamera("Gameplay"));
    CHECK(cameraManager.GetPrimaryCamera() == gameplayCamera);

    cameraManager.ClearPrimaryCamera();
    CHECK(cameraManager.GetPrimaryCamera() == gameplayCamera);

    cameraManager.SetAspectRatioAll(21.0f / 9.0f);
    CHECK(backgroundCamera->GetAspectRatio() == doctest::Approx(21.0f / 9.0f));
    CHECK(gameplayCamera->GetAspectRatio() == doctest::Approx(21.0f / 9.0f));

    CHECK(cameraManager.DestroyCamera("Gameplay"));
    CHECK(cameraManager.GetPrimaryCamera() == backgroundCamera);
    CHECK_FALSE(cameraManager.SetPrimaryCamera("Missing"));
}

TEST_CASE("Window resize events forward to the registered GraphicsDevice service")
{
    Life::Log::Init();

    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    FakeGraphicsDevice graphicsDevice;
    host->GetServices().Register<Life::GraphicsDevice>(graphicsDevice);
    host->Initialize();

    Life::WindowResizeEvent resizeEvent(1280, 720);
    host->HandleEvent(resizeEvent);

    CHECK(graphicsDevice.ResizeCallCount == 1);
    CHECK(graphicsDevice.LastResizeWidth == 1280);
    CHECK(graphicsDevice.LastResizeHeight == 720);
    CHECK_FALSE(resizeEvent.IsHandled());
    CHECK(host->IsRunning());

    host->Finalize();
}

TEST_CASE("Renderer and Renderer2D stay safe no-op services without live frame resources")
{
    FakeGraphicsDevice graphicsDevice;
    Life::Renderer renderer(graphicsDevice);
    Life::Renderer2D renderer2D(renderer);

    CHECK(renderer.GetCurrentFramebuffer() == nullptr);
    CHECK(renderer.CreatePipeline({}) == nullptr);

    renderer.Clear(0.1f, 0.2f, 0.3f, 1.0f);
    renderer.BeginScene(glm::mat4(1.0f));
    renderer.EndScene();

    renderer2D.BeginScene(glm::mat4(1.0f));
    renderer2D.DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
    renderer2D.EndScene();

    renderer2D.BeginScene(glm::mat4(1.0f));
    renderer2D.DrawRotatedQuad({ 0.0f, 0.0f, 0.0f }, { 2.0f, 2.0f }, 0.5f, { 0.0f, 1.0f, 0.0f, 1.0f });
    renderer2D.EndScene();

    CHECK(renderer.GetStats().DrawCalls == 0);
    CHECK(renderer.GetStats().VerticesSubmitted == 0);
    CHECK(renderer2D.GetStats().DrawCalls == 0);
    CHECK(renderer2D.GetStats().QuadCount == 0);
}

TEST_CASE("ApplicationHost registers a safe no-op ImGuiSystem service without graphics")
{
    Life::Log::Init();

    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    REQUIRE(host->GetServices().Has<Life::ImGuiSystem>());

    host->Initialize();

    Life::ImGuiSystem* imguiSystem = host->GetServices().TryGet<Life::ImGuiSystem>();
    REQUIRE(imguiSystem != nullptr);
    CHECK_FALSE(imguiSystem->IsInitialized());
    CHECK_FALSE(imguiSystem->IsAvailable());
    CHECK(imguiSystem->GetBackend() == Life::GraphicsBackend::None);

    host->Finalize();
}

TEST_CASE("Premake and Vulkan dispatcher configuration keep dispatcher storage Engine-owned")
{
    const std::filesystem::path repositoryRoot = FindRepositoryRoot();
    const std::string editorPremake = ReadTextFile(repositoryRoot / "Editor" / "premake5.lua");
    const std::string runtimePremake = ReadTextFile(repositoryRoot / "Runtime" / "premake5.lua");
    const std::string testPremake = ReadTextFile(repositoryRoot / "Test" / "premake5.lua");
    const std::string rootPremake = ReadTextFile(repositoryRoot / "premake5.lua");
    const std::string setupBat = ReadTextFile(repositoryRoot / "Setup.bat");
    const std::string engineDispatcherSource = ReadTextFile(repositoryRoot / "Engine" / "Source" / "Graphics" / "Vulkan" / "VulkanDispatchLoader.cpp");

    CHECK(editorPremake.find("VulkanDispatchLoader.cpp") == std::string::npos);
    CHECK(runtimePremake.find("VulkanDispatchLoader.cpp") == std::string::npos);
    CHECK(testPremake.find("VulkanDispatchLoader.cpp") == std::string::npos);
    CHECK(rootPremake.find("\"nvrhi_vk\", \"nvrhi\", \"Engine\"") != std::string::npos);
    CHECK(setupBat.find("NVRHI_SHARED_LIBRARY_BUILD") == std::string::npos);

    CHECK(engineDispatcherSource.find("VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE") != std::string::npos);
    CHECK(engineDispatcherSource.find("#if !defined(_WIN32)") == std::string::npos);

    CHECK_MESSAGE(rootPremake.find("\"NDEBUG\"") != std::string::npos,
        "Root premake5.lua must define NDEBUG for Release/Dist to match NVRHI CMake layout of DispatchLoaderDynamic");
}

TEST_CASE("ImGui docking vendoring is wired through repository bootstrap and premake")
{
    const std::filesystem::path repositoryRoot = FindRepositoryRoot();
    const std::string gitmodules = ReadTextFile(repositoryRoot / ".gitmodules");
    const std::string rootPremake = ReadTextFile(repositoryRoot / "premake5.lua");
    const std::string enginePremake = ReadTextFile(repositoryRoot / "Engine" / "premake5.lua");
    const std::string editorPremake = ReadTextFile(repositoryRoot / "Editor" / "premake5.lua");
    const std::string imguiPremake = ReadTextFile(repositoryRoot / "Vendor" / "imgui" / "premake5.lua");
    const std::string setupBat = ReadTextFile(repositoryRoot / "Setup.bat");
    const std::string setupSh = ReadTextFile(repositoryRoot / "Setup.sh");
    const std::string bootstrapRepoBat = ReadTextFile(repositoryRoot / "Scripts" / "BootstrapRepo.bat");
    const std::string bootstrapRepoSh = ReadTextFile(repositoryRoot / "Scripts" / "BootstrapRepo.sh");

    CHECK(gitmodules.find("[submodule \"Vendor/imgui\"]") != std::string::npos);
    CHECK(gitmodules.find("path = Vendor/imgui") != std::string::npos);
    CHECK(gitmodules.find("url = https://github.com/ocornut/imgui.git") != std::string::npos);
    CHECK(gitmodules.find("branch = docking") != std::string::npos);
    CHECK(rootPremake.find("IncludeDir[\"imgui\"]") != std::string::npos);
    CHECK(rootPremake.find("include \"Vendor/imgui\"") != std::string::npos);
    CHECK(rootPremake.find("include \"Editor\"") != std::string::npos);
    CHECK(rootPremake.find("startproject \"Editor\"") != std::string::npos);
    CHECK(enginePremake.find("\"ImGui\"") != std::string::npos);
    CHECK(editorPremake.find("project \"Editor\"") != std::string::npos);
    CHECK(imguiPremake.find("project \"ImGui\"") != std::string::npos);
    CHECK(imguiPremake.find("imgui_impl_sdl3.cpp") != std::string::npos);
    CHECK(imguiPremake.find("imgui_impl_vulkan.cpp") != std::string::npos);
    CHECK(setupBat.find(":ensure_imgui_premake") != std::string::npos);
    CHECK(setupBat.find("Vendor\\imgui\\premake5.lua") != std::string::npos);
    CHECK(setupSh.find("ensure_imgui_premake") != std::string::npos);
    CHECK(setupSh.find("$REPO_ROOT/Vendor/imgui/premake5.lua") != std::string::npos);
    CHECK(bootstrapRepoBat.find("submoduleBranch") != std::string::npos);
    CHECK(bootstrapRepoSh.find("submodule_branch") != std::string::npos);
}
