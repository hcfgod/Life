#pragma once

#include "Core/Error.h"
#include "Editor/Shell/EditorShellTypes.h"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Life::Assets
{
    struct Project;
}

namespace EditorApp
{
    enum class EditorLayoutScope : uint8_t
    {
        Global = 0,
        Project = 1
    };

    struct EditorLayoutId
    {
        EditorLayoutScope Scope = EditorLayoutScope::Global;
        std::string Name;

        bool IsValid() const noexcept;
    };

    struct EditorLayoutDefinition
    {
        static constexpr uint32_t CurrentVersion = 1;

        uint32_t Version = CurrentVersion;
        std::string Name;
        EditorLayoutScope Scope = EditorLayoutScope::Global;
        EditorPanelVisibility PanelVisibility;
        std::string ImGuiIni;
    };

    struct EditorLayoutSession
    {
        static constexpr uint32_t CurrentVersion = 1;

        uint32_t Version = CurrentVersion;
        EditorPanelVisibility PanelVisibility;
        std::string ImGuiIni;
        bool UseDefaultLayout = true;
        bool HasActiveLayout = false;
        EditorLayoutId ActiveLayout;
    };

    struct EditorLayoutCatalogEntry
    {
        EditorLayoutId Id;
        std::filesystem::path FilePath;
    };

    class EditorLayoutManager
    {
    public:
        EditorLayoutManager() = default;

        void SetActiveProject(const Life::Assets::Project* project);
        const Life::Assets::Project* GetActiveProject() const noexcept;

        std::filesystem::path GetGlobalLayoutsDirectory() const;
        std::filesystem::path GetProjectLayoutsDirectory() const;
        std::filesystem::path GetLayoutsDirectory(EditorLayoutScope scope) const;
        bool HasProjectScope() const noexcept;

        std::vector<EditorLayoutCatalogEntry> ListLayouts(EditorLayoutScope scope) const;

        Life::Result<void> SaveLayout(const EditorLayoutDefinition& layout) const;
        Life::Result<EditorLayoutDefinition> LoadLayout(const EditorLayoutId& id) const;
        Life::Result<void> DeleteLayout(const EditorLayoutId& id) const;

        Life::Result<void> SaveGlobalSession(const EditorLayoutSession& session) const;
        Life::Result<void> SaveProjectSession(const EditorLayoutSession& session) const;
        Life::Result<EditorLayoutSession> LoadProjectSession() const;
        Life::Result<EditorLayoutSession> LoadGlobalSession() const;

        static EditorPanelVisibility GetDefaultPanelVisibility() noexcept;
        static std::string SanitizeLayoutName(const std::string& value);

    private:
        std::filesystem::path GetSessionPath(EditorLayoutScope scope) const;
        std::filesystem::path BuildLayoutPath(const EditorLayoutId& id) const;

        const Life::Assets::Project* m_ActiveProject = nullptr;
    };
}
