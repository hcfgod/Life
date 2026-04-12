#pragma once

#include "Editor/EditorServices.h"
#include "Engine.h"

#include <functional>
#include <string>
#include <vector>

namespace EditorApp
{
    struct EditorComponentDescriptor
    {
        std::string Id;
        std::string DisplayName;
        bool Removable = true;
        std::function<bool(const Life::Entity&)> HasComponent;
        std::function<bool(const Life::Entity&)> CanAddComponent;
        std::function<void(Life::Entity&)> AddComponent;
        std::function<bool(Life::Entity&)> RemoveComponent;
        std::function<bool(Life::Entity&, const EditorServices&)> DrawInspector;
    };

    class EditorComponentRegistry final
    {
    public:
        static EditorComponentRegistry& Get();

        void Register(EditorComponentDescriptor descriptor);
        const std::vector<EditorComponentDescriptor>& GetDescriptors() const noexcept;

    private:
        EditorComponentRegistry();

        std::vector<EditorComponentDescriptor> m_Descriptors;
    };
}
