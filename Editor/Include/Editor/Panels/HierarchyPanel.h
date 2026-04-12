#pragma once

#include "Editor/Scene/EditorSceneState.h"
#include "Engine.h"

namespace EditorApp
{
    struct EditorServices;

    class HierarchyPanel
    {
    public:
        void Render(bool& isOpen, const EditorServices& services, EditorSceneState& sceneState) const;
    };
}
