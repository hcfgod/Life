#pragma once

namespace Life
{
    // -----------------------------------------------------------------------------
    // Scene
    // Skeleton placeholder for the scene system. Provides enough surface area for
    // LoadingScreen to compile. A real implementation will replace this once the
    // scene system is built out.
    // -----------------------------------------------------------------------------
    class Scene
    {
    public:
        Scene() = default;
        virtual ~Scene() = default;

        enum class State : uint8_t
        {
            Unloaded = 0,
            Loading = 1,
            Ready = 2
        };

        State GetState() const { return m_State; }
        bool IsLoading() const { return m_State == State::Loading; }
        bool IsReady() const { return m_State == State::Ready; }

    protected:
        State m_State = State::Unloaded;
    };
}
