#pragma once

#include "Core/Application.h"

#include <filesystem>
#include <optional>
#include <string>

namespace Life
{
    std::optional<int> TryHandleApplicationCli(ApplicationCommandLineArgs args);
    std::filesystem::path ResolveFirstNonOptionArgument(ApplicationCommandLineArgs args);
}
