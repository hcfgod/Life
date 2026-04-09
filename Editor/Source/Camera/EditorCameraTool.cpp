#include "Editor/Camera/EditorCameraTool.h"

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
        cameraSpecification.Projection = Life::ProjectionType::Orthographic;
        cameraSpecification.AspectRatio = aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f;
        cameraSpecification.OrthoSize = 4.5f;
        cameraSpecification.OrthoNear = 0.1f;
        cameraSpecification.OrthoFar = 10.0f;
        cameraSpecification.ClearColor = { 0.08f, 0.08f, 0.12f, 1.0f };

        const bool alreadyExists = cameraManager.HasCamera(m_CameraName);
        Life::Camera& camera = cameraManager.EnsureCamera(cameraSpecification);
        if (!alreadyExists)
            m_OwnsCamera = true;

        ApplyDefaults(camera, cameraSpecification.AspectRatio);
        cameraManager.SetPrimaryCamera(m_CameraName);
    }

    void EditorCameraTool::Release(Life::CameraManager& cameraManager) noexcept
    {
        if (m_OwnsCamera)
            cameraManager.DestroyCamera(m_CameraName);

        m_OwnsCamera = false;
    }

    void EditorCameraTool::UpdateAspectRatio(float aspectRatio)
    {
        (void)aspectRatio;
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
        Life::OrthographicProjectionParameters orthographicParameters;
        orthographicParameters.Size = 4.5f;
        orthographicParameters.NearClip = 0.1f;
        orthographicParameters.FarClip = 10.0f;
        camera.SetOrthographic(orthographicParameters);
        camera.SetAspectRatio(aspectRatio > 0.0f ? aspectRatio : 16.0f / 9.0f);
        camera.SetPosition({ 0.0f, 0.0f, 1.0f });
        camera.LookAt({ 0.0f, 0.0f, 0.0f });
    }
}
