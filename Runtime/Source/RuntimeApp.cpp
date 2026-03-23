#include "Engine.h"
#include <nlohmann/json.hpp>

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
        const Life::ApplicationSpecification& specification = GetSpecification();
        nlohmann::json startupConfig =
        {
            { "name", specification.Name },
            { "width", specification.Width },
            { "height", specification.Height },
            { "vsync", specification.VSync }
        };

        LOG_INFO("Runtime sandbox boot config: {}", startupConfig.dump());
        LOG_INFO("Runtime sandbox initialized.");
    }

    void OnShutdown() override
    {
        LOG_INFO("Runtime sandbox shutting down.");
    }

    void OnUpdate(float timestep) override
    {
        m_ElapsedTime += timestep;

        if (!m_HasLoggedRuntime && m_ElapsedTime >= 1.0f)
        {
            LOG_INFO("Runtime sandbox running.");
            m_HasLoggedRuntime = true;
        }
    }

private:
    float m_ElapsedTime = 0.0f;
    bool m_HasLoggedRuntime = false;
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
