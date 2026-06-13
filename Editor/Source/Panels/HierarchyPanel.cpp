#include "Editor/Panels/HierarchyPanel.h"

#include "Assets/PrefabAsset.h"
#include "Assets/PrefabSerializer.h"
#include "Editor/EditorServices.h"
#include "Editor/Panels/ProjectAssetDragDrop.h"

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <system_error>

namespace EditorApp
{
    namespace
    {
        constexpr const char* kEntityPayloadType = "EditorSceneEntity";

        std::string g_RenamingEntityId;
        std::string g_RenameBuffer;

        enum class DropMode
        {
            Child,
            Before,
            After
        };

#if __has_include(<imgui.h>)
        void DrawPanelHeader(const char* title, const char* subtitle)
        {
            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "%s", title);
            ImGui::SameLine();
            ImGui::TextDisabled("%s", subtitle);
            ImGui::Separator();
        }

        DropMode DetermineDropMode(const ImVec2& rectMin, const ImVec2& rectMax)
        {
            const float itemHeight = rectMax.y - rectMin.y;
            const float mouseY = ImGui::GetIO().MousePos.y;
            if (itemHeight > 0.0f)
            {
                if (mouseY < rectMin.y + itemHeight * 0.25f)
                    return DropMode::Before;
                if (mouseY > rectMax.y - itemHeight * 0.25f)
                    return DropMode::After;
            }

            return DropMode::Child;
        }

        bool CanApplyDrop(const Life::Entity& dragged, const Life::Entity& target)
        {
            return dragged.IsValid() && target.IsValid() && dragged != target && !target.IsDescendantOf(dragged);
        }

        bool InputTextString(const char* label, std::string& value, ImGuiInputTextFlags flags = 0)
        {
            std::array<char, 1024> buffer{};
            const std::size_t copyLength = std::min(value.size(), buffer.size() - 1);
            std::memcpy(buffer.data(), value.data(), copyLength);
            buffer[copyLength] = '\0';

            if (!ImGui::InputText(label, buffer.data(), buffer.size(), flags))
                return false;

            value = buffer.data();
            return true;
        }

        std::string ToLowerAscii(std::string value)
        {
            std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
            return value;
        }

        bool EndsWith(std::string_view value, std::string_view suffix)
        {
            return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
        }

        bool IsPrefabAssetPath(const std::filesystem::path& relativePath)
        {
            return EndsWith(ToLowerAscii(relativePath.filename().string()), ".prefab.json");
        }

        std::string MakeAssetKey(const std::filesystem::path& relativePath)
        {
            return relativePath.empty() ? std::string{} : "Assets/" + relativePath.generic_string();
        }

        std::optional<Life::Assets::AssetDatabase::Record> ImportPrefabAssetIfPossible(const EditorServices& services,
                                                                                       const std::filesystem::path& relativePath)
        {
            if (!services.AssetDatabase || relativePath.empty() || !IsPrefabAssetPath(relativePath))
                return std::nullopt;

            const auto importResult = services.AssetDatabase->get().ImportOrUpdate(
                MakeAssetKey(relativePath),
                Life::Assets::AssetType::Prefab,
                nlohmann::json::object(),
                1u);
            if (importResult.IsFailure())
                return std::nullopt;

            return importResult.GetValue();
        }

        std::optional<Life::Assets::AssetDatabase::Record> ImportPrefabAssetIfPossible(const EditorServices& services,
                                                                                       const std::filesystem::path& assetsDirectory,
                                                                                       const std::filesystem::path& absolutePath)
        {
            std::error_code ec;
            const std::filesystem::path relativePath = std::filesystem::relative(absolutePath, assetsDirectory, ec).lexically_normal();
            if (ec)
                return std::nullopt;

            return ImportPrefabAssetIfPossible(services, relativePath);
        }

        std::optional<std::string> TryReadAssetMetaGuid(const std::filesystem::path& assetPath)
        {
            const std::filesystem::path metaPath = std::filesystem::path(assetPath.string() + ".meta");
            std::error_code ec;
            if (!std::filesystem::exists(metaPath, ec))
                return std::nullopt;

            try
            {
                std::ifstream input(metaPath, std::ios::in | std::ios::binary);
                if (!input.is_open())
                    return std::nullopt;

                nlohmann::json metaJson;
                input >> metaJson;
                if (!metaJson.contains("guid") || !metaJson["guid"].is_string())
                    return std::nullopt;

                return metaJson["guid"].get<std::string>();
            }
            catch (...)
            {
                return std::nullopt;
            }
        }

