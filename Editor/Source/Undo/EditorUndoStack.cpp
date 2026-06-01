#include "Editor/Undo/EditorUndoStack.h"

#include "Editor/Scene/EditorSceneState.h"

#include <utility>

namespace EditorApp
{
    namespace
    {
        void SelectIfAvailable(EditorCommandContext* context, Life::Scene& scene, const std::string& entityId)
        {
            if (context == nullptr || context->SceneState == nullptr)
                return;

            Life::Entity entity = scene.FindEntityById(entityId);
            if (entity.IsValid())
                context->SceneState->SelectEntity(entity);
            else if (!entityId.empty() && context->SceneState->SelectedEntityId == entityId)
                context->SceneState->ClearSelection();
        }

        void RestoreSnapshotRecursive(Life::Scene& scene, const EditorEntitySnapshot& snapshot, Life::Entity parent)
        {
            Life::Entity entity = scene.CreateEntity(snapshot.Tag);
            entity.GetComponent<Life::IdComponent>().Id = snapshot.Id;
            entity.SetEnabled(snapshot.Enabled);
            entity.GetComponent<Life::TransformComponent>() = snapshot.Transform;

            if (snapshot.Camera.has_value())
                entity.AddComponent<Life::CameraComponent>(*snapshot.Camera);
            if (snapshot.Sprite.has_value())
                entity.AddComponent<Life::SpriteComponent>(*snapshot.Sprite);
            if (snapshot.SpriteRenderer.has_value())
                entity.AddComponent<Life::SpriteRendererComponent>(*snapshot.SpriteRenderer);

            if (parent.IsValid())
                (void)entity.SetParent(parent);
            else if (!snapshot.ParentId.empty())
            {
                Life::Entity storedParent = scene.FindEntityById(snapshot.ParentId);
                if (storedParent.IsValid())
                    (void)entity.SetParent(storedParent);
            }

            (void)scene.SetSiblingIndex(entity, snapshot.SiblingIndex);

            for (const EditorEntitySnapshot& child : snapshot.Children)
                RestoreSnapshotRecursive(scene, child, entity);
        }

        void AssignFreshIds(EditorEntitySnapshot& snapshot)
        {
            Life::Scene temporaryScene("DuplicateIdSource");
            snapshot.Id = temporaryScene.CreateEntity().GetId();
            for (EditorEntitySnapshot& child : snapshot.Children)
            {
                child.ParentId = snapshot.Id;
                AssignFreshIds(child);
            }
        }

        bool ApplyParentWithTransform(Life::Scene& scene,
                                      Life::Entity entity,
                                      const std::string& parentId,
                                      std::size_t siblingIndex,
                                      const Life::TransformComponent& transform)
        {
            if (!entity.IsValid())
                return false;

            if (parentId.empty())
            {
                entity.RemoveParent();
            }
            else
            {
                Life::Entity parent = scene.FindEntityById(parentId);
                if (!parent.IsValid() || !entity.SetParent(parent))
                    return false;
            }

            (void)scene.SetSiblingIndex(entity, siblingIndex);
            entity.GetComponent<Life::TransformComponent>() = transform;
            return true;
        }
    }

    EditorEntitySnapshot CaptureEntitySnapshot(const Life::Entity& entity)
    {
        EditorEntitySnapshot snapshot;
        if (!entity.IsValid())
            return snapshot;

        snapshot.Id = entity.GetId();
        snapshot.Tag = entity.GetTag();
        snapshot.Enabled = entity.IsEnabled();
        snapshot.SiblingIndex = entity.GetScene().GetSiblingIndex(entity);
        snapshot.Transform = entity.GetComponent<Life::TransformComponent>();
        if (Life::Entity parent = entity.GetParent(); parent.IsValid())
            snapshot.ParentId = parent.GetId();
        if (const Life::CameraComponent* camera = entity.TryGetComponent<Life::CameraComponent>())
            snapshot.Camera = *camera;
        if (const Life::SpriteComponent* sprite = entity.TryGetComponent<Life::SpriteComponent>())
            snapshot.Sprite = *sprite;
        if (const Life::SpriteRendererComponent* spriteRenderer = entity.TryGetComponent<Life::SpriteRendererComponent>())
            snapshot.SpriteRenderer = *spriteRenderer;

        for (const Life::Entity child : entity.GetChildren())
            snapshot.Children.push_back(CaptureEntitySnapshot(child));
        return snapshot;
    }

