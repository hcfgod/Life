#pragma once

#include "Scene/SceneRuntime.h"

namespace Life
{
    class AudioSceneSystem final : public ISceneSystem
    {
    public:
        void OnSceneStart(Scene& scene) override;
        void OnSceneUpdate(Scene& scene, float timestep) override;
        void OnSceneStop(Scene& scene) override;
    };
}
