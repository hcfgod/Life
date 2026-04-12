#include "TestSupport.h"

#include <random>

namespace
{
    struct TestVelocityComponent
    {
        float X = 0.0f;
        float Y = 0.0f;
    };

    std::filesystem::path MakeUniqueSceneTestDirectory(const std::string& prefix)
    {
        std::random_device device;
        std::mt19937_64 generator(device());
        std::uniform_int_distribution<unsigned long long> distribution;

        return std::filesystem::temp_directory_path() /
               (prefix + "-" + std::to_string(distribution(generator)));
    }

    struct TemporaryDirectoryScope final
    {
        explicit TemporaryDirectoryScope(std::filesystem::path root)
            : Root(std::move(root))
        {
        }

        ~TemporaryDirectoryScope()
        {
            std::error_code ec;
            std::filesystem::remove_all(Root, ec);
        }

        std::filesystem::path Root;
    };
}

TEST_CASE("Scene creates entities with built-in components and supports custom component lifecycle")
{
    Life::Scene scene("Gameplay");

    Life::Entity entity = scene.CreateEntity("Player");
    REQUIRE(entity.IsValid());

    CHECK(scene.GetName() == "Gameplay");
    CHECK(scene.GetEntityCount() == 1);
    CHECK(entity.GetTag() == "Player");
    CHECK_FALSE(entity.GetId().empty());
    CHECK(entity.HasComponent<Life::IdComponent>());
    CHECK(entity.HasComponent<Life::TagComponent>());
    CHECK(entity.HasComponent<Life::TransformComponent>());
    CHECK(entity.HasComponent<Life::HierarchyComponent>());
    CHECK_FALSE(entity.RemoveComponent<Life::TransformComponent>());

    auto& velocity = entity.AddComponent<TestVelocityComponent>();
    velocity.X = 4.0f;
    velocity.Y = -2.0f;

    REQUIRE(entity.HasComponent<TestVelocityComponent>());
    CHECK(entity.GetComponent<TestVelocityComponent>().X == doctest::Approx(4.0f));
    CHECK(entity.RemoveComponent<TestVelocityComponent>());
    CHECK_FALSE(entity.HasComponent<TestVelocityComponent>());

    CHECK(scene.DestroyEntity(entity));
    CHECK(scene.GetEntityCount() == 0);
}

TEST_CASE("Scene hierarchy maintains parent child relationships and descendant queries")
{
    Life::Scene scene("Hierarchy");

    Life::Entity parent = scene.CreateEntity("Parent");
    Life::Entity child = scene.CreateChildEntity(parent, "Child");
    Life::Entity grandChild = scene.CreateChildEntity(child, "GrandChild");

    REQUIRE(parent.IsValid());
    REQUIRE(child.IsValid());
    REQUIRE(grandChild.IsValid());

    parent.GetComponent<Life::TransformComponent>().LocalPosition = { 2.0f, 3.0f, 0.0f };
    child.GetComponent<Life::TransformComponent>().LocalPosition = { 1.5f, -1.0f, 0.0f };
    const glm::mat4 childWorldTransform = scene.GetWorldTransformMatrix(child);
    CHECK(childWorldTransform[3].x == doctest::Approx(3.5f));
    CHECK(childWorldTransform[3].y == doctest::Approx(2.0f));

    CHECK(child.HasParent());
    CHECK(child.GetParent() == parent);
    CHECK(grandChild.GetParent() == child);
    CHECK(grandChild.IsDescendantOf(parent));
    CHECK(scene.IsDescendantOf(grandChild, parent));
    CHECK_FALSE(parent.IsDescendantOf(grandChild));

    const auto parentChildren = parent.GetChildren();
    REQUIRE(parentChildren.size() == 1);
    CHECK(parentChildren.front() == child);

    CHECK_FALSE(parent.SetParent(grandChild));

    grandChild.RemoveParent();
    CHECK_FALSE(grandChild.HasParent());
    CHECK_FALSE(grandChild.IsDescendantOf(parent));

    CHECK(scene.DestroyEntity(parent));
    CHECK(scene.GetEntityCount() == 1);
    CHECK(grandChild.IsValid());
}

