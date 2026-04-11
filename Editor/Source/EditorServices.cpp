#include "Editor/EditorServices.h"

namespace EditorApp
{
    EditorServices EditorServices::Acquire(Life::Application& application)
    {
        EditorServices services;
        services.Application = Life::MakeOptionalRef(application);
        services.Window = Life::MakeOptionalRef(application.GetService<Life::Window>());
        services.GraphicsDevice = Life::MakeOptionalRef(application.TryGetService<Life::GraphicsDevice>());
        services.InputSystem = Life::MakeOptionalRef(application.GetService<Life::InputSystem>());
        services.AssetManager = Life::MakeOptionalRef(application.GetService<Life::Assets::AssetManager>());
        services.ProjectService = Life::MakeOptionalRef(application.GetService<Life::Assets::ProjectService>());
        services.CameraManager = Life::MakeOptionalRef(application.GetService<Life::CameraManager>());
        services.Renderer = Life::MakeOptionalRef(application.TryGetService<Life::Renderer>());
        services.SceneRenderer2D = Life::MakeOptionalRef(application.TryGetService<Life::SceneRenderer2D>());
        services.ImGuiSystem = Life::MakeOptionalRef(application.TryGetService<Life::ImGuiSystem>());
        return services;
    }

    void EditorServices::Reset() noexcept
    {
        ImGuiSystem.reset();
        SceneRenderer2D.reset();
        Renderer.reset();
        CameraManager.reset();
        ProjectService.reset();
        AssetManager.reset();
        InputSystem.reset();
        GraphicsDevice.reset();
        Window.reset();
        Application.reset();
    }

    bool EditorServices::HasImGui() const noexcept
    {
        return ImGuiSystem
            && ImGuiSystem->get().IsInitialized()
            && ImGuiSystem->get().IsAvailable();
    }
}
