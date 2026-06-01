#include "Editor/Viewport/SceneViewportPanel.h"

#include "Editor/Camera/EditorCameraTool.h"
#include "Editor/Panels/ProjectAssetDragDrop.h"

#include <SDL3/SDL.h>

#if __has_include(<imgui.h>)
#include <imgui.h>
#endif
#if __has_include(<ImGuizmo.h>)
#include <ImGuizmo.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <string_view>
#include <utility>

#include <glm/gtc/type_ptr.hpp>

namespace
{
    float SnapToPixelGrid(float value, float pixelScale)
    {
        const float safePixelScale = std::max(pixelScale, 1.0f);
        return std::round(value * safePixelScale) / safePixelScale;
    }

    ImVec2 ResolveFramebufferScale(const EditorApp::EditorServices& services, void* nativeWindowHandle)
    {
#if __has_include(<imgui.h>)
        const ImVec2 reportedScale = ImGui::GetIO().DisplayFramebufferScale;
        float scaleX = std::max(reportedScale.x, 1.0f);
        float scaleY = std::max(reportedScale.y, 1.0f);

        if (auto* sdlWindow = static_cast<SDL_Window*>(nativeWindowHandle))
        {
            int windowWidth = 0;
            int windowHeight = 0;
            int pixelWidth = 0;
            int pixelHeight = 0;
            if (SDL_GetWindowSize(sdlWindow, &windowWidth, &windowHeight) &&
                SDL_GetWindowSizeInPixels(sdlWindow, &pixelWidth, &pixelHeight) &&
                windowWidth > 0 &&
                windowHeight > 0 &&
                pixelWidth > 0 &&
                pixelHeight > 0)
            {
                scaleX = static_cast<float>(pixelWidth) / static_cast<float>(windowWidth);
                scaleY = static_cast<float>(pixelHeight) / static_cast<float>(windowHeight);
            }
            else
            {
                const float pixelDensity = SDL_GetWindowPixelDensity(sdlWindow);
                if (std::isfinite(pixelDensity) && pixelDensity > 0.0f)
                {
                    scaleX = pixelDensity;
                    scaleY = pixelDensity;
                }
            }
        }

        if (services.GraphicsDevice)
        {
            if (const ImGuiViewport* viewport = ImGui::GetWindowViewport())
            {
                const float viewportWidth = viewport->Size.x;
                const float viewportHeight = viewport->Size.y;
                if (viewportWidth > 0.0f && viewportHeight > 0.0f)
                {
                    const Life::GraphicsDevice& graphicsDevice = services.GraphicsDevice->get();
                    const float derivedScaleX = static_cast<float>(graphicsDevice.GetBackBufferWidth()) / viewportWidth;
                    const float derivedScaleY = static_cast<float>(graphicsDevice.GetBackBufferHeight()) / viewportHeight;
                    if (std::isfinite(derivedScaleX) && derivedScaleX >= scaleX)
                        scaleX = derivedScaleX;
                    if (std::isfinite(derivedScaleY) && derivedScaleY >= scaleY)
                        scaleY = derivedScaleY;
                }
            }
        }

        return { scaleX, scaleY };
#else
        (void)services;
        return { 1.0f, 1.0f };
#endif
    }

    glm::vec2 GetCameraLookDelta(void* nativeWindowHandle)
    {
        if (auto* sdlWindow = static_cast<SDL_Window*>(nativeWindowHandle))
        {
            if (SDL_GetWindowRelativeMouseMode(sdlWindow))
            {
                float deltaX = 0.0f;
                float deltaY = 0.0f;
                SDL_GetRelativeMouseState(&deltaX, &deltaY);
                return { deltaX, deltaY };
            }
        }

#if __has_include(<imgui.h>)
        const ImVec2 mouseDelta = ImGui::GetIO().MouseDelta;
        return { mouseDelta.x, mouseDelta.y };
#else
        return {};
#endif
    }

    void SetCameraNavigationMouseRect(void* nativeWindowHandle, const ImVec2& viewportOrigin, const ImVec2& viewportSize)
    {
        if (auto* sdlWindow = static_cast<SDL_Window*>(nativeWindowHandle))
        {
            SDL_Rect rect{};
            rect.x = std::max(0, static_cast<int>(std::floor(viewportOrigin.x)));
            rect.y = std::max(0, static_cast<int>(std::floor(viewportOrigin.y)));
            rect.w = std::max(1, static_cast<int>(std::ceil(viewportSize.x)));
            rect.h = std::max(1, static_cast<int>(std::ceil(viewportSize.y)));
            SDL_SetWindowMouseRect(sdlWindow, &rect);
        }
    }

#if __has_include(<imgui.h>)
    const char* ResolveViewportToolLabel(EditorApp::EditorViewportTool tool) noexcept
    {
        switch (tool)
        {
            case EditorApp::EditorViewportTool::Select: return "Select";
            case EditorApp::EditorViewportTool::Rotate: return "Rotate";
            case EditorApp::EditorViewportTool::Scale: return "Scale";
            case EditorApp::EditorViewportTool::Translate:
            default: return "Move";
        }
    }

