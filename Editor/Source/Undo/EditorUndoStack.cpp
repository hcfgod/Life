#include "Editor/Undo/EditorUndoStack.h"

#include <utility>

namespace EditorApp
{
    SetEntityTransformCommand::SetEntityTransformCommand(std::string entityId,
                                                         Life::TransformComponent before,
                                                         Life::TransformComponent after)
        : m_EntityId(std::move(entityId))
        , m_Before(before)
        , m_After(after)
    {
    }

    bool SetEntityTransformCommand::Undo(Life::Scene& scene)
    {
        return Apply(scene, m_EntityId, m_Before);
    }

    bool SetEntityTransformCommand::Redo(Life::Scene& scene)
    {
        return Apply(scene, m_EntityId, m_After);
    }

    bool SetEntityTransformCommand::Apply(Life::Scene& scene, const std::string& entityId, const Life::TransformComponent& transform)
    {
        Life::Entity entity = scene.FindEntityById(entityId);
        if (!entity.IsValid())
            return false;

        entity.GetComponent<Life::TransformComponent>() = transform;
        return true;
    }

    bool EditorUndoStack::Execute(std::unique_ptr<EditorCommand> command, Life::Scene& scene)
    {
        if (!command || !command->Redo(scene))
            return false;

        m_Undo.push_back(std::move(command));
        m_Redo.clear();
        return true;
    }

    void EditorUndoStack::CommitExecuted(std::unique_ptr<EditorCommand> command)
    {
        if (!command)
            return;

        m_Undo.push_back(std::move(command));
        m_Redo.clear();
    }

    bool EditorUndoStack::Undo(Life::Scene& scene)
    {
        if (m_Undo.empty())
            return false;

        std::unique_ptr<EditorCommand> command = std::move(m_Undo.back());
        m_Undo.pop_back();
        if (!command->Undo(scene))
            return false;

        m_Redo.push_back(std::move(command));
        return true;
    }

    bool EditorUndoStack::Redo(Life::Scene& scene)
    {
        if (m_Redo.empty())
            return false;

        std::unique_ptr<EditorCommand> command = std::move(m_Redo.back());
        m_Redo.pop_back();
        if (!command->Redo(scene))
            return false;

        m_Undo.push_back(std::move(command));
        return true;
    }

    bool EditorUndoStack::CanUndo() const noexcept
    {
        return !m_Undo.empty();
    }

    bool EditorUndoStack::CanRedo() const noexcept
    {
        return !m_Redo.empty();
    }

    void EditorUndoStack::Clear() noexcept
    {
        m_Undo.clear();
        m_Redo.clear();
    }
}
