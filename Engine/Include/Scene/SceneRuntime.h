#pragma once

#include "Core/Memory.h"

#include <vector>

namespace Life
{
    class Scene;

    class ISceneSystem
    {
    public:
        virtual ~ISceneSystem() = default;
        virtual void OnSceneStart(Scene& scene) { (void)scene; }
        virtual void OnSceneUpdate(Scene& scene, float timestep) { (void)scene; (void)timestep; }
        virtual void OnSceneStop(Scene& scene) { (void)scene; }
    };

    class SceneRuntime final
    {
    public:
        SceneRuntime() = default;
        ~SceneRuntime();

        SceneRuntime(const SceneRuntime&) = delete;
        SceneRuntime& operator=(const SceneRuntime&) = delete;

        void RegisterSystem(Ref<ISceneSystem> system);
        void ClearSystems();

        bool Start(Scene& scene);
        bool Update(Scene& scene, float timestep);
        bool Update(float timestep);
        bool Stop(Scene& scene);
        bool Stop();

        void SetPaused(bool paused) noexcept { m_Paused = paused; }
        bool IsPaused() const noexcept { return m_Paused; }
        void RequestStep() noexcept { m_StepRequested = true; }
        bool IsRunning() const noexcept { return m_Running; }
        Scene* GetActiveScene() noexcept { return m_ActiveScene; }
        const Scene* GetActiveScene() const noexcept { return m_ActiveScene; }

    private:
        bool ShouldTick() noexcept;

        std::vector<Ref<ISceneSystem>> m_Systems;
        Scene* m_ActiveScene = nullptr;
        bool m_Running = false;
        bool m_Paused = false;
        bool m_StepRequested = false;
    };
}
