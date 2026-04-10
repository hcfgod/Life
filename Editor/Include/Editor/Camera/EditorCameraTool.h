#pragma once

#include "Engine.h"

namespace EditorApp
{
    class EditorCameraTool
    {
    public:
        struct FlyCameraInput
        {
            glm::vec2 LookDelta{ 0.0f, 0.0f };
            glm::vec3 MoveAxes{ 0.0f, 0.0f, 0.0f };
            bool Boost = false;
        };

        explicit EditorCameraTool(std::string cameraName = "EditorSceneCamera");

        void Ensure(Life::CameraManager& cameraManager, float aspectRatio);
        void Release(Life::CameraManager& cameraManager) noexcept;
        void UpdateAspectRatio(float aspectRatio);
        void UpdateFlyCamera(Life::Camera& camera, const FlyCameraInput& input, float timestep);
        Life::OptionalRef<Life::Camera> TryGetCamera(Life::CameraManager& cameraManager);
        Life::OptionalRef<const Life::Camera> TryGetCamera(const Life::CameraManager& cameraManager) const;
        const std::string& GetCameraName() const noexcept;

    private:
        void ApplyDefaults(Life::Camera& camera, float aspectRatio) const;
        void SyncAnglesFromCamera(const Life::Camera& camera);

        std::string m_CameraName;
        bool m_OwnsCamera = false;
        bool m_HasOrientationState = false;
        float m_YawRadians = 0.0f;
        float m_PitchRadians = 0.0f;
    };
}
