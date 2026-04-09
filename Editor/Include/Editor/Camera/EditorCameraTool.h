#pragma once

#include "Engine.h"

namespace EditorApp
{
    class EditorCameraTool
    {
    public:
        explicit EditorCameraTool(std::string cameraName = "EditorSceneCamera");

        void Ensure(Life::CameraManager& cameraManager, float aspectRatio);
        void Release(Life::CameraManager& cameraManager) noexcept;
        void UpdateAspectRatio(float aspectRatio);
        Life::OptionalRef<Life::Camera> TryGetCamera(Life::CameraManager& cameraManager);
        Life::OptionalRef<const Life::Camera> TryGetCamera(const Life::CameraManager& cameraManager) const;
        const std::string& GetCameraName() const noexcept;

    private:
        void ApplyDefaults(Life::Camera& camera, float aspectRatio) const;

        std::string m_CameraName;
        bool m_OwnsCamera = false;
    };
}
