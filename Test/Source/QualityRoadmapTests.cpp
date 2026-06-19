#include "TestSupport.h"

#include <nlohmann/json.hpp>

#include <iostream>

using namespace Life::Tests;

namespace
{
    std::filesystem::path FindRepositoryRootForQualityTests()
    {
        std::filesystem::path candidate = std::filesystem::current_path();
        while (!candidate.empty())
        {
            if (std::filesystem::exists(candidate / "premake5.lua") &&
                std::filesystem::exists(candidate / "Engine") &&
                std::filesystem::exists(candidate / "Scripts"))
            {
                return candidate;
            }

            const std::filesystem::path parent = candidate.parent_path();
            if (parent == candidate)
                break;
            candidate = parent;
        }

        return std::filesystem::absolute(std::filesystem::path(__FILE__)).parent_path().parent_path().parent_path();
    }

    bool IsEnabledEnvironmentFlag(const char* name)
    {
        const auto value = Life::PlatformUtils::GetEnvironmentVariable(name);
        if (!value.has_value())
            return false;

        const std::string& text = *value;
        return text != "0" && text != "false" && text != "FALSE";
    }

    void WriteBenchmarkReport(const std::filesystem::path& outputPath)
    {
        const auto benchmarkStart = std::chrono::steady_clock::now();

        Life::Scene scene("BenchmarkScene");
        for (int index = 0; index < 128; ++index)
        {
            Life::Entity entity = scene.CreateEntity("Entity" + std::to_string(index));
            entity.GetComponent<Life::TransformComponent>().LocalPosition = { static_cast<float>(index), 0.0f, 0.0f };
            entity.AddComponent<Life::SpriteRendererComponent>();
        }

        const auto sceneBuildEnd = std::chrono::steady_clock::now();

        Life::SceneRenderer2D::Scene2D renderScene;
        for (int index = 0; index < 512; ++index)
        {
            Life::SceneRenderer2D::QuadCommand quad;
            quad.Position = { static_cast<float>(index % 32), static_cast<float>(index / 32), static_cast<float>(index % 8) };
            quad.SortingOrder = index % 17;
            renderScene.Quads.push_back(quad);
        }
        const auto order = Life::SceneRenderer2D::BuildSubmissionOrder(renderScene);
        (void)order;

        const auto sortEnd = std::chrono::steady_clock::now();

        std::filesystem::create_directories(outputPath.parent_path());
        nlohmann::json report;
        report["schema"] = "life.benchmark-report.v1";
        report["benchmarks"] = {
            {
                { "name", "scene_create_128_entities" },
                { "durationMicroseconds", std::chrono::duration_cast<std::chrono::microseconds>(sceneBuildEnd - benchmarkStart).count() }
            },
            {
                { "name", "renderer2d_sort_512_quads" },
                { "durationMicroseconds", std::chrono::duration_cast<std::chrono::microseconds>(sortEnd - sceneBuildEnd).count() }
            }
        };

        std::ofstream output(outputPath, std::ios::out | std::ios::trunc);
        output << report.dump(2) << '\n';
    }
}

TEST_CASE("BuildInfo exposes release metadata")
{
    const Life::BuildInfo& buildInfo = Life::GetBuildInfo();

    CHECK_FALSE(buildInfo.Version.empty());
    CHECK_FALSE(buildInfo.Commit.empty());
    CHECK_FALSE(buildInfo.BuildDate.empty());
    CHECK_FALSE(buildInfo.Platform.empty());
    CHECK_FALSE(buildInfo.Architecture.empty());
    CHECK_FALSE(buildInfo.Configuration.empty());
    CHECK(Life::FormatBuildVersionLine().find(buildInfo.Commit) != std::string::npos);
    CHECK(Life::FormatBuildDiagnostics().find("Version:") != std::string::npos);
}

TEST_CASE("Application CLI handles stable metadata flags and preserves first project path")
{
    std::ostringstream capturedOutput;
    auto* originalOutput = std::cout.rdbuf(capturedOutput.rdbuf());

    char executable[] = "Runtime";
    char diagnostics[] = "--diagnostics";
    char projectPath[] = "Samples/Test.life";
    char* diagnosticsArgs[] = { executable, diagnostics, projectPath };

    const Life::ApplicationCommandLineArgs commandLine{ 3, diagnosticsArgs };
    const std::optional<int> result = Life::TryHandleApplicationCli(commandLine);
    REQUIRE(result.has_value());
    CHECK(*result == 0);
    CHECK(Life::ResolveFirstNonOptionArgument(commandLine) == std::filesystem::path(projectPath));

    char version[] = "--version";
    char* versionArgs[] = { executable, version };
    CHECK(Life::TryHandleApplicationCli({ 2, versionArgs }).value_or(1) == 0);

    char* projectOnlyArgs[] = { executable, projectPath };
    CHECK_FALSE(Life::TryHandleApplicationCli({ 2, projectOnlyArgs }).has_value());
    CHECK(Life::ResolveFirstNonOptionArgument({ 2, projectOnlyArgs }) == std::filesystem::path(projectPath));

    std::cout.rdbuf(originalOutput);
    CHECK(capturedOutput.str().find("Version:") != std::string::npos);
}

