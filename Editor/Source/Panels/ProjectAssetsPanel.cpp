#include "Editor/Panels/ProjectAssetsPanel.h"

#include "Assets/PrefabAsset.h"
#include "Assets/PrefabSerializer.h"
#include "Editor/EditorServices.h"
#include "Editor/PathSafety.h"
#include "Editor/Panels/ProjectAssetDragDrop.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <functional>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <glm/gtc/matrix_transform.hpp>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif

namespace EditorApp
{
    namespace
    {
        constexpr float kMinGridScale = 0.0f;
        constexpr float kMaxGridScale = 1.8f;
        constexpr const char* kEntityPayloadType = "EditorSceneEntity";

        enum class ProjectEntryKind
        {
            Directory,
            Scene,
            Prefab,
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

        void ImportPrefabAssetIfPossible(const EditorServices& services, const std::filesystem::path& relativePath)
        {
            if (!services.AssetDatabase || relativePath.empty())
                return;

            (void)services.AssetDatabase->get().ImportOrUpdate(
                MakeAssetKey(relativePath),
                Life::Assets::AssetType::Prefab,
                nlohmann::json::object(),
                1u);
        }

        bool EndsWith(std::string_view value, std::string_view suffix)
        {
            return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
        }

        bool IsMetaEntry(const std::filesystem::path& path, bool isDirectory)
        {
            return !isDirectory && ToLowerAscii(path.extension().string()) == ".meta";
        }

        std::filesystem::path GetMetaPathForAsset(const std::filesystem::path& assetPath)
        {
            return std::filesystem::path(assetPath.string() + ".meta");
        }

