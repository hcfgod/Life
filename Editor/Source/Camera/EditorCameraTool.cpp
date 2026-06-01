#include "Editor/Camera/EditorCameraTool.h"

#include <glm/ext/quaternion_common.hpp>
#include <glm/ext/quaternion_trigonometric.hpp>
#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>

namespace
{
    constexpr float PerspectiveFieldOfViewDegrees = 60.0f;
    constexpr float PerspectiveNearClip = 0.1f;
    constexpr float PerspectiveFarClip = 500.0f;
    constexpr float LookSensitivity = 0.0025f;
    constexpr float BaseMoveSpeed = 7.5f;
    constexpr float BoostMultiplier = 3.0f;
    constexpr float MaxPitchRadians = 1.55334306f;
    constexpr float Orthographic2DNearClip = -1000.0f;
    constexpr float Orthographic2DFarClip = 1000.0f;
    constexpr float Orthographic2DCameraZ = 10.0f;
    constexpr float MinOrthographic2DSize = 0.05f;
    constexpr float MaxOrthographic2DSize = 10000.0f;

    float ClampOrthographic2DSize(float size) noexcept
    {
        return std::clamp(size, MinOrthographic2DSize, MaxOrthographic2DSize);
    }
}

namespace EditorApp
{
    EditorCameraTool::EditorCameraTool(std::string cameraName)
        : m_CameraName(std::move(cameraName))
    {
    }

    void EditorCameraTool::Ensure(Life::CameraManager& cameraManager, float aspectRatio)
    {
        Life::CameraSpecification cameraSpecification;
        cameraSpecification.Name = m_CameraName;
        cameraSpecification.Projection = Life::ProjectionType::Perspective;
        cameraSpecification.AspectRatio = aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f;
        cameraSpecification.FieldOfView = PerspectiveFieldOfViewDegrees;
        cameraSpecification.NearClip = PerspectiveNearClip;
        cameraSpecification.FarClip = PerspectiveFarClip;
        cameraSpecification.ClearColor = { 0.08f, 0.08f, 0.12f, 1.0f };

        const bool alreadyExists = cameraManager.HasCamera(m_CameraName);
        Life::Camera& camera = cameraManager.EnsureCamera(cameraSpecification);
        if (!alreadyExists)
        {
            m_OwnsCamera = true;
            ApplyDefaults(camera, cameraSpecification.AspectRatio);
            SyncAnglesFromCamera(camera);
        }
        else
        {
            camera.SetAspectRatio(cameraSpecification.AspectRatio);
            SyncAnglesFromCamera(camera);
        }

        cameraManager.SetPrimaryCamera(m_CameraName);
    }

    void EditorCameraTool::Release(Life::CameraManager& cameraManager) noexcept
    {
        if (m_OwnsCamera)
            cameraManager.DestroyCamera(m_CameraName);

        m_OwnsCamera = false;
        m_HasActiveViewMode = false;
        m_ActiveViewMode = EditorSceneViewMode::ThreeD;
        m_2DState = {};
        m_3DState = {};
        m_HasOrientationState = false;
        m_YawRadians = 0.0f;
        m_PitchRadians = 0.0f;
    }

    void EditorCameraTool::UpdateAspectRatio(float aspectRatio)
    {
        (void)aspectRatio;
    }

    void EditorCameraTool::ApplySceneViewMode(Life::Camera& camera, EditorSceneViewMode mode, float aspectRatio)
    {
        if (!m_HasActiveViewMode)
        {
            m_HasActiveViewMode = true;
            m_ActiveViewMode = mode;
        }
        else if (m_ActiveViewMode != mode)
        {
            SaveActiveState(camera);
            m_ActiveViewMode = mode;
        }

        if (mode == EditorSceneViewMode::TwoD)
            Apply2DState(camera, aspectRatio);
        else
            Apply3DState(camera, aspectRatio);
    }

