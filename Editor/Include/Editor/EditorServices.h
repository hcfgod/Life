#pragma once

#include "Engine.h"

namespace EditorApp
{
    struct EditorServices
    {
        Life::OptionalRef<Life::Application> Application;
        Life::OptionalRef<Life::Window> Window;
        Life::OptionalRef<Life::GraphicsDevice> GraphicsDevice;
        Life::OptionalRef<Life::InputSystem> InputSystem;
        Life::OptionalRef<Life::Assets::AssetManager> AssetManager;
        Life::OptionalRef<Life::CameraManager> CameraManager;
        Life::OptionalRef<Life::Renderer> Renderer;
        Life::OptionalRef<Life::SceneRenderer2D> SceneRenderer2D;
        Life::OptionalRef<Life::ImGuiSystem> ImGuiSystem;

        static EditorServices Acquire(Life::Application& application);
        void Reset() noexcept;
        bool HasImGui() const noexcept;
    };
}
