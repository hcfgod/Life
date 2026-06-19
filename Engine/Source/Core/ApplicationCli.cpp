#include "Core/ApplicationCli.h"

#include "Core/BuildInfo.h"

#include <filesystem>
#include <iostream>
#include <string_view>

namespace Life
{
    namespace
    {
        bool IsOption(std::string_view value)
        {
            return value.size() > 1 && value[0] == '-';
        }

        bool IsHelpOption(std::string_view value)
        {
            return value == "--help" || value == "-h" || value == "/?";
        }

        bool IsVersionOption(std::string_view value)
        {
            return value == "--version" || value == "-v";
        }

        std::string ResolveExecutableName(ApplicationCommandLineArgs args)
        {
            if (args.Count > 0 && args[0] != nullptr)
                return std::filesystem::path(args[0]).filename().string();

            return "Life";
        }

        void PrintHelp(ApplicationCommandLineArgs args)
        {
            const std::string executableName = ResolveExecutableName(args);
            std::cout
                << executableName << '\n'
                << "Usage: " << executableName << " [options] [project-descriptor]\n\n"
                << "Options:\n"
                << "  --help, -h          Show this help text and exit.\n"
                << "  --version, -v       Print build version metadata and exit.\n"
                << "  --diagnostics       Print build and platform diagnostics and exit.\n";
        }
    }

    std::optional<int> TryHandleApplicationCli(ApplicationCommandLineArgs args)
    {
        for (int index = 1; index < args.Count; ++index)
        {
            if (args[index] == nullptr)
                continue;

            const std::string_view argument(args[index]);
            if (IsHelpOption(argument))
            {
                PrintHelp(args);
                return 0;
            }

            if (IsVersionOption(argument))
            {
                std::cout << FormatBuildVersionLine() << '\n';
                return 0;
            }

            if (argument == "--diagnostics")
            {
                std::cout << FormatBuildDiagnostics();
                return 0;
            }
        }

        return std::nullopt;
    }

    std::filesystem::path ResolveFirstNonOptionArgument(ApplicationCommandLineArgs args)
    {
        for (int index = 1; index < args.Count; ++index)
        {
            if (args[index] == nullptr)
                continue;

            const std::string_view argument(args[index]);
            if (!IsOption(argument))
                return std::filesystem::path(args[index]);
        }

        return {};
    }
}
