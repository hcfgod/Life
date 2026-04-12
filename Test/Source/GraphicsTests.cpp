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

    bool IsLiveBackendSmokeEnabled()
    {
        const auto value = Life::PlatformUtils::GetEnvironmentVariable("LIFE_ENABLE_LIVE_BACKEND_SMOKE");
        if (!value.has_value())
            return false;

        return *value != "0" && *value != "false" && *value != "FALSE";
    }

    class LiveBackendSmokeApplication final : public Life::Application
    {
    public:
        LiveBackendSmokeApplication()
            : Life::Application(CreateSpecification())
        {
        }

        static Life::ApplicationSpecification CreateSpecification()
        {
            Life::ApplicationSpecification specification = TestApplication::CreateSpecification();
            specification.Name = "Live Backend Smoke";
            specification.Width = 640;
            specification.Height = 360;
            specification.VSync = false;
            return specification;
        }

        bool SawLiveFrame = false;
        bool RendererResult = false;
        bool SurfaceResizeResult = false;
        bool SurfaceRenderResult = false;
        bool SurfacePresentResult = false;
        bool ImGuiWasAvailable = false;
        bool CompletedFrame = false;
        int UpdateCount = 0;

    protected:
        void OnInit() override
        {
            m_GraphicsDevice = TryGetService<Life::GraphicsDevice>();
            m_Renderer = TryGetService<Life::Renderer>();
            m_SceneRenderer2D = TryGetService<Life::SceneRenderer2D>();
            m_ImGuiSystem = TryGetService<Life::ImGuiSystem>();

            if (m_Renderer != nullptr && m_SceneRenderer2D != nullptr && m_ImGuiSystem != nullptr)
            {
                m_SceneSurface = Life::CreateScope<Life::SceneSurface>(
                    *m_Renderer,
                    m_SceneRenderer2D->GetRenderer2D(),
                    *m_ImGuiSystem);
            }
        }

        void OnShutdown() override
        {
            m_SceneSurface.reset();
        }

        void OnUpdate(float timestep) override
        {
            (void)timestep;
            ++UpdateCount;

            if (CompletedFrame)
            {
                RequestShutdown();
                return;
            }

            if (m_GraphicsDevice == nullptr || m_Renderer == nullptr || m_SceneRenderer2D == nullptr || m_SceneSurface == nullptr)
                return;

            if (m_GraphicsDevice->GetCurrentCommandList() == nullptr || m_GraphicsDevice->GetCurrentBackBuffer() == nullptr)
                return;

            SawLiveFrame = true;

            Life::CameraSpecification cameraSpecification;
            cameraSpecification.Name = "LiveBackendSmokeCamera";
            cameraSpecification.Projection = Life::ProjectionType::Orthographic;
            cameraSpecification.AspectRatio = m_GraphicsDevice->GetBackBufferHeight() > 0
                ? static_cast<float>(m_GraphicsDevice->GetBackBufferWidth()) / static_cast<float>(m_GraphicsDevice->GetBackBufferHeight())
                : 1.0f;
            cameraSpecification.OrthoSize = 4.5f;
            cameraSpecification.OrthoNear = 0.1f;
            cameraSpecification.OrthoFar = 10.0f;
            cameraSpecification.ClearColor = { 0.08f, 0.08f, 0.12f, 1.0f };
            Life::Camera camera(cameraSpecification);
            camera.SetPosition({ 0.0f, 0.0f, 1.0f });
            camera.LookAt({ 0.0f, 0.0f, 0.0f });

            Life::SceneRenderer2D::Scene2D scene;
            scene.Camera = &camera;
            Life::SceneRenderer2D::QuadCommand quad;
            quad.Position = { 0.0f, 0.0f, 0.0f };
            quad.Size = { 1.5f, 1.5f };
            quad.Color = { 0.9f, 0.4f, 0.2f, 1.0f };
            scene.Quads.push_back(quad);

            m_Renderer->Clear(0.05f, 0.05f, 0.07f, 1.0f);
            RendererResult = m_SceneRenderer2D->Render(scene);
            SurfaceResizeResult = m_SceneSurface->Resize(160, 90);
            if (SurfaceResizeResult)
                SurfaceRenderResult = m_SceneRenderer2D->RenderToSurface(*m_SceneSurface, scene);

            ImGuiWasAvailable = m_ImGuiSystem != nullptr && m_ImGuiSystem->IsInitialized() && m_ImGuiSystem->IsAvailable() && m_ImGuiSystem->IsFrameActive();
            if (SurfaceRenderResult)
                SurfacePresentResult = m_SceneSurface->Present(160.0f, 90.0f);

            CompletedFrame = true;
            RequestShutdown();
        }

    private:
        Life::GraphicsDevice* m_GraphicsDevice = nullptr;
        Life::Renderer* m_Renderer = nullptr;
        Life::SceneRenderer2D* m_SceneRenderer2D = nullptr;
        Life::ImGuiSystem* m_ImGuiSystem = nullptr;
        Life::Scope<Life::SceneSurface> m_SceneSurface;
    };
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