    void EditorCameraTool::UpdateFlyCamera(Life::Camera& camera, const FlyCameraInput& input, float timestep)
    {
        if (!m_HasOrientationState)
            SyncAnglesFromCamera(camera);

        m_YawRadians -= input.LookDelta.x * LookSensitivity;
        m_PitchRadians = std::clamp(m_PitchRadians + input.LookDelta.y * LookSensitivity, -MaxPitchRadians, MaxPitchRadians);

        const glm::quat yawRotation = glm::angleAxis(m_YawRadians, glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::quat pitchRotation = glm::angleAxis(m_PitchRadians, glm::vec3(1.0f, 0.0f, 0.0f));
        const glm::quat orientation = glm::normalize(yawRotation * pitchRotation);
        camera.SetOrientation(orientation);

        glm::vec3 moveAxes = input.MoveAxes;
        const float moveLength = glm::length(moveAxes);
        if (moveLength > 1.0f)
            moveAxes /= moveLength;

        const glm::vec3 forward = orientation * glm::vec3(0.0f, 0.0f, -1.0f);
        const glm::vec3 right = orientation * glm::vec3(1.0f, 0.0f, 0.0f);
        const glm::vec3 up(0.0f, 1.0f, 0.0f);

        const float speed = BaseMoveSpeed * (input.Boost ? BoostMultiplier : 1.0f);
        const glm::vec3 delta = ((right * moveAxes.x) + (up * moveAxes.y) + (forward * moveAxes.z)) * speed * std::max(timestep, 0.0f);
        if (glm::length(delta) > 0.0f)
            camera.SetPosition(camera.GetPosition() + delta);

        m_3DState.Position = camera.GetPosition();
        m_3DState.Orientation = camera.GetOrientation();
        m_3DState.YawRadians = m_YawRadians;
        m_3DState.PitchRadians = m_PitchRadians;
        m_3DState.Valid = true;
    }

    void EditorCameraTool::Pan2D(Life::Camera& camera, const glm::vec2& worldDelta)
    {
        m_2DState = Resolve2DStateFromCamera(camera);
        m_2DState.Center += worldDelta;
        Apply2DState(camera, camera.GetAspectRatio());
    }

    void EditorCameraTool::Set2DOrthographicSize(Life::Camera& camera, float size)
    {
        m_2DState = Resolve2DStateFromCamera(camera);
        m_2DState.Size = ClampOrthographic2DSize(size);
        Apply2DState(camera, camera.GetAspectRatio());
    }

    void EditorCameraTool::FrameBounds(Life::Camera& camera, EditorSceneViewMode mode, const glm::vec3& center, float radius)
    {
        const float safeRadius = std::max(radius, 0.5f);
        if (mode == EditorSceneViewMode::TwoD)
        {
            m_2DState.Center = { center.x, center.y };
            m_2DState.Size = ClampOrthographic2DSize(safeRadius * 1.35f);
            Apply2DState(camera, camera.GetAspectRatio());
        }
        else
        {
            const glm::vec3 forward = glm::normalize(camera.GetOrientation() * glm::vec3(0.0f, 0.0f, -1.0f));
            const float halfFovRadians = glm::radians(camera.GetFieldOfView()) * 0.5f;
            const float distance = safeRadius / std::max(std::tan(halfFovRadians), 0.01f);
            camera.SetPosition(center - forward * (distance * 1.35f));
            m_3DState.Position = camera.GetPosition();
            m_3DState.Orientation = camera.GetOrientation();
            m_3DState.YawRadians = m_YawRadians;
            m_3DState.PitchRadians = m_PitchRadians;
            m_3DState.Valid = true;
        }
    }

    void EditorCameraTool::FrameBounds(Life::Camera& camera, const glm::vec3& center, float radius)
    {
        FrameBounds(camera, camera.GetProjectionType() == Life::ProjectionType::Orthographic ? EditorSceneViewMode::TwoD : EditorSceneViewMode::ThreeD, center, radius);
    }

    Life::OptionalRef<Life::Camera> EditorCameraTool::TryGetCamera(Life::CameraManager& cameraManager)
    {
        if (Life::Camera* camera = cameraManager.GetCamera(m_CameraName))
            return Life::MakeOptionalRef(*camera);

        return Life::NullOpt;
    }

    Life::OptionalRef<const Life::Camera> EditorCameraTool::TryGetCamera(const Life::CameraManager& cameraManager) const
    {
        if (const Life::Camera* camera = cameraManager.GetCamera(m_CameraName))
            return Life::MakeOptionalRef(*camera);

        return Life::NullOpt;
    }

    const std::string& EditorCameraTool::GetCameraName() const noexcept
    {
        return m_CameraName;
    }

    void EditorCameraTool::ApplyDefaults(Life::Camera& camera, float aspectRatio) const
    {
        Life::PerspectiveProjectionParameters perspectiveParameters;
        perspectiveParameters.VerticalFieldOfViewDegrees = PerspectiveFieldOfViewDegrees;
        perspectiveParameters.NearClip = PerspectiveNearClip;
        perspectiveParameters.FarClip = PerspectiveFarClip;
        camera.SetPerspective(perspectiveParameters);
        camera.SetAspectRatio(aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f);
        camera.SetPosition({ 0.0f, 2.5f, 10.0f });
        camera.LookAt({ 0.0f, 0.0f, 0.0f });
    }

    void EditorCameraTool::Apply2DState(Life::Camera& camera, float aspectRatio)
    {
        Life::OrthographicProjectionParameters parameters;
        parameters.Size = ClampOrthographic2DSize(m_2DState.Size);
        parameters.NearClip = Orthographic2DNearClip;
        parameters.FarClip = Orthographic2DFarClip;
        camera.SetOrthographic(parameters);
        camera.SetAspectRatio(aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f);
        camera.SetTransform(
            { m_2DState.Center.x, m_2DState.Center.y, Orthographic2DCameraZ },
            glm::quat(1.0f, 0.0f, 0.0f, 0.0f));
        m_2DState.Size = parameters.Size;
    }

    void EditorCameraTool::Apply3DState(Life::Camera& camera, float aspectRatio)
    {
        Life::PerspectiveProjectionParameters perspectiveParameters;
        perspectiveParameters.VerticalFieldOfViewDegrees = PerspectiveFieldOfViewDegrees;
        perspectiveParameters.NearClip = PerspectiveNearClip;
        perspectiveParameters.FarClip = PerspectiveFarClip;
        camera.SetPerspective(perspectiveParameters);
        camera.SetAspectRatio(aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f);

        if (!m_3DState.Valid)
        {
            ApplyDefaults(camera, aspectRatio);
            SyncAnglesFromCamera(camera);
            m_3DState.Position = camera.GetPosition();
            m_3DState.Orientation = camera.GetOrientation();
            m_3DState.YawRadians = m_YawRadians;
            m_3DState.PitchRadians = m_PitchRadians;
            m_3DState.Valid = true;
            return;
        }

        camera.SetTransform(m_3DState.Position, m_3DState.Orientation);
        m_YawRadians = m_3DState.YawRadians;
        m_PitchRadians = m_3DState.PitchRadians;
        m_HasOrientationState = true;
    }

    void EditorCameraTool::SaveActiveState(const Life::Camera& camera)
    {
        if (m_ActiveViewMode == EditorSceneViewMode::TwoD)
        {
            m_2DState = Resolve2DStateFromCamera(camera);
        }
        else
        {
            m_3DState.Position = camera.GetPosition();
            m_3DState.Orientation = camera.GetOrientation();
            m_3DState.YawRadians = m_YawRadians;
            m_3DState.PitchRadians = m_PitchRadians;
            m_3DState.Valid = true;
        }
    }

    void EditorCameraTool::SyncAnglesFromCamera(const Life::Camera& camera)
    {
        const glm::vec3 forward = glm::normalize(camera.GetOrientation() * glm::vec3(0.0f, 0.0f, -1.0f));
        m_YawRadians = std::atan2(-forward.x, -forward.z);
        m_PitchRadians = std::asin(std::clamp(forward.y, -1.0f, 1.0f));
        m_HasOrientationState = true;
    }

    EditorCameraTool::Orthographic2DState EditorCameraTool::Resolve2DStateFromCamera(const Life::Camera& camera) const
    {
        Orthographic2DState state;
        state.Center = { camera.GetPosition().x, camera.GetPosition().y };
        state.Size = camera.GetProjectionType() == Life::ProjectionType::Orthographic
            ? ClampOrthographic2DSize(camera.GetOrthographicSize())
            : m_2DState.Size;
        return state;
    }
}
