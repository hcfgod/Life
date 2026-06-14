#include "Editor/ProjectHub/EditorProjectHub.h"

#include "Editor/PathSafety.h"
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

        ImVec4 HubBand() { return ImVec4(0.135f, 0.145f, 0.16f, 1.0f); }
        ImVec4 HubPanel() { return ImVec4(0.145f, 0.155f, 0.172f, 1.0f); }
        ImVec4 HubPanelAlt() { return ImVec4(0.165f, 0.177f, 0.197f, 1.0f); }
        ImVec4 HubTextMuted() { return ImVec4(0.60f, 0.64f, 0.70f, 1.0f); }
        ImVec4 HubAccent() { return ImVec4(0.31f, 0.55f, 0.78f, 1.0f); }
        ImVec4 HubAccentHover() { return ImVec4(0.37f, 0.63f, 0.88f, 1.0f); }
        ImVec4 HubAccentActive() { return ImVec4(0.25f, 0.47f, 0.70f, 1.0f); }
        ImVec4 HubDanger() { return ImVec4(0.56f, 0.20f, 0.22f, 1.0f); }
        ImVec4 HubDangerHover() { return ImVec4(0.68f, 0.25f, 0.28f, 1.0f); }
        ImVec4 HubDangerActive() { return ImVec4(0.45f, 0.16f, 0.18f, 1.0f); }
        ImVec4 HubSuccess() { return ImVec4(0.32f, 0.70f, 0.45f, 1.0f); }
        ImVec4 HubWarning() { return ImVec4(0.88f, 0.63f, 0.28f, 1.0f); }

        void PushPrimaryButtonStyle()
        {
            ImGui::PushStyleColor(ImGuiCol_Button, HubAccent());
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HubAccentHover());
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, HubAccentActive());
        }

        void PushDangerButtonStyle()
        {
            ImGui::PushStyleColor(ImGuiCol_Button, HubDanger());
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HubDangerHover());
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, HubDangerActive());
        }

        void DrawSectionTitle(const char* title, const char* description)
        {
            ImGui::TextUnformatted(title);
            ImGui::PushStyleColor(ImGuiCol_Text, HubTextMuted());
            ImGui::TextWrapped("%s", description);
            ImGui::PopStyleColor();
            ImGui::Spacing();
        }

        bool DrawLabeledInput(const char* label, const char* id, std::string& value)
        {
            ImGui::TextUnformatted(label);
            ImGui::SetNextItemWidth(-1.0f);
            return InputTextString(id, value);
        }

        void DrawProjectDimensionButton(const char* label,
                                        Life::Assets::ProjectDimension value,
                                        Life::Assets::ProjectDimension& selected,
                                        float width)
        {
            const bool isSelected = selected == value;
            if (isSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, HubAccent());
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, HubAccentHover());
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, HubAccentActive());
            }

            if (ImGui::Button(label, ImVec2(width, 30.0f)))
                selected = value;

            if (isSelected)
                ImGui::PopStyleColor(3);
        }

        bool DrawPrimaryButton(const char* label, const ImVec2& size)
        {
            PushPrimaryButtonStyle();
            const bool clicked = ImGui::Button(label, size);
            ImGui::PopStyleColor(3);
            return clicked;
        }

        bool DrawDangerButton(const char* label, const ImVec2& size)
        {
            PushDangerButtonStyle();
            const bool clicked = ImGui::Button(label, size);
            ImGui::PopStyleColor(3);
            return clicked;
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
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("Project Hub", nullptr, windowFlags);
        ImGui::PopStyleVar(3);

        RenderHeader();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 18.0f));
        ImGui::BeginChild("##ProjectHubBody", ImVec2(0.0f, 0.0f), false);
        const float contentWidth = ImGui::GetContentRegionAvail().x;
        const float gap = 18.0f;
        const float topCardHeight = 310.0f;

        if (contentWidth >= 820.0f)
        {
            const float createWidth = std::max(420.0f, (contentWidth - gap) * 0.50f);
            const float openWidth = std::max(360.0f, contentWidth - createWidth - gap);
            RenderCreateProjectCard(projectService, didEnterWorkspace, createWidth, topCardHeight);
            ImGui::SameLine(0.0f, gap);
            RenderOpenProjectCard(projectService, didEnterWorkspace, openWidth, topCardHeight);
        }
        else
        {
            RenderCreateProjectCard(projectService, didEnterWorkspace, contentWidth, topCardHeight);
            ImGui::Spacing();
            RenderOpenProjectCard(projectService, didEnterWorkspace, contentWidth, 190.0f);
        }

        ImGui::Spacing();
        RenderRecentProjectsCard(projectService, didEnterWorkspace);
        ImGui::EndChild();
        ImGui::PopStyleVar();
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
        options.Dimension = m_CreateProjectDimension;

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
        {
            if (!PathSafety::IsSafeProjectDeleteTarget(rootDirectory, descriptorPath))
            {
                SetStatusMessage("Project folder deletion was rejected because the descriptor is not inside the selected project root.", true);
                return false;
            }
            std::filesystem::remove_all(rootDirectory, ec);
        }
        else
        {
            if (descriptorPath.extension() != ".lifeproject")
            {
                SetStatusMessage("Project descriptor deletion was rejected because the target is not a .lifeproject file.", true);
                return false;
            }
            std::filesystem::remove(descriptorPath, ec);
        }

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
        constexpr float headerHeight = 104.0f;
        constexpr float headerPaddingX = 22.0f;
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 min = ImGui::GetCursorScreenPos();
        const ImVec2 max = ImVec2(min.x + ImGui::GetContentRegionAvail().x, min.y + headerHeight);
        drawList->AddRectFilled(min, max, ImGui::GetColorU32(HubBand()));
        drawList->AddLine(ImVec2(min.x, max.y), max, ImGui::GetColorU32(ImGuiCol_Border));

        ImGui::SetCursorPos(ImVec2(headerPaddingX, 18.0f));
        ImGui::SetWindowFontScale(1.25f);
        ImGui::TextUnformatted("Life Project Hub");
        ImGui::SetWindowFontScale(1.0f);

        ImGui::SetCursorPos(ImVec2(headerPaddingX, 49.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, HubTextMuted());
        ImGui::TextUnformatted("Create, open, and manage projects before entering the editor workspace.");
        ImGui::PopStyleColor();

        if (!m_StatusMessage.empty())
        {
            const ImVec4 color = m_StatusIsError ? ImVec4(0.88f, 0.30f, 0.32f, 1.0f) : HubSuccess();
            ImGui::SetCursorPosX(headerPaddingX);
            ImGui::PushStyleColor(ImGuiCol_Text, color);
            ImGui::TextWrapped("%s", m_StatusMessage.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::SetCursorPosY(headerHeight);
#endif
    }

    void EditorProjectHub::RenderCreateProjectCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace, float width, float height)
    {
#if __has_include(<imgui.h>)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HubPanel());
        ImGui::BeginChild("CreateProjectCard", ImVec2(width, height), true);
        DrawSectionTitle("Create Project", "Start a new Life project and enter the editor workspace.");
        DrawLabeledInput("Project Name", "##CreateProjectName", m_CreateProjectName);
        DrawLabeledInput("Projects Root", "##CreateProjectsRoot", m_CreateProjectRoot);

        ImGui::TextUnformatted("Project Type");
        const float buttonWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        DrawProjectDimensionButton("2D", Life::Assets::ProjectDimension::TwoD, m_CreateProjectDimension, buttonWidth);
        ImGui::SameLine();
        DrawProjectDimensionButton("3D", Life::Assets::ProjectDimension::ThreeD, m_CreateProjectDimension, buttonWidth);

        const bool canCreate = !m_CreateProjectName.empty() && !m_CreateProjectRoot.empty();
        if (!canCreate)
            ImGui::BeginDisabled();
        if (DrawPrimaryButton("Create and Open", ImVec2(-1.0f, 32.0f)))
            didEnterWorkspace = TryCreateProject(projectService);
        if (!canCreate)
            ImGui::EndDisabled();
        ImGui::EndChild();
        ImGui::PopStyleColor();
#else
        (void)projectService;
        (void)didEnterWorkspace;
        (void)width;
        (void)height;
#endif
    }

    void EditorProjectHub::RenderOpenProjectCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace, float width, float height)
    {
#if __has_include(<imgui.h>)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HubPanel());
        ImGui::BeginChild("OpenProjectCard", ImVec2(width, height), true);
        DrawSectionTitle("Open Project", "Open an existing .lifeproject descriptor or project folder.");
        DrawLabeledInput("Descriptor Path", "##OpenProjectPath", m_OpenProjectPath);
        if (DrawPrimaryButton("Open Project", ImVec2(-1.0f, 32.0f)))
            didEnterWorkspace = TryOpenProject(projectService);
        ImGui::EndChild();
        ImGui::PopStyleColor();
#else
        (void)projectService;
        (void)didEnterWorkspace;
        (void)width;
        (void)height;
#endif
    }

    void EditorProjectHub::RenderRecentProjectsCard(Life::Assets::ProjectService& projectService, bool& didEnterWorkspace)
    {
#if __has_include(<imgui.h>)
        ImGui::PushStyleColor(ImGuiCol_ChildBg, HubPanel());
        const float recentHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
        ImGui::BeginChild("RecentProjectsCard", ImVec2(0.0f, recentHeight), true);
        DrawSectionTitle("Recent Projects", "Resume work from projects tracked on this machine.");
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
                const ImVec2 rowStart = ImGui::GetCursorScreenPos();
                const float rowWidth = ImGui::GetContentRegionAvail().x;
                const float rowHeight = 78.0f;
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                drawList->AddRectFilled(rowStart,
                                        ImVec2(rowStart.x + rowWidth, rowStart.y + rowHeight),
                                        ImGui::GetColorU32(HubPanelAlt()),
                                        4.0f);
                drawList->AddRect(rowStart,
                                  ImVec2(rowStart.x + rowWidth, rowStart.y + rowHeight),
                                  ImGui::GetColorU32(ImGuiCol_Border),
                                  4.0f);

                constexpr float rowPaddingX = 14.0f;
                ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + rowPaddingX, ImGui::GetCursorPosY() + 10.0f));
                ImGui::TextUnformatted(projectLabel.c_str());

                const char* availabilityLabel = project.Exists ? "Available" : "Missing";
                const ImVec4 availabilityColor = project.Exists ? HubSuccess() : HubWarning();
                ImGui::SameLine();
                ImGui::PushStyleColor(ImGuiCol_Text, availabilityColor);
                ImGui::TextUnformatted(availabilityLabel);
                ImGui::PopStyleColor();

                const float actionsWidth = 252.0f;
                ImGui::SetCursorScreenPos(ImVec2(rowStart.x + rowPaddingX, rowStart.y + 36.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, HubTextMuted());
                ImGui::PushTextWrapPos(rowStart.x + rowWidth - actionsWidth - 28.0f);
                ImGui::TextWrapped("%s", PathToUiString(project.DescriptorPath).c_str());
                ImGui::PopTextWrapPos();
                ImGui::PopStyleColor();

                const float actionY = rowStart.y + 23.0f;
                ImGui::SetCursorScreenPos(ImVec2(rowStart.x + rowWidth - actionsWidth - 12.0f, actionY));
                if (!project.Exists)
                    ImGui::BeginDisabled();
                if (DrawPrimaryButton("Open", ImVec2(62.0f, 28.0f)))
                {
                    m_OpenProjectPath = PathToUiString(project.DescriptorPath);
                    didEnterWorkspace = TryOpenProject(projectService);
                }
                if (!project.Exists)
                    ImGui::EndDisabled();
                ImGui::SameLine();
                if (DrawDangerButton("Delete", ImVec2(68.0f, 28.0f)))
                    QueueDeleteProject(project);
                ImGui::SameLine();
                bool removeFromList = false;
                if (ImGui::Button("Remove", ImVec2(82.0f, 28.0f)))
                    removeFromList = true;
                ImGui::SetCursorScreenPos(ImVec2(rowStart.x, rowStart.y + rowHeight + 8.0f));
                ImGui::Dummy(ImVec2(rowWidth, 1.0f));
                ImGui::PopID();
                if (removeFromList)
                {
                    RemoveRecentProject(project.DescriptorPath);
                    break;
                }
            }
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
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
