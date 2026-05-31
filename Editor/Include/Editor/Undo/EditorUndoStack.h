#pragma once

#include "Engine.h"

#include <memory>
#include <string>
#include <vector>

namespace EditorApp
{
    class EditorCommand
    {
    public:
        virtual ~EditorCommand() = default;

        virtual bool Undo(Life::Scene& scene) = 0;
        virtual bool Redo(Life::Scene& scene) = 0;
    };

    class SetEntityTransformCommand final : public EditorCommand
    {
    public:
        SetEntityTransformCommand(std::string entityId,
                                  Life::TransformComponent before,
                                  Life::TransformComponent after);

        bool Undo(Life::Scene& scene) override;
        bool Redo(Life::Scene& scene) override;

    private:
        static bool Apply(Life::Scene& scene, const std::string& entityId, const Life::TransformComponent& transform);

        std::string m_EntityId;
        Life::TransformComponent m_Before;
        Life::TransformComponent m_After;
    };

    class EditorUndoStack final
    {
    public:
        bool Execute(std::unique_ptr<EditorCommand> command, Life::Scene& scene);
        void CommitExecuted(std::unique_ptr<EditorCommand> command);
        bool Undo(Life::Scene& scene);
        bool Redo(Life::Scene& scene);
        bool CanUndo() const noexcept;
        bool CanRedo() const noexcept;
        void Clear() noexcept;

    private:
        std::vector<std::unique_ptr<EditorCommand>> m_Undo;
        std::vector<std::unique_ptr<EditorCommand>> m_Redo;
    };
}
