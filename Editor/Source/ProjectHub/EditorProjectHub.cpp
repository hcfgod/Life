#include "Editor/ProjectHub/EditorProjectHub.h"

#include "Platform/PlatformDetection.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <system_error>
#include <utility>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    namespace
    {
        constexpr const char* kRecentProjectsFileName = "RecentProjects.json";

        std::string PathToUiString(const std::filesystem::path& path)
        {
            std::filesystem::path preferred = path;
            preferred.make_preferred();
            return preferred.string();
        }

#if __has_include(<imgui.h>)
        bool InputTextString(const char* label, std::string& value)
        {
            std::array<char, 1024> buffer{};
            const std::size_t copyLength = std::min(value.size(), buffer.size() - 1);
            std::memcpy(buffer.data(), value.data(), copyLength);
            buffer[copyLength] = '\0';

            if (!ImGui::InputText(label, buffer.data(), buffer.size()))
                return false;

            value = buffer.data();
            return true;
        }
#endif
    }

    void EditorProjectHub::Attach()
    {
        LoadRecentProjects();
        if (m_CreateProjectRoot.empty())
        {
            const std::filesystem::path defaultRoot = std::filesystem::path(Life::PlatformDetection::GetUserDataPath()) / "Projects";
            m_CreateProjectRoot = PathToUiString(defaultRoot);
        }
    }

    void EditorProjectHub::Detach()
    {
        m_OpenDeletePopup = false;
        m_DeleteProjectFiles = true;
        m_QueuedDeleteProject = {};
    }

    void EditorProjectHub::RefreshRecentProjects()
    {
        LoadRecentProjects();
    }

    void EditorProjectHub::SetStatusMessage(std::string message, bool isError)
    {
        m_StatusMessage = std::move(message);
        m_StatusIsError = isError;
    }

    bool EditorProjectHub::Render(Life::Assets::ProjectService& projectService)
    {
        bool didEnterWorkspace = false;

#if __has_include(<imgui.h>)
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);

        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoCollapse |
                                       ImGuiWindowFlags_NoResize |
                                       ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoDocking |
                                       ImGuiWindowFlags_NoTitleBar;

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::Begin("Project Hub", nullptr, windowFlags);
        ImGui::PopStyleVar(2);

        RenderHeader();
        ImGui::Spacing();
        ImGui::Columns(2, "ProjectHubColumns", false);
        RenderCreateProjectCard(projectService, didEnterWorkspace);
        ImGui::NextColumn();
        RenderOpenProjectCard(projectService, didEnterWorkspace);
        ImGui::Columns(1);
        ImGui::Spacing();
        RenderRecentProjectsCard(projectService, didEnterWorkspace);
        RenderDeletePopup(projectService);
        ImGui::End();
#else
        (void)projectService;