TEST_CASE("SceneService manages the active scene boundary")
{
    Life::SceneService sceneService;

    CHECK_FALSE(sceneService.HasActiveScene());
    CHECK(sceneService.TryGetActiveScene() == nullptr);

    Life::Scene& createdScene = sceneService.CreateScene("ActiveScene");
    CHECK(sceneService.HasActiveScene());
    CHECK(&sceneService.GetActiveScene() == &createdScene);
    CHECK(createdScene.GetName() == "ActiveScene");
    CHECK(createdScene.IsReady());

    CHECK(sceneService.CloseScene());
    CHECK_FALSE(sceneService.HasActiveScene());
    CHECK(sceneService.TryGetActiveScene() == nullptr);
}

TEST_CASE("SceneSerializer round-trips hierarchy order, ids, transforms, and sprite asset keys")
{
    const std::filesystem::path rootDirectory = MakeUniqueSceneTestDirectory("life-scene-roundtrip");
    TemporaryDirectoryScope cleanup(rootDirectory);
    const std::filesystem::path scenePath = rootDirectory / "Assets" / "Scenes" / "RoundTrip.scene";

    Life::Scene scene("RoundTrip");
    Life::Entity rootA = scene.CreateEntity("RootA");
    Life::Entity rootB = scene.CreateEntity("RootB");
    Life::Entity rootC = scene.CreateEntity("RootC");
    CHECK(scene.SetSiblingIndex(rootC, 1));

    Life::Entity childA = scene.CreateChildEntity(rootA, "ChildA");
    Life::Entity childB = scene.CreateChildEntity(rootA, "ChildB");
    CHECK(scene.SetSiblingIndex(childB, 0));

    rootA.GetComponent<Life::TransformComponent>().LocalPosition = { 1.0f, 2.0f, 3.0f };
    childB.GetComponent<Life::TransformComponent>().LocalScale = { 4.0f, 5.0f, 6.0f };

    Life::SpriteComponent sprite;
    sprite.Size = { 2.5f, 3.5f };
    sprite.Color = { 0.25f, 0.5f, 0.75f, 1.0f };
    sprite.TextureAssetKey = "Assets/Textures/TestChecker.ppm";
    childB.AddComponent<Life::SpriteComponent>(sprite);

    const std::string rootAId = rootA.GetId();
    const std::string childBId = childB.GetId();

    REQUIRE(Life::SceneSerializer::Save(scene, scenePath).IsSuccess());

    auto loadResult = Life::SceneSerializer::Load(scenePath);
    REQUIRE(loadResult.IsSuccess());

    Life::Scope<Life::Scene> loadedScene = std::move(loadResult.GetValue());
    REQUIRE(loadedScene != nullptr);
    CHECK(loadedScene->GetName() == "RoundTrip");
    CHECK(loadedScene->GetEntityCount() == 5);

    const auto roots = loadedScene->GetRootEntities();
    REQUIRE(roots.size() == 3);
    CHECK(roots[0].GetTag() == "RootA");
    CHECK(roots[1].GetTag() == "RootC");
    CHECK(roots[2].GetTag() == "RootB");

    Life::Entity loadedRootA = loadedScene->FindEntityById(rootAId);
    Life::Entity loadedChildB = loadedScene->FindEntityById(childBId);
    REQUIRE(loadedRootA.IsValid());
    REQUIRE(loadedChildB.IsValid());

    const auto loadedChildren = loadedRootA.GetChildren();
    REQUIRE(loadedChildren.size() == 2);
    CHECK(loadedChildren[0].GetTag() == "ChildB");
    CHECK(loadedChildren[1].GetTag() == "ChildA");

    CHECK(loadedRootA.GetComponent<Life::TransformComponent>().LocalPosition.x == doctest::Approx(1.0f));
    CHECK(loadedRootA.GetComponent<Life::TransformComponent>().LocalPosition.y == doctest::Approx(2.0f));
    CHECK(loadedRootA.GetComponent<Life::TransformComponent>().LocalPosition.z == doctest::Approx(3.0f));
    CHECK(loadedChildB.GetComponent<Life::TransformComponent>().LocalScale.x == doctest::Approx(4.0f));
    CHECK(loadedChildB.GetComponent<Life::TransformComponent>().LocalScale.y == doctest::Approx(5.0f));
    CHECK(loadedChildB.GetComponent<Life::TransformComponent>().LocalScale.z == doctest::Approx(6.0f));

    REQUIRE(loadedChildB.HasComponent<Life::SpriteComponent>());
    const Life::SpriteComponent& loadedSprite = loadedChildB.GetComponent<Life::SpriteComponent>();
    CHECK(loadedSprite.Size.x == doctest::Approx(2.5f));
    CHECK(loadedSprite.Size.y == doctest::Approx(3.5f));
    CHECK(loadedSprite.Color.x == doctest::Approx(0.25f));
    CHECK(loadedSprite.Color.y == doctest::Approx(0.5f));
    CHECK(loadedSprite.Color.z == doctest::Approx(0.75f));
    CHECK(loadedSprite.Color.w == doctest::Approx(1.0f));
    CHECK(loadedSprite.TextureAssetKey == "Assets/Textures/TestChecker.ppm");
}

