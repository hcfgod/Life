#include "Engine.h"
#include "Editor/EditorShellOverlay.h"

class EditorApplication final : public Life::Application
{
public:
    explicit EditorApplication(const Life::ApplicationSpecification& specification)
        : Life::Application(specification)
    {
    }

protected:
    void OnInit() override
    {
        auto inputActions = Life::CreateRef<Life::InputActionAsset>();
        Life::InputActionMap& editorMap = inputActions->AddMap("Editor");

        Life::InputAction& quitAction = editorMap.AddAction("Quit", Life::InputActionValueType::Button);
        quitAction.AddBinding(Life::KeyboardButtonBinding{ Life::KeyCodes::Escape });
        quitAction.AddBinding(Life::GamepadButtonBinding{ Life::GamepadButtons::Start, 0 });

        GetService<Life::InputSystem>().SetProjectActionAsset(std::move(inputActions));
        PushOverlay(Life::CreateRef<EditorApp::EditorShellOverlay>());
        LOG_INFO("Editor initialized.");
    }

    void OnShutdown() override
    {
        LOG_INFO("Editor shutting down.");
    }
};

namespace Life
{
    Scope<Application> CreateApplication(ApplicationCommandLineArgs args)
    {
        ApplicationSpecification specification;
        specification.Name = "Editor";
        specification.Width = 1600;
        specification.Height = 900;
        specification.CommandLineArgs = args;

        if (args.Count > 1 && args[1] != nullptr)
            specification.ProjectDescriptorPath = args[1];

        return CreateScope<EditorApplication>(specification);
    }
}

#ifdef LIFE_ENABLE_ENTRYPOINT
#include "Core/EntryPoint.h"
#elif defined(LIFE_ENABLE_SDL_ENTRYPOINT)
#include "Core/SDLEntryPoint.h"
#endif