        ProjectEntryKind ClassifyEntry(const std::filesystem::path& path, bool isDirectory)
        {
            if (isDirectory)
                return ProjectEntryKind::Directory;

            const std::string lowerName = ToLowerAscii(path.filename().string());
            const std::string lowerExtension = ToLowerAscii(path.extension().string());
            if (EndsWith(lowerName, ".scene") || EndsWith(lowerName, ".scene.json"))
                return ProjectEntryKind::Scene;
            if (EndsWith(lowerName, ".prefab.json"))
                return ProjectEntryKind::Prefab;
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

        std::string ResolveVisibleName(const ProjectAssetEntry& entry)
        {
            return ResolveDisplayStem(entry);
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

                if (IsMetaEntry(child.path(), entry.IsDirectory))
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

        void AppendSearchEntries(
            const std::filesystem::path& assetsDirectory,
            const std::filesystem::path& relativeFolder,
            const std::string& filterLower,
            std::vector<ProjectAssetEntry>& outEntries)
        {
            const auto entries = CollectEntries(assetsDirectory, relativeFolder);
            for (const ProjectAssetEntry& entry : entries)
            {
                if (MatchesFilter(entry, filterLower))
                    outEntries.push_back(entry);

                if (entry.IsDirectory)
                    AppendSearchEntries(assetsDirectory, entry.RelativePath, filterLower, outEntries);
            }
        }

        std::vector<ProjectAssetEntry> CollectSearchEntries(const std::filesystem::path& assetsDirectory, const std::string& filterLower)
        {
            std::vector<ProjectAssetEntry> entries;
            if (filterLower.empty())
                return entries;

            AppendSearchEntries(assetsDirectory, {}, filterLower, entries);
            std::sort(entries.begin(), entries.end(), [](const ProjectAssetEntry& left, const ProjectAssetEntry& right)
            {
                if (left.IsDirectory != right.IsDirectory)
                    return left.IsDirectory > right.IsDirectory;

                return ToLowerAscii(left.RelativePath.generic_string()) < ToLowerAscii(right.RelativePath.generic_string());
            });
            return entries;
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

#if __has_include(<imgui.h>)
         ImVec4 ResolveAccentColor(ProjectEntryKind kind)
         {
             switch (kind)
             {
                 case ProjectEntryKind::Directory: return ImVec4(0.38f, 0.62f, 0.96f, 1.0f);
                 case ProjectEntryKind::Scene: return ImVec4(0.40f, 0.82f, 0.60f, 1.0f);
                 case ProjectEntryKind::Prefab: return ImVec4(0.36f, 0.84f, 0.86f, 1.0f);
                 case ProjectEntryKind::Texture: return ImVec4(0.88f, 0.58f, 0.36f, 1.0f);
                 case ProjectEntryKind::Material: return ImVec4(0.74f, 0.52f, 0.92f, 1.0f);
                 case ProjectEntryKind::Shader: return ImVec4(0.96f, 0.72f, 0.36f, 1.0f);
                 case ProjectEntryKind::Other:
                 default: return ImVec4(0.62f, 0.66f, 0.76f, 1.0f);
             }
         }

         void DrawProjectEntryIcon(ProjectEntryKind kind, ImVec2 topLeft, float size, ImVec4 accentColor)
         {
             ImDrawList* drawList = ImGui::GetWindowDrawList();
             const ImU32 color = ImGui::GetColorU32(accentColor);
             const ImU32 muted = ImGui::GetColorU32(ImVec4(accentColor.x, accentColor.y, accentColor.z, 0.34f));
             const ImU32 dark = ImGui::GetColorU32(ImVec4(0.04f, 0.05f, 0.07f, 0.86f));
             const float x = topLeft.x;
             const float y = topLeft.y;
             const float s = size;
             const float stroke = std::max(1.0f, s * 0.075f);

             switch (kind)
             {
                 case ProjectEntryKind::Directory:
                 {
                     drawList->AddRectFilled(ImVec2(x + s * 0.06f, y + s * 0.28f), ImVec2(x + s * 0.94f, y + s * 0.86f), muted, s * 0.08f);
                     drawList->AddRectFilled(ImVec2(x + s * 0.10f, y + s * 0.18f), ImVec2(x + s * 0.46f, y + s * 0.36f), muted, s * 0.05f);
                     drawList->AddRect(ImVec2(x + s * 0.06f, y + s * 0.28f), ImVec2(x + s * 0.94f, y + s * 0.86f), color, s * 0.08f, 0, stroke);
                     break;
                 }
                 case ProjectEntryKind::Scene:
                 {
                     for (int row = 0; row < 2; ++row)
                     {
                         for (int column = 0; column < 2; ++column)
                         {
                             const ImVec2 center(x + s * (0.34f + column * 0.32f), y + s * (0.34f + row * 0.32f));
                             drawList->AddCircleFilled(center, s * 0.10f, muted, 12);
                             drawList->AddCircle(center, s * 0.10f, color, 12, stroke);
                         }
                     }
                     drawList->AddLine(ImVec2(x + s * 0.34f, y + s * 0.34f), ImVec2(x + s * 0.66f, y + s * 0.66f), color, stroke);
                     drawList->AddLine(ImVec2(x + s * 0.66f, y + s * 0.34f), ImVec2(x + s * 0.34f, y + s * 0.66f), color, stroke);
                     break;
                 }
                 case ProjectEntryKind::Prefab:
                 {
                     const ImVec2 a(x + s * 0.50f, y + s * 0.14f);
                     const ImVec2 b(x + s * 0.82f, y + s * 0.32f);
                     const ImVec2 c(x + s * 0.82f, y + s * 0.68f);
                     const ImVec2 d(x + s * 0.50f, y + s * 0.86f);
                     const ImVec2 e(x + s * 0.18f, y + s * 0.68f);
                     const ImVec2 f(x + s * 0.18f, y + s * 0.32f);
                     const std::array<ImVec2, 6> points{ a, b, c, d, e, f };
                     drawList->AddPolyline(points.data(), static_cast<int>(points.size()), color, ImDrawFlags_Closed, stroke);
                     drawList->AddLine(a, d, color, stroke);
                     drawList->AddLine(f, ImVec2(x + s * 0.50f, y + s * 0.50f), color, stroke);
                     drawList->AddLine(b, ImVec2(x + s * 0.50f, y + s * 0.50f), color, stroke);
                     break;
                 }
                 case ProjectEntryKind::Texture:
                 {
                     drawList->AddRectFilled(ImVec2(x + s * 0.16f, y + s * 0.18f), ImVec2(x + s * 0.84f, y + s * 0.82f), dark, s * 0.04f);
                     drawList->AddRect(ImVec2(x + s * 0.16f, y + s * 0.18f), ImVec2(x + s * 0.84f, y + s * 0.82f), color, s * 0.04f, 0, stroke);
                     drawList->AddCircleFilled(ImVec2(x + s * 0.66f, y + s * 0.36f), s * 0.07f, color, 12);
                     drawList->AddTriangleFilled(ImVec2(x + s * 0.24f, y + s * 0.74f), ImVec2(x + s * 0.46f, y + s * 0.50f), ImVec2(x + s * 0.66f, y + s * 0.74f), muted);
                     drawList->AddTriangleFilled(ImVec2(x + s * 0.42f, y + s * 0.74f), ImVec2(x + s * 0.62f, y + s * 0.56f), ImVec2(x + s * 0.78f, y + s * 0.74f), muted);
                     break;
                 }
                 case ProjectEntryKind::Material:
                 {
                     drawList->AddCircleFilled(ImVec2(x + s * 0.50f, y + s * 0.50f), s * 0.34f, muted, 24);
                     drawList->AddCircle(ImVec2(x + s * 0.50f, y + s * 0.50f), s * 0.34f, color, 24, stroke);
                     drawList->AddCircleFilled(ImVec2(x + s * 0.40f, y + s * 0.38f), s * 0.10f, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.30f)), 16);
                     break;
                 }
                 case ProjectEntryKind::Shader:
                 {
                     drawList->AddLine(ImVec2(x + s * 0.38f, y + s * 0.24f), ImVec2(x + s * 0.18f, y + s * 0.50f), color, stroke);
                     drawList->AddLine(ImVec2(x + s * 0.18f, y + s * 0.50f), ImVec2(x + s * 0.38f, y + s * 0.76f), color, stroke);
                     drawList->AddLine(ImVec2(x + s * 0.62f, y + s * 0.24f), ImVec2(x + s * 0.82f, y + s * 0.50f), color, stroke);
                     drawList->AddLine(ImVec2(x + s * 0.82f, y + s * 0.50f), ImVec2(x + s * 0.62f, y + s * 0.76f), color, stroke);
                     drawList->AddLine(ImVec2(x + s * 0.54f, y + s * 0.22f), ImVec2(x + s * 0.42f, y + s * 0.78f), color, stroke);
                     break;
                 }
                 case ProjectEntryKind::Other:
                 default:
                 {
                     drawList->AddRectFilled(ImVec2(x + s * 0.24f, y + s * 0.14f), ImVec2(x + s * 0.76f, y + s * 0.86f), dark, s * 0.04f);
                     drawList->AddRect(ImVec2(x + s * 0.24f, y + s * 0.14f), ImVec2(x + s * 0.76f, y + s * 0.86f), color, s * 0.04f, 0, stroke);
                     drawList->AddLine(ImVec2(x + s * 0.34f, y + s * 0.38f), ImVec2(x + s * 0.66f, y + s * 0.38f), color, stroke);
                     drawList->AddLine(ImVec2(x + s * 0.34f, y + s * 0.54f), ImVec2(x + s * 0.66f, y + s * 0.54f), color, stroke);
                     break;
                 }
             }
         }

         bool DrawTextureThumbnail(Life::TextureResource& texture,
                                   Life::ImGuiSystem& imguiSystem,
                                   ImVec2 minimum,
                                   ImVec2 maximum,
                                   bool preserveAspect = true)
         {
             void* handle = imguiSystem.GetTextureHandle(texture, Life::ImGuiTextureSampling::Nearest);
             if (handle == nullptr)
                 return false;

             ImVec2 imageMin = minimum;
             ImVec2 imageMax = maximum;
             if (preserveAspect)
             {
                 const float boxWidth = std::max(maximum.x - minimum.x, 1.0f);
                 const float boxHeight = std::max(maximum.y - minimum.y, 1.0f);
                 const float textureWidth = static_cast<float>(std::max(texture.GetWidth(), 1u));
                 const float textureHeight = static_cast<float>(std::max(texture.GetHeight(), 1u));
                 const float scale = std::min(boxWidth / textureWidth, boxHeight / textureHeight);
                 const ImVec2 imageSize(textureWidth * scale, textureHeight * scale);
                 imageMin = ImVec2(minimum.x + (boxWidth - imageSize.x) * 0.5f, minimum.y + (boxHeight - imageSize.y) * 0.5f);
                 imageMax = ImVec2(imageMin.x + imageSize.x, imageMin.y + imageSize.y);
             }

             ImDrawList* drawList = ImGui::GetWindowDrawList();
             drawList->AddRectFilled(minimum, maximum, ImGui::GetColorU32(ImVec4(0.04f, 0.05f, 0.07f, 0.92f)), 4.0f);
             drawList->AddImage(ImTextureRef(handle), imageMin, imageMax);
             drawList->AddRect(minimum, maximum, ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, 0.16f)), 4.0f);
             return true;
         }