    EditorEntitySnapshot CreateDuplicateEntitySnapshot(const Life::Entity& entity)
    {
        EditorEntitySnapshot snapshot = CaptureEntitySnapshot(entity);
        const std::string originalRootId = snapshot.Id;
        AssignFreshIds(snapshot);
        snapshot.ParentId = entity.GetParent().IsValid() ? entity.GetParent().GetId() : std::string{};
        snapshot.Tag += " Copy";
        snapshot.SiblingIndex = entity.GetScene().GetSiblingIndex(entity) + 1u;

        for (EditorEntitySnapshot& child : snapshot.Children)
        {
            if (child.ParentId == originalRootId)
                child.ParentId = snapshot.Id;
        }
        return snapshot;
    }

    Life::Entity RestoreEntitySnapshot(Life::Scene& scene, const EditorEntitySnapshot& snapshot)
    {
        if (snapshot.Id.empty())
            return {};

        if (Life::Entity existing = scene.FindEntityById(snapshot.Id); existing.IsValid())
            (void)scene.DestroyEntity(existing);

        RestoreSnapshotRecursive(scene, snapshot, {});
        return scene.FindEntityById(snapshot.Id);
    }

    bool DestroyEntityById(Life::Scene& scene, const std::string& entityId)
    {
        Life::Entity entity = scene.FindEntityById(entityId);
        return entity.IsValid() && scene.DestroyEntity(entity);
    }

    SetEntityTransformCommand::SetEntityTransformCommand(std::string entityId,
                                                         Life::TransformComponent before,
                                                         Life::TransformComponent after)
        : m_EntityId(std::move(entityId))
        , m_Before(before)
        , m_After(after)
    {
    }

    bool SetEntityTransformCommand::Undo(Life::Scene& scene, EditorCommandContext* context)
    {
        const bool applied = Apply(scene, m_EntityId, m_Before);
        if (applied)
            SelectIfAvailable(context, scene, m_EntityId);
        return applied;
    }

    bool SetEntityTransformCommand::Redo(Life::Scene& scene, EditorCommandContext* context)
    {
        const bool applied = Apply(scene, m_EntityId, m_After);
        if (applied)
            SelectIfAvailable(context, scene, m_EntityId);
        return applied;
    }

    bool SetEntityTransformCommand::Apply(Life::Scene& scene, const std::string& entityId, const Life::TransformComponent& transform)
    {
        Life::Entity entity = scene.FindEntityById(entityId);
        if (!entity.IsValid())
            return false;

        entity.GetComponent<Life::TransformComponent>() = transform;
        return true;
    }

    RestoreEntitySnapshotCommand::RestoreEntitySnapshotCommand(EditorEntitySnapshot before, EditorEntitySnapshot after)
        : m_Before(std::move(before))
        , m_After(std::move(after))
    {
    }

    bool RestoreEntitySnapshotCommand::Undo(Life::Scene& scene, EditorCommandContext* context)
    {
        Life::Entity entity = RestoreEntitySnapshot(scene, m_Before);
        if (entity.IsValid())
            SelectIfAvailable(context, scene, m_Before.Id);
        return entity.IsValid();
    }

    bool RestoreEntitySnapshotCommand::Redo(Life::Scene& scene, EditorCommandContext* context)
    {
        Life::Entity entity = RestoreEntitySnapshot(scene, m_After);
        if (entity.IsValid())
            SelectIfAvailable(context, scene, m_After.Id);
        return entity.IsValid();
    }

    CreateEntityCommand::CreateEntityCommand(EditorEntitySnapshot snapshot)
        : m_Snapshot(std::move(snapshot))
    {
    }