TEST_CASE("SceneService saves, reloads, and strictly reports missing scene files")
{
    const std::filesystem::path rootDirectory = MakeUniqueSceneTestDirectory("life-scene-service");
    TemporaryDirectoryScope cleanup(rootDirectory);
    const std::filesystem::path scenePath = rootDirectory / "Assets" / "Scenes" / "Service.scene";
    const std::filesystem::path missingScenePath = rootDirectory / "Assets" / "Scenes" / "Missing.scene";

    Life::SceneService sceneService;
    Life::Scene& scene = sceneService.CreateScene("ServiceScene");
    Life::Entity entity = scene.CreateEntity("PersistentEntity");
    entity.GetComponent<Life::TransformComponent>().LocalPosition = { 9.0f, 8.0f, 7.0f };
    sceneService.MarkActiveSceneDirty();

    const auto saveWithoutPath = sceneService.SaveActiveScene();
    CHECK(saveWithoutPath.IsFailure());
    CHECK(saveWithoutPath.GetError().GetCode() == Life::ErrorCode::InvalidArgument);

    const auto saveAsResult = sceneService.SaveActiveSceneAs(scenePath);
    REQUIRE(saveAsResult.IsSuccess());
    CHECK(sceneService.HasActiveSceneSourcePath());
    CHECK_FALSE(sceneService.IsActiveSceneDirty());

    scene.GetName();
    entity.SetTag("RenamedEntity");
    sceneService.MarkActiveSceneDirty();
    CHECK(sceneService.IsActiveSceneDirty());
    REQUIRE(sceneService.SaveActiveScene().IsSuccess());
    CHECK_FALSE(sceneService.IsActiveSceneDirty());

    const auto loadResult = sceneService.LoadScene(scenePath);
    REQUIRE(loadResult.IsSuccess());
    REQUIRE(sceneService.HasActiveScene());
    CHECK(sceneService.GetActiveScene().FindEntityByTag("RenamedEntity").IsValid());
    CHECK(sceneService.GetActiveScene().FindEntityByTag("RenamedEntity").GetComponent<Life::TransformComponent>().LocalPosition.x == doctest::Approx(9.0f));

    const auto missingResult = sceneService.LoadScene(missingScenePath);
    CHECK(missingResult.IsFailure());
    CHECK(missingResult.GetError().GetCode() == Life::ErrorCode::FileNotFound);
    CHECK(sceneService.HasActiveScene());
    CHECK(sceneService.GetActiveScene().FindEntityByTag("RenamedEntity").IsValid());
}
