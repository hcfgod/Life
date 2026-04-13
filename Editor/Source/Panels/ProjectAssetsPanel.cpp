#include "Editor/Panels/ProjectAssetsPanel.h"

#include "Editor/EditorServices.h"
#include "Editor/Panels/ProjectAssetDragDrop.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    namespace
    {
        constexpr float kMinGridScale = 0.0f;
        constexpr float kMaxGridScale = 1.8f;

        enum class ProjectEntryKind
        {
            Directory,
            Scene,
            Texture,
            Material,
            Shader,
            Other
        };

        struct ProjectAssetEntry
        {
            std::filesystem::path AbsolutePath;
            std::filesystem::path RelativePath;
            std::string DisplayName;
            std::string LowerFileName;
            ProjectEntryKind Kind = ProjectEntryKind::Other;
            bool IsDirectory = false;
        };

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

         bool InputTextStringWithHint(const char* label, const char* hint, std::string& value)
         {
             std::array<char, 1024> buffer{};
             const std::size_t copyLength = std::min(value.size(), buffer.size() - 1);
             std::memcpy(buffer.data(), value.data(), copyLength);
             buffer[copyLength] = '\0';

             if (!ImGui::InputTextWithHint(label, hint, buffer.data(), buffer.size()))
                 return false;

             value = buffer.data();
             return true;
         }
#endif

         float ClampGridScale(float value)
         {
             return std::clamp(value, kMinGridScale, kMaxGridScale);
         }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            return value;
        }

        std::string PathToUiString(const std::filesystem::path& path)
        {
            std::filesystem::path preferred = path;
            preferred.make_preferred();
            return preferred.string();
        }

        bool StartsWithDotDot(const std::filesystem::path& path)
        {
            const std::string text = path.generic_string();
            return text == ".." || text.rfind("../", 0) == 0;
        }

        bool IsSameOrDescendant(const std::filesystem::path& path, const std::filesystem::path& ancestor)
        {
            if (path == ancestor)
                return true;

            const std::filesystem::path relative = path.lexically_relative(ancestor);
            return !relative.empty() && !StartsWithDotDot(relative);
        }

        std::filesystem::path RebasePath(const std::filesystem::path& path, const std::filesystem::path& oldPrefix, const std::filesystem::path& newPrefix)
        {
            if (!IsSameOrDescendant(path, oldPrefix))
                return path;

            if (path == oldPrefix)
                return newPrefix;

            return (newPrefix / path.lexically_relative(oldPrefix)).lexically_normal();
        }

        bool IsPathInside(const std::filesystem::path& root, const std::filesystem::path& candidate)
        {
            std::error_code ec;
            const std::filesystem::path normalizedRoot = std::filesystem::weakly_canonical(root, ec);
            if (ec)
                return false;

            ec.clear();
            const std::filesystem::path normalizedCandidate = std::filesystem::weakly_canonical(candidate, ec);
            if (ec)
                return false;

            const std::string rootText = normalizedRoot.generic_string();
            const std::string candidateText = normalizedCandidate.generic_string();
            return candidateText == rootText || candidateText.rfind(rootText + "/", 0) == 0;
        }

        std::string MakeAssetKey(const std::filesystem::path& relativePath)
        {
            if (relativePath.empty())
                return "Assets";

            return "Assets/" + relativePath.generic_string();
        }

        bool EndsWith(std::string_view value, std::string_view suffix)
        {
            return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
        }

        ProjectEntryKind ClassifyEntry(const std::filesystem::path& path, bool isDirectory)
        {
            if (isDirectory)
                return ProjectEntryKind::Directory;

            const std::string lowerName = ToLowerAscii(path.filename().string());
            const std::string lowerExtension = ToLowerAscii(path.extension().string());
            if (EndsWith(lowerName, ".scene") || EndsWith(lowerName, ".scene.json"))
                return ProjectEntryKind::Scene;
            if (EndsWith(lowerName, ".material.json"))
                return ProjectEntryKind::Material;
            if (lowerExtension == ".glsl" || lowerExtension == ".vert" || lowerExtension == ".frag")
                return ProjectEntryKind::Shader;
            if (lowerExtension == ".png" || lowerExtension == ".jpg" || lowerExtension == ".jpeg" || lowerExtension == ".bmp" ||
                lowerExtension == ".tga" || lowerExtension == ".hdr" || lowerExtension == ".psd" || lowerExtension == ".gif" ||
                lowerExtension == ".ppm" || lowerExtension == ".pnm")
            {
                return ProjectEntryKind::Texture;
            }

            return ProjectEntryKind::Other;
        }

        std::string ResolveSuffixForRename(const ProjectAssetEntry& entry)
        {
            if (entry.IsDirectory)
                return {};

            const std::string& fileName = entry.DisplayName;
            const std::string lowerName = entry.LowerFileName;
            constexpr std::array<std::string_view, 11> specialSuffixes{
                ".scene.json",
                ".material.json",
                ".prefab.json",
                ".tilemap.json",
                ".tileset.json",
                ".tile.json",
                ".tilepalette.json",
                ".animationclip.json",
                ".animation.json",
                ".anim.json",
                ".animcontroller.json"
            };

            for (const std::string_view suffix : specialSuffixes)
            {
                if (EndsWith(lowerName, suffix))
                    return fileName.substr(fileName.size() - suffix.size());
            }

            return entry.AbsolutePath.extension().string();
        }

        std::string ResolveDisplayStem(const ProjectAssetEntry& entry)
        {
            if (entry.IsDirectory)
                return entry.DisplayName;

            const std::string suffix = ResolveSuffixForRename(entry);
            if (!suffix.empty() && entry.DisplayName.size() > suffix.size() && EndsWith(entry.DisplayName, suffix))
                return entry.DisplayName.substr(0, entry.DisplayName.size() - suffix.size());

            return entry.AbsolutePath.stem().string();
        }

        std::string SanitizeName(std::string value)
        {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char character)
            {
                return std::isspace(character) == 0;
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char character)
            {
                return std::isspace(character) == 0;
            }).base(), value.end());

            constexpr std::array<char, 9> invalidCharacters{ '<', '>', ':', '"', '/', '\\', '|', '?', '*' };
            for (char& character : value)
            {
                if (std::find(invalidCharacters.begin(), invalidCharacters.end(), character) != invalidCharacters.end())
                    character = '_';
            }

            return value;
        }

        std::vector<ProjectAssetEntry> CollectEntries(const std::filesystem::path& assetsDirectory, const std::filesystem::path& relativeFolder)
        {
            std::vector<ProjectAssetEntry> entries;
            std::error_code ec;
            const std::filesystem::path folderPath = relativeFolder.empty() ? assetsDirectory : (assetsDirectory / relativeFolder);
            if (!std::filesystem::exists(folderPath, ec) || !std::filesystem::is_directory(folderPath, ec))
                return entries;

            for (const std::filesystem::directory_entry& child : std::filesystem::directory_iterator(folderPath, ec))
            {
                if (ec)
                    break;

                ProjectAssetEntry entry;
                entry.AbsolutePath = child.path();
                entry.RelativePath = std::filesystem::relative(child.path(), assetsDirectory, ec).lexically_normal();
                if (ec)
                    continue;

                entry.IsDirectory = child.is_directory(ec);
                if (ec)
                    continue;

                entry.DisplayName = child.path().filename().string();
                entry.LowerFileName = ToLowerAscii(entry.DisplayName);
                entry.Kind = ClassifyEntry(child.path(), entry.IsDirectory);
                entries.push_back(std::move(entry));
            }

            std::sort(entries.begin(), entries.end(), [](const ProjectAssetEntry& left, const ProjectAssetEntry& right)
            {
                if (left.IsDirectory != right.IsDirectory)
                    return left.IsDirectory > right.IsDirectory;
                return left.LowerFileName < right.LowerFileName;
            });
            return entries;
        }

        bool MatchesFilter(const ProjectAssetEntry& entry, const std::string& filterLower)
        {
            if (filterLower.empty())
                return true;

            if (entry.LowerFileName.find(filterLower) != std::string::npos)
                return true;

            return ToLowerAscii(entry.RelativePath.generic_string()).find(filterLower) != std::string::npos;
        }

        bool DirectoryContainsMatch(const std::filesystem::path& assetsDirectory, const std::filesystem::path& relativeFolder, const std::string& filterLower)
        {
            if (filterLower.empty())
                return true;

            const auto entries = CollectEntries(assetsDirectory, relativeFolder);
            for (const ProjectAssetEntry& entry : entries)
            {
                if (MatchesFilter(entry, filterLower))
                    return true;
                if (entry.IsDirectory && DirectoryContainsMatch(assetsDirectory, entry.RelativePath, filterLower))
                    return true;
            }
            return false;
        }

        const char* ResolveBadge(ProjectEntryKind kind)
        {
            switch (kind)
            {
                case ProjectEntryKind::Directory: return "DIR";
                case ProjectEntryKind::Scene: return "SCN";
                case ProjectEntryKind::Texture: return "TEX";
                case ProjectEntryKind::Material: return "MAT";
                case ProjectEntryKind::Shader: return "SHD";
                case ProjectEntryKind::Other: return "FILE";
            }

            return "FILE";
        }

