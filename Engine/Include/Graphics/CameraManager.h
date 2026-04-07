#pragma once

#include "Graphics/Camera.h"
#include "Core/Memory.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace Life
{
    class CameraManager
    {
    public:
        CameraManager() = default;
        ~CameraManager() = default;

        CameraManager(const CameraManager&) = delete;
        CameraManager& operator=(const CameraManager&) = delete;
        CameraManager(CameraManager&&) = delete;
        CameraManager& operator=(CameraManager&&) = delete;

        Camera* CreateCamera(const CameraSpecification& specification);
        Camera& EnsureCamera(const CameraSpecification& specification);
        bool DestroyCamera(const std::string& name);

        Camera* GetCamera(const std::string& name);
        const Camera* GetCamera(const std::string& name) const;

        Camera* GetPrimaryCamera();
        const Camera* GetPrimaryCamera() const;
        bool SetPrimaryCamera(const std::string& name);
        void ClearPrimaryCamera();

        std::vector<Camera*> GetCamerasByPriority();
        std::vector<const Camera*> GetCamerasByPriority() const;

        void SetAspectRatioAll(float aspectRatio);
        std::size_t GetCameraCount() const noexcept;
        bool HasCamera(const std::string& name) const;
        void Clear();

    private:
        using CameraMap = std::unordered_map<std::string, Scope<Camera>>;

        template<typename TCameraPtr>
        static void SortCameraPointersByPriority(std::vector<TCameraPtr>& cameras);

        CameraMap m_Cameras;
        std::string m_PrimaryCameraName;
    };
}
