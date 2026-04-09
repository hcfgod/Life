#pragma once

#include "Engine.h"

namespace EditorApp
{
    class HierarchyPanel
    {
    public:
        void Render(bool& isOpen, const Life::Application& application) const;
    };
}
