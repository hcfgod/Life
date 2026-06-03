#include "Scene/SceneRuntime.h"

#include "Core/Log.h"
#include "Scene/Scene.h"

namespace Life
{
    SceneRuntime::~SceneRuntime()
    {
        (void)Stop();
    }

    void SceneRuntime::RegisterSystem(Ref<ISceneSystem> system)
    {
        if (system)
            m_Systems.push_back(std::move(system));
    }

    void SceneRuntime::ClearSystems()
    {
        m_Systems.clear();
    }

    bool SceneRuntime::Start(Scene& scene)
    {
        if (m_Running)
            (void)Stop();

        m_ActiveScene = &scene;
        m_Running = true;
        m_Paused = false;
        m_StepRequested = false;

        for (const Ref<ISceneSystem>& system : m_Systems)
        {
            if (system)
                system->OnSceneStart(scene);
        }

        return true;
    }

    bool SceneRuntime::ShouldTick() noexcept
    {
        if (!m_Running)
            return false;

        if (!m_Paused)
            return true;

        if (!m_StepRequested)
            return false;

        m_StepRequested = false;
        return true;
    }

    bool SceneRuntime::Update(Scene& scene, float timestep)
    {
        if (m_ActiveScene != &scene)
            m_ActiveScene = &scene;

        if (!ShouldTick())
            return false;

        for (const Ref<ISceneSystem>& system : m_Systems)
        {
            if (system)
                system->OnSceneUpdate(scene, timestep);
        }

        return true;
    }

    bool SceneRuntime::Update(float timestep)
    {
        if (m_ActiveScene == nullptr)
            return false;

        return Update(*m_ActiveScene, timestep);
    }

    bool SceneRuntime::Stop(Scene& scene)
    {
        if (!m_Running)
            return false;

        Scene* stoppedScene = m_ActiveScene != nullptr ? m_ActiveScene : &scene;
        for (auto it = m_Systems.rbegin(); it != m_Systems.rend(); ++it)
        {
            if (*it)
                (*it)->OnSceneStop(*stoppedScene);
        }

        m_ActiveScene = nullptr;
        m_Running = false;
        m_Paused = false;
        m_StepRequested = false;
        return true;
    }

    bool SceneRuntime::Stop()
    {
        if (m_ActiveScene == nullptr)
            return false;

        return Stop(*m_ActiveScene);
    }
}
