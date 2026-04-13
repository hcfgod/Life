#include "Editor/Shell/EditorLayoutManager.h"

#include "Assets/Project.h"
#include "Platform/PlatformDetection.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <system_error>

namespace EditorApp
{
    namespace
    {
        constexpr std::string_view kLayoutsDirectoryName = "Layouts";
        constexpr std::string_view kEditorDirectoryName = "Editor";
        constexpr std::string_view kSessionFileName = "LastLayout.json";
        constexpr std::string_view kLayoutFileExtension = ".layout.json";

        std::filesystem::path NormalizeAbsolutePath(const std::filesystem::path& path)
        {
            if (path.empty())
                return {};

            std::error_code ec;
            std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
            if (ec)
                return path.lexically_normal();

            return absolutePath.lexically_normal();
        }

        std::string Trim(std::string value)
        {
            const auto begin = std::find_if_not(value.begin(), value.end(), [](unsigned char character)
            {
                return std::isspace(character) != 0;
            });
            const auto end = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character)
            {
                return std::isspace(character) != 0;
            }).base();

            if (begin >= end)
                return {};

            return std::string(begin, end);
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            return value;
        }

        std::string ScopeToString(EditorLayoutScope scope)
        {
            switch (scope)
            {
            case EditorLayoutScope::Project:
                return "Project";
            case EditorLayoutScope::Global:
            default:
                return "Global";
            }
        }

        EditorLayoutScope ScopeFromString(const std::string& value)
        {
            if (ToLowerAscii(value) == "project")
                return EditorLayoutScope::Project;

            return EditorLayoutScope::Global;
        }

        float ClampProjectAssetsGridScale(float value)
        {
            return std::clamp(value, 0.0f, 1.8f);
        }

        nlohmann::json PanelVisibilityToJson(const EditorPanelVisibility& visibility)
        {
            return {
                { "projectAssets", visibility.ShowProjectAssets },
                { "hierarchy", visibility.ShowHierarchy },
                { "inspector", visibility.ShowInspector },
                { "console", visibility.ShowConsole },
                { "rendererStress", visibility.ShowRendererStress },
                { "stats", visibility.ShowStats },
                { "scene", visibility.ShowScene },
                { "fpsOverlay", visibility.ShowFpsOverlay }
            };
        }

        EditorPanelVisibility PanelVisibilityFromJson(const nlohmann::json& json)
        {
            EditorPanelVisibility visibility = EditorLayoutManager::GetDefaultPanelVisibility();
            if (!json.is_object())
                return visibility;

            if (json.contains("projectAssets") && json["projectAssets"].is_boolean())
                visibility.ShowProjectAssets = json["projectAssets"].get<bool>();
            if (json.contains("hierarchy") && json["hierarchy"].is_boolean())
                visibility.ShowHierarchy = json["hierarchy"].get<bool>();
            if (json.contains("inspector") && json["inspector"].is_boolean())
                visibility.ShowInspector = json["inspector"].get<bool>();
            if (json.contains("console") && json["console"].is_boolean())
                visibility.ShowConsole = json["console"].get<bool>();
            if (json.contains("rendererStress") && json["rendererStress"].is_boolean())
                visibility.ShowRendererStress = json["rendererStress"].get<bool>();
            if (json.contains("stats") && json["stats"].is_boolean())
                visibility.ShowStats = json["stats"].get<bool>();
            if (json.contains("scene") && json["scene"].is_boolean())
                visibility.ShowScene = json["scene"].get<bool>();
            if (json.contains("fpsOverlay") && json["fpsOverlay"].is_boolean())
                visibility.ShowFpsOverlay = json["fpsOverlay"].get<bool>();
            return visibility;
        }

        nlohmann::json PanelStateToJson(const EditorPanelState& state)
        {
            return {
                {
                    "projectAssets",
                    {
                        { "gridScale", ClampProjectAssetsGridScale(state.ProjectAssets.GridScale) }
                    }
                }
            };
        }

        EditorPanelState PanelStateFromJson(const nlohmann::json& json)
        {
            EditorPanelState state;
            if (!json.is_object())
                return state;

            if (json.contains("projectAssets") && json["projectAssets"].is_object())
            {
                const nlohmann::json& projectAssets = json["projectAssets"];
                if (projectAssets.contains("gridScale") && projectAssets["gridScale"].is_number())
                    state.ProjectAssets.GridScale = ClampProjectAssetsGridScale(projectAssets["gridScale"].get<float>());
            }

            return state;
        }

        Life::Result<nlohmann::json> ReadJsonFile(const std::filesystem::path& path)
        {
            std::ifstream stream(path, std::ios::in | std::ios::binary);
            if (!stream.is_open())
            {
                return Life::Result<nlohmann::json>(Life::ErrorCode::FileNotFound,
                                                    "Failed to open editor layout file: " + path.string());
            }

            try
            {
                nlohmann::json root;
                stream >> root;
                return root;
            }
            catch (const std::exception& exception)
            {
                return Life::Result<nlohmann::json>(Life::ErrorCode::FileCorrupted,
                                                    std::string("Failed to parse editor layout JSON: ") + exception.what());
            }
        }

        Life::Result<void> WriteJsonFile(const std::filesystem::path& path, const nlohmann::json& root)
        {
            try
            {
                std::filesystem::create_directories(path.parent_path());
                const std::filesystem::path tempPath = path.string() + ".tmp";
                {
                    std::ofstream stream(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
                    if (!stream.is_open())
                    {
                        return Life::Result<void>(Life::ErrorCode::FileAccessDenied,
                                                  "Failed to open editor layout temp file for writing: " + tempPath.string());
                    }

                    stream << root.dump(4);
                    stream.flush();
                    if (!stream.good())
                    {
                        return Life::Result<void>(Life::ErrorCode::FileAccessDenied,
                                                  "Failed to flush editor layout temp file: " + tempPath.string());
                    }
                }

                std::error_code ec;
                std::filesystem::rename(tempPath, path, ec);
                if (ec)
                {
                    ec.clear();
                    std::filesystem::remove(path, ec);
                    ec.clear();
                    std::filesystem::rename(tempPath, path, ec);
                    if (ec)
                    {
                        return Life::Result<void>(Life::ErrorCode::FileAccessDenied,
                                                  "Failed to replace editor layout file: " + ec.message());
                    }
                }

                return Life::Result<void>();
            }
            catch (const std::exception& exception)
            {
                return Life::Result<void>(Life::ErrorCode::FileAccessDenied,
                                          std::string("Failed to write editor layout file: ") + exception.what());
            }
        }

        nlohmann::json LayoutToJson(const EditorLayoutDefinition& layout)
        {
            return {
                { "version", layout.Version },
                { "name", layout.Name },
                { "scope", ScopeToString(layout.Scope) },
                { "panelVisibility", PanelVisibilityToJson(layout.PanelVisibility) },
                { "panelState", PanelStateToJson(layout.PanelState) },
                { "imguiIni", layout.ImGuiIni }
            };
        }

        Life::Result<EditorLayoutDefinition> LayoutFromJson(const nlohmann::json& root)
        {
            if (!root.is_object())
            {
                return Life::Result<EditorLayoutDefinition>(Life::ErrorCode::FileCorrupted,
                                                            "Editor layout root must be a JSON object");
            }

            EditorLayoutDefinition layout;
            if (root.contains("version") && root["version"].is_number_unsigned())
                layout.Version = root["version"].get<uint32_t>();
            if (layout.Version != EditorLayoutDefinition::CurrentVersion)
            {
                return Life::Result<EditorLayoutDefinition>(Life::ErrorCode::ConfigVersionMismatch,
                                                            "Unsupported editor layout version: " + std::to_string(layout.Version));
            }

            if (root.contains("name") && root["name"].is_string())
                layout.Name = root["name"].get<std::string>();
            if (root.contains("scope") && root["scope"].is_string())
                layout.Scope = ScopeFromString(root["scope"].get<std::string>());
            if (root.contains("panelVisibility"))
                layout.PanelVisibility = PanelVisibilityFromJson(root["panelVisibility"]);
            if (root.contains("panelState"))
                layout.PanelState = PanelStateFromJson(root["panelState"]);
            if (root.contains("imguiIni") && root["imguiIni"].is_string())
                layout.ImGuiIni = root["imguiIni"].get<std::string>();

            layout.Name = EditorLayoutManager::SanitizeLayoutName(layout.Name);
            if (layout.Name.empty())
            {
                return Life::Result<EditorLayoutDefinition>(Life::ErrorCode::ConfigValidationError,
                                                            "Editor layout name must not be empty");
            }

            return layout;
        }

        nlohmann::json SessionToJson(const EditorLayoutSession& session)
        {
            nlohmann::json root = {
                { "version", session.Version },
                { "panelVisibility", PanelVisibilityToJson(session.PanelVisibility) },
                { "panelState", PanelStateToJson(session.PanelState) },
                { "imguiIni", session.ImGuiIni },
                { "useDefaultLayout", session.UseDefaultLayout },
                { "hasActiveLayout", session.HasActiveLayout }
            };
            if (session.HasActiveLayout && session.ActiveLayout.IsValid())
            {
                root["activeLayout"] = {
                    { "name", session.ActiveLayout.Name },
                    { "scope", ScopeToString(session.ActiveLayout.Scope) }
                };
            }
            return root;
        }

        Life::Result<EditorLayoutSession> SessionFromJson(const nlohmann::json& root)
        {
            if (!root.is_object())
            {
                return Life::Result<EditorLayoutSession>(Life::ErrorCode::FileCorrupted,
                                                         "Editor layout session root must be a JSON object");
            }

            EditorLayoutSession session;
            if (root.contains("version") && root["version"].is_number_unsigned())
                session.Version = root["version"].get<uint32_t>();
            if (session.Version != EditorLayoutSession::CurrentVersion)
            {
                return Life::Result<EditorLayoutSession>(Life::ErrorCode::ConfigVersionMismatch,
                                                         "Unsupported editor layout session version: " + std::to_string(session.Version));
            }

            if (root.contains("panelVisibility"))
                session.PanelVisibility = PanelVisibilityFromJson(root["panelVisibility"]);
            if (root.contains("panelState"))
                session.PanelState = PanelStateFromJson(root["panelState"]);
            if (root.contains("imguiIni") && root["imguiIni"].is_string())
                session.ImGuiIni = root["imguiIni"].get<std::string>();
            if (root.contains("useDefaultLayout") && root["useDefaultLayout"].is_boolean())
                session.UseDefaultLayout = root["useDefaultLayout"].get<bool>();
            if (root.contains("hasActiveLayout") && root["hasActiveLayout"].is_boolean())
                session.HasActiveLayout = root["hasActiveLayout"].get<bool>();
            if (session.HasActiveLayout && root.contains("activeLayout") && root["activeLayout"].is_object())
            {
                const nlohmann::json& activeLayout = root["activeLayout"];
                if (activeLayout.contains("name") && activeLayout["name"].is_string())
                    session.ActiveLayout.Name = EditorLayoutManager::SanitizeLayoutName(activeLayout["name"].get<std::string>());
                if (activeLayout.contains("scope") && activeLayout["scope"].is_string())
                    session.ActiveLayout.Scope = ScopeFromString(activeLayout["scope"].get<std::string>());
                session.HasActiveLayout = session.ActiveLayout.IsValid();
            }
            return session;
        }
    }

    bool EditorLayoutId::IsValid() const noexcept
    {
        return !Name.empty();
    }

    void EditorLayoutManager::SetActiveProject(const Life::Assets::Project* project)
    {
        m_ActiveProject = project;
    }

    const Life::Assets::Project* EditorLayoutManager::GetActiveProject() const noexcept
    {
        return m_ActiveProject;
    }

    std::filesystem::path EditorLayoutManager::GetGlobalLayoutsDirectory() const
    {
        const std::filesystem::path userDataPath = NormalizeAbsolutePath(Life::PlatformDetection::GetUserDataPath());
        return userDataPath / kEditorDirectoryName / kLayoutsDirectoryName;
    }

    std::filesystem::path EditorLayoutManager::GetProjectLayoutsDirectory() const
    {
        if (m_ActiveProject == nullptr)
            return {};

        return NormalizeAbsolutePath(m_ActiveProject->Paths.SettingsDirectory) / kEditorDirectoryName / kLayoutsDirectoryName;
    }

    std::filesystem::path EditorLayoutManager::GetLayoutsDirectory(EditorLayoutScope scope) const
    {
        return scope == EditorLayoutScope::Project ? GetProjectLayoutsDirectory() : GetGlobalLayoutsDirectory();
    }

    bool EditorLayoutManager::HasProjectScope() const noexcept
    {
        return m_ActiveProject != nullptr;
    }

    std::vector<EditorLayoutCatalogEntry> EditorLayoutManager::ListLayouts(EditorLayoutScope scope) const
    {
        std::vector<EditorLayoutCatalogEntry> layouts;
        const std::filesystem::path root = GetLayoutsDirectory(scope);
        if (root.empty())
            return layouts;

        std::error_code ec;
        if (!std::filesystem::exists(root, ec) || ec)
            return layouts;

        for (const auto& entry : std::filesystem::directory_iterator(root, ec))
        {
            if (ec)
                break;
            if (!entry.is_regular_file())
                continue;
            if (entry.path().extension() != ".json")
                continue;
            if (entry.path().filename() == kSessionFileName)
                continue;

            EditorLayoutId id;
            id.Scope = scope;
            id.Name = SanitizeLayoutName(entry.path().stem().stem().string());
            if (!id.IsValid())
                continue;

            layouts.push_back(EditorLayoutCatalogEntry{ id, entry.path() });
        }

        std::sort(layouts.begin(), layouts.end(), [](const EditorLayoutCatalogEntry& lhs, const EditorLayoutCatalogEntry& rhs)
        {
            return ToLowerAscii(lhs.Id.Name) < ToLowerAscii(rhs.Id.Name);
        });
        return layouts;
    }

    Life::Result<void> EditorLayoutManager::SaveLayout(const EditorLayoutDefinition& layout) const
    {
        EditorLayoutDefinition normalized = layout;
        normalized.Name = SanitizeLayoutName(normalized.Name);
        if (!EditorLayoutId{ normalized.Scope, normalized.Name }.IsValid())
        {
            return Life::Result<void>(Life::ErrorCode::InvalidArgument,
                                      "Editor layout name must not be empty");
        }

        const std::filesystem::path filePath = BuildLayoutPath(EditorLayoutId{ normalized.Scope, normalized.Name });
        if (filePath.empty())
        {
            return Life::Result<void>(Life::ErrorCode::InvalidState,
                                      "Project-scoped editor layouts require an active project");
        }

        return WriteJsonFile(filePath, LayoutToJson(normalized));
    }

    Life::Result<void> EditorLayoutManager::SaveGlobalSession(const EditorLayoutSession& session) const
    {
        return WriteJsonFile(GetSessionPath(EditorLayoutScope::Global), SessionToJson(session));
    }

    Life::Result<EditorLayoutDefinition> EditorLayoutManager::LoadLayout(const EditorLayoutId& id) const
    {
        if (!id.IsValid())
        {
            return Life::Result<EditorLayoutDefinition>(Life::ErrorCode::InvalidArgument,
                                                        "Editor layout identifier is invalid");
        }

        const std::filesystem::path filePath = BuildLayoutPath(id);
        if (filePath.empty())
        {
            return Life::Result<EditorLayoutDefinition>(Life::ErrorCode::InvalidState,
                                                        "Project-scoped editor layouts require an active project");
        }

        const auto readResult = ReadJsonFile(filePath);
        if (readResult.IsFailure())
            return Life::Result<EditorLayoutDefinition>(readResult.GetError());

        auto layoutResult = LayoutFromJson(readResult.GetValue());
        if (layoutResult.IsFailure())
            return layoutResult;

        layoutResult.GetValue().Scope = id.Scope;
        return layoutResult;
    }

    Life::Result<void> EditorLayoutManager::DeleteLayout(const EditorLayoutId& id) const
    {
        if (!id.IsValid())
        {
            return Life::Result<void>(Life::ErrorCode::InvalidArgument,
                                      "Editor layout identifier is invalid");
        }

        const std::filesystem::path filePath = BuildLayoutPath(id);
        if (filePath.empty())
        {
            return Life::Result<void>(Life::ErrorCode::InvalidState,
                                      "Project-scoped editor layouts require an active project");
        }

        std::error_code ec;
        if (!std::filesystem::exists(filePath, ec) || ec)
        {
            return Life::Result<void>(Life::ErrorCode::FileNotFound,
                                      "Editor layout file was not found: " + filePath.string());
        }

        std::filesystem::remove(filePath, ec);
        if (ec)
        {
            return Life::Result<void>(Life::ErrorCode::FileAccessDenied,
                                      "Failed to delete editor layout file: " + ec.message());
        }

        return Life::Result<void>();
    }

    Life::Result<void> EditorLayoutManager::SaveProjectSession(const EditorLayoutSession& session) const
    {
        if (m_ActiveProject == nullptr)
        {
            return Life::Result<void>(Life::ErrorCode::InvalidState,
                                      "Project layout session save requires an active project");
        }

        return WriteJsonFile(GetSessionPath(EditorLayoutScope::Project), SessionToJson(session));
    }

    Life::Result<EditorLayoutSession> EditorLayoutManager::LoadProjectSession() const
    {
        if (m_ActiveProject == nullptr)
        {
            return Life::Result<EditorLayoutSession>(Life::ErrorCode::InvalidState,
                                                     "Project layout session load requires an active project");
        }

        const auto readResult = ReadJsonFile(GetSessionPath(EditorLayoutScope::Project));
        if (readResult.IsFailure())
            return Life::Result<EditorLayoutSession>(readResult.GetError());
        return SessionFromJson(readResult.GetValue());
    }

    Life::Result<EditorLayoutSession> EditorLayoutManager::LoadGlobalSession() const
    {
        const auto readResult = ReadJsonFile(GetSessionPath(EditorLayoutScope::Global));
        if (readResult.IsFailure())
            return Life::Result<EditorLayoutSession>(readResult.GetError());
        return SessionFromJson(readResult.GetValue());
    }

    EditorPanelVisibility EditorLayoutManager::GetDefaultPanelVisibility() noexcept
    {
        return EditorPanelVisibility{};
    }

    std::string EditorLayoutManager::SanitizeLayoutName(const std::string& value)
    {
        std::string sanitized = Trim(value);
        constexpr char invalidCharacters[] = { '<', '>', ':', '"', '/', '\\', '|', '?', '*' };
        for (char& character : sanitized)
        {
            if (std::find(std::begin(invalidCharacters), std::end(invalidCharacters), character) != std::end(invalidCharacters))
                character = '_';
        }
        return sanitized;
    }

    std::filesystem::path EditorLayoutManager::GetSessionPath(EditorLayoutScope scope) const
    {
        const std::filesystem::path root = GetLayoutsDirectory(scope);
        if (root.empty())
            return {};
        return root / kSessionFileName;
    }

    std::filesystem::path EditorLayoutManager::BuildLayoutPath(const EditorLayoutId& id) const
    {
        const std::filesystem::path root = GetLayoutsDirectory(id.Scope);
        if (root.empty())
            return {};
        return root / (SanitizeLayoutName(id.Name) + std::string(kLayoutFileExtension));
    }
}
