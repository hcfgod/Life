#include "Core/ApplicationHost.h"

#include <utility>

namespace Life
{
    Scope<ApplicationHost> CreateApplicationHost(ApplicationCommandLineArgs args)
    {
        return CreateScope<ApplicationHost>(CreateApplication(args));
    }

    Scope<ApplicationHost> CreateApplicationHost(ApplicationCommandLineArgs args, Scope<ApplicationRuntime> runtime)
    {
        return CreateScope<ApplicationHost>(CreateApplication(args), std::move(runtime));
    }
}
