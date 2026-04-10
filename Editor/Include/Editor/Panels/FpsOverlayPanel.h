#pragma once

namespace EditorApp
{
    class FpsOverlayPanel
    {
    public:
        void Update(float timestep) noexcept;
        void Render(bool& isOpen) const;

    private:
        float m_SmoothedFrameSeconds = 1.0f / 60.0f;
        float m_LastFrameSeconds = 1.0f / 60.0f;
    };
}
