#pragma once

#include "Editor/Scene/EditorSceneState.h"
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

        struct Orthographic2DState
        {
            glm::vec2 Center{ 0.0f, 0.0f };
            float Size = 5.0f;
        };

        explicit EditorCameraTool(std::string cameraName = "EditorSceneCamera");

        void Ensure(Life::CameraManager& cameraManager, float aspectRatio);
        void Release(Life::CameraManager& cameraManager) noexcept;
        void UpdateAspectRatio(float aspectRatio);
        void ApplySceneViewMode(Life::Camera& camera, EditorSceneViewMode mode, float aspectRatio);
        void UpdateFlyCamera(Life::Camera& camera, const FlyCameraInput& input, float timestep);
        void Pan2D(Life::Camera& camera, const glm::vec2& worldDelta);
        void Set2DOrthographicSize(Life::Camera& camera, float size);
        void FrameBounds(Life::Camera& camera, EditorSceneViewMode mode, const glm::vec3& center, float radius);
        void FrameBounds(Life::Camera& camera, const glm::vec3& center, float radius);
        Life::OptionalRef<Life::Camera> TryGetCamera(Life::CameraManager& cameraManager);
        Life::OptionalRef<const Life::Camera> TryGetCamera(const Life::CameraManager& cameraManager) const;
        const std::string& GetCameraName() const noexcept;

    private:
        struct Perspective3DState
        {
            glm::vec3 Position{ 0.0f, 2.5f, 10.0f };
            glm::quat Orientation{ 1.0f, 0.0f, 0.0f, 0.0f };
            float YawRadians = 0.0f;
            float PitchRadians = 0.0f;
            bool Valid = false;
        };

        void ApplyDefaults(Life::Camera& camera, float aspectRatio) const;
        void Apply2DState(Life::Camera& camera, float aspectRatio);
        void Apply3DState(Life::Camera& camera, float aspectRatio);
        void SaveActiveState(const Life::Camera& camera);
        void SyncAnglesFromCamera(const Life::Camera& camera);
        Orthographic2DState Resolve2DStateFromCamera(const Life::Camera& camera) const;

        std::string m_CameraName;
        bool m_OwnsCamera = false;
        bool m_HasActiveViewMode = false;
        EditorSceneViewMode m_ActiveViewMode = EditorSceneViewMode::ThreeD;
        Orthographic2DState m_2DState;
        Perspective3DState m_3DState;
        bool m_HasOrientationState = false;
        float m_YawRadians = 0.0f;
        float m_PitchRadians = 0.0f;
    };
}