TEST_CASE("SceneRenderer2D stays a safe no-op boundary without live frame resources")
{
    FakeGraphicsDevice graphicsDevice;
    Life::Renderer renderer(graphicsDevice);
    Life::Renderer2D renderer2D(renderer);
    Life::SceneRenderer2D sceneRenderer(renderer2D);

    Life::CameraSpecification cameraSpecification;
    cameraSpecification.Projection = Life::ProjectionType::Orthographic;
    cameraSpecification.AspectRatio = 1.0f;
    Life::Camera camera(cameraSpecification);

    Life::SceneRenderer2D::Scene2D scene;
    scene.Camera = &camera;

    Life::SceneRenderer2D::QuadCommand quad;
    quad.Position = { 0.0f, 0.0f, 0.0f };
    quad.Size = { 1.0f, 1.0f };
    quad.Color = { 1.0f, 0.0f, 0.0f, 1.0f };
    scene.Quads.push_back(quad);

    CHECK_FALSE(sceneRenderer.Render(scene));
    CHECK(sceneRenderer.GetStats().DrawCalls == 0);
    CHECK(sceneRenderer.GetStats().QuadCount == 0);
}

TEST_CASE("SceneRenderer2D builds stable submission order from quad Z")
{
    Life::SceneRenderer2D::Scene2D scene;

    Life::SceneRenderer2D::QuadCommand nearQuad;
    nearQuad.Position = { 0.0f, 0.0f, 0.25f };

    Life::SceneRenderer2D::QuadCommand middleQuad;
    middleQuad.Position = { 0.0f, 0.0f, -0.5f };

    Life::SceneRenderer2D::QuadCommand farQuad;
    farQuad.Position = { 0.0f, 0.0f, -1.5f };

    scene.Quads.push_back(nearQuad);
    scene.Quads.push_back(middleQuad);
    scene.Quads.push_back(farQuad);

    scene.SortMode = Life::SceneRenderer2D::QuadSortMode::BackToFront;
    auto orderedQuads = Life::SceneRenderer2D::BuildSubmissionOrder(scene);
    REQUIRE(orderedQuads.size() == 3);
    CHECK(orderedQuads[0]->Position.z == doctest::Approx(-1.5f));
    CHECK(orderedQuads[1]->Position.z == doctest::Approx(-0.5f));
    CHECK(orderedQuads[2]->Position.z == doctest::Approx(0.25f));

    scene.SortMode = Life::SceneRenderer2D::QuadSortMode::FrontToBack;
    orderedQuads = Life::SceneRenderer2D::BuildSubmissionOrder(scene);
    REQUIRE(orderedQuads.size() == 3);
    CHECK(orderedQuads[0]->Position.z == doctest::Approx(0.25f));
    CHECK(orderedQuads[1]->Position.z == doctest::Approx(-0.5f));
    CHECK(orderedQuads[2]->Position.z == doctest::Approx(-1.5f));

    scene.SortMode = Life::SceneRenderer2D::QuadSortMode::SubmissionOrder;
    orderedQuads = Life::SceneRenderer2D::BuildSubmissionOrder(scene);
    REQUIRE(orderedQuads.size() == 3);
    CHECK(orderedQuads[0]->Position.z == doctest::Approx(0.25f));
    CHECK(orderedQuads[1]->Position.z == doctest::Approx(-0.5f));
    CHECK(orderedQuads[2]->Position.z == doctest::Approx(-1.5f));
}

TEST_CASE("SceneSurface stays safe without live frame resources")
{
    Life::WindowSpecification windowSpecification;
    windowSpecification.Title = "GraphicsTests";
    TestWindow window(windowSpecification);

    FakeGraphicsDevice graphicsDevice;
    Life::Renderer renderer(graphicsDevice);
    Life::Renderer2D renderer2D(renderer);
    Life::ImGuiSystem imguiSystem(window, nullptr);
    Life::SceneSurface sceneSurface(renderer, renderer2D, imguiSystem);

    Life::CameraSpecification cameraSpecification;
    cameraSpecification.Projection = Life::ProjectionType::Orthographic;
    cameraSpecification.AspectRatio = 1.0f;
    Life::Camera camera(cameraSpecification);

    CHECK_FALSE(sceneSurface.IsReady());
    CHECK_FALSE(sceneSurface.Resize(320, 180));
    CHECK_FALSE(sceneSurface.IsReady());
    CHECK(sceneSurface.GetWidth() == 0);
    CHECK(sceneSurface.GetHeight() == 0);
    CHECK_FALSE(sceneSurface.BeginScene2D(camera));
    CHECK_FALSE(sceneSurface.Present(320.0f, 180.0f));

    sceneSurface.EndScene2D();
    sceneSurface.Reset();

    CHECK_FALSE(sceneSurface.IsReady());
    CHECK(sceneSurface.GetWidth() == 0);
    CHECK(sceneSurface.GetHeight() == 0);
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

TEST_CASE("Live backend smoke validates one real host frame when explicitly enabled")
{
    if (!IsLiveBackendSmokeEnabled())
        return;

    Life::Log::Init();

    auto application = Life::CreateScope<LiveBackendSmokeApplication>();
    auto* applicationInstance = application.get();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application));

    REQUIRE(host->GetGraphicsDevice() != nullptr);
    REQUIRE(host->GetRenderer() != nullptr);
    REQUIRE(host->GetRenderer2D() != nullptr);
    REQUIRE(host->GetSceneRenderer2D() != nullptr);

    host->Initialize();

    for (int frameIndex = 0; frameIndex < 8 && !applicationInstance->CompletedFrame; ++frameIndex)
        host->RunFrame(0.0f);

    CHECK(applicationInstance->SawLiveFrame);
    CHECK(applicationInstance->RendererResult);
    CHECK(applicationInstance->SurfaceResizeResult);
    CHECK(applicationInstance->SurfaceRenderResult);
    if (applicationInstance->ImGuiWasAvailable)
        CHECK(applicationInstance->SurfacePresentResult);

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
