#include "Core/LifePCH.h"
#include "Graphics/Camera.h"

#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_float.hpp>
#include <glm/ext/quaternion_geometric.hpp>
#include <glm/ext/quaternion_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <limits>

namespace Life
{
    namespace
    {
        constexpr glm::vec3 s_DefaultUp(0.0f, 1.0f, 0.0f);
    }

    Camera::Camera(const CameraSpecification& specification)
        : m_Specification(specification)
    {
        if (m_Specification.AspectRatio <= 0.0f)
            m_Specification.AspectRatio = 16.0f / 9.0f;
    }

    void Camera::SetName(std::string name)
    {
        m_Specification.Name = std::move(name);
    }

    void Camera::SetPosition(const glm::vec3& position)
    {
        m_Position = position;
        InvalidateView();
    }

    void Camera::SetOrientation(const glm::quat& orientation)
    {
        m_Orientation = glm::normalize(orientation);
        InvalidateView();
    }

    void Camera::SetTransform(const glm::vec3& position, const glm::quat& orientation)
    {
        m_Position = position;
        m_Orientation = glm::normalize(orientation);
        InvalidateView();
    }

    void Camera::LookAt(const glm::vec3& target, const glm::vec3& up)
    {
        const glm::vec3 forward = target - m_Position;
        if (glm::dot(forward, forward) <= std::numeric_limits<float>::epsilon())
            return;

        const glm::vec3 normalizedForward = glm::normalize(forward);
        const glm::vec3 normalizedUp = glm::dot(up, up) > std::numeric_limits<float>::epsilon()
            ? glm::normalize(up)
            : s_DefaultUp;

        const glm::mat4 lookAtMatrix = glm::inverse(glm::lookAtRH(m_Position, m_Position + normalizedForward, normalizedUp));
        m_Orientation = glm::normalize(glm::quat_cast(lookAtMatrix));
        InvalidateView();
    }

    void Camera::SetPerspective(float verticalFieldOfViewDegrees, float nearClip, float farClip)
    {
        m_Specification.Projection = ProjectionType::Perspective;
        m_Specification.FieldOfView = verticalFieldOfViewDegrees;
        m_Specification.NearClip = nearClip;
        m_Specification.FarClip = farClip;
        InvalidateProjection();
    }

    void Camera::SetOrthographic(float size, float nearClip, float farClip)
    {
        m_Specification.Projection = ProjectionType::Orthographic;
        m_Specification.OrthoSize = size;
        m_Specification.OrthoNear = nearClip;
        m_Specification.OrthoFar = farClip;
        InvalidateProjection();
    }

    void Camera::SetAspectRatio(float aspectRatio)
    {
        m_Specification.AspectRatio = std::max(aspectRatio, 0.0001f);
        InvalidateProjection();
    }

    void Camera::SetPriority(int32_t priority) noexcept
    {
        m_Specification.Priority = priority;
    }

    void Camera::SetClearMode(CameraClearMode clearMode) noexcept
    {
        m_Specification.ClearMode = clearMode;
    }

    void Camera::SetClearColor(const glm::vec4& clearColor) noexcept
    {
        m_Specification.ClearColor = clearColor;
    }

    void Camera::SetViewportRect(const Viewport& viewportRect)
    {
        m_Specification.ViewportRect.X = std::clamp(viewportRect.X, 0.0f, 1.0f);
        m_Specification.ViewportRect.Y = std::clamp(viewportRect.Y, 0.0f, 1.0f);
        m_Specification.ViewportRect.Width = std::clamp(viewportRect.Width, 0.0f, 1.0f - m_Specification.ViewportRect.X);
        m_Specification.ViewportRect.Height = std::clamp(viewportRect.Height, 0.0f, 1.0f - m_Specification.ViewportRect.Y);
        m_Specification.ViewportRect.MinDepth = std::clamp(viewportRect.MinDepth, 0.0f, 1.0f);
        m_Specification.ViewportRect.MaxDepth = std::clamp(viewportRect.MaxDepth, m_Specification.ViewportRect.MinDepth, 1.0f);
    }

    glm::mat4 Camera::GetViewMatrix() const
    {
        if (m_ViewDirty)
            RecalculateViewMatrix();

        return m_ViewMatrix;
    }

    glm::mat4 Camera::GetProjectionMatrix() const
    {
        if (m_ProjectionDirty)
            RecalculateProjectionMatrix();

        return m_ProjectionMatrix;
    }

    glm::mat4 Camera::GetViewProjectionMatrix() const
    {
        return GetProjectionMatrix() * GetViewMatrix();
    }

    Viewport Camera::GetPixelViewport(uint32_t framebufferWidth, uint32_t framebufferHeight) const
    {
        const Viewport& normalizedViewport = m_Specification.ViewportRect;

        Viewport pixelViewport;
        pixelViewport.X = normalizedViewport.X * static_cast<float>(framebufferWidth);
        pixelViewport.Y = normalizedViewport.Y * static_cast<float>(framebufferHeight);
        pixelViewport.Width = normalizedViewport.Width * static_cast<float>(framebufferWidth);
        pixelViewport.Height = normalizedViewport.Height * static_cast<float>(framebufferHeight);
        pixelViewport.MinDepth = normalizedViewport.MinDepth;
        pixelViewport.MaxDepth = normalizedViewport.MaxDepth;
        return pixelViewport;
    }

    void Camera::InvalidateView() noexcept
    {
        m_ViewDirty = true;
    }

    void Camera::InvalidateProjection() noexcept
    {
        m_ProjectionDirty = true;
    }

    void Camera::RecalculateViewMatrix() const
    {
        const glm::mat4 rotation = glm::mat4_cast(glm::conjugate(m_Orientation));
        const glm::mat4 translation = glm::translate(glm::mat4(1.0f), -m_Position);
        m_ViewMatrix = rotation * translation;
        m_ViewDirty = false;
    }

    void Camera::RecalculateProjectionMatrix() const
    {
        if (m_Specification.Projection == ProjectionType::Perspective)
        {
            m_ProjectionMatrix = glm::perspectiveRH_ZO(
                glm::radians(m_Specification.FieldOfView),
                std::max(m_Specification.AspectRatio, 0.0001f),
                m_Specification.NearClip,
                m_Specification.FarClip);
        }
        else
        {
            const float halfHeight = std::max(m_Specification.OrthoSize, 0.0001f);
            const float halfWidth = halfHeight * std::max(m_Specification.AspectRatio, 0.0001f);
            m_ProjectionMatrix = glm::orthoRH_ZO(
                -halfWidth,
                halfWidth,
                -halfHeight,
                halfHeight,
                m_Specification.OrthoNear,
                m_Specification.OrthoFar);
        }

        m_ProjectionMatrix[1][1] *= -1.0f;
        m_ProjectionDirty = false;
    }
}