    bool CreateEntityCommand::Undo(Life::Scene& scene, EditorCommandContext* context)
    {
        const bool destroyed = DestroyEntityById(scene, m_Snapshot.Id);
        if (destroyed && context != nullptr && context->SceneState != nullptr && context->SceneState->SelectedEntityId == m_Snapshot.Id)
            context->SceneState->ClearSelection();
        return destroyed;
    }

    bool CreateEntityCommand::Redo(Life::Scene& scene, EditorCommandContext* context)
    {
        Life::Entity entity = RestoreEntitySnapshot(scene, m_Snapshot);
        if (entity.IsValid())
            SelectIfAvailable(context, scene, m_Snapshot.Id);
        return entity.IsValid();
    }

    DeleteEntityCommand::DeleteEntityCommand(EditorEntitySnapshot snapshot)
        : m_Snapshot(std::move(snapshot))
    {
    }

    bool DeleteEntityCommand::Undo(Life::Scene& scene, EditorCommandContext* context)
    {
        Life::Entity entity = RestoreEntitySnapshot(scene, m_Snapshot);
        if (entity.IsValid())
            SelectIfAvailable(context, scene, m_Snapshot.Id);
        return entity.IsValid();
    }

    bool DeleteEntityCommand::Redo(Life::Scene& scene, EditorCommandContext* context)
    {
        const bool destroyed = DestroyEntityById(scene, m_Snapshot.Id);
        if (destroyed && context != nullptr && context->SceneState != nullptr && context->SceneState->SelectedEntityId == m_Snapshot.Id)
            context->SceneState->ClearSelection();
        return destroyed;
    }

    DuplicateEntityCommand::DuplicateEntityCommand(EditorEntitySnapshot snapshot)
        : m_Snapshot(std::move(snapshot))
    {
    }

    bool DuplicateEntityCommand::Undo(Life::Scene& scene, EditorCommandContext* context)
    {
        const bool destroyed = DestroyEntityById(scene, m_Snapshot.Id);
        if (destroyed && context != nullptr && context->SceneState != nullptr && context->SceneState->SelectedEntityId == m_Snapshot.Id)
            context->SceneState->ClearSelection();
        return destroyed;
    }

    bool DuplicateEntityCommand::Redo(Life::Scene& scene, EditorCommandContext* context)
    {
        Life::Entity entity = RestoreEntitySnapshot(scene, m_Snapshot);
        if (entity.IsValid())
            SelectIfAvailable(context, scene, m_Snapshot.Id);
        return entity.IsValid();
    }

    RenameEntityCommand::RenameEntityCommand(std::string entityId, std::string before, std::string after)
        : m_EntityId(std::move(entityId))
        , m_Before(std::move(before))
        , m_After(std::move(after))
    {
    }

    bool RenameEntityCommand::Undo(Life::Scene& scene, EditorCommandContext* context)
    {
        return Apply(scene, m_Before, context);
    }

    bool RenameEntityCommand::Redo(Life::Scene& scene, EditorCommandContext* context)
    {
        return Apply(scene, m_After, context);
    }

    bool RenameEntityCommand::Apply(Life::Scene& scene, const std::string& tag, EditorCommandContext* context) const
    {
        Life::Entity entity = scene.FindEntityById(m_EntityId);
        if (!entity.IsValid())
            return false;

        entity.SetTag(tag);
        SelectIfAvailable(context, scene, m_EntityId);
        return true;
    }

    SetEntityEnabledCommand::SetEntityEnabledCommand(std::string entityId, bool before, bool after)
        : m_EntityId(std::move(entityId))
        , m_Before(before)
        , m_After(after)
    {
    }

    bool SetEntityEnabledCommand::Undo(Life::Scene& scene, EditorCommandContext* context)
    {
        return Apply(scene, m_Before, context);
    }

    bool SetEntityEnabledCommand::Redo(Life::Scene& scene, EditorCommandContext* context)
    {
        return Apply(scene, m_After, context);
    }

