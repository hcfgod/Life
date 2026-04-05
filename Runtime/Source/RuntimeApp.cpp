#include "Engine.h"
#include "Runtime/GameLayer.h"
#include "Runtime/RuntimeDiagnosticsOverlay.h"

class RuntimeApplication final : public Life::Application
{
public:
    explicit RuntimeApplication(const Life::ApplicationSpecification& specification)
        : Life::Application(specification)
    {
    }

protected:
    void OnInit() override
    {
        auto inputActions = Life::CreateRef<Life::InputActionAsset>();
        Life::InputActionMap& gameplayMap = inputActions->AddMap("Gameplay");

        Life::InputAction& quitAction = gameplayMap.AddAction("Quit", Life::InputActionValueType::Button);
        quitAction.AddBinding(Life::KeyboardButtonBinding{ Life::KeyCodes::Escape });
        quitAction.AddBinding(Life::GamepadButtonBinding{ Life::GamepadButtons::Start, 0 });

        Life::InputAction& toggleCameraAction = gameplayMap.AddAction("ToggleCamera", Life::InputActionValueType::Button);
        toggleCameraAction.AddBinding(Life::KeyboardButtonBinding{ Life::KeyCodes::C });
        toggleCameraAction.AddBinding(Life::GamepadButtonBinding{ Life::GamepadButtons::RightShoulder, 0 });

        Life::InputAction& moveAction = gameplayMap.AddAction("Move", Life::InputActionValueType::Axis2D);
        Life::KeyboardAxis2DBinding movementKeys{};
        movementKeys.Up = Life::KeyCodes::W;
        movementKeys.Down = Life::KeyCodes::S;
        movementKeys.Left = Life::KeyCodes::A;
        movementKeys.Right = Life::KeyCodes::D;
        moveAction.AddBinding(movementKeys);

        Life::GamepadAxis2DBinding leftStick{};
        leftStick.XAxis = Life::GamepadAxes::LeftX;
        leftStick.YAxis = Life::GamepadAxes::LeftY;
        leftStick.InvertY = true;
        moveAction.AddBinding(leftStick);

        Life::InputAction& lookAction = gameplayMap.AddAction("Look", Life::InputActionValueType::Axis2D);
        lookAction.AddBinding(Life::MouseDeltaBinding{ 0.1f, false });

        Life::GamepadAxis2DBinding rightStick{};
        rightStick.XAxis = Life::GamepadAxes::RightX;
        rightStick.YAxis = Life::GamepadAxes::RightY;
        rightStick.InvertY = true;
        lookAction.AddBinding(rightStick);

        GetService<Life::InputSystem>().SetProjectActionAsset(std::move(inputActions));
        PushLayer(Life::CreateRef<RuntimeApp::GameLayer>(GetSpecification()));
        PushOverlay(Life::CreateRef<RuntimeApp::RuntimeDiagnosticsOverlay>());
        LOG_INFO("Runtime initialized.");
    }

    void OnShutdown() override
    {
        LOG_INFO("Runtime shutting down.");
    }
};

namespace Life
{
    Scope<Application> CreateApplication(ApplicationCommandLineArgs args)
    {
        ApplicationSpecification specification;
        specification.Name = "Runtime";
        specification.Width = 1600;
        specification.Height = 900;
        specification.CommandLineArgs = args;

        return CreateScope<RuntimeApplication>(specification);
    }
}

#ifdef LIFE_ENABLE_ENTRYPOINT
#include "Core/EntryPoint.h"
#elif defined(LIFE_ENABLE_SDL_ENTRYPOINT)
#include "Core/SDLEntryPoint.h"
#endif
