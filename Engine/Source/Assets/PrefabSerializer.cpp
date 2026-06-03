#include "Assets/PrefabSerializer.h"

#include "Assets/AssetManager.h"
#include "Scene/Scene.h"
#include "Scene/SceneSerializer.h"

#include <functional>
#include <vector>

namespace Life::Assets
{
    namespace
    {
        void CopyEntitySubtreePreservingIds(Scene& destinationScene, Entity sourceEntity, Entity destinationParent)
        {
            Entity destinationEntity = destinationParent.IsValid()
                ? destinationScene.CreateChildEntity(destinationParent, sourceEntity.GetTag())
                : destinationScene.CreateEntity(sourceEntity.GetTag());

            destinationEntity.GetComponent<IdComponent>().Id = sourceEntity.GetId();
            destinationEntity.SetEnabled(sourceEntity.IsEnabled());
            destinationEntity.GetComponent<TransformComponent>() = sourceEntity.GetComponent<TransformComponent>();

            if (const CameraComponent* camera = sourceEntity.TryGetComponent<CameraComponent>())
                destinationEntity.AddComponent<CameraComponent>(*camera);
            if (const SpriteComponent* sprite = sourceEntity.TryGetComponent<SpriteComponent>())
                destinationEntity.AddComponent<SpriteComponent>(*sprite);
            if (const SpriteRendererComponent* spriteRenderer = sourceEntity.TryGetComponent<SpriteRendererComponent>())
                destinationEntity.AddComponent<SpriteRendererComponent>(*spriteRenderer);
            if (const PrefabInstanceComponent* prefabInstance = sourceEntity.TryGetComponent<PrefabInstanceComponent>())
                destinationEntity.AddComponent<PrefabInstanceComponent>(*prefabInstance);

            for (const Entity child : sourceEntity.GetChildren())
                CopyEntitySubtreePreservingIds(destinationScene, child, destinationEntity);
        }
    }

    Result<void> PrefabSerializer::SaveEntityAsPrefab(const Scene& sourceScene, Entity rootEntity, const std::filesystem::path& destinationPath)
    {
        if (!sourceScene.IsValid(rootEntity))
            return Result<void>(ErrorCode::InvalidArgument, "PrefabSerializer::SaveEntityAsPrefab requires a valid source entity");

        Scene prefabScene(rootEntity.GetTag().empty() ? "Prefab" : rootEntity.GetTag());
        prefabScene.SetState(Scene::State::Ready);
        prefabScene.SetSpriteSortingLayers(sourceScene.GetSpriteSortingLayers());
        CopyEntitySubtreePreservingIds(prefabScene, rootEntity, {});
        return SceneSerializer::Save(prefabScene, destinationPath);
    }

    Result<void> PrefabSerializer::SaveSceneAsPrefab(const Scene& prefabScene, const std::filesystem::path& destinationPath)
    {
        const std::vector<Entity> roots = prefabScene.GetRootEntities();
        if (roots.empty())
            return Result<void>(ErrorCode::InvalidArgument, "Prefab must contain one root entity");
        if (roots.size() > 1u)
            return Result<void>(ErrorCode::InvalidArgument, "Prefab must contain exactly one root entity. Parent extra roots under the prefab root before saving");

        return SceneSerializer::Save(prefabScene, destinationPath);
    }

    Result<Scope<Scene>> PrefabSerializer::Load(const std::filesystem::path& sourcePath, AssetManager* assetManager)
    {
        return SceneSerializer::Load(sourcePath, assetManager);
    }
}
