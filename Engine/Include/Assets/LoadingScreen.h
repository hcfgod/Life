#pragma once

#include <string>

namespace Life
{
    class Scene;
}

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // LoadingScreen
    // Aggregates asset load progress, scene load state, and shader readiness to
    // drive loading screen UI overlays.
    // -----------------------------------------------------------------------------
    class LoadingScreen final
    {
    public:
        struct Context
        {
            bool SceneLoading = false;
            bool SceneObjectsReady = true;
            bool PhysicsWorldReady = true;
            bool ShaderReady = true;
            const char* ShaderProgressKey = nullptr;
        };

        struct State
        {
            bool IsLoading = false;
            float Progress = 1.0f;
            std::string StatusText;
        };

        static Context BuildContext(const Scene* scene, bool shaderReady, const char* shaderProgressKey = nullptr);
        static State GetState(const Context& context);
    };
}