        std::optional<Life::Assets::AssetDatabase::Record> BuildPrefabRecordFromPath(const std::filesystem::path& assetsDirectory,
                                                                                     const std::filesystem::path& absolutePath,
                                                                                     const std::string& prefabGuid)
        {
            std::error_code ec;
            const std::filesystem::path relativePath = std::filesystem::relative(absolutePath, assetsDirectory, ec).lexically_normal();
            if (ec || relativePath.empty())
                return std::nullopt;

            Life::Assets::AssetDatabase::Record record;
            record.Guid = prefabGuid;
            record.Key = MakeAssetKey(relativePath);
            record.ResolvedPath = absolutePath.string();
            record.Type = Life::Assets::AssetType::Prefab;
            return record;
        }

        std::string SanitizePrefabFileStem(std::string value)
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

            return value.empty() ? std::string("Prefab") : value;
        }

        std::filesystem::path MakeUniquePrefabPath(const std::filesystem::path& prefabDirectory, const std::string& stem)
        {
            std::filesystem::path candidate = prefabDirectory / (stem + ".prefab.json");
            if (!std::filesystem::exists(candidate))
                return candidate;

            for (std::size_t index = 2; index < 10000; ++index)
            {
                candidate = prefabDirectory / (stem + "_" + std::to_string(index) + ".prefab.json");
                if (!std::filesystem::exists(candidate))
                    return candidate;
            }

            return prefabDirectory / (stem + "_copy.prefab.json");
        }

        bool CreatePrefabFromEntity(const EditorServices& services, EditorSceneState& sceneState, const Life::Entity& entity)
        {
            if (!entity.IsValid())
                return false;
            if (!services.ProjectService || !services.ProjectService->get().HasActiveProject())
            {
                sceneState.SetStatusMessage("Open a project before creating prefab assets.", true);
                return false;
            }

            const Life::Assets::Project& project = services.ProjectService->get().GetActiveProject();
            const std::filesystem::path prefabDirectory = project.Paths.AssetsDirectory / "Prefabs";
            std::error_code ec;
            std::filesystem::create_directories(prefabDirectory, ec);
            if (ec)
            {
                sceneState.SetStatusMessage("Failed to create Assets/Prefabs.", true);
                return false;
            }

            const std::string stem = SanitizePrefabFileStem(entity.GetTag());
            const std::filesystem::path destinationPath = MakeUniquePrefabPath(prefabDirectory, stem);
            const auto result = Life::Assets::PrefabSerializer::SaveEntityAsPrefab(entity.GetScene(), entity, destinationPath);
            if (result.IsFailure())
            {
                sceneState.SetStatusMessage(result.GetError().GetErrorMessage(), true);
                return false;
            }

            const std::filesystem::path relativePath = std::filesystem::relative(destinationPath, project.Paths.AssetsDirectory, ec).lexically_normal();
            (void)ImportPrefabAssetIfPossible(services, relativePath);
            sceneState.SetStatusMessage("Created prefab '" + relativePath.generic_string() + "'.", false);
            return true;
        }

        std::optional<Life::Assets::AssetDatabase::Record> ResolvePrefabRecordByGuid(const EditorServices& services, EditorSceneState& sceneState, const std::string& prefabGuid)
        {
            if (prefabGuid.empty())
                return std::nullopt;
            if (!services.AssetDatabase)
            {
                sceneState.SetStatusMessage("Prefab source lookup requires the asset database.", true);
                return std::nullopt;
            }

            auto recordResult = services.AssetDatabase->get().FindByGuid(prefabGuid);
            if (recordResult.IsFailure())
            {
                if (services.ProjectService && services.ProjectService->get().HasActiveProject())
                {
                    const Life::Assets::Project& project = services.ProjectService->get().GetActiveProject();
                    std::error_code ec;
                    if (std::filesystem::exists(project.Paths.AssetsDirectory, ec))
                    {
                        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(project.Paths.AssetsDirectory, ec))
                        {
                            if (ec)
                                break;
                            if (!entry.is_regular_file(ec) || !IsPrefabAssetPath(entry.path()))
                                continue;

                            const auto importedRecord = ImportPrefabAssetIfPossible(services, project.Paths.AssetsDirectory, entry.path());
                            if (importedRecord.has_value() && importedRecord->Guid == prefabGuid)
                            {
                                recordResult = *importedRecord;
                                break;
                            }

                            const auto metaGuid = TryReadAssetMetaGuid(entry.path());
                            if (metaGuid.has_value() && metaGuid.value() == prefabGuid)
                            {
                                const auto fallbackRecord = BuildPrefabRecordFromPath(project.Paths.AssetsDirectory, entry.path(), prefabGuid);
                                if (fallbackRecord.has_value())
                                {
                                    recordResult = *fallbackRecord;
                                    break;
                                }
                            }
                        }
                    }
                }

                if (recordResult.IsFailure())
                {
                    sceneState.SetStatusMessage("Could not find prefab source asset for GUID '" + prefabGuid + "'.", true);
                    return std::nullopt;
                }
            }

            Life::Assets::AssetDatabase::Record record = recordResult.GetValue();
            if (record.Type != Life::Assets::AssetType::Prefab)
            {
                sceneState.SetStatusMessage("Resolved prefab GUID does not point to a prefab asset.", true);
                return std::nullopt;
            }

            return record;
        }

        std::filesystem::path AssetKeyToProjectRelativePath(const std::string& assetKey)
        {
            constexpr std::string_view prefix = "Assets/";
            if (assetKey.rfind(prefix.data(), 0) == 0)
                return std::filesystem::path(assetKey.substr(prefix.size())).lexically_normal();
            return std::filesystem::path(assetKey).lexically_normal();
        }

        bool RequestOpenPrefabFromInstance(const EditorServices& services, EditorSceneState& sceneState, const Life::Entity& entity)
        {
            const Life::PrefabInstanceComponent* prefabInstance = entity.TryGetComponent<Life::PrefabInstanceComponent>();
            if (prefabInstance == nullptr)
                return false;

            const auto record = ResolvePrefabRecordByGuid(services, sceneState, prefabInstance->PrefabGuid);
            if (!record.has_value())
                return false;

            sceneState.RequestedOpenPrefabAssetKey = record->Key;
            return true;
        }

        bool SelectPrefabAssetFromInstance(const EditorServices& services, EditorSceneState& sceneState, const Life::Entity& entity)
        {
            const Life::PrefabInstanceComponent* prefabInstance = entity.TryGetComponent<Life::PrefabInstanceComponent>();
            if (prefabInstance == nullptr)
                return false;

            const auto record = ResolvePrefabRecordByGuid(services, sceneState, prefabInstance->PrefabGuid);
            if (!record.has_value())
                return false;

            sceneState.SelectProjectAsset(AssetKeyToProjectRelativePath(record->Key));
            sceneState.SetStatusMessage("Selected prefab asset '" + record->Key + "'.", false);
            return true;
        }

        Life::Entity ResolvePrefabInstanceRoot(Life::Entity entity)
        {
            const Life::PrefabInstanceComponent* prefabInstance = entity.TryGetComponent<Life::PrefabInstanceComponent>();
            if (prefabInstance == nullptr || prefabInstance->PrefabGuid.empty())
                return entity;

            const std::string prefabGuid = prefabInstance->PrefabGuid;
            Life::Entity root = entity;
            for (Life::Entity parent = entity.GetParent(); parent.IsValid(); parent = parent.GetParent())
            {
                const Life::PrefabInstanceComponent* parentPrefab = parent.TryGetComponent<Life::PrefabInstanceComponent>();
                if (parentPrefab == nullptr || parentPrefab->PrefabGuid != prefabGuid)
                    break;

                root = parent;
            }

            return root;
        }

        void RemovePrefabLinksRecursive(Life::Entity entity, const std::optional<std::string>& matchingGuid)
        {
            if (!entity.IsValid())
                return;

            if (Life::PrefabInstanceComponent* prefabInstance = entity.TryGetComponent<Life::PrefabInstanceComponent>())
            {
                if (!matchingGuid.has_value() || prefabInstance->PrefabGuid == matchingGuid.value())
                    (void)entity.RemoveComponent<Life::PrefabInstanceComponent>();
            }

            for (const Life::Entity child : entity.GetChildren())
                RemovePrefabLinksRecursive(child, matchingGuid);
        }

        bool UnpackPrefabInstance(Life::Scene& scene, Life::Entity entity, bool complete, EditorSceneState& sceneState, EditorUndoStack& undoStack)
        {
            if (!entity.IsValid() || !entity.HasComponent<Life::PrefabInstanceComponent>())
                return false;

            Life::Entity root = ResolvePrefabInstanceRoot(entity);
            const Life::PrefabInstanceComponent* rootPrefab = root.TryGetComponent<Life::PrefabInstanceComponent>();
            if (rootPrefab == nullptr)
                return false;

            const std::optional<std::string> matchingGuid = complete
                ? std::optional<std::string>{}
                : std::optional<std::string>{ rootPrefab->PrefabGuid };
            const EditorEntitySnapshot before = CaptureEntitySnapshot(root);
            RemovePrefabLinksRecursive(root, matchingGuid);
            const EditorEntitySnapshot after = CaptureEntitySnapshot(root);
            undoStack.CommitExecuted(std::make_unique<RestoreEntitySnapshotCommand>(before, after));
            sceneState.SelectEntity(root);
            sceneState.SetStatusMessage(complete ? "Unpacked prefab completely." : "Unpacked prefab instance.", false);
            return true;
        }

        bool InstantiatePrefabAsset(Life::Scene& scene,
                                    const EditorServices& services,
                                    EditorSceneState& sceneState,
                                    EditorUndoStack& undoStack,
                                    const std::filesystem::path& relativePath,
                                    Life::Entity parent = {})
        {
            if (!services.AssetManager)
            {
                sceneState.SetStatusMessage("Prefab instantiation requires an asset manager.", true);
                return false;
            }
            if (relativePath.empty() || !IsPrefabAssetPath(relativePath))
            {
                sceneState.SetStatusMessage("Dropped asset is not a prefab.", true);
                return false;
            }

            const std::string assetKey = MakeAssetKey(relativePath);
            Life::Ref<Life::Assets::PrefabAsset> prefab = services.AssetManager->get().GetOrLoad<Life::Assets::PrefabAsset>(assetKey);
            if (!prefab || prefab->GetPrefabScene() == nullptr)
            {
                sceneState.SetStatusMessage("Failed to load prefab '" + assetKey + "'.", true);
                return false;
            }

            Life::Entity created = scene.InstantiatePrefab(*prefab->GetPrefabScene(), parent, prefab->GetGuid());
            if (!created.IsValid())
            {
                sceneState.SetStatusMessage("Prefab '" + assetKey + "' did not contain any entities.", true);
                return false;
            }

            sceneState.SelectEntity(created);
            undoStack.CommitExecuted(std::make_unique<CreateEntityCommand>(CaptureEntitySnapshot(created)));
            sceneState.SetStatusMessage("Instantiated prefab '" + assetKey + "'.", false);
            return true;
        }

        std::string GetParentId(const Life::Entity& entity)
        {
            const Life::Entity parent = entity.GetParent();
            return parent.IsValid() ? parent.GetId() : std::string{};
        }

        void DrawDropPreview(const ImVec2& rectMin, const ImVec2& rectMax, DropMode mode, bool valid)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 lineColor = ImGui::GetColorU32(valid ? ImVec4(0.24f, 0.60f, 0.96f, 0.95f) : ImVec4(0.90f, 0.26f, 0.26f, 0.95f));
            const ImU32 fillColor = ImGui::GetColorU32(valid ? ImVec4(0.24f, 0.60f, 0.96f, 0.14f) : ImVec4(0.90f, 0.26f, 0.26f, 0.12f));
            const float thickness = 2.0f;

            switch (mode)
            {
                case DropMode::Child:
                    drawList->AddRectFilled(rectMin, rectMax, fillColor, 4.0f);
                    drawList->AddRect(rectMin, rectMax, lineColor, 4.0f, 0, thickness);
                    break;

                case DropMode::Before:
                    drawList->AddLine(ImVec2(rectMin.x, rectMin.y + 1.0f), ImVec2(rectMax.x, rectMin.y + 1.0f), lineColor, thickness);
                    break;

                case DropMode::After:
                    drawList->AddLine(ImVec2(rectMin.x, rectMax.y - 1.0f), ImVec2(rectMax.x, rectMax.y - 1.0f), lineColor, thickness);
                    break;
            }
        }

        void DrawRootDropPreview(const ImVec2& rectMin, const ImVec2& rectMax, bool valid)
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            const ImU32 lineColor = ImGui::GetColorU32(valid ? ImVec4(0.24f, 0.60f, 0.96f, 0.95f) : ImVec4(0.90f, 0.26f, 0.26f, 0.95f));
            const ImU32 fillColor = ImGui::GetColorU32(valid ? ImVec4(0.24f, 0.60f, 0.96f, 0.14f) : ImVec4(0.90f, 0.26f, 0.26f, 0.12f));
            drawList->AddRectFilled(rectMin, rectMax, fillColor, 4.0f);
            drawList->AddRect(rectMin, rectMax, lineColor, 4.0f, 0, 2.0f);
        }

        bool BeginEntityDragSource(const Life::Entity& entity)
        {
            if (!entity.IsValid() || !ImGui::BeginDragDropSource())
                return false;

            const std::string& entityId = entity.GetId();
            ImGui::SetDragDropPayload(kEntityPayloadType, entityId.c_str(), entityId.size() + 1u);
            ImGui::TextUnformatted(entity.GetTag().c_str());
            return true;
        }

        void DrawPrefabInstanceBadge()
        {
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.56f, 0.94f, 0.96f, 1.0f));
            ImGui::TextUnformatted("P");
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Prefab instance");
        }

        Life::Entity ResolvePayloadEntity(const ImGuiPayload* payload, Life::Scene& scene)
        {
            if (payload == nullptr || payload->Data == nullptr || payload->DataSize <= 0)
                return {};

            const char* payloadText = static_cast<const char*>(payload->Data);
            return scene.FindEntityById(payloadText);
        }

        bool ApplyDrop(Life::Scene& scene, Life::Entity dragged, const Life::Entity& target, DropMode mode, EditorUndoStack& undoStack)
        {
            if (!CanApplyDrop(dragged, target))
                return false;

            const std::string entityId = dragged.GetId();
            const std::string beforeParentId = GetParentId(dragged);
            const std::size_t beforeSiblingIndex = scene.GetSiblingIndex(dragged);
            const Life::TransformComponent beforeTransform = dragged.GetComponent<Life::TransformComponent>();
            const glm::mat4 worldTransform = scene.GetWorldTransformMatrix(dragged);
            bool changed = false;

            switch (mode)
            {
                case DropMode::Child:
                    changed = dragged.SetParent(target);
                    break;

                case DropMode::Before:
                {
                    const Life::Entity parent = target.GetParent();
                    if (parent.IsValid())
                    {
                        if (!dragged.SetParent(parent))
                            return false;
                    }
                    else
                    {
                        dragged.RemoveParent();
                    }

                    changed = scene.SetSiblingIndex(dragged, scene.GetSiblingIndex(target));
                    break;
                }

                case DropMode::After:
                {
                    const Life::Entity parent = target.GetParent();
                    if (parent.IsValid())
                    {
                        if (!dragged.SetParent(parent))
                            return false;
                    }
                    else
                    {
                        dragged.RemoveParent();
                    }

                    changed = scene.SetSiblingIndex(dragged, scene.GetSiblingIndex(target) + 1u);
                    break;
                }
            }

            if (!changed)
                return false;

            (void)Life::SetEntityWorldTransform(scene, dragged, worldTransform);
            const std::string afterParentId = GetParentId(dragged);
            const std::size_t afterSiblingIndex = scene.GetSiblingIndex(dragged);
            const Life::TransformComponent afterTransform = dragged.GetComponent<Life::TransformComponent>();
            undoStack.CommitExecuted(std::make_unique<SetEntityParentCommand>(
                entityId,
                beforeParentId,
                beforeSiblingIndex,
                beforeTransform,
                afterParentId,
                afterSiblingIndex,
                afterTransform));
            return true;
        }

        bool AcceptEntityDrop(Life::Scene& scene,
                              const Life::Entity& target,
                              const EditorServices& services,
                              EditorSceneState& sceneState,
                              EditorUndoStack& undoStack)
        {
            if (!ImGui::BeginDragDropTarget())
                return false;

            bool changed = false;
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                const Life::Entity dragged = ResolvePayloadEntity(payload, scene);
                const ImVec2 rectMin = ImGui::GetItemRectMin();
                const ImVec2 rectMax = ImGui::GetItemRectMax();
                const DropMode mode = DetermineDropMode(rectMin, rectMax);
                const bool valid = CanApplyDrop(dragged, target);

                DrawDropPreview(rectMin, rectMax, mode, valid);

                if (payload->Delivery && valid)
                    changed = ApplyDrop(scene, dragged, target, mode, undoStack);
            }
            else if (const ImGuiPayload* assetDropPayload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(assetDropPayload->Data);
                const bool valid = assetPayload != nullptr &&
                    assetPayload->RelativePath[0] != '\0' &&
                    (assetPayload->Kind == ProjectAssetPayloadKind::Prefab ||
                        IsPrefabAssetPath(std::filesystem::path(assetPayload->RelativePath.data())));

                DrawDropPreview(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), DropMode::Child, valid);

                if (assetDropPayload->Delivery && valid && TryConsumeProjectAssetDropDelivery(*assetPayload))
                    changed = InstantiatePrefabAsset(scene, services, sceneState, undoStack, std::filesystem::path(assetPayload->RelativePath.data()), target);
            }

            ImGui::EndDragDropTarget();
            return changed;
        }

        bool RenderRootDropTarget(Life::Scene& scene, const EditorServices& services, EditorSceneState& sceneState, EditorUndoStack& undoStack)
        {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextUnformatted("Scene Root");
            const ImVec2 targetSize(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight() * 1.35f);
            ImGui::Selectable("Drop here to reparent to root", false, ImGuiSelectableFlags_None, targetSize);

            bool changed = false;
            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kEntityPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
                {
                    Life::Entity dragged = ResolvePayloadEntity(payload, scene);
                    const bool valid = dragged.IsValid() && dragged.HasParent();
                    DrawRootDropPreview(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), valid);

                    if (payload->Delivery && valid)
                    {
                        const std::string entityId = dragged.GetId();
                        const std::string beforeParentId = GetParentId(dragged);
                        const std::size_t beforeSiblingIndex = scene.GetSiblingIndex(dragged);
                        const Life::TransformComponent beforeTransform = dragged.GetComponent<Life::TransformComponent>();
                        const glm::mat4 worldTransform = scene.GetWorldTransformMatrix(dragged);
                        dragged.RemoveParent();
                        (void)Life::SetEntityWorldTransform(scene, dragged, worldTransform);
                        undoStack.CommitExecuted(std::make_unique<SetEntityParentCommand>(
                            entityId,
                            beforeParentId,
                            beforeSiblingIndex,
                            beforeTransform,
                            std::string{},
                            scene.GetSiblingIndex(dragged),
                            dragged.GetComponent<Life::TransformComponent>()));
                        changed = true;
                    }
                }
                else if (const ImGuiPayload* assetDropPayload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
                {
                    const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(assetDropPayload->Data);
                    const bool valid = assetPayload != nullptr &&
                        assetPayload->RelativePath[0] != '\0' &&
                        (assetPayload->Kind == ProjectAssetPayloadKind::Prefab ||
                            IsPrefabAssetPath(std::filesystem::path(assetPayload->RelativePath.data())));
                    DrawRootDropPreview(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), valid);

                    if (assetDropPayload->Delivery && valid && TryConsumeProjectAssetDropDelivery(*assetPayload))
                        changed = InstantiatePrefabAsset(scene, services, sceneState, undoStack, std::filesystem::path(assetPayload->RelativePath.data()));
                }

                ImGui::EndDragDropTarget();
            }

            return changed;
        }

        bool RenderEntityNode(Life::Scene& scene,
                              const Life::Entity& entity,
                              const EditorServices& services,
                              EditorSceneState& sceneState,
                              EditorUndoStack& undoStack,
                              bool& entityContextHandled)
        {
            bool changed = false;
            const bool isSelected = sceneState.SelectedEntityId == entity.GetId();
            const bool isPrefabInstance = entity.HasComponent<Life::PrefabInstanceComponent>();
            const auto children = entity.GetChildren();

            ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
            if (children.empty())
                nodeFlags |= ImGuiTreeNodeFlags_Leaf;
            if (isSelected)
                nodeFlags |= ImGuiTreeNodeFlags_Selected;

            const bool isRenaming = g_RenamingEntityId == entity.GetId();
            if (isPrefabInstance)
            {
                ImGui::PushStyleColor(ImGuiCol_Header, isSelected ? ImVec4(0.14f, 0.42f, 0.46f, 0.92f) : ImVec4(0.10f, 0.28f, 0.32f, 0.72f));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.14f, 0.46f, 0.50f, 0.92f));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.16f, 0.52f, 0.56f, 0.96f));
            }
            const bool nodeOpen = ImGui::TreeNodeEx(entity.GetId().c_str(), nodeFlags, "%s", isRenaming ? "" : entity.GetTag().c_str());
            const bool entityItemClicked = ImGui::IsItemClicked();
            const bool entityItemDoubleClicked = ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
            const bool entityItemRightClicked = ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right);
            if (!isRenaming && BeginEntityDragSource(entity))
                ImGui::EndDragDropSource();

            changed |= AcceptEntityDrop(scene, entity, services, sceneState, undoStack);

            const std::string contextPopupId = "##EntityContext_" + entity.GetId();
            if (entityItemRightClicked)
            {
                entityContextHandled = true;
                ImGui::OpenPopup(contextPopupId.c_str());
            }
            if (isPrefabInstance)
                ImGui::PopStyleColor(3);
            if (isPrefabInstance && !isRenaming)
                DrawPrefabInstanceBadge();
            if (entityItemClicked)
                sceneState.SelectEntity(entity);
            if (entityItemDoubleClicked)
            {
                if (isPrefabInstance)
                {
                    (void)RequestOpenPrefabFromInstance(services, sceneState, entity);
                }
                else
                {
                    g_RenamingEntityId = entity.GetId();
                    g_RenameBuffer = entity.GetTag();
                }
            }

            if (isRenaming)
            {
                ImGui::SameLine();
                ImGui::SetNextItemWidth(-1.0f);
                ImGui::SetKeyboardFocusHere();
                const bool submitted = InputTextString("##RenameEntityInline", g_RenameBuffer, ImGuiInputTextFlags_EnterReturnsTrue);
                const bool finished = submitted || (ImGui::IsItemDeactivatedAfterEdit() && !ImGui::IsItemActive());
                if (finished)
                {
                    if (!g_RenameBuffer.empty() && g_RenameBuffer != entity.GetTag())
                    {
                        changed |= undoStack.Execute(
                            std::make_unique<RenameEntityCommand>(entity.GetId(), entity.GetTag(), g_RenameBuffer),
                            scene,
                            sceneState);
                    }
                    g_RenamingEntityId.clear();
                    g_RenameBuffer.clear();
                }
            }

            if (ImGui::BeginPopup(contextPopupId.c_str()))
            {
                entityContextHandled = true;
                if (isPrefabInstance)
                {
                    if (ImGui::MenuItem("Open Prefab"))
                        (void)RequestOpenPrefabFromInstance(services, sceneState, entity);
                    if (ImGui::MenuItem("Select Prefab Asset"))
                        (void)SelectPrefabAssetFromInstance(services, sceneState, entity);
                    if (ImGui::MenuItem("Unpack Prefab"))
                        changed |= UnpackPrefabInstance(scene, entity, false, sceneState, undoStack);
                    if (ImGui::MenuItem("Unpack Prefab Completely"))
                        changed |= UnpackPrefabInstance(scene, entity, true, sceneState, undoStack);
                    ImGui::Separator();
                }

                if (ImGui::MenuItem("Create Child"))
                {
                    const Life::Entity child = scene.CreateChildEntity(entity, "Entity");
                    sceneState.SelectEntity(child);
                    undoStack.CommitExecuted(std::make_unique<CreateEntityCommand>(CaptureEntitySnapshot(child)));
                    changed = true;
                }

                if (ImGui::MenuItem("Create Sprite Child"))
                {
                    Life::Entity child = scene.CreateChildEntity(entity, "Sprite");
                    child.AddComponent<Life::SpriteComponent>();
                    child.AddComponent<Life::SpriteRendererComponent>();
                    sceneState.SelectEntity(child);
                    undoStack.CommitExecuted(std::make_unique<CreateEntityCommand>(CaptureEntitySnapshot(child)));
                    changed = true;
                }

                if (ImGui::MenuItem("Duplicate"))
                    changed |= undoStack.Execute(std::make_unique<DuplicateEntityCommand>(CreateDuplicateEntitySnapshot(entity)), scene, sceneState);

                if (ImGui::MenuItem("Create Prefab"))
                    (void)CreatePrefabFromEntity(services, sceneState, entity);

                if (ImGui::MenuItem("Rename"))
                {
                    g_RenamingEntityId = entity.GetId();
                    g_RenameBuffer = entity.GetTag();
                }

                if (ImGui::MenuItem("Delete Entity"))
                    changed |= undoStack.Execute(std::make_unique<DeleteEntityCommand>(CaptureEntitySnapshot(entity)), scene, sceneState);

                ImGui::EndPopup();
            }

            if (nodeOpen)
            {
                for (const Life::Entity child : children)
                    changed |= RenderEntityNode(scene, child, services, sceneState, undoStack, entityContextHandled);
                ImGui::TreePop();
            }

            return changed;
        }