#if __has_include(<imgui.h>)
         ImVec4 ResolveAccentColor(ProjectEntryKind kind)
         {
             switch (kind)
             {
                 case ProjectEntryKind::Directory: return ImVec4(0.38f, 0.62f, 0.96f, 1.0f);
                 case ProjectEntryKind::Scene: return ImVec4(0.40f, 0.82f, 0.60f, 1.0f);
                 case ProjectEntryKind::Texture: return ImVec4(0.88f, 0.58f, 0.36f, 1.0f);
                 case ProjectEntryKind::Material: return ImVec4(0.74f, 0.52f, 0.92f, 1.0f);
                 case ProjectEntryKind::Shader: return ImVec4(0.96f, 0.72f, 0.36f, 1.0f);
                 case ProjectEntryKind::Other:
                 default: return ImVec4(0.62f, 0.66f, 0.76f, 1.0f);
             }
         }
#endif

        bool LoadSceneIntoEditor(const std::filesystem::path& sceneIdentifier, const EditorServices& services, EditorSceneState& sceneState)
        {
            if (!services.SceneService)
                return false;

            const auto result = services.SceneService->get().LoadScene(sceneIdentifier);
            if (result.IsFailure())
            {
                sceneState.SetStatusMessage(result.GetError().GetErrorMessage(), true);
                return false;
            }

            sceneState.ClearSelection();
            sceneState.SetStatusMessage("Opened scene '" + services.SceneService->get().GetActiveScene().GetName() + "'.", false);
            return true;
        }

        bool SaveStartupScene(const std::string& assetKey, const EditorServices& services, EditorSceneState& sceneState)
        {
            if (!services.ProjectService || !services.ProjectService->get().HasActiveProject())
                return false;

            services.ProjectService->get().GetActiveProject().Descriptor.Startup.Scene = assetKey;
            const auto saveResult = services.ProjectService->get().SaveProject();
            if (saveResult.IsFailure())
            {
                sceneState.SetStatusMessage(saveResult.GetError().GetErrorMessage(), true);
                return false;
            }

            sceneState.SetStatusMessage("Set startup scene to '" + assetKey + "'.", false);
            return true;
        }

        bool CreateFolder(const std::filesystem::path& assetsDirectory, const std::filesystem::path& parentRelativePath, const std::string& folderName, EditorSceneState& sceneState)
        {
            const std::string sanitizedName = SanitizeName(folderName);
            if (sanitizedName.empty())
            {
                sceneState.SetStatusMessage("Folder name must not be empty.", true);
                return false;
            }

            const std::filesystem::path targetPath = assetsDirectory / parentRelativePath / sanitizedName;
            if (!IsPathInside(assetsDirectory, targetPath.parent_path()))
            {
                sceneState.SetStatusMessage("Folder target must remain inside the project Assets directory.", true);
                return false;
            }

            std::error_code ec;
            if (std::filesystem::exists(targetPath, ec))
            {
                sceneState.SetStatusMessage("A file or folder with that name already exists.", true);
                return false;
            }

            if (!std::filesystem::create_directories(targetPath, ec) || ec)
            {
                sceneState.SetStatusMessage("Failed to create folder '" + PathToUiString(targetPath) + "'.", true);
                return false;
            }

            sceneState.SetStatusMessage("Created folder '" + sanitizedName + "'.", false);
            return true;
        }

        bool CreateSceneAsset(const std::filesystem::path& assetsDirectory, const std::filesystem::path& parentRelativePath, const std::string& sceneName, EditorSceneState& sceneState)
        {
            const std::string sanitizedName = SanitizeName(sceneName);
            if (sanitizedName.empty())
            {
                sceneState.SetStatusMessage("Scene name must not be empty.", true);
                return false;
            }

            const std::filesystem::path targetPath = assetsDirectory / parentRelativePath / (sanitizedName + ".scene");
            if (!IsPathInside(assetsDirectory, targetPath.parent_path()))
            {
                sceneState.SetStatusMessage("Scene target must remain inside the project Assets directory.", true);
                return false;
            }

            std::error_code ec;
            if (std::filesystem::exists(targetPath, ec))
            {
                sceneState.SetStatusMessage("A scene with that name already exists.", true);
                return false;
            }

            Life::Scene scene(sanitizedName);
            scene.SetState(Life::Scene::State::Ready);
            const auto saveResult = Life::SceneSerializer::Save(scene, targetPath);
            if (saveResult.IsFailure())
            {
                sceneState.SetStatusMessage(saveResult.GetError().GetErrorMessage(), true);
                return false;
            }

            sceneState.SetStatusMessage("Created scene '" + sanitizedName + "'.", false);
            return true;
        }

        bool RenameEntry(const std::filesystem::path& assetsDirectory, const ProjectAssetEntry& entry, const std::string& name, EditorSceneState& sceneState)
        {
            const std::string sanitizedName = SanitizeName(name);
            if (sanitizedName.empty())
            {
                sceneState.SetStatusMessage("Name must not be empty.", true);
                return false;
            }

            const std::filesystem::path destination = entry.AbsolutePath.parent_path() / (sanitizedName + ResolveSuffixForRename(entry));
            if (destination == entry.AbsolutePath)
                return false;

            if (!IsPathInside(assetsDirectory, destination.parent_path()))
            {
                sceneState.SetStatusMessage("Rename target must remain inside the project Assets directory.", true);
                return false;
            }

            std::error_code ec;
            if (std::filesystem::exists(destination, ec))
            {
                sceneState.SetStatusMessage("A file or folder with that name already exists.", true);
                return false;
            }

            std::filesystem::rename(entry.AbsolutePath, destination, ec);
            if (ec)
            {
                sceneState.SetStatusMessage("Failed to rename '" + entry.DisplayName + "'.", true);
                return false;
            }

            sceneState.SetStatusMessage("Renamed '" + entry.DisplayName + "'.", false);
            return true;
        }

        bool DeleteEntry(const std::filesystem::path& assetsDirectory, const ProjectAssetEntry& entry, EditorSceneState& sceneState)
        {
            std::error_code ec;
            if (entry.IsDirectory)
            {
                std::filesystem::remove_all(entry.AbsolutePath, ec);
                if (ec)
                {
                    sceneState.SetStatusMessage("Failed to delete folder '" + entry.DisplayName + "'.", true);
                    return false;
                }
            }
            else
            {
                if (!std::filesystem::remove(entry.AbsolutePath, ec) || ec)
                {
                    sceneState.SetStatusMessage("Failed to delete file '" + entry.DisplayName + "'.", true);
                    return false;
                }
            }

            (void)assetsDirectory;
            sceneState.SetStatusMessage("Deleted '" + entry.DisplayName + "'.", false);
            return true;
        }

        bool MoveEntry(const std::filesystem::path& assetsDirectory, const std::filesystem::path& sourceRelativePath, const std::filesystem::path& destinationFolderRelativePath, EditorSceneState& sceneState)
        {
            if (sourceRelativePath.empty())
                return false;

            const std::filesystem::path sourcePath = assetsDirectory / sourceRelativePath;
            const std::filesystem::path destinationFolder = destinationFolderRelativePath.empty() ? assetsDirectory : (assetsDirectory / destinationFolderRelativePath);
            const std::filesystem::path destinationPath = destinationFolder / sourcePath.filename();

            if (!IsPathInside(assetsDirectory, destinationFolder))
            {
                sceneState.SetStatusMessage("Move target must remain inside the project Assets directory.", true);
                return false;
            }

            std::error_code ec;
            if (std::filesystem::is_directory(sourcePath, ec) && IsSameOrDescendant(destinationFolderRelativePath, sourceRelativePath))
            {
                sceneState.SetStatusMessage("Cannot move a folder into itself.", true);
                return false;
            }

            ec.clear();
            if (std::filesystem::equivalent(sourcePath.parent_path(), destinationFolder, ec))
                return false;
            ec.clear();
            if (std::filesystem::exists(destinationPath, ec))
            {
                sceneState.SetStatusMessage("A file or folder with that name already exists in the target folder.", true);
                return false;
            }

            std::filesystem::rename(sourcePath, destinationPath, ec);
            if (ec)
            {
                sceneState.SetStatusMessage("Failed to move '" + sourcePath.filename().string() + "'.", true);
                return false;
            }

            sceneState.SetStatusMessage("Moved '" + sourcePath.filename().string() + "'.", false);
            return true;
        }

        ProjectAssetEntry FindEntryByRelativePath(const std::filesystem::path& assetsDirectory, const std::filesystem::path& relativePath)
        {
            ProjectAssetEntry entry;
            entry.RelativePath = relativePath.lexically_normal();
            entry.AbsolutePath = assetsDirectory / entry.RelativePath;
            entry.DisplayName = entry.AbsolutePath.filename().string();
            entry.LowerFileName = ToLowerAscii(entry.DisplayName);
            std::error_code ec;
            entry.IsDirectory = std::filesystem::is_directory(entry.AbsolutePath, ec);
            entry.Kind = ClassifyEntry(entry.AbsolutePath, entry.IsDirectory);
            return entry;
        }

        void SetPayload(const ProjectAssetEntry& entry)
        {
#if __has_include(<imgui.h>)
            ProjectAssetDragPayload payload{};
            payload.Kind = entry.Kind == ProjectEntryKind::Scene
                ? ProjectAssetPayloadKind::Scene
                : (entry.IsDirectory ? ProjectAssetPayloadKind::Directory : ProjectAssetPayloadKind::File);
            const std::string relativeText = entry.RelativePath.generic_string();
            const std::size_t copyLength = std::min(relativeText.size(), payload.RelativePath.size() - 1);
            std::memcpy(payload.RelativePath.data(), relativeText.data(), copyLength);
            payload.RelativePath[copyLength] = '\0';
            ImGui::SetDragDropPayload(kProjectAssetDragPayloadType, &payload, sizeof(payload), ImGuiCond_Once);
#endif
        }

        bool ImportExternalPath(
            const std::filesystem::path& assetsDirectory,
            const std::filesystem::path& destinationRelativePath,
            const std::filesystem::path& sourceAbsolutePath,
            EditorSceneState& sceneState)
        {
            std::error_code ec;
            if (!std::filesystem::exists(sourceAbsolutePath, ec))
            {
                sceneState.SetStatusMessage("Dropped path does not exist: '" + PathToUiString(sourceAbsolutePath) + "'.", true);
                return false;
            }

            const std::filesystem::path destinationFolder = destinationRelativePath.empty()
                ? assetsDirectory
                : (assetsDirectory / destinationRelativePath);
            if (!IsPathInside(assetsDirectory, destinationFolder))
            {
                sceneState.SetStatusMessage("Import target must remain inside the project Assets directory.", true);
                return false;
            }

            const std::filesystem::path destinationPath = destinationFolder / sourceAbsolutePath.filename();
            if (sourceAbsolutePath.lexically_normal() == destinationPath.lexically_normal())
                return false;

            ec.clear();
            if (std::filesystem::exists(destinationPath, ec))
            {
                sceneState.SetStatusMessage("An asset named '" + sourceAbsolutePath.filename().string() + "' already exists in the target folder.", true);
                return false;
            }

            ec.clear();
            std::filesystem::create_directories(destinationFolder, ec);
            if (ec)
            {
                sceneState.SetStatusMessage("Failed to prepare import folder '" + PathToUiString(destinationFolder) + "'.", true);
                return false;
            }

            ec.clear();
            if (std::filesystem::is_directory(sourceAbsolutePath, ec))
            {
                ec.clear();
                std::filesystem::copy(sourceAbsolutePath, destinationPath, std::filesystem::copy_options::recursive, ec);
            }
            else
            {
                ec.clear();
                std::filesystem::copy_file(sourceAbsolutePath, destinationPath, std::filesystem::copy_options::none, ec);
            }

            if (ec)
            {
                sceneState.SetStatusMessage("Failed to import '" + sourceAbsolutePath.filename().string() + "'.", true);
                return false;
            }

            sceneState.SetStatusMessage("Imported '" + sourceAbsolutePath.filename().string() + "'.", false);
            return true;
        }