         std::array<glm::vec3, 4> ResolveSpriteWorldCorners(const Life::Scene& scene, const Life::Entity& entity, const Life::SpriteComponent& sprite)
         {
             const glm::mat4 worldTransform = scene.GetWorldTransformMatrix(entity);
             const glm::vec3 center = glm::vec3(worldTransform[3]);
             const glm::vec3 xAxis = glm::vec3(worldTransform * glm::vec4(sprite.Size.x, 0.0f, 0.0f, 0.0f));
             const glm::vec3 yAxis = glm::vec3(worldTransform * glm::vec4(0.0f, sprite.Size.y, 0.0f, 0.0f));
             return {{
                 center - (xAxis * 0.5f) - (yAxis * 0.5f),
                 center + (xAxis * 0.5f) - (yAxis * 0.5f),
                 center + (xAxis * 0.5f) + (yAxis * 0.5f),
                 center - (xAxis * 0.5f) + (yAxis * 0.5f)
             }};
         }

         bool ResolvePrefabPreviewBounds(const Life::Scene& scene, glm::vec3& center, float& orthographicSize)
         {
             glm::vec3 minimum(std::numeric_limits<float>::infinity());
             glm::vec3 maximum(-std::numeric_limits<float>::infinity());
             bool hasBounds = false;
             for (const Life::Entity entity : scene.GetEntities())
             {
                 if (!entity.IsEnabled())
                     continue;

                 const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>();
                 if (sprite == nullptr)
                     continue;

                 const auto corners = ResolveSpriteWorldCorners(scene, entity, *sprite);
                 for (const glm::vec3& corner : corners)
                 {
                     minimum = glm::min(minimum, corner);
                     maximum = glm::max(maximum, corner);
                     hasBounds = true;
                 }
             }

             if (!hasBounds)
                 return false;

             center = (minimum + maximum) * 0.5f;
             const glm::vec3 span = maximum - minimum;
             orthographicSize = std::max({ span.x, span.y, 1.0f }) * 1.18f;
             return true;
         }

         void ResolvePrefabSpriteTextures(Life::Scene& scene, const EditorServices& services)
         {
             if (!services.AssetManager)
                 return;

             for (Life::Entity entity : scene.GetEntities())
             {
                 Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>();
                 if (sprite == nullptr || sprite->TextureAsset || sprite->TextureAssetKey.empty())
                     continue;

                 sprite->TextureAsset = services.AssetManager->get().GetOrLoad<Life::Assets::TextureAsset>(sprite->TextureAssetKey);
             }
         }

         bool RenderPrefabPreviewToSurface(Life::SceneSurface& surface,
                                           Life::Scene& scene,
                                           const EditorServices& services,
                                           uint32_t thumbnailSize)
         {
             if (!services.SceneRenderer2D || thumbnailSize == 0u)
                 return false;

             glm::vec3 center{ 0.0f };
             float orthographicSize = 1.0f;
             if (!ResolvePrefabPreviewBounds(scene, center, orthographicSize))
                 return false;

             if (!surface.Resize(thumbnailSize, thumbnailSize))
                 return false;

             Life::CameraSpecification cameraSpec;
             cameraSpec.Name = "Prefab Preview";
             cameraSpec.Projection = Life::ProjectionType::Orthographic;
             cameraSpec.OrthoSize = orthographicSize;
             cameraSpec.OrthoNear = -1000.0f;
             cameraSpec.OrthoFar = 1000.0f;
             cameraSpec.AspectRatio = 1.0f;
             cameraSpec.ClearMode = Life::CameraClearMode::SolidColor;
             cameraSpec.ClearColor = { 0.035f, 0.040f, 0.052f, 1.0f };
             Life::Camera camera(cameraSpec);
             camera.SetPosition({ center.x, center.y, center.z + 10.0f });

             ResolvePrefabSpriteTextures(scene, services);
             return services.SceneRenderer2D->get().RenderToSurface(
                 surface,
                 scene,
                 camera,
                 Life::SceneRenderer2D::QuadSortMode::BackToFront);
         }

         bool DrawTextureAssetThumbnail(const ProjectAssetEntry& entry,
                                        const EditorServices& services,
                                        ImVec2 minimum,
                                        ImVec2 maximum)
         {
             if (!services.AssetManager || !services.ImGuiSystem)
                 return false;

             Life::Ref<Life::Assets::TextureAsset> textureAsset = services.AssetManager->get().GetOrLoad<Life::Assets::TextureAsset>(MakeAssetKey(entry.RelativePath));
             Life::TextureResource* texture = textureAsset ? textureAsset->TryGetTextureResource() : nullptr;
             return texture != nullptr && DrawTextureThumbnail(*texture, services.ImGuiSystem->get(), minimum, maximum);
         }

         bool DrawPrefabAssetThumbnail(const ProjectAssetEntry& entry,
                                       const EditorServices& services,
                                       std::unordered_map<std::string, Life::Scope<Life::SceneSurface>>& previewSurfaces,
                                       ImVec2 minimum,
                                       ImVec2 maximum)
         {
             if (!services.AssetManager || !services.Renderer || !services.SceneRenderer2D || !services.ImGuiSystem)
                 return false;

             const std::string assetKey = MakeAssetKey(entry.RelativePath);
             Life::Ref<Life::Assets::PrefabAsset> prefab = services.AssetManager->get().GetOrLoad<Life::Assets::PrefabAsset>(assetKey);
             if (!prefab || prefab->GetPrefabScene() == nullptr)
                 return false;

             Life::Scope<Life::SceneSurface>& surface = previewSurfaces[assetKey];
             if (!surface)
             {
                 surface = Life::CreateScope<Life::SceneSurface>(
                     services.Renderer->get(),
                     services.SceneRenderer2D->get().GetRenderer2D(),
                     services.ImGuiSystem->get());
             }

             const float boxSize = std::max(std::min(maximum.x - minimum.x, maximum.y - minimum.y), 1.0f);
             const uint32_t renderSize = std::clamp(static_cast<uint32_t>(std::lround(boxSize * ImGui::GetIO().DisplayFramebufferScale.x)), 32u, 192u);
             if (!RenderPrefabPreviewToSurface(*surface, *prefab->GetPrefabScene(), services, renderSize))
                 return false;

             Life::TextureResource* texture = surface->GetColorTarget();
             return texture != nullptr && DrawTextureThumbnail(*texture, services.ImGuiSystem->get(), minimum, maximum, false);
         }

