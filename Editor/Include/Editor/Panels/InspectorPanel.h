#pragma once

#include "Engine.h"

namespace EditorApp
{
    class EditorCameraTool;
    struct EditorServices;

    class InspectorPanel
    {
    public:
        void Render(bool& isOpen, const EditorServices& services, const EditorCameraTool& cameraTool) const;
    };
}
