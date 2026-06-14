#include "Editor/EditorShellOverlay.h"

#include "Assets/AssetPaths.h"
#include "Assets/PrefabSerializer.h"
#include "Graphics/Renderer.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string_view>
#include <vector>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    namespace
    {
        constexpr glm::vec4 kEditorBackBufferClearColor(0.07f, 0.085f, 0.11f, 1.0f);

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

        std::vector<Life::Entity> FilterTopLevelSelection(const std::vector<Life::Entity>& selectedEntities)
        {
            std::vector<Life::Entity> filtered;
            filtered.reserve(selectedEntities.size());
            for (const Life::Entity& candidate : selectedEntities)
            {
                if (!candidate.IsValid())
                    continue;

                bool hasSelectedAncestor = false;
                for (const Life::Entity& other : selectedEntities)
                {
                    if (!other.IsValid() || other == candidate)
                        continue;
                    if (candidate.IsDescendantOf(other))
                    {
                        hasSelectedAncestor = true;
                        break;
                    }
                }

                if (!hasSelectedAncestor)
                    filtered.push_back(candidate);
            }
            return filtered;
        }

        std::string SanitizeSceneStem(std::string value)
        {
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char character)
            {
                return std::isspace(character) == 0;
            }));
            value.erase(std::find_if(value.rbegin(), value.rend(), [](unsigned char character)
            {
                return std::isspace(character) == 0;
            }).base(), value.end());

            if (value.empty())
                return "Scene";

            constexpr std::array<char, 9> invalidCharacters{ '<', '>', ':', '"', '/', '\\', '|', '?', '*' };
            for (char& character : value)
            {
                if (std::find(invalidCharacters.begin(), invalidCharacters.end(), character) != invalidCharacters.end())
                    character = '_';
            }

            return value;
        }

        std::string PathToUiString(const std::filesystem::path& path)
        {
            std::filesystem::path preferred = path;
            preferred.make_preferred();
            return preferred.string();
        }

        std::string ResolvePrefabDisplayName(const std::filesystem::path& path, const std::string& assetKey)
        {
            std::string name = path.empty() ? std::filesystem::path(assetKey).filename().string() : path.filename().string();
            constexpr std::string_view suffix = ".prefab.json";
            if (name.size() > suffix.size() && name.substr(name.size() - suffix.size()) == suffix)
                name.resize(name.size() - suffix.size());
            return name.empty() ? std::string("Prefab") : name;
        }

        void CollectTopMostPrefabInstances(Life::Entity entity, const std::string& prefabGuid, std::vector<Life::Entity>& matches)
        {
            if (!entity.IsValid())
                return;

            const Life::PrefabInstanceComponent* prefabInstance = entity.TryGetComponent<Life::PrefabInstanceComponent>();
            if (prefabInstance != nullptr && prefabInstance->PrefabGuid == prefabGuid)
            {
                matches.push_back(entity);
                return;
            }

            for (const Life::Entity child : entity.GetChildren())
                CollectTopMostPrefabInstances(child, prefabGuid, matches);
        }

        std::vector<Life::Entity> FindTopMostPrefabInstances(Life::Scene& scene, const std::string& prefabGuid)
        {
            std::vector<Life::Entity> matches;
            for (const Life::Entity root : scene.GetRootEntities())
                CollectTopMostPrefabInstances(root, prefabGuid, matches);
            return matches;
        }

        EditorEntitySnapshot BuildPrefabApplySnapshotRecursive(const Life::Entity& sourceEntity,
                                                               Life::Scene& idSourceScene,
                                                               const std::string& prefabGuid,
                                                               const std::string& parentId,
                                                               std::size_t siblingIndex)
        {
            EditorEntitySnapshot snapshot = CaptureEntitySnapshot(sourceEntity);
            const std::string sourceEntityId = snapshot.Id;
            snapshot.Id = idSourceScene.CreateEntity().GetId();
            snapshot.ParentId = parentId;
            snapshot.SiblingIndex = siblingIndex;
            snapshot.PrefabInstance = Life::PrefabInstanceComponent{
                .PrefabGuid = prefabGuid,
                .SourceEntityId = sourceEntityId
            };
            snapshot.Children.clear();

            const auto children = sourceEntity.GetChildren();
            snapshot.Children.reserve(children.size());
            for (std::size_t childIndex = 0; childIndex < children.size(); ++childIndex)
            {
                snapshot.Children.push_back(BuildPrefabApplySnapshotRecursive(
                    children[childIndex],
                    idSourceScene,
                    prefabGuid,
                    snapshot.Id,
                    childIndex));
            }
            return snapshot;
        }

        std::vector<EditorEntitySnapshot> BuildPrefabApplySnapshots(const Life::Scene& prefabScene,
                                                                    Life::Scene& idSourceScene,
                                                                    const std::string& prefabGuid,
                                                                    const EditorEntitySnapshot& instanceSnapshot)
        {
            std::vector<EditorEntitySnapshot> snapshots;
            const auto prefabRoots = prefabScene.GetRootEntities();
            if (prefabRoots.empty())
                return snapshots;

            EditorEntitySnapshot snapshot = BuildPrefabApplySnapshotRecursive(
                prefabRoots.front(),
                idSourceScene,
                prefabGuid,
                instanceSnapshot.ParentId,
                instanceSnapshot.SiblingIndex);
            snapshot.Transform = instanceSnapshot.Transform;
            snapshot.Enabled = instanceSnapshot.Enabled;
            snapshots.push_back(std::move(snapshot));
            return snapshots;
        }

        void TryUpdateProjectStartupScene(Life::Assets::ProjectService& projectService, const Life::SceneService& sceneService)
        {
            if (!projectService.HasActiveProject() || !sceneService.HasActiveSceneSourcePath())
                return;

            Life::Assets::Project& project = projectService.GetActiveProject();
            const std::filesystem::path scenePath = sceneService.GetActiveSceneSourcePath().lexically_normal();
            const std::filesystem::path relativePath = scenePath.lexically_relative(project.Paths.RootDirectory);
            if (relativePath.empty())
                return;

            const std::string relativeString = relativePath.generic_string();
            if (relativeString.rfind("..", 0) == 0)
                return;

            project.Descriptor.Startup.Scene = relativeString;
            (void)projectService.SaveProject();
        }
    }

    EditorShellOverlay::EditorShellOverlay()
        : Life::Layer("EditorShellOverlay")
    {
    }

    void EditorShellOverlay::SetMode(Mode mode)
    {
        if (m_Mode == mode)
            return;

        m_Mode = mode;
        m_Shell.ResetLayout();

        if (mode == Mode::ProjectHub)
        {
            m_SceneState.ResetRuntimeState();
            m_SceneState.ResetPrefabMode();
            m_UndoStack.Clear();
            m_PrefabUndoStack.Clear();
            if (m_Services.SceneService)
                m_Services.SceneService->get().CloseScene();
            m_SceneState.ClearSelection();
            m_SceneState.ClearStatus();
            m_ProjectHub.RefreshRecentProjects();
        }
        else if (mode == Mode::Workspace)
        {
            if (m_Services.ProjectService && m_Services.SceneService)
            {
                Life::Assets::ProjectService& projectService = m_Services.ProjectService->get();
                Life::SceneService& sceneService = m_Services.SceneService->get();
                if (projectService.HasActiveProject() && !sceneService.HasActiveScene())
                {
                    const Life::Assets::Project& project = projectService.GetActiveProject();
                    m_SceneState.SceneViewMode = project.Descriptor.Dimension == Life::Assets::ProjectDimension::ThreeD
                        ? EditorSceneViewMode::ThreeD
                        : EditorSceneViewMode::TwoD;
                    if (!project.Descriptor.Startup.Scene.empty())
                    {
                        sceneService.OpenScene(project.Descriptor.Startup.Scene);
                        SetSceneStatus("Opened startup scene '" + sceneService.GetActiveScene().GetName() + "'.", false);
                    }
                    else
                    {
                        Life::Scene& scene = sceneService.CreateScene(project.Descriptor.Name.empty() ? "EditorScene" : project.Descriptor.Name);
                        scene.EnsureAtLeastOneCamera();
                        sceneService.MarkActiveSceneDirty();
                        SetSceneStatus("Created editor scene document.", false);
                    }
                }

                m_SceneState.ClearSelection();
                m_UndoStack.Clear();
                m_PrefabUndoStack.Clear();
            }
        }
    }

    void EditorShellOverlay::HandleShellActions(const EditorShellActions& actions)
    {
        if (!m_Services.SceneService)
            return;

        Life::SceneService& sceneService = m_Services.SceneService->get();

        if (actions.RequestPlayScene)
            BeginSceneExecution(EditorSceneExecutionMode::Play);

        if (actions.RequestSimulateScene)
            BeginSceneExecution(EditorSceneExecutionMode::Simulation);

        if (actions.RequestPauseScene && m_SceneState.IsRuntimeMode())
        {
            if (!m_SceneState.SupportsRuntimeTicks)
            {
                SetSceneStatus("Pause and resume are unavailable until runtime scene tick hooks are connected.", true);
            }
            else
            {
                m_SceneState.Paused = !m_SceneState.Paused;
                m_SceneState.StepSingleFrame = false;
                SetSceneStatus(m_SceneState.Paused ? "Scene execution paused." : "Scene execution resumed.", false);
            }
        }

        if (actions.RequestStepScene)
        {
            if (!m_SceneState.IsRuntimeMode())
            {
                SetSceneStatus("Step is only available while the scene is playing or simulating.", true);
            }
            else if (!m_SceneState.SupportsRuntimeTicks)
            {
                SetSceneStatus("Single-frame step is unavailable until runtime scene tick hooks are connected.", true);
            }
            else if (!m_SceneState.Paused)
            {
                SetSceneStatus("Pause scene execution before stepping a single frame.", true);
            }
            else
            {
                m_SceneState.StepSingleFrame = true;
                SetSceneStatus("Advancing scene execution by one frame.", false);
            }
        }

        if (actions.RequestStopScene)
            StopSceneExecution();

        if (actions.RequestNewScene)
        {
            if (m_SceneState.IsPrefabMode())
            {
                SetSceneStatus("Exit Prefab Mode before creating a new scene.", true);
                return;
            }
            m_NewSceneName = "Untitled";
            m_NewScenePath = BuildDefaultScenePath(m_NewSceneName);
            m_OpenNewScenePopup = true;
        }

        if (actions.RequestOpenScene)
        {
            if (m_SceneState.IsPrefabMode())
            {
                SetSceneStatus("Exit Prefab Mode before opening a scene.", true);
                return;
            }
            m_OpenScenePath = sceneService.HasActiveSceneSourcePath()
                ? PathToUiString(sceneService.GetActiveSceneSourcePath())
                : BuildDefaultScenePath("Scene");
            m_OpenOpenScenePopup = true;
        }

        if (actions.RequestSaveScene)
        {
            if (m_SceneState.IsPrefabMode())
            {
                (void)SavePrefabMode();
            }
            else if (!sceneService.HasActiveScene())
            {
                SetSceneStatus("No active scene is available to save.", true);
            }
            else if (sceneService.HasActiveSceneSourcePath())
            {
                const auto saveResult = sceneService.SaveActiveScene();
                if (saveResult.IsFailure())
                {
                    SetSceneStatus(saveResult.GetError().GetErrorMessage(), true);
                }
                else
                {
                    if (m_Services.ProjectService)
                        TryUpdateProjectStartupScene(m_Services.ProjectService->get(), sceneService);
                    SetSceneStatus("Saved scene '" + sceneService.GetActiveScene().GetName() + "'.", false);
                }
            }
            else
            {
                m_SaveScenePath = BuildDefaultScenePath(sceneService.GetActiveScene().GetName());
                m_OpenSaveSceneAsPopup = true;
            }
        }

        if (actions.RequestSaveSceneAs)
        {
            if (m_SceneState.IsPrefabMode())
            {
                SetSceneStatus("Save As is not available in Prefab Mode. Use Save, Discard, or Back.", true);
            }
            else if (!sceneService.HasActiveScene())
            {
                SetSceneStatus("No active scene is available to save.", true);
            }
            else
            {
                m_SaveScenePath = sceneService.HasActiveSceneSourcePath()
                    ? PathToUiString(sceneService.GetActiveSceneSourcePath())
                    : BuildDefaultScenePath(sceneService.GetActiveScene().GetName());
                m_OpenSaveSceneAsPopup = true;
            }
        }

        if (actions.RequestCloseScene)
        {
            if (m_SceneState.IsPrefabMode())
            {
                SetSceneStatus("Exit Prefab Mode before closing the scene.", true);
                return;
            }
            m_SceneState.ResetRuntimeState();
            if (sceneService.CloseScene())
            {
                m_SceneState.ClearSelection();
                m_UndoStack.Clear();
                SetSceneStatus("Scene closed.", false);
            }
        }
    }

    void EditorShellOverlay::RenderSceneDialogs()
    {
#if __has_include(<imgui.h>)
        if (!m_Services.SceneService)
            return;

        Life::SceneService& sceneService = m_Services.SceneService->get();

        if (m_OpenNewScenePopup)
        {
            ImGui::OpenPopup("New Scene");
            m_OpenNewScenePopup = false;
        }
        if (m_OpenOpenScenePopup)
        {
            ImGui::OpenPopup("Open Scene");
            m_OpenOpenScenePopup = false;
        }
        if (m_OpenSaveSceneAsPopup)
        {
            ImGui::OpenPopup("Save Scene As");
            m_OpenSaveSceneAsPopup = false;
        }

        if (ImGui::BeginPopupModal("New Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Scene Name", m_NewSceneName);
            InputTextString("Initial Path", m_NewScenePath);

            if (ImGui::Button("Create", ImVec2(120.0f, 0.0f)))
            {
                m_SceneState.ResetRuntimeState();
                Life::Scene& scene = sceneService.CreateScene(m_NewSceneName.empty() ? "Untitled" : m_NewSceneName);
                scene.EnsureAtLeastOneCamera();
                m_SceneState.ClearSelection();
                m_UndoStack.Clear();
                if (!m_NewScenePath.empty())
                {
                    const auto saveResult = sceneService.SaveActiveSceneAs(m_NewScenePath);
                    if (saveResult.IsFailure())
                    {
                        sceneService.MarkActiveSceneDirty();
                        SetSceneStatus(saveResult.GetError().GetErrorMessage(), true);
                    }
                    else
                    {
                        if (m_Services.ProjectService)
                            TryUpdateProjectStartupScene(m_Services.ProjectService->get(), sceneService);
                        SetSceneStatus("Created scene '" + scene.GetName() + "'.", false);
                    }
                }
                else
                {
                    sceneService.MarkActiveSceneDirty();
                    SetSceneStatus("Created unsaved scene '" + scene.GetName() + "'.", false);
                }

                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Open Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Scene Path", m_OpenScenePath);

            if (ImGui::Button("Open", ImVec2(120.0f, 0.0f)))
            {
                m_SceneState.ResetRuntimeState();
                const auto loadResult = sceneService.LoadScene(m_OpenScenePath);
                if (loadResult.IsFailure())
                {
                    SetSceneStatus(loadResult.GetError().GetErrorMessage(), true);
                }
                else
                {
                    m_SceneState.ClearSelection();
                    m_UndoStack.Clear();
                    if (m_Services.ProjectService)
                        TryUpdateProjectStartupScene(m_Services.ProjectService->get(), sceneService);
                    SetSceneStatus("Opened scene '" + sceneService.GetActiveScene().GetName() + "'.", false);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }

        if (ImGui::BeginPopupModal("Save Scene As", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            InputTextString("Scene Path", m_SaveScenePath);

            if (ImGui::Button("Save", ImVec2(120.0f, 0.0f)))
            {
                const auto saveResult = sceneService.SaveActiveSceneAs(m_SaveScenePath);
                if (saveResult.IsFailure())
                {
                    SetSceneStatus(saveResult.GetError().GetErrorMessage(), true);
                }
                else
                {
                    if (m_Services.ProjectService)
                        TryUpdateProjectStartupScene(m_Services.ProjectService->get(), sceneService);
                    SetSceneStatus("Saved scene '" + sceneService.GetActiveScene().GetName() + "'.", false);
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(120.0f, 0.0f)))
                ImGui::CloseCurrentPopup();

            ImGui::EndPopup();
        }
#endif
    }

    std::string EditorShellOverlay::BuildDefaultScenePath(const std::string& sceneName) const
    {
        const std::string fileName = SanitizeSceneStem(sceneName) + ".scene";
        if (m_Services.ProjectService && m_Services.ProjectService->get().HasActiveProject())
        {
            const Life::Assets::Project& project = m_Services.ProjectService->get().GetActiveProject();
            return PathToUiString(project.Paths.AssetsDirectory / "Scenes" / fileName);
        }

        return fileName;
    }

    void EditorShellOverlay::SetSceneStatus(std::string message, bool isError)
    {
        m_SceneState.SetStatusMessage(message, isError);
        if (isError)
            LOG_WARN("{}", m_SceneState.StatusMessage);
        else
            LOG_INFO("{}", m_SceneState.StatusMessage);
    }

    bool EditorShellOverlay::OpenPrefabMode(const std::string& assetKey)
    {
        if (assetKey.empty())
        {
            SetSceneStatus("Prefab asset key is empty.", true);
            return false;
        }
        if (m_SceneState.IsPrefabMode() && m_SceneState.PrefabDirty)
        {
            SetSceneStatus("Save or discard the current prefab before opening another prefab.", true);
            return false;
        }

        const auto resolvedPath = Life::Assets::ResolveAssetKeyToPath(assetKey);
        if (resolvedPath.IsFailure())
        {
            SetSceneStatus(resolvedPath.GetError().GetErrorMessage(), true);
            return false;
        }

        Life::Assets::AssetManager* assetManager = m_Services.AssetManager ? &m_Services.AssetManager->get() : nullptr;
        auto loadResult = Life::Assets::PrefabSerializer::Load(resolvedPath.GetValue(), assetManager);
        if (loadResult.IsFailure())
        {
            SetSceneStatus(loadResult.GetError().GetErrorMessage(), true);
            return false;
        }

        StopSceneExecution();
        m_SceneState.ResetPrefabMode();
        m_SceneState.PrefabScene = std::move(loadResult.GetValue());
        m_SceneState.PrefabAssetKey = assetKey;
        m_SceneState.PrefabAssetPath = resolvedPath.GetValue();
        m_SceneState.PrefabDisplayName = ResolvePrefabDisplayName(m_SceneState.PrefabAssetPath, assetKey);
        m_SceneState.PrefabDirty = false;
        m_SceneState.ClearSelection();
        m_PrefabUndoStack.Clear();
        SetSceneStatus("Opened prefab '" + m_SceneState.PrefabDisplayName + "'.", false);
        return true;
    }

    bool EditorShellOverlay::SavePrefabMode()
    {
        if (!m_SceneState.IsPrefabMode() || !m_SceneState.PrefabScene)
        {
            SetSceneStatus("No prefab is open.", true);
            return false;
        }

        const auto saveResult = Life::Assets::PrefabSerializer::SaveSceneAsPrefab(*m_SceneState.PrefabScene, m_SceneState.PrefabAssetPath);
        if (saveResult.IsFailure())
        {
            SetSceneStatus(saveResult.GetError().GetErrorMessage(), true);
            return false;
        }

        if (m_Services.AssetManager && !m_SceneState.PrefabAssetKey.empty())
            (void)m_Services.AssetManager->get().ReloadCachedAssetByKey(m_SceneState.PrefabAssetKey);

        m_SceneState.PrefabDirty = false;
        SetSceneStatus("Saved prefab '" + m_SceneState.PrefabDisplayName + "'.", false);
        return true;
    }

    bool EditorShellOverlay::ApplyPrefabModeToOpenScene()
    {
        if (!m_SceneState.IsPrefabMode() || !m_SceneState.PrefabScene)
        {
            SetSceneStatus("No prefab is open.", true);
            return false;
        }
        if (!m_Services.SceneService || !m_Services.SceneService->get().HasActiveScene())
        {
            SetSceneStatus("Applying prefab changes requires an open scene.", true);
            return false;
        }
        if (!m_Services.AssetManager)
        {
            SetSceneStatus("Applying prefab changes requires an asset manager.", true);
            return false;
        }

        if (m_SceneState.PrefabDirty && !SavePrefabMode())
            return false;

        Life::Ref<Life::Assets::PrefabAsset> prefab = m_Services.AssetManager->get().GetOrLoad<Life::Assets::PrefabAsset>(m_SceneState.PrefabAssetKey);
        if (!prefab || prefab->GetPrefabScene() == nullptr)
        {
            SetSceneStatus("Failed to load saved prefab '" + m_SceneState.PrefabDisplayName + "'.", true);
            return false;
        }

        const std::string& prefabGuid = prefab->GetGuid();
        if (prefabGuid.empty())
        {
            SetSceneStatus("Prefab asset has no GUID.", true);
            return false;
        }

        Life::SceneService& sceneService = m_Services.SceneService->get();
        Life::Scene& scene = sceneService.GetActiveScene();
        const std::vector<Life::Entity> instanceRoots = FindTopMostPrefabInstances(scene, prefabGuid);
        if (instanceRoots.empty())
        {
            SetSceneStatus("No open-scene instances use prefab '" + m_SceneState.PrefabDisplayName + "'.", false);
            return true;
        }

        std::vector<EditorEntitySnapshot> beforeSnapshots;
        std::vector<EditorEntitySnapshot> afterSnapshots;
        beforeSnapshots.reserve(instanceRoots.size());

        Life::Scene idSourceScene("PrefabApplyIds");
        for (const Life::Entity& instanceRoot : instanceRoots)
        {
            EditorEntitySnapshot before = CaptureEntitySnapshot(instanceRoot);
            std::vector<EditorEntitySnapshot> replacements = BuildPrefabApplySnapshots(
                *prefab->GetPrefabScene(),
                idSourceScene,
                prefabGuid,
                before);

            beforeSnapshots.push_back(std::move(before));
            for (EditorEntitySnapshot& replacement : replacements)
                afterSnapshots.push_back(std::move(replacement));
        }

        if (afterSnapshots.empty())
        {
            SetSceneStatus("Prefab '" + m_SceneState.PrefabDisplayName + "' does not contain any entities.", true);
            return false;
        }

        const bool applied = m_UndoStack.Execute(
            std::make_unique<RestoreEntitySnapshotsCommand>(std::move(beforeSnapshots), std::move(afterSnapshots)),
            scene);
        if (!applied)
        {
            SetSceneStatus("Failed to apply prefab changes to the open scene.", true);
            return false;
        }

        sceneService.MarkActiveSceneDirty();
        SetSceneStatus("Applied prefab '" + m_SceneState.PrefabDisplayName + "' to " + std::to_string(instanceRoots.size()) + " open-scene instance(s).", false);
        return true;
    }

    bool EditorShellOverlay::DiscardPrefabMode()
    {
        if (!m_SceneState.IsPrefabMode())
        {
            SetSceneStatus("No prefab is open.", true);
            return false;
        }

        const std::string assetKey = m_SceneState.PrefabAssetKey;
        const std::filesystem::path assetPath = m_SceneState.PrefabAssetPath;
        Life::Assets::AssetManager* assetManager = m_Services.AssetManager ? &m_Services.AssetManager->get() : nullptr;
        auto loadResult = Life::Assets::PrefabSerializer::Load(assetPath, assetManager);
        if (loadResult.IsFailure())
        {
            SetSceneStatus(loadResult.GetError().GetErrorMessage(), true);
            return false;
        }

        m_SceneState.PrefabScene = std::move(loadResult.GetValue());
        m_SceneState.PrefabAssetKey = assetKey;
        m_SceneState.PrefabAssetPath = assetPath;
        m_SceneState.PrefabDisplayName = ResolvePrefabDisplayName(assetPath, assetKey);
        m_SceneState.PrefabDirty = false;
        m_SceneState.ClearSelection();
        m_PrefabUndoStack.Clear();
        SetSceneStatus("Discarded prefab changes.", false);
        return true;
    }

    bool EditorShellOverlay::ExitPrefabMode()
    {
        if (!m_SceneState.IsPrefabMode())
            return true;
        if (m_SceneState.PrefabDirty)
        {
            SetSceneStatus("Save or discard prefab changes before leaving Prefab Mode.", true);
            return false;
        }

        const std::string displayName = m_SceneState.PrefabDisplayName;
        m_SceneState.ResetPrefabMode();
        m_SceneState.ClearSelection();
        m_PrefabUndoStack.Clear();
        SetSceneStatus("Closed prefab '" + displayName + "'.", false);
        return true;
    }

    void EditorShellOverlay::RenderPrefabModeBanner()
    {
#if __has_include(<imgui.h>)
        if (!m_SceneState.IsPrefabMode())
            return;

        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.23f, 0.27f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.42f, 0.70f, 0.78f, 0.72f));
        if (ImGui::BeginChild("##PrefabModeBanner", ImVec2(0.0f, 42.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
        {
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(ImVec4(0.62f, 0.76f, 0.90f, 1.0f), "Prefab");
            ImGui::SameLine();
            ImGui::TextUnformatted(m_SceneState.PrefabDisplayName.c_str());
            if (m_SceneState.PrefabDirty)
            {
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.88f, 0.63f, 0.28f, 1.0f), "Modified");
            }

            const float buttonWidth = 96.0f;
            const float totalWidth = buttonWidth * 4.0f + ImGui::GetStyle().ItemSpacing.x * 3.0f;
            const float startX = std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - totalWidth);
            ImGui::SameLine(startX);
            if (ImGui::Button("Save", ImVec2(buttonWidth, 0.0f)))
                (void)SavePrefabMode();
            ImGui::SameLine();
            if (ImGui::Button("Apply to Scene", ImVec2(buttonWidth, 0.0f)))
                (void)ApplyPrefabModeToOpenScene();
            ImGui::SameLine();
            if (ImGui::Button("Discard", ImVec2(buttonWidth, 0.0f)))
                (void)DiscardPrefabMode();
            ImGui::SameLine();
            if (ImGui::Button("Back", ImVec2(buttonWidth, 0.0f)))
                (void)ExitPrefabMode();
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
#endif
    }

    void EditorShellOverlay::HandlePendingPrefabModeRequests()
    {
        if (m_SceneState.RequestedOpenPrefabAssetKey.empty())
            return;

        const std::string assetKey = std::move(m_SceneState.RequestedOpenPrefabAssetKey);
        m_SceneState.RequestedOpenPrefabAssetKey.clear();
        (void)OpenPrefabMode(assetKey);
    }

    void EditorShellOverlay::HandleWorkspaceShortcuts()
    {
#if __has_include(<imgui.h>)
        ImGuiIO& io = ImGui::GetIO();
        if (io.WantTextInput)
            return;

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false))
        {
            if (m_Services.SceneService)
            {
                Life::SceneService& sceneService = m_Services.SceneService->get();
                Life::Scene* editableScene = m_SceneState.GetEditableScene(sceneService);
                EditorUndoStack& undoStack = m_SceneState.IsPrefabMode() ? m_PrefabUndoStack : m_UndoStack;
                if (editableScene != nullptr && undoStack.Undo(*editableScene, m_SceneState))
                {
                    m_SceneState.MarkEditableDocumentDirty(sceneService);
                    SetSceneStatus("Undo.", false);
                }
            }
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false))
        {
            if (m_Services.SceneService)
            {
                Life::SceneService& sceneService = m_Services.SceneService->get();
                Life::Scene* editableScene = m_SceneState.GetEditableScene(sceneService);
                EditorUndoStack& undoStack = m_SceneState.IsPrefabMode() ? m_PrefabUndoStack : m_UndoStack;
                if (editableScene != nullptr && undoStack.Redo(*editableScene, m_SceneState))
                {
                    m_SceneState.MarkEditableDocumentDirty(sceneService);
                    SetSceneStatus("Redo.", false);
                }
            }
            return;
        }

        if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false))
        {
            if (m_Services.SceneService)
            {
                Life::SceneService& sceneService = m_Services.SceneService->get();
                Life::Scene* editableScene = m_SceneState.GetEditableScene(sceneService);
                EditorUndoStack& undoStack = m_SceneState.IsPrefabMode() ? m_PrefabUndoStack : m_UndoStack;
                const std::vector<Life::Entity> selectedEntities = editableScene != nullptr
                    ? FilterTopLevelSelection(m_SceneState.GetSelectedEntities(*editableScene))
                    : std::vector<Life::Entity>{};
                std::vector<EditorEntitySnapshot> duplicateSnapshots;
                duplicateSnapshots.reserve(selectedEntities.size());
                for (const Life::Entity& selected : selectedEntities)
                    duplicateSnapshots.push_back(CreateDuplicateEntitySnapshot(selected));

                if (editableScene != nullptr &&
                    !duplicateSnapshots.empty() &&
                    undoStack.Execute(
                        std::make_unique<RestoreEntitySnapshotsCommand>(std::vector<EditorEntitySnapshot>{}, std::move(duplicateSnapshots)),
                        *editableScene,
                        m_SceneState))
                {
                    m_SceneState.MarkEditableDocumentDirty(sceneService);
                    SetSceneStatus(selectedEntities.size() == 1u ? "Duplicated entity." : "Duplicated entities.", false);
                }
            }
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Delete, false))
        {
            if (m_Services.SceneService)
            {
                Life::SceneService& sceneService = m_Services.SceneService->get();
                Life::Scene* editableScene = m_SceneState.GetEditableScene(sceneService);
                EditorUndoStack& undoStack = m_SceneState.IsPrefabMode() ? m_PrefabUndoStack : m_UndoStack;
                const std::vector<Life::Entity> selectedEntities = editableScene != nullptr
                    ? FilterTopLevelSelection(m_SceneState.GetSelectedEntities(*editableScene))
                    : std::vector<Life::Entity>{};
                std::vector<EditorEntitySnapshot> deleteSnapshots;
                deleteSnapshots.reserve(selectedEntities.size());
                for (const Life::Entity& selected : selectedEntities)
                    deleteSnapshots.push_back(CaptureEntitySnapshot(selected));

                if (editableScene != nullptr &&
                    !deleteSnapshots.empty() &&
                    undoStack.Execute(
                        std::make_unique<RestoreEntitySnapshotsCommand>(std::move(deleteSnapshots), std::vector<EditorEntitySnapshot>{}),
                        *editableScene,
                        m_SceneState))
                {
                    m_SceneState.MarkEditableDocumentDirty(sceneService);
                    SetSceneStatus(selectedEntities.size() == 1u ? "Deleted entity." : "Deleted entities.", false);
                }
            }
            return;
        }

        if (io.KeyCtrl || io.KeyAlt || io.KeyShift)
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_Q, false))
            m_SceneState.ViewportTool = EditorViewportTool::Select;
        else if (ImGui::IsKeyPressed(ImGuiKey_W, false))
            m_SceneState.ViewportTool = EditorViewportTool::Translate;
        else if (ImGui::IsKeyPressed(ImGuiKey_E, false))
            m_SceneState.ViewportTool = EditorViewportTool::Rotate;
        else if (ImGui::IsKeyPressed(ImGuiKey_R, false))
            m_SceneState.ViewportTool = EditorViewportTool::Scale;