         bool DrawProjectEntryThumbnail(const ProjectAssetEntry& entry,
                                        const EditorServices& services,
                                        std::unordered_map<std::string, Life::Scope<Life::SceneSurface>>& prefabPreviewSurfaces,
                                        ImVec2 minimum,
                                        ImVec2 maximum)
         {
             if (entry.Kind == ProjectEntryKind::Texture)
                 return DrawTextureAssetThumbnail(entry, services, minimum, maximum);
             if (entry.Kind == ProjectEntryKind::Prefab)
                 return DrawPrefabAssetThumbnail(entry, services, prefabPreviewSurfaces, minimum, maximum);
             return false;
         }
#endif

        bool LoadSceneIntoEditor(const std::filesystem::path& sceneIdentifier, const EditorServices& services, EditorSceneState& sceneState)
        {
            if (sceneState.IsPrefabMode())
            {
                sceneState.SetStatusMessage("Exit Prefab Mode before opening a scene.", true);
                return false;
            }
            if (!services.SceneService)
                return false;

            sceneState.ResetRuntimeState();
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

        void SyncSceneReferencesAfterAssetRebase(
            const std::filesystem::path& assetsDirectory,
            const std::filesystem::path& oldRelativePath,
            const std::filesystem::path& newRelativePath,
            bool updateActiveSceneName,
            const std::string& renamedSceneName,
            const EditorServices& services,
            EditorSceneState& sceneState)
        {
            if (services.SceneService && services.SceneService->get().HasActiveSceneSourcePath())
            {
                Life::SceneService& sceneService = services.SceneService->get();
                const std::filesystem::path oldAbsolutePath = (assetsDirectory / oldRelativePath).lexically_normal();
                const std::filesystem::path newAbsolutePath = (assetsDirectory / newRelativePath).lexically_normal();
                const std::filesystem::path activeSceneSourcePath = sceneService.GetActiveSceneSourcePath().lexically_normal();
                if (IsSameOrDescendant(activeSceneSourcePath, oldAbsolutePath))
                {
                    const bool wasSceneDirty = sceneService.IsActiveSceneDirty();
                    Life::Scene& activeScene = sceneService.GetActiveScene();
                    activeScene.SetSourcePath(RebasePath(activeSceneSourcePath, oldAbsolutePath, newAbsolutePath).lexically_normal());
                    if (updateActiveSceneName)
                    {
                        activeScene.SetName(renamedSceneName);
                        if (wasSceneDirty)
                        {
                            sceneService.MarkActiveSceneDirty();
                        }
                        else
                        {
                            const auto saveResult = sceneService.SaveActiveScene();
                            if (saveResult.IsFailure())
                            {
                                sceneService.MarkActiveSceneDirty();
                                sceneState.SetStatusMessage(
                                    "Renamed the active scene asset, but failed to save the updated scene name: " + saveResult.GetError().GetErrorMessage(),
                                    true);
                            }
                        }
                    }
                }
            }

            if (services.ProjectService && services.ProjectService->get().HasActiveProject())
            {
                Life::Assets::ProjectService& projectService = services.ProjectService->get();
                Life::Assets::Project& project = projectService.GetActiveProject();
                if (!project.Descriptor.Startup.Scene.empty())
                {
                    const std::filesystem::path oldStartupPath = std::filesystem::path(MakeAssetKey(oldRelativePath)).lexically_normal();
                    const std::filesystem::path newStartupPath = std::filesystem::path(MakeAssetKey(newRelativePath)).lexically_normal();
                    const std::filesystem::path configuredStartupPath = std::filesystem::path(project.Descriptor.Startup.Scene).lexically_normal();
                    if (IsSameOrDescendant(configuredStartupPath, oldStartupPath))
                    {
                        project.Descriptor.Startup.Scene = RebasePath(configuredStartupPath, oldStartupPath, newStartupPath).generic_string();
                        const auto saveResult = projectService.SaveProject();
                        if (saveResult.IsFailure())
                            sceneState.SetStatusMessage(saveResult.GetError().GetErrorMessage(), true);
                    }
                }
            }
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
            scene.EnsureAtLeastOneCamera();
            const auto saveResult = Life::SceneSerializer::Save(scene, targetPath);
            if (saveResult.IsFailure())
            {
                sceneState.SetStatusMessage(saveResult.GetError().GetErrorMessage(), true);
                return false;
            }

            sceneState.SetStatusMessage("Created scene '" + sanitizedName + "'.", false);
            return true;
        }

        std::filesystem::path MakeUniquePrefabPath(const std::filesystem::path& folderPath, const std::string& stem)
        {
            std::filesystem::path candidate = folderPath / (stem + ".prefab.json");
            if (!std::filesystem::exists(candidate))
                return candidate;

            for (std::size_t index = 2; index < 10000; ++index)
            {
                candidate = folderPath / (stem + "_" + std::to_string(index) + ".prefab.json");
                if (!std::filesystem::exists(candidate))
                    return candidate;
            }

            return folderPath / (stem + "_copy.prefab.json");
        }

        bool CreatePrefabFromEntityDrop(const std::filesystem::path& assetsDirectory,
                                        const std::filesystem::path& destinationRelativePath,
                                        const char* entityId,
                                        const EditorServices& services,
                                        EditorSceneState& sceneState,
                                        std::filesystem::path& outRelativePath)
        {
            outRelativePath.clear();
            if (entityId == nullptr || entityId[0] == '\0')
                return false;
            if (sceneState.ExecutionMode != EditorSceneExecutionMode::Edit)
            {
                sceneState.SetStatusMessage("Create prefabs from the edit scene, not Play or Simulation mode.", true);
                return false;
            }
            if (!services.SceneService || !services.SceneService->get().HasActiveScene())
            {
                sceneState.SetStatusMessage("Open a scene before creating a prefab.", true);
                return false;
            }

            Life::Scene* editableScene = sceneState.GetEditableScene(services.SceneService->get());
            if (editableScene == nullptr)
            {
                sceneState.SetStatusMessage("Create prefabs from an editable scene or prefab document.", true);
                return false;
            }

            Life::Scene& scene = *editableScene;
            Life::Entity entity = scene.FindEntityById(entityId);
            if (!entity.IsValid())
            {
                sceneState.SetStatusMessage("Dragged entity no longer exists.", true);
                return false;
            }

            const std::filesystem::path destinationFolder = destinationRelativePath.empty()
                ? assetsDirectory
                : (assetsDirectory / destinationRelativePath).lexically_normal();
            if (!IsPathInside(assetsDirectory, destinationFolder))
            {
                sceneState.SetStatusMessage("Prefab target must remain inside the project Assets directory.", true);
                return false;
            }

            std::error_code ec;
            if (!std::filesystem::exists(destinationFolder, ec))
                std::filesystem::create_directories(destinationFolder, ec);
            if (ec || !std::filesystem::is_directory(destinationFolder, ec))
            {
                sceneState.SetStatusMessage("Prefab target folder is not available.", true);
                return false;
            }

            const std::string stem = SanitizeName(entity.GetTag()).empty() ? std::string("Prefab") : SanitizeName(entity.GetTag());
            const std::filesystem::path prefabPath = MakeUniquePrefabPath(destinationFolder, stem);
            const auto saveResult = Life::Assets::PrefabSerializer::SaveEntityAsPrefab(scene, entity, prefabPath);
            if (saveResult.IsFailure())
            {
                sceneState.SetStatusMessage(saveResult.GetError().GetErrorMessage(), true);
                return false;
            }

            outRelativePath = std::filesystem::relative(prefabPath, assetsDirectory, ec).lexically_normal();
            if (ec)
                outRelativePath = (destinationRelativePath / prefabPath.filename()).lexically_normal();

            ImportPrefabAssetIfPossible(services, outRelativePath);
            sceneState.SelectProjectAsset(outRelativePath);
            sceneState.SetStatusMessage("Created prefab '" + outRelativePath.generic_string() + "'.", false);
            return true;
        }

        bool AcceptEntityPrefabDropToFolder(const std::filesystem::path& assetsDirectory,
                                            const std::filesystem::path& destinationRelativePath,
                                            const EditorServices& services,
                                            EditorSceneState& sceneState,
                                            std::filesystem::path& selectedRelativePath)
        {
#if __has_include(<imgui.h>)
            if (const ImGuiPayload* entityPayload = ImGui::AcceptDragDropPayload(kEntityPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery))
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();
                const ImU32 fillColor = ImGui::GetColorU32(ImVec4(0.20f, 0.62f, 0.66f, 0.16f));
                const ImU32 borderColor = ImGui::GetColorU32(ImVec4(0.36f, 0.84f, 0.86f, 0.92f));
                drawList->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), fillColor, 4.0f);
                drawList->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), borderColor, 4.0f, 0, 2.0f);

                if (!entityPayload->Delivery)
                    return true;

                const char* entityId = static_cast<const char*>(entityPayload->Data);
                std::filesystem::path createdRelativePath;
                if (CreatePrefabFromEntityDrop(assetsDirectory, destinationRelativePath, entityId, services, sceneState, createdRelativePath))
                {
                    selectedRelativePath = createdRelativePath;
                    return true;
                }
            }