    bool SetEntityEnabledCommand::Apply(Life::Scene& scene, bool enabled, EditorCommandContext* context) const
    {
        Life::Entity entity = scene.FindEntityById(m_EntityId);
        if (!entity.IsValid())
            return false;

        entity.SetEnabled(enabled);
        SelectIfAvailable(context, scene, m_EntityId);
        return true;
    }

    SetEntityParentCommand::SetEntityParentCommand(std::string entityId,
                                                   std::string beforeParentId,
                                                   std::size_t beforeSiblingIndex,
                                                   Life::TransformComponent beforeTransform,
                                                   std::string afterParentId,
                                                   std::size_t afterSiblingIndex,
                                                   Life::TransformComponent afterTransform)
        : m_EntityId(std::move(entityId))
        , m_BeforeParentId(std::move(beforeParentId))
        , m_BeforeSiblingIndex(beforeSiblingIndex)
        , m_BeforeTransform(beforeTransform)
        , m_AfterParentId(std::move(afterParentId))
        , m_AfterSiblingIndex(afterSiblingIndex)
        , m_AfterTransform(afterTransform)
    {
    }

    bool SetEntityParentCommand::Undo(Life::Scene& scene, EditorCommandContext* context)
    {
        return Apply(scene, m_BeforeParentId, m_BeforeSiblingIndex, m_BeforeTransform, context);
    }

    bool SetEntityParentCommand::Redo(Life::Scene& scene, EditorCommandContext* context)
    {
        return Apply(scene, m_AfterParentId, m_AfterSiblingIndex, m_AfterTransform, context);
    }

    bool SetEntityParentCommand::Apply(Life::Scene& scene,
                                       const std::string& parentId,
                                       std::size_t siblingIndex,
                                       const Life::TransformComponent& transform,
                                       EditorCommandContext* context) const
    {
        Life::Entity entity = scene.FindEntityById(m_EntityId);
        const bool applied = ApplyParentWithTransform(scene, entity, parentId, siblingIndex, transform);
        if (applied)
            SelectIfAvailable(context, scene, m_EntityId);
        return applied;
    }

    bool EditorUndoStack::Execute(std::unique_ptr<EditorCommand> command, Life::Scene& scene)
    {
        EditorCommandContext context;
        return Execute(std::move(command), scene, context);
    }

    bool EditorUndoStack::Execute(std::unique_ptr<EditorCommand> command, Life::Scene& scene, EditorSceneState& sceneState)
    {
        EditorCommandContext context{ .SceneState = &sceneState };
        return Execute(std::move(command), scene, context);
    }

    bool EditorUndoStack::Execute(std::unique_ptr<EditorCommand> command, Life::Scene& scene, EditorCommandContext& context)
    {
        if (!command || !command->Redo(scene, &context))
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
        EditorCommandContext context;
        return Undo(scene, context);
    }

    bool EditorUndoStack::Undo(Life::Scene& scene, EditorSceneState& sceneState)
    {
        EditorCommandContext context{ .SceneState = &sceneState };
        return Undo(scene, context);
    }

    bool EditorUndoStack::Undo(Life::Scene& scene, EditorCommandContext& context)
    {
        if (m_Undo.empty())
            return false;

        std::unique_ptr<EditorCommand> command = std::move(m_Undo.back());
        m_Undo.pop_back();
        if (!command->Undo(scene, &context))
            return false;

        m_Redo.push_back(std::move(command));
        return true;
    }

    bool EditorUndoStack::Redo(Life::Scene& scene)
    {
        EditorCommandContext context;
        return Redo(scene, context);
    }

    bool EditorUndoStack::Redo(Life::Scene& scene, EditorSceneState& sceneState)
    {
        EditorCommandContext context{ .SceneState = &sceneState };
        return Redo(scene, context);
    }

    bool EditorUndoStack::Redo(Life::Scene& scene, EditorCommandContext& context)
    {
        if (m_Redo.empty())
            return false;

        std::unique_ptr<EditorCommand> command = std::move(m_Redo.back());
        m_Redo.pop_back();
        if (!command->Redo(scene, &context))
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
