#pragma once

#include "Graphics/GraphicsTypes.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>

namespace Life
{
    enum class ProjectionType : uint8_t
    {
        Perspective = 0,
        Orthographic
    };

    enum class CameraClearMode : uint8_t
    {
        SolidColor = 0,
        DepthOnly,
        None
    };

    struct CameraSpecification
    {
        std::string Name = "Camera";
        ProjectionType Projection = ProjectionType::Perspective;
        float FieldOfView = 60.0f;
        float NearClip = 0.1f;
        float FarClip = 1000.0f;
        float OrthoSize = 5.0f;
        float OrthoNear = -1.0f;
        float OrthoFar = 1.0f;
        float AspectRatio = 16.0f / 9.0f;
        int32_t Priority = 0;
        CameraClearMode ClearMode = CameraClearMode::SolidColor;
        glm::vec4 ClearColor = { 0.1f, 0.1f, 0.1f, 1.0f };
        Viewport ViewportRect = { 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
    };

    class Camera
    {
    public:
        explicit Camera(const CameraSpecification& specification = {});
        ~Camera() = default;

        Camera(const Camera&) = delete;
        Camera& operator=(const Camera&) = delete;
        Camera(Camera&&) noexcept = default;
        Camera& operator=(Camera&&) noexcept = default;

        const CameraSpecification& GetSpecification() const noexcept { return m_Specification; }

        const std::string& GetName() const noexcept { return m_Specification.Name; }
        ProjectionType GetProjectionType() const noexcept { return m_Specification.Projection; }
        float GetAspectRatio() const noexcept { return m_Specification.AspectRatio; }
        float GetFieldOfView() const noexcept { return m_Specification.FieldOfView; }
        float GetNearClip() const noexcept { return m_Specification.NearClip; }
        float GetFarClip() const noexcept { return m_Specification.FarClip; }
        float GetOrthographicSize() const noexcept { return m_Specification.OrthoSize; }
        int32_t GetPriority() const noexcept { return m_Specification.Priority; }
        CameraClearMode GetClearMode() const noexcept { return m_Specification.ClearMode; }
        const glm::vec4& GetClearColor() const noexcept { return m_Specification.ClearColor; }
        const Viewport& GetViewportRect() const noexcept { return m_Specification.ViewportRect; }

        const glm::vec3& GetPosition() const noexcept { return m_Position; }
        const glm::quat& GetOrientation() const noexcept { return m_Orientation; }

        void SetName(std::string name);
        void SetPosition(const glm::vec3& position);
        void SetOrientation(const glm::quat& orientation);
        void SetTransform(const glm::vec3& position, const glm::quat& orientation);
        void LookAt(const glm::vec3& target, const glm::vec3& up = glm::vec3(0.0f, 1.0f, 0.0f));

        void SetPerspective(float verticalFieldOfViewDegrees, float nearClip, float farClip);
        void SetOrthographic(float size, float nearClip, float farClip);
        void SetAspectRatio(float aspectRatio);
        void SetPriority(int32_t priority) noexcept;
        void SetClearMode(CameraClearMode clearMode) noexcept;
        void SetClearColor(const glm::vec4& clearColor) noexcept;
        void SetViewportRect(const Viewport& viewportRect);

        glm::mat4 GetViewMatrix() const;
        glm::mat4 GetProjectionMatrix() const;
        glm::mat4 GetViewProjectionMatrix() const;
        Viewport GetPixelViewport(uint32_t framebufferWidth, uint32_t framebufferHeight) const;

    private:
        void InvalidateView() noexcept;
        void InvalidateProjection() noexcept;
        void RecalculateViewMatrix() const;
        void RecalculateProjectionMatrix() const;

        CameraSpecification m_Specification;
        glm::vec3 m_Position{ 0.0f, 0.0f, 0.0f };
        glm::quat m_Orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
        mutable glm::mat4 m_ViewMatrix{ 1.0f };
        mutable glm::mat4 m_ProjectionMatrix{ 1.0f };
        mutable bool m_ViewDirty = true;
        mutable bool m_ProjectionDirty = true;
    };
}
