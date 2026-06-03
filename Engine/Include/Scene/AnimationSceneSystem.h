#pragma once

#include "Scene/SceneRuntime.h"

namespace Life
{
    class AnimationSceneSystem final : public ISceneSystem
    {
    public:
        void OnSceneStart(Scene& scene) override;
        void OnSceneUpdate(Scene& scene, float timestep) override;
    };
}
