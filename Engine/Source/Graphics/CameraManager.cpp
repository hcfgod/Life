#include "Core/LifePCH.h"
#include "Graphics/CameraManager.h"

#include <stdexcept>

namespace Life
{
    Camera* CameraManager::CreateCamera(const CameraSpecification& specification)
    {
        const std::string& name = specification.Name;
        if (name.empty())
            return nullptr;

        auto camera = CreateScope<Camera>(specification);
        Camera* cameraPtr = camera.get();
        m_Cameras[name] = std::move(camera);

        if (m_PrimaryCameraName.empty())
            m_PrimaryCameraName = name;

        return cameraPtr;
    }

    Camera& CameraManager::EnsureCamera(const CameraSpecification& specification)
    {
        const std::string& name = specification.Name;
        if (name.empty())
            throw std::invalid_argument("CameraManager::EnsureCamera requires a non-empty camera name.");

        if (Camera* existingCamera = GetCamera(name))
            return *existingCamera;

        Camera* createdCamera = CreateCamera(specification);
        if (createdCamera == nullptr)
            throw std::runtime_error("CameraManager::EnsureCamera failed to create the requested camera.");

        return *createdCamera;
    }

    bool CameraManager::DestroyCamera(const std::string& name)
    {
        const auto it = m_Cameras.find(name);
        if (it == m_Cameras.end())
            return false;

        m_Cameras.erase(it);
        if (m_PrimaryCameraName == name)
            m_PrimaryCameraName.clear();

        return true;
    }

    Camera* CameraManager::GetCamera(const std::string& name)
    {
        const auto it = m_Cameras.find(name);
        return it != m_Cameras.end() ? it->second.get() : nullptr;
    }

    const Camera* CameraManager::GetCamera(const std::string& name) const
    {
        const auto it = m_Cameras.find(name);
        return it != m_Cameras.end() ? it->second.get() : nullptr;
    }

    Camera* CameraManager::GetPrimaryCamera()
    {
        if (!m_PrimaryCameraName.empty())
        {
            if (Camera* primaryCamera = GetCamera(m_PrimaryCameraName))
                return primaryCamera;
        }

        auto cameras = GetCamerasByPriority();
        return cameras.empty() ? nullptr : cameras.back();
    }

    const Camera* CameraManager::GetPrimaryCamera() const
    {
        if (!m_PrimaryCameraName.empty())
        {
            if (const Camera* primaryCamera = GetCamera(m_PrimaryCameraName))
                return primaryCamera;
        }

        auto cameras = GetCamerasByPriority();
        return cameras.empty() ? nullptr : cameras.back();
    }

    bool CameraManager::SetPrimaryCamera(const std::string& name)
    {
        if (!HasCamera(name))
            return false;

        m_PrimaryCameraName = name;
        return true;
    }

    void CameraManager::ClearPrimaryCamera()
    {
        m_PrimaryCameraName.clear();
    }

    std::vector<Camera*> CameraManager::GetCamerasByPriority()
    {
        std::vector<Camera*> cameras;
        cameras.reserve(m_Cameras.size());

        for (auto& [name, camera] : m_Cameras)
            cameras.push_back(camera.get());

        SortCameraPointersByPriority(cameras);
        return cameras;
    }

    std::vector<const Camera*> CameraManager::GetCamerasByPriority() const
    {
        std::vector<const Camera*> cameras;
        cameras.reserve(m_Cameras.size());

        for (const auto& [name, camera] : m_Cameras)
            cameras.push_back(camera.get());

        SortCameraPointersByPriority(cameras);
        return cameras;
    }

    void CameraManager::SetAspectRatioAll(float aspectRatio)
    {
        for (auto& [name, camera] : m_Cameras)
            camera->SetAspectRatio(aspectRatio);
    }

    std::size_t CameraManager::GetCameraCount() const noexcept
    {
        return m_Cameras.size();
    }

    bool CameraManager::HasCamera(const std::string& name) const
    {
        return m_Cameras.find(name) != m_Cameras.end();
    }

    void CameraManager::Clear()
    {
        m_Cameras.clear();
        m_PrimaryCameraName.clear();
    }

    template<typename TCameraPtr>
    void CameraManager::SortCameraPointersByPriority(std::vector<TCameraPtr>& cameras)
    {
        std::sort(cameras.begin(), cameras.end(), [](const TCameraPtr& left, const TCameraPtr& right)
        {
            if (left->GetPriority() != right->GetPriority())
                return left->GetPriority() < right->GetPriority();

            return left->GetName() < right->GetName();
        });
    }

    template void CameraManager::SortCameraPointersByPriority(std::vector<Camera*>& cameras);
    template void CameraManager::SortCameraPointersByPriority(std::vector<const Camera*>& cameras);
}
