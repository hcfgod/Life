#pragma once

#include "Engine.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace EditorApp
{
    struct EditorSceneState;

    struct EditorCommandContext
    {
        EditorSceneState* SceneState = nullptr;
    };

    struct EditorEntitySnapshot
    {
        std::string Id;
        std::string ParentId;
        std::string Tag = "Entity";
        bool Enabled = true;
        std::size_t SiblingIndex = 0;
        Life::TransformComponent Transform;
        std::optional<Life::CameraComponent> Camera;
        std::optional<Life::SpriteComponent> Sprite;
        std::optional<Life::SpriteRendererComponent> SpriteRenderer;
        std::vector<EditorEntitySnapshot> Children;
    };

    EditorEntitySnapshot CaptureEntitySnapshot(const Life::Entity& entity);
    EditorEntitySnapshot CreateDuplicateEntitySnapshot(const Life::Entity& entity);
    Life::Entity RestoreEntitySnapshot(Life::Scene& scene, const EditorEntitySnapshot& snapshot);
    bool DestroyEntityById(Life::Scene& scene, const std::string& entityId);

    class EditorCommand
    {
    public:
        virtual ~EditorCommand() = default;

        virtual bool Undo(Life::Scene& scene, EditorCommandContext* context) = 0;
        virtual bool Redo(Life::Scene& scene, EditorCommandContext* context) = 0;
    };

    class SetEntityTransformCommand final : public EditorCommand
    {
    public:
        SetEntityTransformCommand(std::string entityId,
                                  Life::TransformComponent before,
                                  Life::TransformComponent after);

        bool Undo(Life::Scene& scene, EditorCommandContext* context) override;
        bool Redo(Life::Scene& scene, EditorCommandContext* context) override;

    private:
        static bool Apply(Life::Scene& scene, const std::string& entityId, const Life::TransformComponent& transform);

        std::string m_EntityId;
        Life::TransformComponent m_Before;
        Life::TransformComponent m_After;
    };

    class RestoreEntitySnapshotCommand final : public EditorCommand
    {
    public:
        RestoreEntitySnapshotCommand(EditorEntitySnapshot before, EditorEntitySnapshot after);

        bool Undo(Life::Scene& scene, EditorCommandContext* context) override;
        bool Redo(Life::Scene& scene, EditorCommandContext* context) override;

    private:
        EditorEntitySnapshot m_Before;
        EditorEntitySnapshot m_After;
    };

    class CreateEntityCommand final : public EditorCommand
    {
    public:
        explicit CreateEntityCommand(EditorEntitySnapshot snapshot);

        bool Undo(Life::Scene& scene, EditorCommandContext* context) override;
        bool Redo(Life::Scene& scene, EditorCommandContext* context) override;

    private:
        EditorEntitySnapshot m_Snapshot;
    };

    class DeleteEntityCommand final : public EditorCommand
    {
    public:
        explicit DeleteEntityCommand(EditorEntitySnapshot snapshot);

        bool Undo(Life::Scene& scene, EditorCommandContext* context) override;
        bool Redo(Life::Scene& scene, EditorCommandContext* context) override;

    private:
        EditorEntitySnapshot m_Snapshot;
    };

    class DuplicateEntityCommand final : public EditorCommand
    {
    public:
        explicit DuplicateEntityCommand(EditorEntitySnapshot snapshot);

        bool Undo(Life::Scene& scene, EditorCommandContext* context) override;
        bool Redo(Life::Scene& scene, EditorCommandContext* context) override;

    private:
        EditorEntitySnapshot m_Snapshot;
    };

    class RenameEntityCommand final : public EditorCommand
    {
    public:
        RenameEntityCommand(std::string entityId, std::string before, std::string after);

        bool Undo(Life::Scene& scene, EditorCommandContext* context) override;
        bool Redo(Life::Scene& scene, EditorCommandContext* context) override;

    private:
        bool Apply(Life::Scene& scene, const std::string& tag, EditorCommandContext* context) const;

        std::string m_EntityId;
        std::string m_Before;
        std::string m_After;
    };

    class SetEntityEnabledCommand final : public EditorCommand
    {
    public:
        SetEntityEnabledCommand(std::string entityId, bool before, bool after);

        bool Undo(Life::Scene& scene, EditorCommandContext* context) override;
        bool Redo(Life::Scene& scene, EditorCommandContext* context) override;

    private:
        bool Apply(Life::Scene& scene, bool enabled, EditorCommandContext* context) const;

        std::string m_EntityId;
        bool m_Before = true;
        bool m_After = true;
    };

    class SetEntityParentCommand final : public EditorCommand
    {
    public:
        SetEntityParentCommand(std::string entityId,
                               std::string beforeParentId,
                               std::size_t beforeSiblingIndex,
                               Life::TransformComponent beforeTransform,
                               std::string afterParentId,
                               std::size_t afterSiblingIndex,
                               Life::TransformComponent afterTransform);

        bool Undo(Life::Scene& scene, EditorCommandContext* context) override;
        bool Redo(Life::Scene& scene, EditorCommandContext* context) override;

    private:
        bool Apply(Life::Scene& scene,
                   const std::string& parentId,
                   std::size_t siblingIndex,
                   const Life::TransformComponent& transform,
                   EditorCommandContext* context) const;

        std::string m_EntityId;
        std::string m_BeforeParentId;
        std::size_t m_BeforeSiblingIndex = 0;
        Life::TransformComponent m_BeforeTransform;
        std::string m_AfterParentId;
        std::size_t m_AfterSiblingIndex = 0;
        Life::TransformComponent m_AfterTransform;
    };

    class EditorUndoStack final
    {
    public:
        bool Execute(std::unique_ptr<EditorCommand> command, Life::Scene& scene);
        bool Execute(std::unique_ptr<EditorCommand> command, Life::Scene& scene, EditorSceneState& sceneState);
        void CommitExecuted(std::unique_ptr<EditorCommand> command);
        bool Undo(Life::Scene& scene);
        bool Undo(Life::Scene& scene, EditorSceneState& sceneState);
        bool Redo(Life::Scene& scene);
        bool Redo(Life::Scene& scene, EditorSceneState& sceneState);
        bool CanUndo() const noexcept;
        bool CanRedo() const noexcept;
        void Clear() noexcept;

    private:
        bool Execute(std::unique_ptr<EditorCommand> command, Life::Scene& scene, EditorCommandContext& context);
        bool Undo(Life::Scene& scene, EditorCommandContext& context);
        bool Redo(Life::Scene& scene, EditorCommandContext& context);

        std::vector<std::unique_ptr<EditorCommand>> m_Undo;
        std::vector<std::unique_ptr<EditorCommand>> m_Redo;
    };
}