#endif
    }

    bool EditorShellOverlay::SupportsRuntimeSceneTicks() const noexcept
    {
        return m_Services.SceneRuntime.has_value();
    }

    bool EditorShellOverlay::BeginSceneExecution(EditorSceneExecutionMode executionMode)
    {
        if (!m_Services.SceneService)
        {
            SetSceneStatus("Scene execution is unavailable because SceneService is not registered.", true);
            return false;
        }

        Life::SceneService& sceneService = m_Services.SceneService->get();
        if (m_SceneState.IsPrefabMode())
        {
            SetSceneStatus("Play and Simulation are unavailable in Prefab Mode. Use Back to return to the scene.", true);
            return false;
        }

        if (!sceneService.HasActiveScene())
        {
            SetSceneStatus("Open or create a scene before entering play or simulation mode.", true);
            return false;
        }

        if (executionMode == EditorSceneExecutionMode::Edit)
        {
            StopSceneExecution();
            return true;
        }

        Life::Scene& editScene = sceneService.GetActiveScene();
        if (!editScene.HasCamera())
        {
            SetSceneStatus("The active scene needs a camera before it can be played.", true);
            return false;
        }

        Life::Scope<Life::Scene> runtimeScene = editScene.Clone();
        if (!runtimeScene || !runtimeScene->HasCamera())
        {
            SetSceneStatus("Failed to prepare a runtime scene copy with a usable camera.", true);
            return false;
        }

        m_SceneState.RuntimeScene = std::move(runtimeScene);
        m_SceneState.ExecutionMode = executionMode;
        m_SceneState.Paused = false;
        m_SceneState.StepSingleFrame = false;
        m_SceneState.SupportsRuntimeTicks = SupportsRuntimeSceneTicks();
        if (m_SceneState.SupportsRuntimeTicks)
            m_Services.SceneRuntime->get().Start(*m_SceneState.RuntimeScene);

        const char* modeLabel = executionMode == EditorSceneExecutionMode::Simulation ? "Simulation" : "Play";
        if (m_SceneState.SupportsRuntimeTicks)
        {
            SetSceneStatus(std::string(modeLabel) + " mode started.", false);
        }
        else
        {
            SetSceneStatus(
                std::string(modeLabel) +
                    " preview started. The editor is rendering a runtime scene copy through the scene camera, but runtime update ticks are not connected yet.",
                false);
        }
        return true;
    }

    void EditorShellOverlay::StopSceneExecution()
    {
        if (!m_SceneState.IsRuntimeMode())
            return;

        if (m_Services.SceneRuntime)
            (void)m_Services.SceneRuntime->get().Stop();
        m_SceneState.ResetRuntimeState();
        SetSceneStatus("Returned to edit mode.", false);
    }

    void EditorShellOverlay::UpdateSceneExecution(float timestep)
    {
        (void)timestep;

        if (!m_SceneState.IsRuntimeMode())
            return;

        if (!m_SceneState.RuntimeScene)
        {
            StopSceneExecution();
            SetSceneStatus("Runtime scene state was lost. Returning to edit mode.", true);
            return;
        }

        if (!m_SceneState.SupportsRuntimeTicks)
        {
            m_SceneState.StepSingleFrame = false;
            return;
        }

        Life::SceneRuntime& runtime = m_Services.SceneRuntime->get();
        runtime.SetPaused(m_SceneState.Paused);
        if (m_SceneState.StepSingleFrame)
        {
            runtime.RequestStep();
            m_SceneState.StepSingleFrame = false;
        }
        (void)runtime.Update(*m_SceneState.RuntimeScene, timestep);
    }

    void EditorShellOverlay::OnAttach()
    {
        m_Services = EditorServices::Acquire(GetApplication());
        m_Shell.ResetLayout();
        m_ProjectHub.Attach();

        if (m_Services.CameraManager)
            m_CameraTool.Ensure(m_Services.CameraManager->get(), 16.0f / 9.0f);

        m_SceneViewportPanel.Attach(m_Services);

        if (m_Services.ProjectService && m_Services.ProjectService->get().HasActiveProject())
            SetMode(Mode::Workspace);
        else
            SetMode(Mode::ProjectHub);

        LOG_INFO("Editor shell overlay attached.");
    }

    void EditorShellOverlay::OnDetach()
    {
        m_SceneState.ResetRuntimeState();
        m_SceneState.ResetPrefabMode();
        m_PrefabUndoStack.Clear();
        m_ProjectHub.Detach();
        m_SceneViewportPanel.Detach();

        if (m_Services.CameraManager)
            m_CameraTool.Release(m_Services.CameraManager->get());

        m_Services.Reset();
        LOG_INFO("Editor shell overlay detached.");
    }

    void EditorShellOverlay::OnUpdate(float timestep)
    {
        if (m_Mode == Mode::Workspace)
        {
            UpdateSceneExecution(timestep);
            m_SceneViewportPanel.Update(m_Services, timestep);
            m_FpsOverlayPanel.Update(timestep);
        }

        if (m_Services.InputSystem && m_Services.Application && m_Services.InputSystem->get().WasActionStartedThisFrame("Editor", "Quit"))
            m_Services.Application->get().RequestShutdown();
    }

    void EditorShellOverlay::OnRender()
    {
        if (!m_Services.Application || !m_Services.HasImGui() || !m_Services.ProjectService)
            return;

        if (m_Services.Renderer)
        {
            m_Services.Renderer->get().Clear(
                kEditorBackBufferClearColor.r,
                kEditorBackBufferClearColor.g,
                kEditorBackBufferClearColor.b,
                kEditorBackBufferClearColor.a);
        }

        Life::Assets::ProjectService& projectService = m_Services.ProjectService->get();
        if (m_Mode == Mode::ProjectHub)
        {
            if (m_ProjectHub.Render(projectService))
                SetMode(Mode::Workspace);
            return;
        }

        EditorShellActions actions{};
        EditorShell::FrameContext frameContext{};
        frameContext.ActiveProjectName = projectService.HasActiveProject()
            ? projectService.GetActiveProject().Descriptor.Name.c_str()
            : nullptr;
        frameContext.ActiveProject = projectService.HasActiveProject()
            ? &projectService.GetActiveProject()
            : nullptr;
        if (m_Services.SceneService && m_Services.SceneService->get().HasActiveScene())
        {
            Life::SceneService& sceneService = m_Services.SceneService->get();
            frameContext.ActiveSceneName = m_SceneState.IsPrefabMode()
                ? m_SceneState.PrefabDisplayName.c_str()
                : sceneService.GetActiveScene().GetName().c_str();
            frameContext.HasActiveScene = true;
            frameContext.IsSceneDirty = m_SceneState.IsPrefabMode()
                ? m_SceneState.PrefabDirty
                : sceneService.IsActiveSceneDirty();
            frameContext.HasSceneCamera = m_SceneState.IsPrefabMode()
                ? (m_SceneState.PrefabScene && m_SceneState.PrefabScene->HasCamera())
                : sceneService.ActiveSceneHasCamera();
        }
        frameContext.ExecutionMode = m_SceneState.ExecutionMode;
        frameContext.IsPaused = m_SceneState.Paused;
        frameContext.SupportsRuntimeTicks = m_SceneState.SupportsRuntimeTicks;

        m_Shell.Begin(m_PanelVisibility, m_PanelState, actions, frameContext);
        RenderPrefabModeBanner();
        HandleWorkspaceShortcuts();
        EditorUndoStack& activeUndoStack = m_SceneState.IsPrefabMode() ? m_PrefabUndoStack : m_UndoStack;
        m_ProjectAssetsPanel.ApplyState(m_PanelState.ProjectAssets);
        m_ProjectAssetsPanel.Render(m_PanelVisibility.ShowProjectAssets, m_Services, m_SceneState);
        m_PanelState.ProjectAssets = m_ProjectAssetsPanel.CaptureState();
        m_HierarchyPanel.Render(m_PanelVisibility.ShowHierarchy, m_Services, m_SceneState, activeUndoStack);
        m_InspectorPanel.Render(m_PanelVisibility.ShowInspector, m_Services, m_SceneState, activeUndoStack);
        m_ConsolePanel.Render(m_PanelVisibility.ShowConsole);
        m_StatsPanel.Render(m_PanelVisibility.ShowStats, m_Services, m_SceneViewportPanel.GetState());
        m_SceneViewportPanel.Render(m_PanelVisibility.ShowScene, m_Services, m_SceneState, m_CameraTool, activeUndoStack);
        m_FpsOverlayPanel.Render(m_PanelVisibility.ShowFpsOverlay);
        RenderSceneDialogs();
        m_Shell.End(m_PanelVisibility, m_PanelState);

        HandlePendingPrefabModeRequests();
        HandleShellActions(actions);

        if (actions.RequestCloseProject)
        {
            if (m_SceneState.IsPrefabMode())
            {
                if (m_SceneState.PrefabDirty)
                {
                    SetSceneStatus("Save or discard prefab changes before closing the project.", true);
                    return;
                }

                m_SceneState.ResetPrefabMode();
                m_PrefabUndoStack.Clear();
            }

            const auto closeResult = projectService.CloseProject();
            if (closeResult.IsFailure())
                m_ProjectHub.SetStatusMessage(closeResult.GetError().GetErrorMessage(), true);
            else
            {
                m_ProjectHub.SetStatusMessage("Project closed.", false);
                SetMode(Mode::ProjectHub);
            }
        }
    }

    void EditorShellOverlay::OnEvent(Life::Event& event)
    {
        Life::EventDispatcher dispatcher(event);
        dispatcher.Dispatch<Life::WindowResizeEvent>([this](Life::WindowResizeEvent&)
        {
            m_Shell.ResetLayout();
            return false;
        });
        dispatcher.Dispatch<Life::WindowFileDroppedEvent>([this](Life::WindowFileDroppedEvent& dropEvent)
        {
            if (m_Mode != Mode::Workspace ||
                !m_PanelVisibility.ShowProjectAssets ||
                !m_Services.ProjectService ||
                !m_Services.ProjectService->get().HasActiveProject())
                return false;

            m_ProjectAssetsPanel.QueueExternalFileDrop(dropEvent.GetPath(), dropEvent.GetX(), dropEvent.GetY());
            return false;
        });
    }
}