#endif
    }

    void HierarchyPanel::Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState, EditorUndoStack& undoStack) const
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
            return;

        if (ImGui::Begin("Hierarchy", &isOpen))
        {
            DrawPanelHeader("Hierarchy", "Scene structure and parenting");

            if (!sceneState.StatusMessage.empty())
            {
                const ImVec4 color = sceneState.StatusIsError
                    ? ImVec4(0.95f, 0.35f, 0.35f, 1.0f)
                    : ImVec4(0.35f, 0.85f, 0.45f, 1.0f);
                ImGui::TextColored(color, "%s", sceneState.StatusMessage.c_str());
                ImGui::Separator();
            }

            if (!services.SceneService || !services.SceneService->get().HasActiveScene())
            {
                ImGui::TextUnformatted("No active scene.");
            }
            else
            {
                Life::SceneService& sceneService = services.SceneService->get();
                Life::Scene* effectiveScene = sceneState.GetEffectiveScene(sceneService);
                if (effectiveScene == nullptr)
                {
                    ImGui::TextUnformatted("No active scene.");
                    ImGui::End();
                    return;
                }

                Life::Scene& scene = *effectiveScene;
                bool changed = false;
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.33f, 0.54f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.41f, 0.64f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.29f, 0.48f, 1.0f));
                if (ImGui::Button("Create Entity", ImVec2(-1.0f, 0.0f)))
                {
                    const Life::Entity entity = scene.CreateEntity("Entity");
                    sceneState.SelectEntity(entity);
                    undoStack.CommitExecuted(std::make_unique<CreateEntityCommand>(CaptureEntitySnapshot(entity)));
                    changed = true;
                }
                ImGui::PopStyleColor(3);

                ImGui::SeparatorText("Entities");

                bool entityContextHandled = false;
                const auto roots = scene.GetRootEntities();
                if (roots.empty())
                    ImGui::TextDisabled("No entities in the active scene.");
                for (const Life::Entity root : roots)
                    changed |= RenderEntityNode(scene, root, services, sceneState, undoStack, entityContextHandled);

                changed |= RenderRootDropTarget(scene, services, sceneState, undoStack);

                if (!entityContextHandled && ImGui::BeginPopupContextWindow("HierarchyContext", ImGuiPopupFlags_NoOpenOverItems | ImGuiPopupFlags_MouseButtonRight))
                {
                    if (ImGui::MenuItem("Create Entity"))
                    {
                        const Life::Entity entity = scene.CreateEntity("Entity");
                        sceneState.SelectEntity(entity);
                        undoStack.CommitExecuted(std::make_unique<CreateEntityCommand>(CaptureEntitySnapshot(entity)));
                        changed = true;
                    }
                    if (ImGui::MenuItem("Create Sprite"))
                    {
                        Life::Entity entity = scene.CreateEntity("Sprite");
                        entity.AddComponent<Life::SpriteComponent>();
                        entity.AddComponent<Life::SpriteRendererComponent>();
                        sceneState.SelectEntity(entity);
                        undoStack.CommitExecuted(std::make_unique<CreateEntityCommand>(CaptureEntitySnapshot(entity)));
                        changed = true;
                    }
                    ImGui::EndPopup();
                }

                if (changed)
                {
                    sceneState.MarkEditableDocumentDirty(sceneService);
                }
            }
        }
        ImGui::End();
#else
        (void)isOpen;
        (void)services;
        (void)sceneState;
#endif
    }
}