#if __has_include(<imgui.h>)
        bool ContainsPoint(const ImVec2& min, const ImVec2& max, float x, float y)
        {
            return x >= min.x && x <= max.x && y >= min.y && y <= max.y;
        }
#endif
    }

    void ProjectAssetsPanel::ApplyState(const ProjectAssetsPanelState& state) noexcept
    {
        m_GridScale = ClampGridScale(state.GridScale);
    }

    ProjectAssetsPanelState ProjectAssetsPanel::CaptureState() const noexcept
    {
        ProjectAssetsPanelState state;
        state.GridScale = ClampGridScale(m_GridScale);
        return state;
    }

    void ProjectAssetsPanel::QueueExternalFileDrop(std::filesystem::path absolutePath, float x, float y)
    {
        if (absolutePath.empty())
            return;

        PendingExternalDrop drop;
        drop.AbsolutePath = std::move(absolutePath);
        drop.X = x;
        drop.Y = y;
        m_PendingExternalDrops.push_back(std::move(drop));
    }

    void ProjectAssetsPanel::Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState)
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (!ImGui::Begin("Project Assets", &isOpen))
        {
            ImGui::End();
            return;
        }

        if (!services.ProjectService || !services.ProjectService->get().HasActiveProject())
        {
            ImGui::TextUnformatted("No active project.");
            ImGui::End();
            return;
        }

        const std::filesystem::path assetsDirectory = services.ProjectService->get().GetActiveProject().Paths.AssetsDirectory;
        std::error_code ec;
        if (!std::filesystem::exists(assetsDirectory, ec) || !std::filesystem::is_directory(assetsDirectory, ec))
        {
            ImGui::TextUnformatted("Project Assets directory is unavailable.");
            ImGui::End();
            return;
        }

        const std::filesystem::path activeFolderAbsolutePath = m_ActiveFolderRelativePath.empty() ? assetsDirectory : (assetsDirectory / m_ActiveFolderRelativePath);
        if (!m_ActiveFolderRelativePath.empty() && (!std::filesystem::exists(activeFolderAbsolutePath, ec) || !std::filesystem::is_directory(activeFolderAbsolutePath, ec)))
            m_ActiveFolderRelativePath.clear();

        m_GridScale = ClampGridScale(m_GridScale);
        const auto currentEntries = CollectEntries(assetsDirectory, m_ActiveFolderRelativePath);
        const std::string activeFolderLabel = m_ActiveFolderRelativePath.empty()
            ? std::string("Assets")
            : std::string("Assets/") + m_ActiveFolderRelativePath.generic_string();

        const ImVec2 panelMin = ImGui::GetWindowPos();
        const ImVec2 panelMax(panelMin.x + ImGui::GetWindowSize().x, panelMin.y + ImGui::GetWindowSize().y);
        bool importedExternalAsset = false;
        if (!m_PendingExternalDrops.empty())
        {
            for (const PendingExternalDrop& drop : m_PendingExternalDrops)
            {
                if (!ContainsPoint(panelMin, panelMax, drop.X, drop.Y))
                    continue;

                importedExternalAsset |= ImportExternalPath(assetsDirectory, m_ActiveFolderRelativePath, drop.AbsolutePath, sceneState);
            }

            m_PendingExternalDrops.clear();
        }

        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 7.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10.0f, 8.0f));

        if (ImGui::BeginChild("##ProjectAssetsToolbar", ImVec2(0.0f, 104.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "Project Assets");
            ImGui::SameLine();
            ImGui::TextDisabled("%zu items", currentEntries.size());
            ImGui::TextDisabled("%s", activeFolderLabel.c_str());
            ImGui::Separator();

            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.33f, 0.54f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.41f, 0.64f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.29f, 0.48f, 1.0f));
            if (ImGui::Button("Create", ImVec2(90.0f, 0.0f)))
                ImGui::OpenPopup("##ProjectAssetsCreateMenu");
            ImGui::PopStyleColor(3);
            if (ImGui::BeginPopup("##ProjectAssetsCreateMenu"))
            {
                if (ImGui::MenuItem("Create Folder"))
                {
                    m_PendingPopup = PendingPopup::CreateFolder;
                    m_PopupTargetRelativePath = m_ActiveFolderRelativePath;
                    m_PopupName = "New Folder";
                    m_OpenPendingPopup = true;
                }
                if (ImGui::MenuItem("Create Scene"))
                {
                    m_PendingPopup = PendingPopup::CreateScene;
                    m_PopupTargetRelativePath = m_ActiveFolderRelativePath;
                    m_PopupName = "NewScene";
                    m_OpenPendingPopup = true;
                }
                ImGui::EndPopup();
            }
            ImGui::SameLine();
            ImGui::AlignTextToFramePadding();
            ImGui::TextDisabled("Scale");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(170.0f);
            ImGui::SliderFloat("##ProjectAssetsScale", &m_GridScale, kMinGridScale, kMaxGridScale, "%.2fx");
            m_GridScale = ClampGridScale(m_GridScale);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-1.0f);
            InputTextStringWithHint("##ProjectAssetsSearch", "Search assets", m_SearchFilter);
        }
        ImGui::EndChild();

        const std::string filterLower = ToLowerAscii(m_SearchFilter);
        const bool useCompactList = m_GridScale <= 0.25f;

        if (m_OpenPendingPopup)
        {
            switch (m_PendingPopup)
            {
                case PendingPopup::CreateFolder: ImGui::OpenPopup("Create Folder"); break;
                case PendingPopup::CreateScene: ImGui::OpenPopup("Create Scene"); break;
                case PendingPopup::Rename: ImGui::OpenPopup("Rename Entry"); break;
                case PendingPopup::None: break;
            }
            m_OpenPendingPopup = false;
        }

        if (ImGui::BeginPopupModal("Create Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Folder Name", m_PopupName);
            if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
            {
                if (CreateFolder(assetsDirectory, m_PopupTargetRelativePath, m_PopupName, sceneState))
                    ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Create Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Scene Name", m_PopupName);
            if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
            {
                if (CreateSceneAsset(assetsDirectory, m_PopupTargetRelativePath, m_PopupName, sceneState))
                    ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Rename Entry", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Name", m_PopupName);
            if (ImGui::Button("Rename", ImVec2(120.0f, 0.0f)))
            {
                const ProjectAssetEntry entry = FindEntryByRelativePath(assetsDirectory, m_PopupTargetRelativePath);
                const std::filesystem::path renamedRelativePath = (entry.RelativePath.parent_path() / (m_PopupName + ResolveSuffixForRename(entry))).lexically_normal();
                if (RenameEntry(assetsDirectory, entry, m_PopupName, sceneState))
                {
                    m_SelectedRelativePath = RebasePath(m_SelectedRelativePath, entry.RelativePath, renamedRelativePath);
                    m_ActiveFolderRelativePath = RebasePath(m_ActiveFolderRelativePath, entry.RelativePath, renamedRelativePath);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
        }

        if (ImGui::BeginChild("##ProjectAssetsBrowser", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_NoScrollbar))
        {
            if (ImGui::BeginChild("##ProjectAssetsTree", ImVec2(280.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar))
            {
                ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "Folders");
                ImGui::SameLine();
                ImGui::TextDisabled("Project structure");
                ImGui::Separator();

                std::function<void(const std::filesystem::path&, bool)> drawFolderNode =
                    [&](const std::filesystem::path& relativePath, bool isRoot)
                {
                    const std::string key = relativePath.generic_string();
                    const std::string label = isRoot
                        ? std::string("Assets##ProjectAssetsRoot")
                        : (relativePath.filename().string() + "##" + key);
                    bool openState = isRoot ? true : (m_ExpandedFolders.contains(key) ? m_ExpandedFolders[key] : false);
                    ImGui::SetNextItemOpen(openState, ImGuiCond_Once);
                    const bool nodeOpen = ImGui::TreeNodeEx(
                        label.c_str(),
                        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                            ((relativePath == m_ActiveFolderRelativePath) ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None));
                    if (!isRoot)
                        m_ExpandedFolders[key] = nodeOpen;

                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    {
                        m_ActiveFolderRelativePath = relativePath;
                        m_SelectedRelativePath = relativePath;
                    }

                    if (ImGui::BeginPopupContextItem())
                    {
                        if (ImGui::MenuItem("Create Folder"))
                        {
                            m_PendingPopup = PendingPopup::CreateFolder;
                            m_PopupTargetRelativePath = relativePath;
                            m_PopupName = "New Folder";
                            m_OpenPendingPopup = true;
                        }
                        if (ImGui::MenuItem("Create Scene"))
                        {
                            m_PendingPopup = PendingPopup::CreateScene;
                            m_PopupTargetRelativePath = relativePath;
                            m_PopupName = "NewScene";
                            m_OpenPendingPopup = true;
                        }
                        if (!isRoot)
                        {
                            ImGui::Separator();
                            if (ImGui::MenuItem("Rename"))
                            {
                                const ProjectAssetEntry entry = FindEntryByRelativePath(assetsDirectory, relativePath);
                                m_PendingPopup = PendingPopup::Rename;
                                m_PopupTargetRelativePath = relativePath;
                                m_PopupName = ResolveDisplayStem(entry);
                                m_OpenPendingPopup = true;
                            }
                            if (ImGui::MenuItem("Delete"))
                            {
                                const ProjectAssetEntry entry = FindEntryByRelativePath(assetsDirectory, relativePath);
                                if (DeleteEntry(assetsDirectory, entry, sceneState))
                                {
                                    if (IsSameOrDescendant(m_ActiveFolderRelativePath, relativePath))
                                        m_ActiveFolderRelativePath = relativePath.parent_path();
                                    if (IsSameOrDescendant(m_SelectedRelativePath, relativePath))
                                        m_SelectedRelativePath.clear();
                                }
                            }
                        }
                        ImGui::EndPopup();
                    }

                    if (ImGui::BeginDragDropSource())
                    {
                        const ProjectAssetEntry entry = FindEntryByRelativePath(assetsDirectory, relativePath);
                        SetPayload(entry);
                        ImGui::TextUnformatted(entry.DisplayName.c_str());
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                        {
                            const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                            if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                            {
                                const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                                if (MoveEntry(assetsDirectory, sourceRelativePath, relativePath, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                                    m_SelectedRelativePath = relativePath / sourceRelativePath.filename();
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }

                    if (nodeOpen)
                    {
                        const auto childEntries = CollectEntries(assetsDirectory, relativePath);
                        for (const ProjectAssetEntry& child : childEntries)
                        {
                            if (!child.IsDirectory)
                                continue;
                            if (!filterLower.empty() && !DirectoryContainsMatch(assetsDirectory, child.RelativePath, filterLower) && !MatchesFilter(child, filterLower))
                                continue;
                            drawFolderNode(child.RelativePath, false);
                        }
                        ImGui::TreePop();
                    }
                };

                drawFolderNode({}, true);
            }
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("##ProjectAssetsGrid", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
            {
                ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "%s", activeFolderLabel.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled(useCompactList ? "List" : "Grid");
                ImGui::Separator();

                if (!m_ActiveFolderRelativePath.empty())
                {
                    if (ImGui::Button("Up"))
                        m_ActiveFolderRelativePath = m_ActiveFolderRelativePath.parent_path();
                    ImGui::SameLine();
                }

                if (ImGui::SmallButton("Assets"))
                    m_ActiveFolderRelativePath.clear();

                std::filesystem::path breadcrumb;
                for (const std::filesystem::path& segment : m_ActiveFolderRelativePath)
                {
                    breadcrumb /= segment;
                    ImGui::SameLine();
                    ImGui::TextUnformatted("/");
                    ImGui::SameLine();
                    ImGui::PushID(breadcrumb.generic_string().c_str());
                    if (ImGui::SmallButton(segment.string().c_str()))
                        m_ActiveFolderRelativePath = breadcrumb;
                    ImGui::PopID();
                }

                if (ImGui::BeginPopupContextWindow("##ProjectAssetsGridContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
                {
                    if (ImGui::MenuItem("Create Folder"))
                    {
                        m_PendingPopup = PendingPopup::CreateFolder;
                        m_PopupTargetRelativePath = m_ActiveFolderRelativePath;
                        m_PopupName = "New Folder";
                        m_OpenPendingPopup = true;
                    }
                    if (ImGui::MenuItem("Create Scene"))
                    {
                        m_PendingPopup = PendingPopup::CreateScene;
                        m_PopupTargetRelativePath = m_ActiveFolderRelativePath;
                        m_PopupName = "NewScene";
                        m_OpenPendingPopup = true;
                    }
                    ImGui::EndPopup();
                }

                if (ImGui::BeginDragDropTarget())
                {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                    {
                        const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                        if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                        {
                            const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                            if (MoveEntry(assetsDirectory, sourceRelativePath, m_ActiveFolderRelativePath, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                                m_SelectedRelativePath = m_ActiveFolderRelativePath / sourceRelativePath.filename();
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                ImGui::Separator();

                std::vector<ProjectAssetEntry> visibleEntries;
                visibleEntries.reserve(currentEntries.size());
                for (const ProjectAssetEntry& entry : currentEntries)
                {
                    if (MatchesFilter(entry, filterLower))
                        visibleEntries.push_back(entry);
                }

                if (visibleEntries.empty())
                {
                    ImGui::Dummy(ImVec2(0.0f, 12.0f));
                    ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), filterLower.empty() ? "No assets in this folder." : "No assets match the current filter.");
                    ImGui::TextDisabled(filterLower.empty() ? "Create a folder, create a scene, or import files from Explorer." : "Try a different search term or clear the filter.");
                }
                else if (useCompactList)
                {
                    for (const ProjectAssetEntry& entry : visibleEntries)
                    {
                        ImGui::PushID(entry.RelativePath.generic_string().c_str());
                        const bool selected = m_SelectedRelativePath == entry.RelativePath;
                        const ImVec4 accentColor = ResolveAccentColor(entry.Kind);
                        ImGui::PushStyleColor(ImGuiCol_Header, selected ? ImVec4(accentColor.x * 0.42f, accentColor.y * 0.42f, accentColor.z * 0.42f, 0.88f) : ImVec4(0.12f, 0.15f, 0.20f, 0.68f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(accentColor.x * 0.32f, accentColor.y * 0.32f, accentColor.z * 0.32f, 0.90f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accentColor.x * 0.46f, accentColor.y * 0.46f, accentColor.z * 0.46f, 0.94f));
                        const std::string rowLabel = std::string("[") + ResolveBadge(entry.Kind) + "] " + entry.DisplayName;
                        if (ImGui::Selectable(rowLabel.c_str(), selected, ImGuiSelectableFlags_AllowDoubleClick))
                        {
                            m_SelectedRelativePath = entry.RelativePath;
                            if (entry.IsDirectory && ImGui::IsMouseDoubleClicked(0))
                            {
                                m_ActiveFolderRelativePath = entry.RelativePath;
                            }
                            else if (entry.Kind == ProjectEntryKind::Scene && ImGui::IsMouseDoubleClicked(0))
                            {
                                (void)LoadSceneIntoEditor(MakeAssetKey(entry.RelativePath), services, sceneState);
                            }
                        }
                        ImGui::PopStyleColor(3);

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (entry.Kind == ProjectEntryKind::Scene)
                            {
                                if (ImGui::MenuItem("Open Scene"))
                                    (void)LoadSceneIntoEditor(MakeAssetKey(entry.RelativePath), services, sceneState);
                                if (ImGui::MenuItem("Set As Startup Scene"))
                                    (void)SaveStartupScene(MakeAssetKey(entry.RelativePath), services, sceneState);
                                ImGui::Separator();
                            }

                            if (entry.IsDirectory)
                            {
                                if (ImGui::MenuItem("Create Folder"))
                                {
                                    m_PendingPopup = PendingPopup::CreateFolder;
                                    m_PopupTargetRelativePath = entry.RelativePath;
                                    m_PopupName = "New Folder";
                                    m_OpenPendingPopup = true;
                                }
                                if (ImGui::MenuItem("Create Scene"))
                                {
                                    m_PendingPopup = PendingPopup::CreateScene;
                                    m_PopupTargetRelativePath = entry.RelativePath;
                                    m_PopupName = "NewScene";
                                    m_OpenPendingPopup = true;
                                }
                                ImGui::Separator();
                            }

                            if (ImGui::MenuItem("Rename"))
                            {
                                m_PendingPopup = PendingPopup::Rename;
                                m_PopupTargetRelativePath = entry.RelativePath;
                                m_PopupName = ResolveDisplayStem(entry);
                                m_OpenPendingPopup = true;
                            }
                            if (ImGui::MenuItem("Delete"))
                            {
                                if (DeleteEntry(assetsDirectory, entry, sceneState))
                                {
                                    if (entry.IsDirectory && IsSameOrDescendant(m_ActiveFolderRelativePath, entry.RelativePath))
                                        m_ActiveFolderRelativePath = entry.RelativePath.parent_path();
                                    if (IsSameOrDescendant(m_SelectedRelativePath, entry.RelativePath))
                                        m_SelectedRelativePath.clear();
                                }
                            }
                            ImGui::EndPopup();
                        }

                        if (ImGui::BeginDragDropSource())
                        {
                            SetPayload(entry);
                            ImGui::TextUnformatted(entry.DisplayName.c_str());
                            ImGui::EndDragDropSource();
                        }

                        if (entry.IsDirectory && ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                            {
                                const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                                if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                                {
                                    const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                                    if (MoveEntry(assetsDirectory, sourceRelativePath, entry.RelativePath, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                                        m_SelectedRelativePath = entry.RelativePath / sourceRelativePath.filename();
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        ImGui::PopID();
                    }
                }
                else
                {
                    const float cellSize = 72.0f + m_GridScale * 72.0f;
                    const float availableWidth = std::max(ImGui::GetContentRegionAvail().x, cellSize);
                    int columns = static_cast<int>(availableWidth / cellSize);
                    columns = std::max(columns, 1);
                    ImGui::Columns(columns, "ProjectAssetsColumns", false);

                    for (const ProjectAssetEntry& entry : visibleEntries)
                    {
                        ImGui::PushID(entry.RelativePath.generic_string().c_str());
                        ImGui::BeginGroup();
                        const bool selected = m_SelectedRelativePath == entry.RelativePath;
                        const ImVec4 accentColor = ResolveAccentColor(entry.Kind);
                        const ImVec4 cardColor = selected
                            ? ImVec4(accentColor.x * 0.52f, accentColor.y * 0.52f, accentColor.z * 0.52f, 0.95f)
                            : ImVec4(0.12f + accentColor.x * 0.08f, 0.14f + accentColor.y * 0.08f, 0.18f + accentColor.z * 0.08f, 1.0f);
                        const ImVec4 cardHovered = ImVec4(cardColor.x + 0.05f, cardColor.y + 0.05f, cardColor.z + 0.05f, cardColor.w);
                        const ImVec4 cardActive = ImVec4(cardColor.x + 0.02f, cardColor.y + 0.02f, cardColor.z + 0.02f, cardColor.w);
                        ImGui::PushStyleColor(ImGuiCol_Button, cardColor);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cardHovered);
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, cardActive);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                        if (ImGui::Button((std::string("[") + ResolveBadge(entry.Kind) + "]##Button").c_str(), ImVec2(cellSize - 12.0f, cellSize - 28.0f)))
                        {
                            m_SelectedRelativePath = entry.RelativePath;
                            if (entry.IsDirectory)
                                m_ActiveFolderRelativePath = entry.RelativePath;
                        }
                        ImGui::PopStyleVar();
                        ImGui::PopStyleColor(3);

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                        {
                            if (entry.IsDirectory)
                                m_ActiveFolderRelativePath = entry.RelativePath;
                            else if (entry.Kind == ProjectEntryKind::Scene)
                                (void)LoadSceneIntoEditor(MakeAssetKey(entry.RelativePath), services, sceneState);
                        }

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (entry.Kind == ProjectEntryKind::Scene)
                            {
                                if (ImGui::MenuItem("Open Scene"))
                                    (void)LoadSceneIntoEditor(MakeAssetKey(entry.RelativePath), services, sceneState);
                                if (ImGui::MenuItem("Set As Startup Scene"))
                                    (void)SaveStartupScene(MakeAssetKey(entry.RelativePath), services, sceneState);
                                ImGui::Separator();
                            }

                            if (entry.IsDirectory)
                            {
                                if (ImGui::MenuItem("Create Folder"))
                                {
                                    m_PendingPopup = PendingPopup::CreateFolder;
                                    m_PopupTargetRelativePath = entry.RelativePath;
                                    m_PopupName = "New Folder";
                                    m_OpenPendingPopup = true;
                                }
                                if (ImGui::MenuItem("Create Scene"))
                                {
                                    m_PendingPopup = PendingPopup::CreateScene;
                                    m_PopupTargetRelativePath = entry.RelativePath;
                                    m_PopupName = "NewScene";
                                    m_OpenPendingPopup = true;
                                }
                                ImGui::Separator();
                            }

                            if (ImGui::MenuItem("Rename"))
                            {
                                m_PendingPopup = PendingPopup::Rename;
                                m_PopupTargetRelativePath = entry.RelativePath;
                                m_PopupName = ResolveDisplayStem(entry);
                                m_OpenPendingPopup = true;
                            }
                            if (ImGui::MenuItem("Delete"))
                            {
                                if (DeleteEntry(assetsDirectory, entry, sceneState))
                                {
                                    if (entry.IsDirectory && IsSameOrDescendant(m_ActiveFolderRelativePath, entry.RelativePath))
                                        m_ActiveFolderRelativePath = entry.RelativePath.parent_path();
                                    if (IsSameOrDescendant(m_SelectedRelativePath, entry.RelativePath))
                                        m_SelectedRelativePath.clear();
                                }
                            }
                            ImGui::EndPopup();
                        }

                        if (ImGui::BeginDragDropSource())
                        {
                            SetPayload(entry);
                            ImGui::TextUnformatted(entry.DisplayName.c_str());
                            ImGui::EndDragDropSource();
                        }

                        if (entry.IsDirectory && ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                            {
                                const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                                if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                                {
                                    const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                                    if (MoveEntry(assetsDirectory, sourceRelativePath, entry.RelativePath, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                                        m_SelectedRelativePath = entry.RelativePath / sourceRelativePath.filename();
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        ImGui::TextWrapped("%s", entry.DisplayName.c_str());
                        ImGui::EndGroup();
                        ImGui::NextColumn();
                        ImGui::PopID();
                    }

                    ImGui::Columns(1);
                }
            }
            ImGui::EndChild();
        }
        ImGui::EndChild();

        ImGui::PopStyleVar(2);
        (void)importedExternalAsset;
        ImGui::End();
#else
        (void)isOpen;
        (void)services;
        (void)sceneState;
#endif
    }
}