TEST_CASE("Crash diagnostics writes machine-readable JSON sidecar")
{
    CrashDiagnosticsTestScope crashScope(std::filesystem::temp_directory_path() / "LifeTests" / "CrashJsonSidecar");

    const std::filesystem::path reportPath = Life::CrashDiagnostics::ReportMessage({
        "quality-roadmap",
        "synthetic crash json sidecar test",
        "json-sidecar"
    });

    REQUIRE_FALSE(reportPath.empty());
    CHECK(std::filesystem::exists(reportPath));

    std::filesystem::path jsonPath = reportPath;
    jsonPath.replace_extension(".json");
    REQUIRE(std::filesystem::exists(jsonPath));

    const nlohmann::json report = nlohmann::json::parse(ReadTextFile(jsonPath));
    CHECK(report["schema"] == "life.crash-report.v1");
    CHECK(report["category"] == "quality-roadmap");
    CHECK(report["phase"] == "json-sidecar");
    CHECK(report["buildVersion"].is_string());
    CHECK(report["buildCommit"].is_string());
    CHECK(report["reportPath"].get<std::string>().find(reportPath.filename().string()) != std::string::npos);
}

TEST_CASE("Quality roadmap fuzz cases keep malformed project and scene data contained")
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "LifeTests" / "QualityFuzz";
    std::error_code cleanupError;
    std::filesystem::remove_all(root, cleanupError);
    std::filesystem::create_directories(root);

    const std::vector<std::string> malformedPayloads = {
        "",
        "{",
        "[]",
        "{\"entities\":\"not-array\"}",
        "{\"project\":{\"version\":\"bad\"}}"
    };

    for (std::size_t index = 0; index < malformedPayloads.size(); ++index)
    {
        const std::filesystem::path scenePath = root / ("bad-scene-" + std::to_string(index) + ".scene");
        {
            std::ofstream output(scenePath, std::ios::out | std::ios::trunc);
            output << malformedPayloads[index];
        }

        const auto loadResult = Life::SceneSerializer::Load(scenePath);
        CHECK(loadResult.IsFailure());
    }

    std::filesystem::remove_all(root, cleanupError);
}

TEST_CASE("Editor hardening source invariants stay wired")
{
    const std::filesystem::path repositoryRoot = FindRepositoryRootForQualityTests();
    const std::string undoSource = ReadTextFile(repositoryRoot / "Editor" / "Source" / "Undo" / "EditorUndoStack.cpp");
    const std::string viewportSource = ReadTextFile(repositoryRoot / "Editor" / "Source" / "Viewport" / "SceneViewportPanel.cpp");
    const std::string hierarchySource = ReadTextFile(repositoryRoot / "Editor" / "Source" / "Panels" / "HierarchyPanel.cpp");

    CHECK(undoSource.find("SetMultiEntityTransformCommand") != std::string::npos);
    CHECK(undoSource.find("SelectAllAvailable") != std::string::npos);
    CHECK(viewportSource.find("ImGuizmo::Manipulate") != std::string::npos);
    CHECK(viewportSource.find("CommitExecuted(std::make_unique<SetMultiEntityTransformCommand>") != std::string::npos);
    CHECK(hierarchySource.find("!target.IsDescendantOf(dragged)") != std::string::npos);
    CHECK(hierarchySource.find("SetEntityParentCommand") != std::string::npos);
}

TEST_CASE("Report-only benchmarks emit JSON when explicitly enabled")
{
    if (!IsEnabledEnvironmentFlag("LIFE_ENABLE_BENCHMARKS"))
        return;

    const auto outputOverride = Life::PlatformUtils::GetEnvironmentVariable("LIFE_BENCHMARK_OUTPUT");
    const std::filesystem::path outputPath = outputOverride.has_value()
        ? std::filesystem::path(*outputOverride)
        : (std::filesystem::current_path() / "BenchmarkReports" / "benchmarks.json");

    WriteBenchmarkReport(outputPath);
    REQUIRE(std::filesystem::exists(outputPath));

    const nlohmann::json report = nlohmann::json::parse(ReadTextFile(outputPath));
    CHECK(report["schema"] == "life.benchmark-report.v1");
    CHECK(report["benchmarks"].is_array());
    CHECK(report["benchmarks"].size() >= 2);
}