    void DrawToolButton(const char* label, EditorApp::EditorViewportTool value, EditorApp::EditorSceneState& sceneState)
    {
        const bool selected = sceneState.ViewportTool == value;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.42f, 0.70f, 1.0f));
        if (ImGui::Button(label, ImVec2(68.0f, 0.0f)))
            sceneState.ViewportTool = value;
        if (selected)
            ImGui::PopStyleColor();
    }

    void DrawViewModeButton(const char* label, EditorApp::EditorSceneViewMode value, EditorApp::EditorSceneState& sceneState)
    {
        const bool selected = sceneState.SceneViewMode == value;
        if (selected)
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.42f, 0.70f, 1.0f));
        if (ImGui::Button(label, ImVec2(44.0f, 0.0f)))
            sceneState.SceneViewMode = value;
        if (selected)
            ImGui::PopStyleColor();
    }

    const char* ResolveGridModeLabel(EditorApp::EditorViewportGridMode mode) noexcept
    {
        switch (mode)
        {
            case EditorApp::EditorViewportGridMode::WorldPlane: return "World";
            case EditorApp::EditorViewportGridMode::Screen: return "Screen";
            case EditorApp::EditorViewportGridMode::Auto:
            default: return "Auto";
        }
    }

    void DrawViewportToolbar(EditorApp::EditorSceneState& sceneState)
    {
        DrawViewModeButton("2D", EditorApp::EditorSceneViewMode::TwoD, sceneState);
        ImGui::SameLine();
        DrawViewModeButton("3D", EditorApp::EditorSceneViewMode::ThreeD, sceneState);
        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();
        DrawToolButton("Select", EditorApp::EditorViewportTool::Select, sceneState);
        ImGui::SameLine();
        DrawToolButton("Move", EditorApp::EditorViewportTool::Translate, sceneState);
        ImGui::SameLine();
        DrawToolButton("Rotate", EditorApp::EditorViewportTool::Rotate, sceneState);
        ImGui::SameLine();
        DrawToolButton("Scale", EditorApp::EditorViewportTool::Scale, sceneState);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(82.0f);
        if (ImGui::BeginCombo("##TransformSpace", sceneState.TransformSpace == EditorApp::EditorTransformSpace::Local ? "Local" : "World"))
        {
            if (ImGui::Selectable("Local", sceneState.TransformSpace == EditorApp::EditorTransformSpace::Local))
                sceneState.TransformSpace = EditorApp::EditorTransformSpace::Local;
            if (ImGui::Selectable("World", sceneState.TransformSpace == EditorApp::EditorTransformSpace::World))
                sceneState.TransformSpace = EditorApp::EditorTransformSpace::World;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::Checkbox("Snap", &sceneState.SnapEnabled);
        if (sceneState.SnapEnabled)
        {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(76.0f);
            ImGui::DragFloat("##TranslationSnap", &sceneState.TranslationSnap, 0.01f, 0.001f, 100.0f, "T %.2f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(76.0f);
            ImGui::DragFloat("##RotationSnap", &sceneState.RotationSnapDegrees, 0.25f, 0.1f, 180.0f, "R %.1f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(76.0f);
            ImGui::DragFloat("##ScaleSnap", &sceneState.ScaleSnap, 0.01f, 0.001f, 10.0f, "S %.2f");
        }
        ImGui::SameLine();
        ImGui::Checkbox("Grid", &sceneState.ShowGrid);
        ImGui::SameLine();
        if (ImGui::Checkbox("Grid Snap", &sceneState.SnapToGrid) && sceneState.SnapToGrid)
            sceneState.SnapEnabled = true;
        ImGui::SameLine();
        ImGui::SetNextItemWidth(76.0f);
        ImGui::DragFloat("##GridSize", &sceneState.GridSize, 0.05f, 0.05f, 100.0f, "G %.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(82.0f);
        if (ImGui::BeginCombo("##GridMode", ResolveGridModeLabel(sceneState.GridMode)))
        {
            if (ImGui::Selectable("Auto", sceneState.GridMode == EditorApp::EditorViewportGridMode::Auto))
                sceneState.GridMode = EditorApp::EditorViewportGridMode::Auto;
            if (ImGui::Selectable("World", sceneState.GridMode == EditorApp::EditorViewportGridMode::WorldPlane))
                sceneState.GridMode = EditorApp::EditorViewportGridMode::WorldPlane;
            if (ImGui::Selectable("Screen", sceneState.GridMode == EditorApp::EditorViewportGridMode::Screen))
                sceneState.GridMode = EditorApp::EditorViewportGridMode::Screen;
            ImGui::EndCombo();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", ResolveViewportToolLabel(sceneState.ViewportTool));
    }

    std::string ToLowerAscii(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
        {
            return static_cast<char>(std::tolower(character));
        });
        return value;
    }

    bool IsTextureAssetPath(const std::filesystem::path& relativePath)
    {
        const std::string extension = ToLowerAscii(relativePath.extension().string());
        return extension == ".png" ||
            extension == ".jpg" ||
            extension == ".jpeg" ||
            extension == ".bmp" ||
            extension == ".tga" ||
            extension == ".hdr" ||
            extension == ".psd" ||
            extension == ".gif" ||
            extension == ".ppm" ||
            extension == ".pnm";
    }

    std::string MakeAssetKey(const std::filesystem::path& relativePath)
    {
        return relativePath.empty() ? std::string{} : "Assets/" + relativePath.generic_string();
    }

    struct ViewportProjection
    {
        glm::mat4 ViewProjection{ 1.0f };
        glm::mat4 InverseViewProjection{ 1.0f };
        ImVec2 TopLeft{ 0.0f, 0.0f };
        ImVec2 Size{ 0.0f, 0.0f };
        bool Valid = false;
    };

    ViewportProjection BuildViewportProjection(const Life::Camera& camera, ImVec2 topLeft, ImVec2 size)
    {
        ViewportProjection projection;
        projection.ViewProjection = camera.GetViewProjectionMatrix();
        projection.InverseViewProjection = glm::inverse(projection.ViewProjection);
        projection.TopLeft = topLeft;
        projection.Size = size;
        projection.Valid = size.x > 0.0f && size.y > 0.0f;
        return projection;
    }

    bool IsClipPointInside(const glm::vec4& clip) noexcept
    {
        constexpr float epsilon = 0.00001f;
        if (!std::isfinite(clip.x) || !std::isfinite(clip.y) || !std::isfinite(clip.z) || !std::isfinite(clip.w))
            return false;

        return clip.w > epsilon &&
            clip.x >= -clip.w && clip.x <= clip.w &&
            clip.y >= -clip.w && clip.y <= clip.w &&
            clip.z >= 0.0f && clip.z <= clip.w;
    }

    bool ProjectClipToScreen(const ViewportProjection& projection, const glm::vec4& clip, ImVec2& screenPosition)
    {
        constexpr float epsilon = 0.00001f;
        if (!projection.Valid || clip.w <= epsilon)
            return false;

        const glm::vec3 ndc = glm::vec3(clip) / clip.w;
        screenPosition.x = projection.TopLeft.x + ((ndc.x + 1.0f) * 0.5f * projection.Size.x);
        screenPosition.y = projection.TopLeft.y + ((1.0f - ndc.y) * 0.5f * projection.Size.y);
        return std::isfinite(screenPosition.x) && std::isfinite(screenPosition.y);
    }

    bool TryProjectWorldPointToViewport(const ViewportProjection& projection, const glm::vec3& worldPosition, ImVec2& screenPosition)
    {
        const glm::vec4 clip = projection.ViewProjection * glm::vec4(worldPosition, 1.0f);
        return IsClipPointInside(clip) && ProjectClipToScreen(projection, clip, screenPosition);
    }

    bool ClipSegmentPlane(float planeAtStart, float planeAtEnd, float& tStart, float& tEnd)
    {
        if (planeAtStart >= 0.0f && planeAtEnd >= 0.0f)
            return true;
        if (planeAtStart < 0.0f && planeAtEnd < 0.0f)
            return false;

        const float t = planeAtStart / (planeAtStart - planeAtEnd);
        if (planeAtStart < 0.0f)
            tStart = std::max(tStart, t);
        else
            tEnd = std::min(tEnd, t);

        return tStart <= tEnd;
    }

    bool ClipClipSpaceSegment(glm::vec4& start, glm::vec4& end)
    {
        float tStart = 0.0f;
        float tEnd = 1.0f;
        const glm::vec4 delta = end - start;

        auto clipPlane = [&](auto plane)
        {
            return ClipSegmentPlane(plane(start), plane(end), tStart, tEnd);
        };

        const bool visible =
            clipPlane([](const glm::vec4& value) { return value.x + value.w; }) &&
            clipPlane([](const glm::vec4& value) { return value.w - value.x; }) &&
            clipPlane([](const glm::vec4& value) { return value.y + value.w; }) &&
            clipPlane([](const glm::vec4& value) { return value.w - value.y; }) &&
            clipPlane([](const glm::vec4& value) { return value.z; }) &&
            clipPlane([](const glm::vec4& value) { return value.w - value.z; });
        if (!visible)
            return false;

        end = start + delta * tEnd;
        start = start + delta * tStart;
        return true;
    }

    bool ClipWorldSegmentToViewport(const ViewportProjection& projection, const glm::vec3& start, const glm::vec3& end, ImVec2& screenStart, ImVec2& screenEnd)
    {
        if (!projection.Valid)
            return false;

        glm::vec4 clipStart = projection.ViewProjection * glm::vec4(start, 1.0f);
        glm::vec4 clipEnd = projection.ViewProjection * glm::vec4(end, 1.0f);
        if (!ClipClipSpaceSegment(clipStart, clipEnd))
            return false;

        return ProjectClipToScreen(projection, clipStart, screenStart) &&
            ProjectClipToScreen(projection, clipEnd, screenEnd);
    }

    void DrawClippedWorldLine(ImDrawList& drawList,
                              const ViewportProjection& projection,
                              const glm::vec3& start,
                              const glm::vec3& end,
                              ImU32 color,
                              float thickness)
    {
        ImVec2 screenStart;
        ImVec2 screenEnd;
        if (ClipWorldSegmentToViewport(projection, start, end, screenStart, screenEnd))
            drawList.AddLine(screenStart, screenEnd, color, thickness);
    }

    bool ContainsPoint(const ImVec2& minimum, const ImVec2& maximum, const ImVec2& point) noexcept
    {
        return point.x >= minimum.x && point.x <= maximum.x && point.y >= minimum.y && point.y <= maximum.y;
    }

    Life::SceneRenderer2D::RenderOptions ResolveSceneRenderOptions(const EditorApp::EditorServices& services)
    {
        Life::SceneRenderer2D::RenderOptions options;
        if (services.ProjectService && services.ProjectService->get().HasActiveProject())
        {
            options.EnableDepthTesting =
                services.ProjectService->get().GetActiveProject().Descriptor.Dimension == Life::Assets::ProjectDimension::ThreeD;
        }
        return options;
    }

    bool TransformChanged(const Life::TransformComponent& left, const Life::TransformComponent& right) noexcept
    {
        constexpr float epsilon = 0.0001f;
        return glm::length(left.LocalPosition - right.LocalPosition) > epsilon ||
            glm::length(left.LocalRotation - right.LocalRotation) > epsilon ||
            glm::length(left.LocalScale - right.LocalScale) > epsilon;
    }

    Life::Entity PickSpriteEntity(const Life::Scene& scene,
                                  const Life::Camera& camera,
                                  const ViewportProjection& projection,
                                  const ImVec2& mousePosition)
    {
        struct PickRank
        {
            std::size_t SortingLayerIndex = 0;
            int32_t SortingOrder = 0;
            float CameraDepth = -std::numeric_limits<float>::infinity();
            std::size_t SubmissionIndex = 0;
        };

        const auto resolveSortingLayerPriority = [&scene](std::string_view sortingLayer) noexcept
        {
            const std::size_t resolvedIndex = scene.ResolveSpriteSortingLayerIndex(sortingLayer);
            if (resolvedIndex != 0u || sortingLayer == "Default")
                return resolvedIndex;

            return scene.GetSpriteSortingLayers().size();
        };

        const auto isBetterPick = [](const PickRank& candidate, const PickRank& current) noexcept
        {
            if (candidate.SortingLayerIndex != current.SortingLayerIndex)
                return candidate.SortingLayerIndex > current.SortingLayerIndex;
            if (candidate.SortingOrder != current.SortingOrder)
                return candidate.SortingOrder > current.SortingOrder;
            if (candidate.CameraDepth != current.CameraDepth)
                return candidate.CameraDepth > current.CameraDepth;
            return candidate.SubmissionIndex > current.SubmissionIndex;
        };

        Life::Entity bestEntity;
        PickRank bestRank;
        std::size_t submissionIndex = 0;

        for (const Life::Entity entity : scene.GetEntities())
        {
            const std::size_t currentSubmissionIndex = submissionIndex++;
            if (!entity.IsEnabled())
                continue;

            const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>();
            if (sprite == nullptr)
                continue;

            const glm::mat4 worldTransform = scene.GetWorldTransformMatrix(entity);
            const glm::vec3 center = glm::vec3(worldTransform[3]);
            const glm::vec3 xAxis = glm::vec3(worldTransform * glm::vec4(sprite->Size.x, 0.0f, 0.0f, 0.0f));
            const glm::vec3 yAxis = glm::vec3(worldTransform * glm::vec4(0.0f, sprite->Size.y, 0.0f, 0.0f));
            const std::array<glm::vec3, 4> corners{{
                center - (xAxis * 0.5f) - (yAxis * 0.5f),
                center + (xAxis * 0.5f) - (yAxis * 0.5f),
                center + (xAxis * 0.5f) + (yAxis * 0.5f),
                center - (xAxis * 0.5f) + (yAxis * 0.5f)
            }};

            ImVec2 minPoint(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
            ImVec2 maxPoint(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
            bool anyProjected = false;
            for (const glm::vec3& corner : corners)
            {
                ImVec2 projected;
                if (!TryProjectWorldPointToViewport(projection, corner, projected))
                    continue;

                minPoint.x = std::min(minPoint.x, projected.x);
                minPoint.y = std::min(minPoint.y, projected.y);
                maxPoint.x = std::max(maxPoint.x, projected.x);
                maxPoint.y = std::max(maxPoint.y, projected.y);
                anyProjected = true;
            }

            if (!anyProjected || !ContainsPoint(minPoint, maxPoint, mousePosition))
                continue;

            PickRank rank;
            rank.CameraDepth = (camera.GetViewMatrix() * glm::vec4(center, 1.0f)).z;
            rank.SubmissionIndex = currentSubmissionIndex;
            if (const Life::SpriteRendererComponent* spriteRenderer = entity.TryGetComponent<Life::SpriteRendererComponent>())
            {
                rank.SortingLayerIndex = resolveSortingLayerPriority(spriteRenderer->SortingLayer);
                rank.SortingOrder = spriteRenderer->SortingOrder;
            }

            if (!bestEntity.IsValid() || isBetterPick(rank, bestRank))
            {
                bestRank = rank;
                bestEntity = entity;
            }
        }

        return bestEntity;
    }

    bool ScreenToWorldOnZPlane(const ViewportProjection& projection,
                               const ImVec2& screenPosition,
                               float planeZ,
                               glm::vec3& worldPosition)
    {
        if (!projection.Valid)
            return false;

        const float ndcX = ((screenPosition.x - projection.TopLeft.x) / projection.Size.x) * 2.0f - 1.0f;
        const float ndcY = 1.0f - ((screenPosition.y - projection.TopLeft.y) / projection.Size.y) * 2.0f;
        const glm::vec4 nearClip = projection.InverseViewProjection * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
        const glm::vec4 farClip = projection.InverseViewProjection * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        if (std::abs(nearClip.w) <= std::numeric_limits<float>::epsilon() ||
            std::abs(farClip.w) <= std::numeric_limits<float>::epsilon())
            return false;

        const glm::vec3 nearWorld = glm::vec3(nearClip) / nearClip.w;
        const glm::vec3 farWorld = glm::vec3(farClip) / farClip.w;
        const glm::vec3 direction = farWorld - nearWorld;
        if (std::abs(direction.z) <= std::numeric_limits<float>::epsilon())
            return false;

        const float t = (planeZ - nearWorld.z) / direction.z;
        if (t < 0.0f || t > 1.0f)
            return false;

        worldPosition = nearWorld + direction * t;
        return std::isfinite(worldPosition.x) && std::isfinite(worldPosition.y) && std::isfinite(worldPosition.z);
    }

    bool TryUnprojectViewportNdc(const ViewportProjection& projection,
                                 const glm::vec3& ndc,
                                 glm::vec3& worldPosition)
    {
        if (!projection.Valid)
            return false;

        const glm::vec4 world = projection.InverseViewProjection * glm::vec4(ndc, 1.0f);
        if (std::abs(world.w) <= std::numeric_limits<float>::epsilon())
            return false;

        worldPosition = glm::vec3(world) / world.w;
        return std::isfinite(worldPosition.x) && std::isfinite(worldPosition.y) && std::isfinite(worldPosition.z);
    }

    bool TryIntersectSegmentWithZPlane(const glm::vec3& start,
                                       const glm::vec3& end,
                                       float planeZ,
                                       glm::vec3& intersection)
    {
        const float startDistance = start.z - planeZ;
        const float endDistance = end.z - planeZ;
        if ((startDistance > 0.0f && endDistance > 0.0f) ||
            (startDistance < 0.0f && endDistance < 0.0f))
            return false;

        const float denominator = start.z - end.z;
        if (std::abs(denominator) <= std::numeric_limits<float>::epsilon())
            return false;

        const float t = (start.z - planeZ) / denominator;
        if (t < 0.0f || t > 1.0f)
            return false;

        intersection = start + (end - start) * t;
        return std::isfinite(intersection.x) && std::isfinite(intersection.y) && std::isfinite(intersection.z);
    }

    bool TryIntersectCameraRayWithZPlane(const Life::Camera& camera,
                                         float planeZ,
                                         glm::vec3& intersection)
    {
        const glm::vec3 direction = glm::normalize(camera.GetOrientation() * glm::vec3(0.0f, 0.0f, -1.0f));
        if (std::abs(direction.z) <= std::numeric_limits<float>::epsilon())
            return false;

        const float t = (planeZ - camera.GetPosition().z) / direction.z;
        if (t < 0.0f)
            return false;

        intersection = camera.GetPosition() + direction * t;
        return std::isfinite(intersection.x) && std::isfinite(intersection.y) && std::isfinite(intersection.z);
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

    bool ShouldDrawScreenGrid(const EditorApp::EditorSceneState& sceneState, const Life::Camera& camera) noexcept
    {
        (void)camera;
        switch (sceneState.GridMode)
        {
            case EditorApp::EditorViewportGridMode::Screen:
                return true;
            case EditorApp::EditorViewportGridMode::WorldPlane:
                return false;
            case EditorApp::EditorViewportGridMode::Auto:
            default:
                return sceneState.SceneViewMode == EditorApp::EditorSceneViewMode::TwoD;
        }
    }

    void DrawSelectedSpriteScreenBounds(const Life::Scene& scene,
                                        const Life::Entity& entity,
                                        const ViewportProjection& projection,
                                        ImU32 color)
    {
        const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>();
        if (sprite == nullptr)
            return;

        const auto corners = ResolveSpriteWorldCorners(scene, entity, *sprite);
        ImVec2 minimum(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity());
        ImVec2 maximum(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
        bool anyProjected = false;
        for (const glm::vec3& corner : corners)
        {
            ImVec2 screen;
            if (!TryProjectWorldPointToViewport(projection, corner, screen))
                continue;

            minimum.x = std::min(minimum.x, screen.x);
            minimum.y = std::min(minimum.y, screen.y);
            maximum.x = std::max(maximum.x, screen.x);
            maximum.y = std::max(maximum.y, screen.y);
            anyProjected = true;
        }

        if (!anyProjected)
            return;

        ImGui::GetWindowDrawList()->AddRect(minimum, maximum, color, 0.0f, 0, 2.0f);
    }

    void DrawSelectedSpriteOutline(const Life::Scene& scene,
                                   const Life::Entity& entity,
                                   const ViewportProjection& projection,
                                   bool drawScreenBounds)
    {
        const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>();
        if (sprite == nullptr)
            return;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 color = ImGui::GetColorU32(ImVec4(0.20f, 0.62f, 1.0f, 1.0f));
        if (drawScreenBounds)
        {
            DrawSelectedSpriteScreenBounds(scene, entity, projection, color);
            return;
        }

        const auto corners = ResolveSpriteWorldCorners(scene, entity, *sprite);
        for (std::size_t index = 0; index < corners.size(); ++index)
            DrawClippedWorldLine(*drawList, projection, corners[index], corners[(index + 1u) % corners.size()], color, 2.0f);
    }

    bool ComputeGridPlaneViewBounds(const ViewportProjection& projection,
                                    const Life::Camera& camera,
                                    float gridSize,
                                    float& minX,
                                    float& maxX,
                                    float& minY,
                                    float& maxY)
    {
        constexpr float planeZ = 0.0f;
        constexpr float fallbackHalfExtent = 64.0f;
        constexpr float maxWorldMagnitude = 100000.0f;

        std::array<glm::vec3, 8> frustumCorners{};
        const std::array<glm::vec3, 8> ndcCorners{{
            { -1.0f, -1.0f, 0.0f },
            { 1.0f, -1.0f, 0.0f },
            { 1.0f, 1.0f, 0.0f },
            { -1.0f, 1.0f, 0.0f },
            { -1.0f, -1.0f, 1.0f },
            { 1.0f, -1.0f, 1.0f },
            { 1.0f, 1.0f, 1.0f },
            { -1.0f, 1.0f, 1.0f }
        }};

        for (std::size_t index = 0; index < ndcCorners.size(); ++index)
        {
            if (!TryUnprojectViewportNdc(projection, ndcCorners[index], frustumCorners[index]))
                return false;
        }

        minX = std::numeric_limits<float>::infinity();
        maxX = -std::numeric_limits<float>::infinity();
        minY = std::numeric_limits<float>::infinity();
        maxY = -std::numeric_limits<float>::infinity();

        glm::vec2 center{ camera.GetPosition().x, camera.GetPosition().y };
        int pointCount = 0;
        auto includePoint = [&](const glm::vec3& point)
        {
            if (std::abs(point.x) > maxWorldMagnitude || std::abs(point.y) > maxWorldMagnitude)
                return;

            center += glm::vec2(point.x, point.y);
            minX = std::min(minX, point.x);
            maxX = std::max(maxX, point.x);
            minY = std::min(minY, point.y);
            maxY = std::max(maxY, point.y);
            ++pointCount;
        };

        for (const glm::vec3& corner : frustumCorners)
        {
            if (std::abs(corner.z - planeZ) <= gridSize * 0.001f)
                includePoint(corner);
        }

        const std::array<std::pair<std::size_t, std::size_t>, 12> edges{{
            { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
            { 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
            { 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
        }};
        for (const auto& [startIndex, endIndex] : edges)
        {
            glm::vec3 intersection;
            if (TryIntersectSegmentWithZPlane(frustumCorners[startIndex], frustumCorners[endIndex], planeZ, intersection))
                includePoint(intersection);
        }

        glm::vec3 lookPoint;
        if (TryIntersectCameraRayWithZPlane(camera, planeZ, lookPoint))
            includePoint(lookPoint);

        if (pointCount > 0)
        {
            center /= static_cast<float>(pointCount + 1);
            const float padding = gridSize * 8.0f;
            minX -= padding;
            maxX += padding;
            minY -= padding;
            maxY += padding;
        }
        else
        {
            const glm::vec3 forward = glm::normalize(camera.GetOrientation() * glm::vec3(0.0f, 0.0f, -1.0f));
            center += glm::vec2(forward.x, forward.y) * fallbackHalfExtent * gridSize;
            minX = center.x - fallbackHalfExtent * gridSize;
            maxX = center.x + fallbackHalfExtent * gridSize;
            minY = center.y - fallbackHalfExtent * gridSize;
            maxY = center.y + fallbackHalfExtent * gridSize;
        }

        auto repairAxis = [gridSize](float& minimum, float& maximum, float axisCenter)
        {
            if (!std::isfinite(minimum) || !std::isfinite(maximum) || minimum >= maximum)
            {
                const float fallbackSpan = gridSize * 128.0f;
                minimum = axisCenter - fallbackSpan * 0.5f;
                maximum = axisCenter + fallbackSpan * 0.5f;
            }
        };

        repairAxis(minX, maxX, center.x);
        repairAxis(minY, maxY, center.y);
        return true;
    }

    void DrawWorldGridOverlay(const EditorApp::EditorSceneState& sceneState,
                              const Life::Camera& camera,
                              const ViewportProjection& projection)
    {
        const float gridSize = std::max(sceneState.GridSize, 0.05f);
        float minX = 0.0f;
        float maxX = 0.0f;
        float minY = 0.0f;
        float maxY = 0.0f;
        if (!ComputeGridPlaneViewBounds(projection, camera, gridSize, minX, maxX, minY, maxY))
            return;

        constexpr int maxGridLinesPerAxis = 801;
        const float spanX = std::max(maxX - minX, gridSize);
        const float spanY = std::max(maxY - minY, gridSize);
        const float largestSpan = std::max(spanX, spanY);
        const float spacingMultiplier = std::max(1.0f, std::ceil(largestSpan / (gridSize * static_cast<float>(maxGridLinesPerAxis - 1))));
        const float visibleGridSize = gridSize * spacingMultiplier;

        minX = std::floor(minX / visibleGridSize) * visibleGridSize;
        maxX = std::ceil(maxX / visibleGridSize) * visibleGridSize;
        minY = std::floor(minY / visibleGridSize) * visibleGridSize;
        maxY = std::ceil(maxY / visibleGridSize) * visibleGridSize;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 minorColor = ImGui::GetColorU32(ImVec4(0.55f, 0.62f, 0.70f, 0.18f));
        const ImU32 axisColor = ImGui::GetColorU32(ImVec4(0.55f, 0.72f, 0.95f, 0.45f));
        for (float x = minX; x <= maxX + visibleGridSize * 0.5f; x += visibleGridSize)
        {
            const bool isAxis = std::abs(x) <= visibleGridSize * 0.001f;
            DrawClippedWorldLine(*drawList, projection, { x, minY, 0.0f }, { x, maxY, 0.0f }, isAxis ? axisColor : minorColor, isAxis ? 1.5f : 1.0f);
        }
        for (float y = minY; y <= maxY + visibleGridSize * 0.5f; y += visibleGridSize)
        {
            const bool isAxis = std::abs(y) <= visibleGridSize * 0.001f;
            DrawClippedWorldLine(*drawList, projection, { minX, y, 0.0f }, { maxX, y, 0.0f }, isAxis ? axisColor : minorColor, isAxis ? 1.5f : 1.0f);
        }
    }

    void DrawScreenGridOverlay(const EditorApp::EditorSceneState& sceneState,
                               const ViewportProjection& projection)
    {
        if (!projection.Valid)
            return;

        const float gridSize = std::max(sceneState.GridSize, 0.05f);
        const float gridPixels = std::clamp(48.0f * gridSize, 12.0f, 128.0f);
        const float left = projection.TopLeft.x;
        const float top = projection.TopLeft.y;
        const float right = projection.TopLeft.x + projection.Size.x;
        const float bottom = projection.TopLeft.y + projection.Size.y;
        const float firstX = std::ceil(left / gridPixels) * gridPixels;
        const float firstY = std::ceil(top / gridPixels) * gridPixels;

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImU32 minorColor = ImGui::GetColorU32(ImVec4(0.55f, 0.62f, 0.70f, 0.16f));
        for (float x = firstX; x <= right; x += gridPixels)
            drawList->AddLine(ImVec2(x, top), ImVec2(x, bottom), minorColor, 1.0f);
        for (float y = firstY; y <= bottom; y += gridPixels)
            drawList->AddLine(ImVec2(left, y), ImVec2(right, y), minorColor, 1.0f);
    }

    void DrawGridOverlay(const EditorApp::EditorSceneState& sceneState,
                         const Life::Camera& camera,
                         const ViewportProjection& projection)
    {
        if (!sceneState.ShowGrid)
            return;

        if (ShouldDrawScreenGrid(sceneState, camera))
            DrawScreenGridOverlay(sceneState, projection);
        else
            DrawWorldGridOverlay(sceneState, camera, projection);
    }

    bool ApplyTextureToSprite(Life::Entity entity, const EditorApp::EditorServices& services, std::string textureAssetKey)
    {
        Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>();
        if (sprite == nullptr || sprite->TextureAssetKey == textureAssetKey)
            return false;

        sprite->TextureAssetKey = std::move(textureAssetKey);
        sprite->TextureAsset = services.AssetManager && !sprite->TextureAssetKey.empty()
            ? services.AssetManager->get().GetOrLoad<Life::Assets::TextureAsset>(sprite->TextureAssetKey)
            : nullptr;
        return true;
    }

#if __has_include(<ImGuizmo.h>)
    ImGuizmo::OPERATION ResolveGizmoOperation(EditorApp::EditorViewportTool tool) noexcept
    {
        switch (tool)
        {
            case EditorApp::EditorViewportTool::Rotate: return ImGuizmo::ROTATE;
            case EditorApp::EditorViewportTool::Scale: return ImGuizmo::SCALE;
            case EditorApp::EditorViewportTool::Translate: return ImGuizmo::TRANSLATE;
            case EditorApp::EditorViewportTool::Select:
            default: return ImGuizmo::TRANSLATE;
        }
    }

    ImGuizmo::MODE ResolveGizmoMode(EditorApp::EditorTransformSpace space) noexcept
    {
        return space == EditorApp::EditorTransformSpace::World ? ImGuizmo::WORLD : ImGuizmo::LOCAL;
    }

    std::array<float, 3> ResolveSnapValues(const EditorApp::EditorSceneState& sceneState)
    {
        switch (sceneState.ViewportTool)
        {
            case EditorApp::EditorViewportTool::Rotate:
                return { sceneState.RotationSnapDegrees, sceneState.RotationSnapDegrees, sceneState.RotationSnapDegrees };
            case EditorApp::EditorViewportTool::Scale:
                return { sceneState.ScaleSnap, sceneState.ScaleSnap, sceneState.ScaleSnap };
            case EditorApp::EditorViewportTool::Translate:
            case EditorApp::EditorViewportTool::Select:
            default:
            {
                const float snap = sceneState.SnapToGrid ? sceneState.GridSize : sceneState.TranslationSnap;
                return { snap, snap, snap };
            }
        }
    }
#endif
#endif
}

namespace EditorApp
{
    void SceneViewportPanel::Attach(const EditorServices& services)
    {
        m_NativeWindowHandle = services.Window ? services.Window->get().GetNativeHandle() : nullptr;

        if (services.Renderer && services.SceneRenderer2D && services.ImGuiSystem)
        {
            m_SceneSurface = Life::CreateScope<Life::SceneSurface>(
                services.Renderer->get(),
                services.SceneRenderer2D->get().GetRenderer2D(),
                services.ImGuiSystem->get());
        }
    }

    void SceneViewportPanel::Detach() noexcept
    {
        SetCameraNavigationActive(false);
        m_2DPanning = false;
        m_SceneSurface.reset();
        m_State = {};
        m_LastTimestep = 0.0f;
        m_NativeWindowHandle = nullptr;
    }

    void SceneViewportPanel::Update(const EditorServices& services, float timestep)
    {
        m_LastTimestep = timestep;
        if (m_NativeWindowHandle == nullptr && services.Window)
            m_NativeWindowHandle = services.Window->get().GetNativeHandle();
    }

    void SceneViewportPanel::Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState, EditorCameraTool& cameraTool, EditorUndoStack& undoStack)
    {
#if __has_include(<imgui.h>)
        if (!isOpen)
        {
            SetCameraNavigationActive(false);
            return;
        }

        if (ImGui::Begin("Scene", &isOpen))
        {
            ImGui::TextColored(ImVec4(0.60f, 0.78f, 1.0f, 1.0f), "Scene");
            ImGui::SameLine();
            ImGui::TextDisabled("Viewport and scene interaction");
            ImGui::Separator();
            DrawViewportToolbar(sceneState);
            ImGui::Separator();

            const ImVec2 availableRegion = ImGui::GetContentRegionAvail();
            if (availableRegion.x >= 1.0f && availableRegion.y >= 1.0f)
            {
                ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
                if (ImGui::BeginChild(
                        "SceneViewportSurface",
                        availableRegion,
                        false,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
                {
                    const ImVec2 viewportTopLeft = ImGui::GetCursorScreenPos();
                    const ImVec2 viewportRegion = ImGui::GetContentRegionAvail();
                    const ImVec2 framebufferScale = ResolveFramebufferScale(services, m_NativeWindowHandle);
                    const float framebufferScaleX = std::max(framebufferScale.x, 1.0f);
                    const float framebufferScaleY = std::max(framebufferScale.y, 1.0f);
                    const ImVec2 snappedViewportTopLeft = {
                        SnapToPixelGrid(viewportTopLeft.x, framebufferScaleX),
                        SnapToPixelGrid(viewportTopLeft.y, framebufferScaleY)
                    };
                    const uint32_t renderWidth = std::max(1u, static_cast<uint32_t>(std::lround(viewportRegion.x * framebufferScaleX)));
                    const uint32_t renderHeight = std::max(1u, static_cast<uint32_t>(std::lround(viewportRegion.y * framebufferScaleY)));
                    const ImVec2 snappedViewportRegion = {
                        static_cast<float>(renderWidth) / framebufferScaleX,
                        static_cast<float>(renderHeight) / framebufferScaleY
                    };
                    const ImVec2 mainViewportPosition = ImGui::GetWindowViewport()->Pos;
                    const ImVec2 viewportWindowLocalTopLeft = {
                        snappedViewportTopLeft.x - mainViewportPosition.x,
                        snappedViewportTopLeft.y - mainViewportPosition.y
                    };
                    const bool viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
                    const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
                    m_State.DisplayWidth = snappedViewportRegion.x;
                    m_State.DisplayHeight = snappedViewportRegion.y;
                    m_State.FramebufferScaleX = framebufferScaleX;
                    m_State.FramebufferScaleY = framebufferScaleY;
                    m_State.RequestedRenderWidth = renderWidth;
                    m_State.RequestedRenderHeight = renderHeight;
                    if (services.GraphicsDevice)
                    {
                        m_State.BackBufferWidth = services.GraphicsDevice->get().GetBackBufferWidth();
                        m_State.BackBufferHeight = services.GraphicsDevice->get().GetBackBufferHeight();
                    }
                    else
                    {
                        m_State.BackBufferWidth = 0;
                        m_State.BackBufferHeight = 0;
                    }
                    if (renderWidth >= 1u && renderHeight >= 1u)
                    {
                        ImGui::SetCursorScreenPos(snappedViewportTopLeft);
                        const bool rendered = RenderSceneSurface(
                            renderWidth,
                            renderHeight,
                            snappedViewportRegion.x,
                            snappedViewportRegion.y,
                            snappedViewportTopLeft.x,
                            snappedViewportTopLeft.y,
                            services,
                            sceneState,
                            cameraTool,
                            undoStack,
                            viewportHovered,
                            viewportFocused);
                        if (m_CameraNavigationActive)
                            SetCameraNavigationMouseRect(m_NativeWindowHandle, viewportWindowLocalTopLeft, snappedViewportRegion);

                        if (!rendered)
                        {
                            SetCameraNavigationActive(false);
                            ImGui::TextUnformatted("Scene surface rendering is unavailable.");
                        }
                        else if (ImGui::BeginDragDropTarget())
                        {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                            {
                                const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                                if (assetPayload != nullptr &&
                                    assetPayload->Kind == ProjectAssetPayloadKind::Scene &&
                                    assetPayload->RelativePath[0] != '\0' &&
                                    services.SceneService)
                                {
                                    sceneState.ResetRuntimeState();
                                    const std::string sceneAssetKey = std::string("Assets/") + assetPayload->RelativePath.data();
                                    const auto loadResult = services.SceneService->get().LoadScene(sceneAssetKey);
                                    if (loadResult.IsFailure())
                                    {
                                        sceneState.SetStatusMessage(loadResult.GetError().GetErrorMessage(), true);
                                    }
                                    else
                                    {
                                        sceneState.ClearSelection();
                                        sceneState.SetStatusMessage(
                                            "Opened scene '" + services.SceneService->get().GetActiveScene().GetName() + "'.",
                                            false);
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                    else
                    {
                        SetCameraNavigationActive(false);
                        ImGui::TextUnformatted("Scene surface has no drawable area.");
                    }
                }
                ImGui::EndChild();
                ImGui::PopStyleVar();
            }
            else
            {
                SetCameraNavigationActive(false);
                ImGui::TextUnformatted("Scene surface has no drawable area.");
            }
        }
        else
        {
            SetCameraNavigationActive(false);
        }
        ImGui::End();
#else
        (void)isOpen;
        (void)services;
        (void)sceneState;
        (void)cameraTool;
        (void)undoStack;
#endif
    }

    const SceneViewportState& SceneViewportPanel::GetState() const noexcept
    {
        return m_State;
    }

    void SceneViewportPanel::UpdateCameraNavigation(EditorCameraTool& cameraTool,
                                                    Life::Camera& camera,
                                                    EditorSceneState& sceneState,
                                                    bool viewportHovered,
                                                    bool viewportFocused,
                                                    float viewportScreenX,
                                                    float viewportScreenY,
                                                    float displayWidth,
                                                    float displayHeight)
    {
#if __has_include(<imgui.h>)
        if (sceneState.SceneViewMode == EditorSceneViewMode::TwoD)
        {
            SetCameraNavigationActive(false);

            const ImGuiIO& io = ImGui::GetIO();
            const bool hasViewportInput = viewportFocused && (viewportHovered || m_2DPanning);
            if (!hasViewportInput)
            {
                m_2DPanning = false;
                return;
            }

            const ImVec2 viewportTopLeft(viewportScreenX, viewportScreenY);
            const ImVec2 viewportSize(displayWidth, displayHeight);
            const ImVec2 mousePosition = io.MousePos;
            auto zoomAtMouse = [&](float zoomFactor)
            {
                if (!std::isfinite(zoomFactor) || zoomFactor <= 0.0f || std::abs(zoomFactor - 1.0f) <= 0.0001f)
                    return;

                const ViewportProjection beforeProjection = BuildViewportProjection(camera, viewportTopLeft, viewportSize);
                glm::vec3 beforeWorld{ 0.0f };
                const bool hasAnchor = ScreenToWorldOnZPlane(beforeProjection, mousePosition, 0.0f, beforeWorld);

                cameraTool.Set2DOrthographicSize(camera, camera.GetOrthographicSize() * zoomFactor);

                if (!hasAnchor)
                    return;

                const ViewportProjection afterProjection = BuildViewportProjection(camera, viewportTopLeft, viewportSize);
                glm::vec3 afterWorld{ 0.0f };
                if (ScreenToWorldOnZPlane(afterProjection, mousePosition, 0.0f, afterWorld))
                    cameraTool.Pan2D(camera, glm::vec2(beforeWorld.x - afterWorld.x, beforeWorld.y - afterWorld.y));
            };

            if (viewportHovered && std::abs(io.MouseWheel) > 0.0001f)
                zoomAtMouse(std::pow(0.85f, io.MouseWheel));

            const bool middleDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
            const bool ctrlDown = io.KeyCtrl;
            if (middleDown && ctrlDown)
            {
                m_2DPanning = false;
                if (std::abs(io.MouseDelta.y) > 0.0001f)
                    zoomAtMouse(std::pow(1.01f, io.MouseDelta.y));
                return;
            }

            if (viewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
            {
                const ViewportProjection projection = BuildViewportProjection(camera, viewportTopLeft, viewportSize);
                m_2DPanning = ScreenToWorldOnZPlane(projection, mousePosition, 0.0f, m_2DPanAnchorWorld);
            }

            if (!middleDown)
            {
                m_2DPanning = false;
                return;
            }

            if (m_2DPanning)
            {
                const ViewportProjection projection = BuildViewportProjection(camera, viewportTopLeft, viewportSize);
                glm::vec3 currentWorld{ 0.0f };
                if (ScreenToWorldOnZPlane(projection, mousePosition, 0.0f, currentWorld))
                    cameraTool.Pan2D(camera, glm::vec2(m_2DPanAnchorWorld.x - currentWorld.x, m_2DPanAnchorWorld.y - currentWorld.y));
            }
            return;
        }

        m_2DPanning = false;
        const bool activateNavigation = viewportFocused && ImGui::IsMouseDown(ImGuiMouseButton_Right) && (viewportHovered || m_CameraNavigationActive);
        SetCameraNavigationActive(activateNavigation);
        if (!m_CameraNavigationActive)
            return;

        EditorCameraTool::FlyCameraInput input;
        input.LookDelta = GetCameraLookDelta(m_NativeWindowHandle);
        input.MoveAxes.x = (ImGui::IsKeyDown(ImGuiKey_D) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_A) ? 1.0f : 0.0f);
        input.MoveAxes.y = (ImGui::IsKeyDown(ImGuiKey_E) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_Q) ? 1.0f : 0.0f);
        input.MoveAxes.z = (ImGui::IsKeyDown(ImGuiKey_W) ? 1.0f : 0.0f) - (ImGui::IsKeyDown(ImGuiKey_S) ? 1.0f : 0.0f);
        input.Boost = ImGui::IsKeyDown(ImGuiKey_LeftShift) || ImGui::IsKeyDown(ImGuiKey_RightShift);
        cameraTool.UpdateFlyCamera(camera, input, m_LastTimestep);
#else
        (void)cameraTool;
        (void)camera;
        (void)sceneState;
        (void)viewportHovered;
        (void)viewportFocused;
        (void)viewportScreenX;
        (void)viewportScreenY;
        (void)displayWidth;
        (void)displayHeight;
#endif
    }

    void SceneViewportPanel::SetCameraNavigationActive(bool active) noexcept
    {
        if (m_CameraNavigationActive == active)
            return;

        m_CameraNavigationActive = active;

        if (auto* sdlWindow = static_cast<SDL_Window*>(m_NativeWindowHandle))
        {
            SDL_SetWindowMouseGrab(sdlWindow, active);
            SDL_CaptureMouse(active);
            SDL_SetWindowRelativeMouseMode(sdlWindow, active);
            if (!active)
                SDL_SetWindowMouseRect(sdlWindow, nullptr);

            if (active && SDL_GetWindowRelativeMouseMode(sdlWindow))
            {
                float deltaX = 0.0f;
                float deltaY = 0.0f;
                SDL_GetRelativeMouseState(&deltaX, &deltaY);
            }
        }
    }

    bool SceneViewportPanel::RenderSceneSurface(uint32_t renderWidth,
                                                uint32_t renderHeight,
                                                float displayWidth,
                                                float displayHeight,
                                                float viewportScreenX,
                                                float viewportScreenY,
                                                const EditorServices& services,
                                                EditorSceneState& sceneState,
                                                EditorCameraTool& cameraTool,
                                                EditorUndoStack& undoStack,
                                                bool viewportHovered,
                                                bool viewportFocused)
    {
        m_State.LastRenderSucceeded = false;
        m_State.ExecutionMode = sceneState.ExecutionMode;
        m_State.SceneViewMode = sceneState.SceneViewMode;
        m_State.UsingEditorCamera = sceneState.ExecutionMode == EditorSceneExecutionMode::Edit;
        m_State.UsingSceneCamera = sceneState.ExecutionMode != EditorSceneExecutionMode::Edit;
        m_State.SurfaceReady = m_SceneSurface && m_SceneSurface->IsReady();
        m_State.SurfaceWidth = m_SceneSurface ? m_SceneSurface->GetWidth() : 0;
        m_State.SurfaceHeight = m_SceneSurface ? m_SceneSurface->GetHeight() : 0;
        m_State.RequestedRenderWidth = renderWidth;
        m_State.RequestedRenderHeight = renderHeight;
        m_State.DisplayWidth = displayWidth;
        m_State.DisplayHeight = displayHeight;
        m_State.BackBufferWidth = services.GraphicsDevice ? services.GraphicsDevice->get().GetBackBufferWidth() : 0;
        m_State.BackBufferHeight = services.GraphicsDevice ? services.GraphicsDevice->get().GetBackBufferHeight() : 0;
        m_State.RequestedQuadCount = 0;
        m_State.TexturedQuadCount = 0;
        m_State.UntexturedQuadCount = 0;
        m_State.RendererStats = {};

        if (!m_SceneSurface || !services.SceneRenderer2D || !services.CameraManager)
        {
            SetCameraNavigationActive(false);
            return false;
        }

        if (!m_SceneSurface->Resize(renderWidth, renderHeight))
        {
            SetCameraNavigationActive(false);
            return false;
        }

        const float requestedAspectRatio = displayHeight > 0.0f ? displayWidth / displayHeight : 16.0f / 9.0f;
        const float actualAspectRatio = m_SceneSurface->GetHeight() > 0
            ? static_cast<float>(m_SceneSurface->GetWidth()) / static_cast<float>(m_SceneSurface->GetHeight())
            : requestedAspectRatio;

        Life::Scene* effectiveScene = services.SceneService ? sceneState.GetEffectiveScene(services.SceneService->get()) : nullptr;
        Life::Camera sceneCamera;
        Life::Camera* activeCamera = nullptr;
        const bool useSceneCamera = sceneState.ExecutionMode != EditorSceneExecutionMode::Edit;
        if (useSceneCamera)
        {
            SetCameraNavigationActive(false);
            if (effectiveScene == nullptr || !effectiveScene->BuildPrimaryCamera(actualAspectRatio, sceneCamera))
                return false;
            activeCamera = &sceneCamera;
        }
        else
        {
            cameraTool.Ensure(services.CameraManager->get(), requestedAspectRatio);
            auto editorCamera = cameraTool.TryGetCamera(services.CameraManager->get());
            if (!editorCamera)
            {
                SetCameraNavigationActive(false);
                return false;
            }

            Life::Camera& camera = editorCamera->get();
            camera.SetAspectRatio(actualAspectRatio);
            cameraTool.ApplySceneViewMode(camera, sceneState.SceneViewMode, actualAspectRatio);
            UpdateCameraNavigation(
                cameraTool,
                camera,
                sceneState,
                viewportHovered,
                viewportFocused,
                viewportScreenX,
                viewportScreenY,
                displayWidth,
                displayHeight);
            activeCamera = &camera;
        }

        m_State.UsingEditorCamera = !useSceneCamera;
        m_State.UsingSceneCamera = useSceneCamera;

        bool renderSucceeded = false;
        if (effectiveScene != nullptr)
        {
            m_State.RequestedQuadCount = 0;
            m_State.TexturedQuadCount = 0;
            m_State.UntexturedQuadCount = 0;

            for (const Life::Entity entity : effectiveScene->GetEntities())
            {
                if (const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>())
                {
                    ++m_State.RequestedQuadCount;
                    if (sprite->TextureAsset)
                        ++m_State.TexturedQuadCount;
                    else
                        ++m_State.UntexturedQuadCount;
                }
            }

            const Life::SceneRenderer2D::RenderOptions renderOptions = ResolveSceneRenderOptions(services);
            renderSucceeded = services.SceneRenderer2D->get().RenderToSurface(
                *m_SceneSurface,
                *effectiveScene,
                *activeCamera,
                Life::SceneRenderer2D::QuadSortMode::BackToFront,
                renderOptions);
        }
        else
        {
            const Life::SceneRenderer2D::Scene2D emptyScene{ .Camera = activeCamera };
            renderSucceeded = services.SceneRenderer2D->get().RenderToSurface(
                *m_SceneSurface,
                emptyScene,
                ResolveSceneRenderOptions(services));
        }

        if (!renderSucceeded)
            return false;

        m_State.SurfaceReady = m_SceneSurface->IsReady();
        m_State.SurfaceWidth = m_SceneSurface->GetWidth();
        m_State.SurfaceHeight = m_SceneSurface->GetHeight();
        m_State.RendererStats = services.SceneRenderer2D->get().GetStats();

        const bool presented = m_SceneSurface->Present(displayWidth, displayHeight);
        m_State.LastRenderSucceeded = presented;
#if __has_include(<imgui.h>)
        if (presented &&
            !useSceneCamera &&
            effectiveScene != nullptr &&
            services.SceneService)
        {
            const ImVec2 viewportTopLeft(viewportScreenX, viewportScreenY);
            const ImVec2 viewportSize(displayWidth, displayHeight);
            const ViewportProjection viewportProjection = BuildViewportProjection(*activeCamera, viewportTopLeft, viewportSize);
            const ImVec2 mousePosition = ImGui::GetIO().MousePos;
            Life::Entity selectedEntity = sceneState.GetSelectedEntity(*effectiveScene);
            const bool screenGridMode = ShouldDrawScreenGrid(sceneState, *activeCamera);

            DrawGridOverlay(sceneState, *activeCamera, viewportProjection);
            if (selectedEntity.IsValid())
                DrawSelectedSpriteOutline(*effectiveScene, selectedEntity, viewportProjection, screenGridMode);

            if (viewportHovered && viewportFocused && ImGui::IsKeyPressed(ImGuiKey_F, false) && selectedEntity.IsValid())
            {
                glm::vec3 center = glm::vec3(effectiveScene->GetWorldTransformMatrix(selectedEntity)[3]);
                float radius = 1.0f;
                if (const Life::SpriteComponent* sprite = selectedEntity.TryGetComponent<Life::SpriteComponent>())
                {
                    const auto corners = ResolveSpriteWorldCorners(*effectiveScene, selectedEntity, *sprite);
                    radius = 0.0f;
                    for (const glm::vec3& corner : corners)
                        radius = std::max(radius, glm::length(corner - center));
                }
                cameraTool.FrameBounds(*activeCamera, sceneState.SceneViewMode, center, radius);
            }

            if (ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(kProjectAssetDragPayloadType))
                {
                    const ProjectAssetDragPayload* assetPayload = static_cast<const ProjectAssetDragPayload*>(payload->Data);
                    if (assetPayload != nullptr &&
                        assetPayload->RelativePath[0] != '\0')
                    {
                        const std::filesystem::path relativePath(assetPayload->RelativePath.data());
                        if (assetPayload->Kind == ProjectAssetPayloadKind::Scene)
                        {
                            sceneState.ResetRuntimeState();
                            const std::string sceneAssetKey = MakeAssetKey(relativePath);
                            const auto loadResult = services.SceneService->get().LoadScene(sceneAssetKey);
                            if (loadResult.IsFailure())
                            {
                                sceneState.SetStatusMessage(loadResult.GetError().GetErrorMessage(), true);
                            }
                            else
                            {
                                sceneState.ClearSelection();
                                sceneState.SetStatusMessage(
                                    "Opened scene '" + services.SceneService->get().GetActiveScene().GetName() + "'.",
                                    false);
                            }
                        }
                        else if (assetPayload->Kind == ProjectAssetPayloadKind::File && IsTextureAssetPath(relativePath))
                        {
                            const std::string assetKey = MakeAssetKey(relativePath);
                            Life::Entity target = PickSpriteEntity(*effectiveScene, *activeCamera, viewportProjection, mousePosition);
                            if (target.IsValid())
                            {
                                const EditorEntitySnapshot before = CaptureEntitySnapshot(target);
                                if (ApplyTextureToSprite(target, services, assetKey))
                                {
                                    undoStack.CommitExecuted(std::make_unique<RestoreEntitySnapshotCommand>(
                                        before,
                                        CaptureEntitySnapshot(target)));
                                    sceneState.SelectEntity(target);
                                    services.SceneService->get().MarkActiveSceneDirty();
                                }
                            }
                            else
                            {
                                glm::vec3 worldPosition{ 0.0f };
                                if (!ScreenToWorldOnZPlane(viewportProjection, mousePosition, 0.0f, worldPosition))
                                    worldPosition = glm::vec3(0.0f);
                                if (sceneState.SnapToGrid)
                                {
                                    const float gridSize = std::max(sceneState.GridSize, 0.05f);
                                    worldPosition.x = std::round(worldPosition.x / gridSize) * gridSize;
                                    worldPosition.y = std::round(worldPosition.y / gridSize) * gridSize;
                                }

                                Life::Entity spriteEntity = effectiveScene->CreateEntity(relativePath.stem().string().empty() ? std::string("Sprite") : relativePath.stem().string());
                                spriteEntity.GetComponent<Life::TransformComponent>().LocalPosition = worldPosition;
                                Life::SpriteComponent& sprite = spriteEntity.AddComponent<Life::SpriteComponent>();
                                spriteEntity.AddComponent<Life::SpriteRendererComponent>();
                                sprite.TextureAssetKey = assetKey;
                                if (services.AssetManager)
                                    sprite.TextureAsset = services.AssetManager->get().GetOrLoad<Life::Assets::TextureAsset>(assetKey);
                                sceneState.SelectEntity(spriteEntity);
                                undoStack.CommitExecuted(std::make_unique<CreateEntityCommand>(CaptureEntitySnapshot(spriteEntity)));
                                services.SceneService->get().MarkActiveSceneDirty();
                            }
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

#if __has_include(<ImGuizmo.h>)
            ImGuizmo::BeginFrame();
            ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
            ImGuizmo::SetRect(viewportTopLeft.x, viewportTopLeft.y, viewportSize.x, viewportSize.y);
            ImGuizmo::SetOrthographic(activeCamera->GetProjectionType() == Life::ProjectionType::Orthographic);

            const bool canShowGizmo = selectedEntity.IsValid() &&
                sceneState.ViewportTool != EditorViewportTool::Select &&
                selectedEntity.HasComponent<Life::TransformComponent>();

            if (canShowGizmo)
            {
                glm::mat4 worldTransform = effectiveScene->GetWorldTransformMatrix(selectedEntity);
                const glm::mat4 viewMatrix = activeCamera->GetViewMatrix();
                const glm::mat4 projectionMatrix = activeCamera->GetProjectionMatrix();
                std::array<float, 3> snapValues = ResolveSnapValues(sceneState);
                const float* snap = sceneState.SnapEnabled ? snapValues.data() : nullptr;

                const bool manipulated = ImGuizmo::Manipulate(
                    glm::value_ptr(viewMatrix),
                    glm::value_ptr(projectionMatrix),
                    ResolveGizmoOperation(sceneState.ViewportTool),
                    ResolveGizmoMode(sceneState.TransformSpace),
                    glm::value_ptr(worldTransform),
                    nullptr,
                    snap);

                const bool usingGizmo = ImGuizmo::IsUsing();
                if (usingGizmo && (!m_GizmoManipulating || m_GizmoEntityId != selectedEntity.GetId()))
                {
                    m_GizmoManipulating = true;
                    m_GizmoEntityId = selectedEntity.GetId();
                    m_GizmoTransformBefore = selectedEntity.GetComponent<Life::TransformComponent>();
                }

                if (manipulated)
                    (void)Life::SetEntityWorldTransform(*effectiveScene, selectedEntity, worldTransform);

                if (!usingGizmo && m_GizmoManipulating)
                {
                    Life::Entity manipulatedEntity = effectiveScene->FindEntityById(m_GizmoEntityId);
                    if (manipulatedEntity.IsValid())
                    {
                        const Life::TransformComponent after = manipulatedEntity.GetComponent<Life::TransformComponent>();
                        if (TransformChanged(m_GizmoTransformBefore, after))
                        {
                            undoStack.CommitExecuted(std::make_unique<SetEntityTransformCommand>(
                                m_GizmoEntityId,
                                m_GizmoTransformBefore,
                                after));
                            services.SceneService->get().MarkActiveSceneDirty();
                        }
                    }

                    m_GizmoManipulating = false;
                    m_GizmoEntityId.clear();
                    m_GizmoTransformBefore = {};
                }
            }
            else if (m_GizmoManipulating && !ImGuizmo::IsUsing())
            {
                m_GizmoManipulating = false;
                m_GizmoEntityId.clear();
                m_GizmoTransformBefore = {};
            }

            const bool gizmoOwnsMouse = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
#else
            const bool gizmoOwnsMouse = false;
            (void)undoStack;
#endif
            if (viewportHovered &&
                viewportFocused &&
                !m_CameraNavigationActive &&
                !gizmoOwnsMouse &&
                ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                Life::Entity pickedEntity = PickSpriteEntity(*effectiveScene, *activeCamera, viewportProjection, mousePosition);
                if (pickedEntity.IsValid())
                    sceneState.SelectEntity(pickedEntity);
                else
                    sceneState.ClearSelection();
            }
        }
        else
        {
            m_GizmoManipulating = false;
            m_GizmoEntityId.clear();
            m_GizmoTransformBefore = {};
        }
#else
        (void)viewportScreenX;
        (void)viewportScreenY;
        (void)undoStack;
#endif
        return presented;
    }
}