#endif

        return didEnterWorkspace;
    }

    void EditorProjectHub::LoadRecentProjects()
    {
        m_RecentProjects.clear();

        const std::filesystem::path filePath = GetRecentProjectsFilePath();
        std::ifstream stream(filePath, std::ios::in | std::ios::binary);
        if (!stream.is_open())
            return;

        try
        {
            nlohmann::json root;
            stream >> root;
            if (!root.is_object() || !root.contains("projects") || !root["projects"].is_array())
                return;

            for (const auto& entry : root["projects"])
            {
                if (!entry.is_object() || !entry.contains("descriptorPath") || !entry["descriptorPath"].is_string())
                    continue;

                const std::filesystem::path descriptorPath(entry["descriptorPath"].get<std::string>());
                m_RecentProjects.push_back(BuildRecentProjectEntry(descriptorPath));
            }
        }
        catch (...)
        {
            m_RecentProjects.clear();
        }
    }

    void EditorProjectHub::SaveRecentProjects() const
    {
        const std::filesystem::path filePath = GetRecentProjectsFilePath();
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);

        nlohmann::json root;
        root["projects"] = nlohmann::json::array();
        for (const RecentProjectEntry& project : m_RecentProjects)
        {
            root["projects"].push_back({
                { "name", project.Name },
                { "descriptorPath", project.DescriptorPath.string() }
            });
        }

        std::ofstream stream(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!stream.is_open())
            return;

        stream << root.dump(4);
    }

    void EditorProjectHub::RecordRecentProject(const Life::Assets::Project& project)
    {
        const std::filesystem::path descriptorPath = NormalizePath(project.Paths.DescriptorPath);
        m_RecentProjects.erase(
            std::remove_if(
                m_RecentProjects.begin(),
                m_RecentProjects.end(),
                [&](const RecentProjectEntry& entry)
                {
                    return NormalizePath(entry.DescriptorPath) == descriptorPath;
                }),
            m_RecentProjects.end());

        m_RecentProjects.insert(m_RecentProjects.begin(), BuildRecentProjectEntry(descriptorPath));
        if (m_RecentProjects.size() > 12)
            m_RecentProjects.resize(12);

        SaveRecentProjects();
    }

    void EditorProjectHub::RemoveRecentProject(const std::filesystem::path& descriptorPath)
    {
        const std::filesystem::path normalized = NormalizePath(descriptorPath);
        m_RecentProjects.erase(
            std::remove_if(
                m_RecentProjects.begin(),
                m_RecentProjects.end(),
                [&](const RecentProjectEntry& entry)
                {
                    return NormalizePath(entry.DescriptorPath) == normalized;
                }),
            m_RecentProjects.end());
        SaveRecentProjects();
    }

    void EditorProjectHub::QueueDeleteProject(const RecentProjectEntry& project)
    {
        m_QueuedDeleteProject = project;
        m_DeleteProjectFiles = true;
        m_OpenDeletePopup = true;
    }

    bool EditorProjectHub::TryCreateProject(Life::Assets::ProjectService& projectService)
    {
        Life::Assets::ProjectCreateOptions options;
        options.Name = m_CreateProjectName;
        options.RootDirectory = std::filesystem::path(m_CreateProjectRoot) / m_CreateProjectName;

        const auto createResult = projectService.CreateProject(options, true);
        if (createResult.IsFailure())
        {
            SetStatusMessage(createResult.GetError().GetErrorMessage(), true);
            return false;
        }

        RecordRecentProject(createResult.GetValue());
        m_OpenProjectPath.clear();
        m_CreateProjectName.clear();
        SetStatusMessage("Created project '" + createResult.GetValue().Descriptor.Name + "'.", false);
        return true;
    }

    bool EditorProjectHub::TryOpenProject(Life::Assets::ProjectService& projectService)
    {
        const std::filesystem::path descriptorPath = ResolveDescriptorPath(m_OpenProjectPath);
        if (descriptorPath.empty())
        {
            SetStatusMessage("Enter a .lifeproject file path to open a project.", true);
            return false;
        }

        const auto openResult = projectService.OpenProject(descriptorPath);
        if (openResult.IsFailure())
        {
            SetStatusMessage(openResult.GetError().GetErrorMessage(), true);
            return false;
        }

        RecordRecentProject(openResult.GetValue());
        SetStatusMessage("Opened project '" + openResult.GetValue().Descriptor.Name + "'.", false);
        return true;
    }

    bool EditorProjectHub::TryDeleteQueuedProject(Life::Assets::ProjectService& projectService)
    {
        if (m_QueuedDeleteProject.DescriptorPath.empty())
            return false;

        const std::filesystem::path descriptorPath = NormalizePath(m_QueuedDeleteProject.DescriptorPath);
        const std::filesystem::path rootDirectory = NormalizePath(m_QueuedDeleteProject.RootDirectory);

        if (projectService.HasActiveProject() &&
            NormalizePath(projectService.GetActiveProject().Paths.DescriptorPath) == descriptorPath)
        {
            const auto closeResult = projectService.CloseProject();
            if (closeResult.IsFailure())
            {
                SetStatusMessage(closeResult.GetError().GetErrorMessage(), true);
                return false;
            }
        }

        std::error_code ec;
        if (m_DeleteProjectFiles)
            std::filesystem::remove_all(rootDirectory, ec);
        else
            std::filesystem::remove(descriptorPath, ec);

        if (ec)
        {
            SetStatusMessage("Failed to delete project: " + ec.message(), true);
            return false;
        }

        RemoveRecentProject(descriptorPath);
        SetStatusMessage("Deleted project '" + m_QueuedDeleteProject.Name + "'.", false);
        m_QueuedDeleteProject = {};
        return true;
    }

    void EditorProjectHub::RenderHeader()
    {
#if __has_include(<imgui.h>)
        ImGui::TextUnformatted("Life Project Hub");
        ImGui::TextDisabled("Create, open, and manage projects before entering the editor workspace.");
        if (!m_StatusMessage.empty())
        {
            const ImVec4 color = m_StatusIsError ? ImVec4(0.90f, 0.35f, 0.35f, 1.0f) : ImVec4(0.35f, 0.85f, 0.50f, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", m_StatusMessage.c_str());
            ImGui::PopStyleColor();
        }
#endif
    }

    void EditorProjectHub::RenderCreateProjectCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace)
    {
#if __has_include(<imgui.h>)
        ImGui::BeginChild("CreateProjectCard", ImVec2(0.0f, 240.0f), true);
        ImGui::TextUnformatted("Create Project");
        ImGui::Separator();
        InputTextString("Project Name", m_CreateProjectName);
        InputTextString("Projects Root", m_CreateProjectRoot);

        const bool canCreate = !m_CreateProjectName.empty() && !m_CreateProjectRoot.empty();
        if (!canCreate)
            ImGui::BeginDisabled();
        if (ImGui::Button("Create and Open", ImVec2(-1.0f, 0.0f)))
            didEnterWorkspace = TryCreateProject(projectService);
        if (!canCreate)
            ImGui::EndDisabled();
        ImGui::EndChild();
#else
        (void)projectService;
        (void)didEnterWorkspace;
#endif
    }

    void EditorProjectHub::RenderOpenProjectCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace)
    {
#if __has_include(<imgui.h>)
        ImGui::BeginChild("OpenProjectCard", ImVec2(0.0f, 240.0f), true);
        ImGui::TextUnformatted("Open Project");
        ImGui::Separator();
        InputTextString("Descriptor Path", m_OpenProjectPath);
        if (ImGui::Button("Open Project", ImVec2(-1.0f, 0.0f)))
            didEnterWorkspace = TryOpenProject(projectService);
        ImGui::EndChild();
#else
        (void)projectService;
        (void)didEnterWorkspace;
#endif
    }

    void EditorProjectHub::RenderRecentProjectsCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace)
    {
#if __has_include(<imgui.h>)
        ImGui::BeginChild("RecentProjectsCard", ImVec2(0.0f, 0.0f), true);
        ImGui::TextUnformatted("Recent Projects");
        ImGui::Separator();

        if (m_RecentProjects.empty())
        {
            ImGui::TextDisabled("No recent projects yet.");
        }
        else
        {
            for (std::size_t index = 0; index < m_RecentProjects.size(); ++index)
            {
                RecentProjectEntry& project = m_RecentProjects[index];
                project = BuildRecentProjectEntry(project.DescriptorPath);
                const std::string projectLabel = project.Name.empty()
                    ? project.DescriptorPath.filename().string()
                    : project.Name;

                ImGui::PushID(static_cast<int>(index));
                ImGui::BeginGroup();
                ImGui::TextUnformatted(projectLabel.c_str());
                ImGui::TextDisabled("%s", PathToUiString(project.DescriptorPath).c_str());
                ImGui::TextDisabled("%s", project.Exists ? "Available" : "Missing");
                if (!project.Exists)
                    ImGui::BeginDisabled();
                if (ImGui::Button("Open"))
                {
                    m_OpenProjectPath = PathToUiString(project.DescriptorPath);
                    didEnterWorkspace = TryOpenProject(projectService);
                }
                if (!project.Exists)
                    ImGui::EndDisabled();
                ImGui::SameLine();
                if (ImGui::Button("Delete"))
                    QueueDeleteProject(project);
                ImGui::SameLine();
                if (ImGui::Button("Remove From List"))
                {
                    RemoveRecentProject(project.DescriptorPath);
                    ImGui::EndGroup();
                    ImGui::PopID();
                    break;
                }
                ImGui::Separator();
                ImGui::EndGroup();
                ImGui::PopID();
            }
        }

        ImGui::EndChild();
#else
        (void)projectService;
        (void)didEnterWorkspace;
#endif
    }

    void EditorProjectHub::RenderDeletePopup(Life::Assets::ProjectService& projectService)
    {
#if __has_include(<imgui.h>)
        if (m_OpenDeletePopup)
        {
            ImGui::OpenPopup("Delete Project");
            m_OpenDeletePopup = false;
        }

        if (ImGui::BeginPopupModal("Delete Project", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextWrapped("Delete project '%s'?", m_QueuedDeleteProject.Name.c_str());
            ImGui::Checkbox("Delete entire project folder", &m_DeleteProjectFiles);
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
            {
                m_QueuedDeleteProject = {};
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Delete", ImVec2(120.0f, 0.0f)))
            {
                if (TryDeleteQueuedProject(projectService))
                    ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
#else
        (void)projectService;
#endif
    }

    std::filesystem::path EditorProjectHub::GetRecentProjectsFilePath() const
    {
        std::filesystem::path userDataPath = Life::PlatformDetection::GetUserDataPath();
        if (userDataPath.empty())
            userDataPath = std::filesystem::temp_directory_path();

        return userDataPath / "Editor" / kRecentProjectsFileName;
    }

    std::filesystem::path EditorProjectHub::NormalizePath(const std::filesystem::path& path) const
    {
        if (path.empty())
            return {};

        std::error_code ec;
        const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, ec);
        if (!ec)
            return canonical;

        ec.clear();
        const std::filesystem::path absolute = std::filesystem::absolute(path, ec);
        if (!ec)
            return absolute.lexically_normal();

        return path.lexically_normal();
    }

    std::filesystem::path EditorProjectHub::ResolveDescriptorPath(const std::filesystem::path& inputPath) const
    {
        if (inputPath.empty())
            return {};

        std::filesystem::path resolved = NormalizePath(inputPath);
        const std::filesystem::path descriptorExtension{ std::string(Life::Assets::ProjectDescriptorFileExtension) };

        if (resolved.extension() == descriptorExtension)
            return resolved;

        std::error_code ec;
        if (std::filesystem::exists(resolved, ec) && std::filesystem::is_directory(resolved, ec))
        {
            for (const auto& entry : std::filesystem::directory_iterator(resolved, ec))
            {
                if (ec)
                    break;

                if (!entry.is_regular_file())
                    continue;

                if (entry.path().extension() == descriptorExtension)
                    return NormalizePath(entry.path());
            }

            const std::string defaultDescriptorName = resolved.filename().string() + std::string(Life::Assets::ProjectDescriptorFileExtension);
            return resolved / defaultDescriptorName;
        }

        if (!resolved.has_extension())
            resolved += std::string(Life::Assets::ProjectDescriptorFileExtension);

        return NormalizePath(resolved);
    }

    EditorProjectHub::RecentProjectEntry EditorProjectHub::BuildRecentProjectEntry(const std::filesystem::path& descriptorPath) const
    {
        RecentProjectEntry entry;
        entry.DescriptorPath = NormalizePath(descriptorPath);
        entry.RootDirectory = entry.DescriptorPath.parent_path();
        entry.Exists = std::filesystem::exists(entry.DescriptorPath);
        entry.Name = entry.DescriptorPath.stem().string();

        const auto loadResult = Life::Assets::ProjectSerializer::Load(entry.DescriptorPath);
        if (loadResult.IsSuccess())
        {
            entry.Name = loadResult.GetValue().Descriptor.Name;
            entry.RootDirectory = loadResult.GetValue().Paths.RootDirectory;
            entry.Exists = true;
        }

        return entry;
    }
}
