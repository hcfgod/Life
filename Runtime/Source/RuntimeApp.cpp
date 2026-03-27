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