#else
            (void)assetsDirectory;
            (void)destinationRelativePath;
            (void)services;
            (void)sceneState;
            (void)selectedRelativePath;
#endif
            return false;
        }

        bool RenameEntry(const std::filesystem::path& assetsDirectory, const ProjectAssetEntry& entry, const std::string& name, const EditorServices& services, EditorSceneState& sceneState)
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

            const std::filesystem::path sourceMetaPath = GetMetaPathForAsset(entry.AbsolutePath);
            const std::filesystem::path destinationMetaPath = GetMetaPathForAsset(destination);
            const std::filesystem::path renamedRelativePath = (entry.RelativePath.parent_path() / destination.filename()).lexically_normal();

            std::filesystem::rename(entry.AbsolutePath, destination, ec);
            if (ec)
            {
                sceneState.SetStatusMessage("Failed to rename '" + entry.DisplayName + "'.", true);
                return false;
            }

            if (!entry.IsDirectory && std::filesystem::exists(sourceMetaPath, ec))
            {
                ec.clear();
                std::filesystem::rename(sourceMetaPath, destinationMetaPath, ec);
                if (ec)
                {
                    std::error_code rollbackEc;
                    std::filesystem::rename(destination, entry.AbsolutePath, rollbackEc);
                    sceneState.SetStatusMessage("Failed to rename metadata for '" + entry.DisplayName + "'.", true);
                    return false;
                }
            }

            sceneState.SetStatusMessage("Renamed '" + entry.DisplayName + "'.", false);
            SyncSceneReferencesAfterAssetRebase(
                assetsDirectory,
                entry.RelativePath,
                renamedRelativePath,
                entry.Kind == ProjectEntryKind::Scene && !entry.IsDirectory,
                sanitizedName,
                services,
                sceneState);
            return true;
        }

        bool DeleteEntry(const std::filesystem::path& assetsDirectory, const ProjectAssetEntry& entry, EditorSceneState& sceneState)
        {
            if (!PathSafety::IsSafeAssetDeleteTarget(assetsDirectory, entry.AbsolutePath))
            {
                sceneState.SetStatusMessage("Delete target must be inside the project Assets directory and cannot be the Assets root.", true);
                return false;
            }

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

                ec.clear();
                const std::filesystem::path metaPath = GetMetaPathForAsset(entry.AbsolutePath);
                if (std::filesystem::exists(metaPath, ec))
                {
                    ec.clear();
                    if (!std::filesystem::remove(metaPath, ec) || ec)
                    {
                        sceneState.SetStatusMessage("Failed to delete metadata for '" + entry.DisplayName + "'.", true);
                        return false;
                    }
                }
            }

            (void)assetsDirectory;
            sceneState.SetStatusMessage("Deleted '" + entry.DisplayName + "'.", false);
            return true;
        }

        bool MoveEntry(const std::filesystem::path& assetsDirectory, const std::filesystem::path& sourceRelativePath, const std::filesystem::path& destinationFolderRelativePath, const EditorServices& services, EditorSceneState& sceneState)
        {
            if (sourceRelativePath.empty())
                return false;

            const std::filesystem::path sourcePath = assetsDirectory / sourceRelativePath;
            const std::filesystem::path destinationFolder = destinationFolderRelativePath.empty() ? assetsDirectory : (assetsDirectory / destinationFolderRelativePath);
            const std::filesystem::path destinationPath = destinationFolder / sourcePath.filename();
            const std::filesystem::path movedRelativePath = (destinationFolderRelativePath / sourcePath.filename()).lexically_normal();

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

            const std::filesystem::path sourceMetaPath = GetMetaPathForAsset(sourcePath);
            const std::filesystem::path destinationMetaPath = GetMetaPathForAsset(destinationPath);

            std::filesystem::rename(sourcePath, destinationPath, ec);
            if (ec)
            {
                sceneState.SetStatusMessage("Failed to move '" + sourcePath.filename().string() + "'.", true);
                return false;
            }

            ec.clear();
            if (!std::filesystem::is_directory(destinationPath, ec) && std::filesystem::exists(sourceMetaPath, ec))
            {
                ec.clear();
                std::filesystem::rename(sourceMetaPath, destinationMetaPath, ec);
                if (ec)
                {
                    std::error_code rollbackEc;
                    std::filesystem::rename(destinationPath, sourcePath, rollbackEc);
                    sceneState.SetStatusMessage("Failed to move metadata for '" + sourcePath.filename().string() + "'.", true);
                    return false;
                }
            }

            sceneState.SetStatusMessage("Moved '" + sourcePath.filename().string() + "'.", false);
            SyncSceneReferencesAfterAssetRebase(
                assetsDirectory,
                sourceRelativePath,
                movedRelativePath,
                false,
                {},
                services,
                sceneState);
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
            if (entry.Kind == ProjectEntryKind::Scene)
                payload.Kind = ProjectAssetPayloadKind::Scene;
            else if (entry.Kind == ProjectEntryKind::Prefab)
                payload.Kind = ProjectAssetPayloadKind::Prefab;
            else
                payload.Kind = entry.IsDirectory ? ProjectAssetPayloadKind::Directory : ProjectAssetPayloadKind::File;
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
        const std::string filterLower = ToLowerAscii(m_SearchFilter);
        const auto currentEntries = CollectEntries(assetsDirectory, m_ActiveFolderRelativePath);
        const auto searchEntries = CollectSearchEntries(assetsDirectory, filterLower);
        const bool useGlobalSearchResults = !filterLower.empty();
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
            ImGui::TextDisabled("%zu items", useGlobalSearchResults ? searchEntries.size() : currentEntries.size());
            ImGui::TextDisabled("%s", useGlobalSearchResults ? "Project-wide search" : activeFolderLabel.c_str());
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
                const std::string sanitizedPopupName = SanitizeName(m_PopupName);
                const std::filesystem::path renamedRelativePath = (entry.RelativePath.parent_path() / (sanitizedPopupName + ResolveSuffixForRename(entry))).lexically_normal();
                if (RenameEntry(assetsDirectory, entry, m_PopupName, services, sceneState))
                {
                    m_SelectedRelativePath = RebasePath(m_SelectedRelativePath, entry.RelativePath, renamedRelativePath);
                    m_ActiveFolderRelativePath = RebasePath(m_ActiveFolderRelativePath, entry.RelativePath, renamedRelativePath);
                    if (!m_SelectedRelativePath.empty())
                        sceneState.SelectProjectAsset(m_SelectedRelativePath);
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
                        ? std::string("    Assets##ProjectAssetsRoot")
                        : ("    " + relativePath.filename().string() + "##" + key);
                    bool openState = isRoot ? true : (m_ExpandedFolders.contains(key) ? m_ExpandedFolders[key] : false);
                    ImGui::SetNextItemOpen(openState, ImGuiCond_Once);
                    const bool nodeOpen = ImGui::TreeNodeEx(
                        label.c_str(),
                        ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_OpenOnArrow |
                            ((relativePath == m_ActiveFolderRelativePath) ? ImGuiTreeNodeFlags_Selected : ImGuiTreeNodeFlags_None));
                    {
                        const ImVec2 itemMin = ImGui::GetItemRectMin();
                        const float iconSize = ImGui::GetTextLineHeight() * 0.92f;
                        DrawProjectEntryIcon(ProjectEntryKind::Directory, ImVec2(itemMin.x + ImGui::GetTreeNodeToLabelSpacing(), itemMin.y + 1.0f), iconSize, ResolveAccentColor(ProjectEntryKind::Directory));
                    }
                    if (!isRoot)
                        m_ExpandedFolders[key] = nodeOpen;

                    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
                    {
                        m_ActiveFolderRelativePath = relativePath;
                        m_SelectedRelativePath = relativePath;
                        sceneState.SelectProjectAsset(relativePath);
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
                                    {
                                        m_SelectedRelativePath.clear();
                                        sceneState.ClearSelection();
                                    }
                                }
                            }
                        }
                        ImGui::EndPopup();
                    }

                    if (ImGui::BeginDragDropSource())
                    {
                        const ProjectAssetEntry entry = FindEntryByRelativePath(assetsDirectory, relativePath);
                        SetPayload(entry);
                        const std::string visibleName = ResolveVisibleName(entry);
                        ImGui::TextUnformatted(visibleName.c_str());
                        ImGui::EndDragDropSource();
                    }

                    if (ImGui::BeginDragDropTarget())
                    {
                        if (AcceptEntityPrefabDropToFolder(assetsDirectory, relativePath, services, sceneState, m_SelectedRelativePath))
                        {
                        }
                        else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                        {
                            const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                            if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                            {
                                const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                                if (MoveEntry(assetsDirectory, sourceRelativePath, relativePath, services, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                                {
                                    m_SelectedRelativePath = relativePath / sourceRelativePath.filename();
                                    sceneState.SelectProjectAsset(m_SelectedRelativePath);
                                }
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
                const ImVec2 treeDropAvail = ImGui::GetContentRegionAvail();
                if (treeDropAvail.x > 1.0f && treeDropAvail.y > ImGui::GetFrameHeight())
                {
                    ImGui::InvisibleButton("##ProjectAssetsTreeRootDropArea", ImVec2(treeDropAvail.x, treeDropAvail.y));
                    if (ImGui::BeginDragDropTarget())
                    {
                        (void)AcceptEntityPrefabDropToFolder(assetsDirectory, {}, services, sceneState, m_SelectedRelativePath);
                        ImGui::EndDragDropTarget();
                    }
                }
            }
            ImGui::EndChild();

            ImGui::SameLine();

            if (ImGui::BeginChild("##ProjectAssetsGrid", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
            {
                const std::string gridLabel = useGlobalSearchResults ? std::string("Search Results") : activeFolderLabel;
                ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "%s", gridLabel.c_str());
                ImGui::SameLine();
                ImGui::TextDisabled(useGlobalSearchResults ? (useCompactList ? "Search List" : "Search Grid") : (useCompactList ? "List" : "Grid"));
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

                const ImVec2 currentFolderDropSize(std::max(ImGui::GetContentRegionAvail().x, 1.0f), ImGui::GetFrameHeight() * 0.7f);
                ImGui::InvisibleButton("##ProjectAssetsCurrentFolderDropArea", currentFolderDropSize);
                if (ImGui::BeginDragDropTarget())
                {
                    if (AcceptEntityPrefabDropToFolder(assetsDirectory, m_ActiveFolderRelativePath, services, sceneState, m_SelectedRelativePath))
                    {
                    }
                    else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                    {
                        const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                        if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                        {
                            const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                            if (MoveEntry(assetsDirectory, sourceRelativePath, m_ActiveFolderRelativePath, services, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                            {
                                m_SelectedRelativePath = m_ActiveFolderRelativePath / sourceRelativePath.filename();
                                sceneState.SelectProjectAsset(m_SelectedRelativePath);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
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

                ImGui::Separator();

                std::vector<ProjectAssetEntry> visibleEntries;
                if (useGlobalSearchResults)
                {
                    visibleEntries = searchEntries;
                }
                else
                {
                    visibleEntries.reserve(currentEntries.size());
                    for (const ProjectAssetEntry& entry : currentEntries)
                    {
                        if (MatchesFilter(entry, filterLower))
                            visibleEntries.push_back(entry);
                    }
                }

                if (visibleEntries.empty())
                {
                    ImGui::Dummy(ImVec2(0.0f, 12.0f));
                    ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), filterLower.empty() ? "No assets in this folder." : "No assets match the current search.");
                    ImGui::TextDisabled(filterLower.empty() ? "Create a folder, create a scene, or import files from Explorer." : "Search scans the full Assets tree. Try a different term or clear the filter.");
                }
                else if (useCompactList)
                {
                    for (const ProjectAssetEntry& entry : visibleEntries)
                    {
                        ImGui::PushID(entry.RelativePath.generic_string().c_str());
                        const bool selected = m_SelectedRelativePath == entry.RelativePath;
                        const ImVec4 accentColor = ResolveAccentColor(entry.Kind);
                        const bool isPrefab = entry.Kind == ProjectEntryKind::Prefab;
                        ImGui::PushStyleColor(ImGuiCol_Header, selected
                            ? ImVec4(accentColor.x * 0.46f, accentColor.y * 0.46f, accentColor.z * 0.46f, 0.92f)
                            : (isPrefab ? ImVec4(0.08f, 0.24f, 0.27f, 0.78f) : ImVec4(0.12f, 0.15f, 0.20f, 0.68f)));
                        ImGui::PushStyleColor(ImGuiCol_HeaderHovered, isPrefab
                            ? ImVec4(0.12f, 0.34f, 0.38f, 0.92f)
                            : ImVec4(accentColor.x * 0.32f, accentColor.y * 0.32f, accentColor.z * 0.32f, 0.90f));
                        ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(accentColor.x * 0.46f, accentColor.y * 0.46f, accentColor.z * 0.46f, 0.94f));
                        const std::string visibleName = ResolveVisibleName(entry);
                        const float rowHeight = std::max(ImGui::GetFrameHeight(), 24.0f);
                        if (ImGui::Selectable("##ProjectAssetRow", selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0.0f, rowHeight)))
                        {
                            m_SelectedRelativePath = entry.RelativePath;
                            sceneState.SelectProjectAsset(entry.RelativePath);
                            if (entry.IsDirectory && ImGui::IsMouseDoubleClicked(0))
                            {
                                m_ActiveFolderRelativePath = entry.RelativePath;
                            }
                            else if (entry.Kind == ProjectEntryKind::Scene && ImGui::IsMouseDoubleClicked(0))
                            {
                                (void)LoadSceneIntoEditor(MakeAssetKey(entry.RelativePath), services, sceneState);
                            }
                            else if (entry.Kind == ProjectEntryKind::Prefab && ImGui::IsMouseDoubleClicked(0))
                            {
                                sceneState.RequestedOpenPrefabAssetKey = MakeAssetKey(entry.RelativePath);
                            }
                        }
                        const ImVec2 rowMin = ImGui::GetItemRectMin();
                        const float iconSize = std::min(rowHeight - 4.0f, 20.0f);
                        const ImVec2 iconMin(rowMin.x + 4.0f, rowMin.y + (rowHeight - iconSize) * 0.5f);
                        const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
                        if (!DrawProjectEntryThumbnail(entry, services, m_PrefabPreviewSurfaces, iconMin, iconMax))
                            DrawProjectEntryIcon(entry.Kind, iconMin, iconSize, accentColor);
                        ImGui::GetWindowDrawList()->AddText(
                            ImVec2(rowMin.x + iconSize + 12.0f, rowMin.y + (rowHeight - ImGui::GetTextLineHeight()) * 0.5f),
                            ImGui::GetColorU32(ImGuiCol_Text),
                            visibleName.c_str());
                        ImGui::PopStyleColor(3);

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (entry.Kind == ProjectEntryKind::Prefab)
                            {
                                if (ImGui::MenuItem("Open Prefab"))
                                    sceneState.RequestedOpenPrefabAssetKey = MakeAssetKey(entry.RelativePath);
                                ImGui::Separator();
                            }

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
                                    {
                                        m_SelectedRelativePath.clear();
                                        sceneState.ClearSelection();
                                    }
                                }
                            }
                            ImGui::EndPopup();
                        }

                        if (ImGui::BeginDragDropSource())
                        {
                            SetPayload(entry);
                            ImGui::TextUnformatted(visibleName.c_str());
                            ImGui::EndDragDropSource();
                        }

                        if (entry.IsDirectory && ImGui::BeginDragDropTarget())
                        {
                            if (AcceptEntityPrefabDropToFolder(assetsDirectory, entry.RelativePath, services, sceneState, m_SelectedRelativePath))
                            {
                            }
                            else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                            {
                                const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                                if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                                {
                                    const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                                    if (MoveEntry(assetsDirectory, sourceRelativePath, entry.RelativePath, services, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                                    {
                                        m_SelectedRelativePath = entry.RelativePath / sourceRelativePath.filename();
                                        sceneState.SelectProjectAsset(m_SelectedRelativePath);
                                    }
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
                        const bool isPrefab = entry.Kind == ProjectEntryKind::Prefab;
                        ImGui::PushStyleColor(ImGuiCol_Button, cardColor);
                        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, cardHovered);
                        ImGui::PushStyleColor(ImGuiCol_ButtonActive, cardActive);
                        if (isPrefab)
                        {
                            ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.46f, 0.92f, 0.94f, 0.92f));
                            ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
                        }
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 10.0f);
                        const ImVec2 buttonSize(cellSize - 12.0f, cellSize - 28.0f);
                        if (ImGui::Button("##Button", buttonSize))
                        {
                            m_SelectedRelativePath = entry.RelativePath;
                            sceneState.SelectProjectAsset(entry.RelativePath);
                            if (entry.IsDirectory)
                                m_ActiveFolderRelativePath = entry.RelativePath;
                        }
                        {
                            const ImVec2 buttonMin = ImGui::GetItemRectMin();
                            const float iconSize = std::clamp(buttonSize.y * 0.48f, 22.0f, 48.0f);
                            const ImVec2 iconMin(buttonMin.x + (buttonSize.x - iconSize) * 0.5f, buttonMin.y + (buttonSize.y - iconSize) * 0.5f);
                            const ImVec2 iconMax(iconMin.x + iconSize, iconMin.y + iconSize);
                            if (!DrawProjectEntryThumbnail(entry, services, m_PrefabPreviewSurfaces, iconMin, iconMax))
                                DrawProjectEntryIcon(entry.Kind, iconMin, iconSize, accentColor);
                        }
                        ImGui::PopStyleVar();
                        if (isPrefab)
                        {
                            ImGui::PopStyleVar();
                            ImGui::PopStyleColor();
                        }
                        ImGui::PopStyleColor(3);

                        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
                        {
                            if (entry.IsDirectory)
                                m_ActiveFolderRelativePath = entry.RelativePath;
                            else if (entry.Kind == ProjectEntryKind::Scene)
                                (void)LoadSceneIntoEditor(MakeAssetKey(entry.RelativePath), services, sceneState);
                            else if (entry.Kind == ProjectEntryKind::Prefab)
                                sceneState.RequestedOpenPrefabAssetKey = MakeAssetKey(entry.RelativePath);
                        }

                        if (ImGui::BeginPopupContextItem())
                        {
                            if (entry.Kind == ProjectEntryKind::Prefab)
                            {
                                if (ImGui::MenuItem("Open Prefab"))
                                    sceneState.RequestedOpenPrefabAssetKey = MakeAssetKey(entry.RelativePath);
                                ImGui::Separator();
                            }

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
                                    {
                                        m_SelectedRelativePath.clear();
                                        sceneState.ClearSelection();
                                    }
                                }
                            }
                            ImGui::EndPopup();
                        }

                        if (ImGui::BeginDragDropSource())
                        {
                            SetPayload(entry);
                            const std::string visibleName = ResolveVisibleName(entry);
                            ImGui::TextUnformatted(visibleName.c_str());
                            ImGui::EndDragDropSource();
                        }

                        if (entry.IsDirectory && ImGui::BeginDragDropTarget())
                        {
                            if (AcceptEntityPrefabDropToFolder(assetsDirectory, entry.RelativePath, services, sceneState, m_SelectedRelativePath))
                            {
                            }
                            else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                            {
                                const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                                if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                                {
                                    const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                                    if (MoveEntry(assetsDirectory, sourceRelativePath, entry.RelativePath, services, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                                    {
                                        m_SelectedRelativePath = entry.RelativePath / sourceRelativePath.filename();
                                        sceneState.SelectProjectAsset(m_SelectedRelativePath);
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }

                        const std::string visibleName = ResolveVisibleName(entry);
                        ImGui::TextWrapped("%s", visibleName.c_str());
                        ImGui::EndGroup();
                        ImGui::NextColumn();
                        ImGui::PopID();
                    }

                    ImGui::Columns(1);
                }

                const ImVec2 emptyDropAvail = ImGui::GetContentRegionAvail();
                if (emptyDropAvail.x > 1.0f && emptyDropAvail.y > ImGui::GetFrameHeight())
                {
                    ImGui::InvisibleButton("##ProjectAssetsEmptyFolderDropArea", ImVec2(emptyDropAvail.x, emptyDropAvail.y));
                    if (ImGui::BeginDragDropTarget())
                    {
                        if (AcceptEntityPrefabDropToFolder(assetsDirectory, m_ActiveFolderRelativePath, services, sceneState, m_SelectedRelativePath))
                        {
                        }
                        else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                        {
                            const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                            if (assetPayload != nullptr && assetPayload->RelativePath[0] != '\0')
                            {
                                const std::filesystem::path sourceRelativePath(assetPayload->RelativePath.data());
                                if (MoveEntry(assetsDirectory, sourceRelativePath, m_ActiveFolderRelativePath, services, sceneState) && m_SelectedRelativePath == sourceRelativePath)
                                {
                                    m_SelectedRelativePath = m_ActiveFolderRelativePath / sourceRelativePath.filename();
                                    sceneState.SelectProjectAsset(m_SelectedRelativePath);
                                }
                            }
                        }
                        ImGui::EndDragDropTarget();
                    }
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
