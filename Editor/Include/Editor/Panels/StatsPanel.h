#pragma once

#include "Editor/Viewport/SceneViewportPanel.h"

namespace EditorApp
{
    struct EditorServices;

    class StatsPanel
    {
    public:
        void Render(bool& isOpen, const EditorServices& services, const SceneViewportState& viewportState) const;
    };
}
