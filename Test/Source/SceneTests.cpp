#include "TestSupport.h"

#include <array>
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

    class CountingSceneSystem final : public Life::ISceneSystem
    {
    public:
        void OnSceneStart(Life::Scene& scene) override
        {
            LastStartedScene = &scene;
            ++StartCount;
        }

        void OnSceneUpdate(Life::Scene& scene, float timestep) override
        {
            LastUpdatedScene = &scene;
            LastTimestep = timestep;
            ++UpdateCount;
        }

        void OnSceneStop(Life::Scene& scene) override
        {
            LastStoppedScene = &scene;
            ++StopCount;
        }

        Life::Scene* LastStartedScene = nullptr;
        Life::Scene* LastUpdatedScene = nullptr;
        Life::Scene* LastStoppedScene = nullptr;
        float LastTimestep = 0.0f;
        int StartCount = 0;
        int UpdateCount = 0;
        int StopCount = 0;
    };

    struct TemporaryServiceRegistryScope final
    {
        explicit TemporaryServiceRegistryScope(Life::ServiceRegistry& registry)
        {
            Life::SetGlobalServiceRegistry(&registry);
        }

        ~TemporaryServiceRegistryScope()
        {
            Life::SetGlobalServiceRegistry(nullptr);
        }
    };

    void WriteTextFile(const std::filesystem::path& path, const std::string& text)
    {
        std::error_code ec;
        std::filesystem::create_directories(path.parent_path(), ec);
        std::ofstream stream(path, std::ios::out | std::ios::binary | std::ios::trunc);
        stream << text;
    }

    void WriteU16(std::vector<uint8_t>& bytes, uint16_t value)
    {
        bytes.push_back(static_cast<uint8_t>(value & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
    }

    void WriteU32(std::vector<uint8_t>& bytes, uint32_t value)
    {
        bytes.push_back(static_cast<uint8_t>(value & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((value >> 8u) & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((value >> 16u) & 0xFFu));
        bytes.push_back(static_cast<uint8_t>((value >> 24u) & 0xFFu));
    }

    void WriteBytes(std::vector<uint8_t>& bytes, const char* text)
    {
        while (*text != '\0')
            bytes.push_back(static_cast<uint8_t>(*text++));
    }

    std::vector<uint8_t> CreateTinyPcm16Wav()
    {
        std::vector<uint8_t> bytes;
        const uint16_t channels = 1;
        const uint32_t sampleRate = 8000;
        const uint16_t bitsPerSample = 16;
        const uint16_t blockAlign = channels * bitsPerSample / 8;
        const uint32_t byteRate = sampleRate * blockAlign;
        const std::array<int16_t, 4> samples{ 0, 16384, -16384, 32767 };
        const uint32_t dataSize = static_cast<uint32_t>(samples.size() * sizeof(int16_t));

        WriteBytes(bytes, "RIFF");
        WriteU32(bytes, 36u + dataSize);
        WriteBytes(bytes, "WAVE");
        WriteBytes(bytes, "fmt ");
        WriteU32(bytes, 16);
        WriteU16(bytes, 1);
        WriteU16(bytes, channels);
        WriteU32(bytes, sampleRate);
        WriteU32(bytes, byteRate);
        WriteU16(bytes, blockAlign);
        WriteU16(bytes, bitsPerSample);
        WriteBytes(bytes, "data");
        WriteU32(bytes, dataSize);
        for (const int16_t sample : samples)
            WriteU16(bytes, static_cast<uint16_t>(sample));

        return bytes;
    }
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

TEST_CASE("SceneRuntime starts updates pauses steps and stops registered systems")
{
    Life::Scene scene("Runtime");
    auto system = Life::CreateRef<CountingSceneSystem>();

    Life::SceneRuntime runtime;
    runtime.RegisterSystem(system);

    CHECK(runtime.Start(scene));
    CHECK(runtime.IsRunning());
    CHECK(runtime.GetActiveScene() == &scene);
    CHECK(system->StartCount == 1);
    CHECK(system->LastStartedScene == &scene);

    CHECK(runtime.Update(scene, 0.25f));
    CHECK(system->UpdateCount == 1);
    CHECK(system->LastUpdatedScene == &scene);
    CHECK(system->LastTimestep == doctest::Approx(0.25f));

    runtime.SetPaused(true);
    CHECK_FALSE(runtime.Update(scene, 0.5f));
    CHECK(system->UpdateCount == 1);

    runtime.RequestStep();
    CHECK(runtime.Update(scene, 0.5f));
    CHECK(system->UpdateCount == 2);
    CHECK_FALSE(runtime.Update(scene, 0.5f));
    CHECK(system->UpdateCount == 2);

    CHECK(runtime.Stop());
    CHECK_FALSE(runtime.IsRunning());
    CHECK(runtime.GetActiveScene() == nullptr);
    CHECK(system->StopCount == 1);
    CHECK(system->LastStoppedScene == &scene);
}

TEST_CASE("AnimationSceneSystem samples clip transform tracks deterministically")
{
    const std::filesystem::path root = MakeUniqueSceneTestDirectory("life-animation-system");
    TemporaryDirectoryScope cleanup(root);
    const std::filesystem::path clipPath = root / "Move.animationclip.json";
    WriteTextFile(clipPath, R"json({
        "name": "Move",
        "loop": false,
        "durationSeconds": 1.0,
        "positionTrack": [
            { "time": 0.0, "x": 0.0, "y": 0.0, "z": 0.0, "interpolation": 1 },
            { "time": 1.0, "x": 10.0, "y": 0.0, "z": 0.0, "interpolation": 1 }
        ]
    })json");

    Life::Assets::AnimationClipAsset::Ptr clip = Life::Assets::AnimationClipAsset::LoadBlocking(clipPath.string());
    REQUIRE(clip != nullptr);

    Life::ServiceRegistry services;
    TemporaryServiceRegistryScope registryScope(services);
    Life::Assets::AssetDatabase database;
    Life::Assets::AssetManager assetManager;
    assetManager.BindDatabase(database);
    assetManager.Cache(clipPath.string(), clip->GetGuid(), clip);
    services.Register<Life::Assets::AssetManager>(assetManager);

    Life::Scene scene("Animation");
    Life::Entity entity = scene.CreateEntity("Animated");
    Life::AnimatorComponent animator;
    animator.ClipAssetKey = clipPath.string();
    entity.AddComponent<Life::AnimatorComponent>(animator);

    Life::SceneRuntime runtime;
    runtime.RegisterSystem(Life::CreateRef<Life::AnimationSceneSystem>());
    REQUIRE(runtime.Start(scene));

    CHECK(runtime.Update(scene, 0.5f));
    CHECK(entity.GetComponent<Life::TransformComponent>().LocalPosition.x == doctest::Approx(5.0f));
    REQUIRE(entity.HasComponent<Life::AnimatorComponent>());
    CHECK(entity.GetComponent<Life::AnimatorComponent>().Playing);

    CHECK(runtime.Update(scene, 0.5f));
    CHECK(entity.GetComponent<Life::TransformComponent>().LocalPosition.x == doctest::Approx(10.0f));
    CHECK_FALSE(entity.GetComponent<Life::AnimatorComponent>().Playing);
}

TEST_CASE("AudioSceneSystem advances and stops attached audio sources")
{
    const std::filesystem::path root = MakeUniqueSceneTestDirectory("life-audio-system");
    TemporaryDirectoryScope cleanup(root);
    const std::filesystem::path wavPath = root / "Tone.wav";
    const std::vector<uint8_t> wavBytes = CreateTinyPcm16Wav();
    {
        std::error_code ec;
        std::filesystem::create_directories(wavPath.parent_path(), ec);
        std::ofstream stream(wavPath, std::ios::out | std::ios::binary | std::ios::trunc);
        stream.write(reinterpret_cast<const char*>(wavBytes.data()), static_cast<std::streamsize>(wavBytes.size()));
    }

    Life::Assets::AudioClipAsset::Ptr clip = Life::Assets::AudioClipAsset::LoadBlocking(wavPath.string());
    REQUIRE(clip != nullptr);

    Life::ServiceRegistry services;
    TemporaryServiceRegistryScope registryScope(services);
    Life::Assets::AssetDatabase database;
    Life::Assets::AssetManager assetManager;
    Life::AudioDevice audioDevice;
    assetManager.BindDatabase(database);
    assetManager.Cache(wavPath.string(), clip->GetGuid(), clip);
    services.Register<Life::Assets::AssetManager>(assetManager);
    services.Register<Life::AudioDevice>(audioDevice);

    Life::Scene scene("Audio");
    Life::Entity entity = scene.CreateEntity("Source");
    Life::AudioSourceComponent source;
    source.ClipAssetKey = wavPath.string();
    source.PlayOnStart = true;
    source.Loop = false;
    entity.AddComponent<Life::AudioSourceComponent>(source);

    Life::SceneRuntime runtime;
    runtime.RegisterSystem(Life::CreateRef<Life::AudioSceneSystem>());
    REQUIRE(runtime.Start(scene));
    CHECK(entity.GetComponent<Life::AudioSourceComponent>().Playing);
    CHECK(audioDevice.GetActiveVoiceCount() == 1);

    CHECK(runtime.Update(scene, 0.01f));
    CHECK_FALSE(entity.GetComponent<Life::AudioSourceComponent>().Playing);
    CHECK(audioDevice.GetActiveVoiceCount() == 0);
    CHECK(entity.GetComponent<Life::AudioSourceComponent>().PlaybackTimeSeconds == doctest::Approx(0.0005f));
}

TEST_CASE("PrefabSerializer round trips an entity subtree and instantiate remaps ids")
{
    const std::filesystem::path root = MakeUniqueSceneTestDirectory("life-prefab");
    TemporaryDirectoryScope cleanup(root);
    const std::filesystem::path prefabPath = root / "Player.prefab.json";

    Life::Scene source("Source");
    source.AddSpriteSortingLayer("Characters");
    Life::Entity player = source.CreateEntity("Player");
    const std::string sourcePlayerId = player.GetId();
    player.GetComponent<Life::TransformComponent>().LocalPosition = { 2.0f, 3.0f, 0.0f };
    Life::SpriteComponent sprite;
    sprite.Size = { 1.5f, 2.0f };
    sprite.Color = { 0.2f, 0.4f, 0.8f, 1.0f };
    player.AddComponent<Life::SpriteComponent>(sprite);
    player.AddComponent<Life::SpriteRendererComponent>(Life::SpriteRendererComponent{ "Characters", 7 });

    Life::Entity child = source.CreateChildEntity(player, "Weapon");
    const std::string sourceChildId = child.GetId();
    child.GetComponent<Life::TransformComponent>().LocalPosition = { 0.5f, 0.0f, 0.0f };

    const auto saveResult = Life::Assets::PrefabSerializer::SaveEntityAsPrefab(source, player, prefabPath);
    REQUIRE(saveResult.IsSuccess());

    auto loadResult = Life::Assets::PrefabSerializer::Load(prefabPath);
    REQUIRE(loadResult.IsSuccess());
    Life::Scope<Life::Scene> prefabScene = std::move(loadResult.GetValue());
    REQUIRE(prefabScene != nullptr);

    Life::Entity prefabRoot = prefabScene->FindEntityById(sourcePlayerId);
    REQUIRE(prefabRoot.IsValid());
    CHECK(prefabRoot.GetTag() == "Player");
    REQUIRE(prefabRoot.GetChildren().size() == 1);
    CHECK(prefabRoot.GetChildren().front().GetId() == sourceChildId);

    Life::Scene target("Target");
    Life::Entity instanceRoot = target.InstantiatePrefab(*prefabScene, {}, "prefab-guid");
    REQUIRE(instanceRoot.IsValid());
    CHECK(instanceRoot.GetTag() == "Player");
    CHECK(instanceRoot.GetId() != sourcePlayerId);
    REQUIRE(instanceRoot.HasComponent<Life::PrefabInstanceComponent>());
    CHECK(instanceRoot.GetComponent<Life::PrefabInstanceComponent>().PrefabGuid == "prefab-guid");
    CHECK(instanceRoot.GetComponent<Life::PrefabInstanceComponent>().SourceEntityId == sourcePlayerId);
    CHECK(instanceRoot.GetComponent<Life::TransformComponent>().LocalPosition.x == doctest::Approx(2.0f));

    const auto instanceChildren = instanceRoot.GetChildren();
    REQUIRE(instanceChildren.size() == 1);
    CHECK(instanceChildren.front().GetId() != sourceChildId);
    REQUIRE(instanceChildren.front().HasComponent<Life::PrefabInstanceComponent>());
    CHECK(instanceChildren.front().GetComponent<Life::PrefabInstanceComponent>().SourceEntityId == sourceChildId);
}

TEST_CASE("Prefab instantiation uses one authored root")
{
    const std::filesystem::path root = MakeUniqueSceneTestDirectory("life-prefab-single-root");
    TemporaryDirectoryScope cleanup(root);
    const std::filesystem::path prefabPath = root / "Invalid.prefab.json";

    Life::Scene prefabScene("Prefab");
    Life::Entity rootA = prefabScene.CreateEntity("RootA");
    Life::Entity rootB = prefabScene.CreateEntity("RootB");
    prefabScene.CreateChildEntity(rootA, "ChildA");

    Life::Scene target("Target");
    Life::Entity instanceRoot = target.InstantiatePrefab(prefabScene, {}, "prefab-guid");
    REQUIRE(instanceRoot.IsValid());
    CHECK(instanceRoot.GetTag() == "RootA");
    CHECK(instanceRoot.GetChildren().size() == 1);

    const auto targetRoots = target.GetRootEntities();
    REQUIRE(targetRoots.size() == 1);
    CHECK(targetRoots.front().GetTag() == "RootA");
    CHECK(target.FindEntityById(rootB.GetId()).IsValid() == false);

    const auto saveResult = Life::Assets::PrefabSerializer::SaveSceneAsPrefab(prefabScene, prefabPath);
    REQUIRE(saveResult.IsFailure());
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

TEST_CASE("Scene transform utilities compose and decompose transform components")
{
    Life::TransformComponent transform;
    transform.LocalPosition = { 3.0f, -2.0f, 5.0f };
    transform.LocalRotation = { 0.0f, 0.0f, 0.75f };
    transform.LocalScale = { 2.0f, 3.0f, 1.0f };

    Life::TransformComponent decomposed;
    REQUIRE(Life::DecomposeTransform(Life::ComposeTransform(transform), decomposed));

    CHECK(decomposed.LocalPosition.x == doctest::Approx(transform.LocalPosition.x));
    CHECK(decomposed.LocalPosition.y == doctest::Approx(transform.LocalPosition.y));
    CHECK(decomposed.LocalPosition.z == doctest::Approx(transform.LocalPosition.z));
    CHECK(decomposed.LocalRotation.z == doctest::Approx(transform.LocalRotation.z));
    CHECK(decomposed.LocalScale.x == doctest::Approx(transform.LocalScale.x));
    CHECK(decomposed.LocalScale.y == doctest::Approx(transform.LocalScale.y));
    CHECK(decomposed.LocalScale.z == doctest::Approx(transform.LocalScale.z));
}

TEST_CASE("Scene world transform assignment preserves parented entity world placement")
{
    Life::Scene scene("WorldTransform");
    Life::Entity parent = scene.CreateEntity("Parent");
    Life::Entity child = scene.CreateChildEntity(parent, "Child");

    parent.GetComponent<Life::TransformComponent>().LocalPosition = { 10.0f, 2.0f, 0.0f };

    Life::TransformComponent desiredWorld;
    desiredWorld.LocalPosition = { 14.0f, 7.0f, -3.0f };
    desiredWorld.LocalScale = { 2.0f, 1.5f, 1.0f };

    REQUIRE(Life::SetEntityWorldTransform(scene, child, Life::ComposeTransform(desiredWorld)));

    const glm::mat4 childWorldTransform = scene.GetWorldTransformMatrix(child);
    CHECK(childWorldTransform[3].x == doctest::Approx(14.0f));
    CHECK(childWorldTransform[3].y == doctest::Approx(7.0f));
    CHECK(childWorldTransform[3].z == doctest::Approx(-3.0f));

    const Life::TransformComponent& childLocal = child.GetComponent<Life::TransformComponent>();
    CHECK(childLocal.LocalPosition.x == doctest::Approx(4.0f));
    CHECK(childLocal.LocalPosition.y == doctest::Approx(5.0f));
    CHECK(childLocal.LocalPosition.z == doctest::Approx(-3.0f));
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
    CHECK_FALSE(createdScene.HasCamera());

    CHECK(sceneService.CloseScene());
    CHECK_FALSE(sceneService.HasActiveScene());
    CHECK(sceneService.TryGetActiveScene() == nullptr);
}

TEST_CASE("SceneSerializer round-trips hierarchy order, ids, transforms, sprite asset keys, and scene cameras")
{
    const std::filesystem::path rootDirectory = MakeUniqueSceneTestDirectory("life-scene-roundtrip");
    TemporaryDirectoryScope cleanup(rootDirectory);
    const std::filesystem::path scenePath = rootDirectory / "Assets" / "Scenes" / "RoundTrip.scene";

    Life::Scene scene("RoundTrip");
    Life::Entity rootA = scene.CreateEntity("RootA");
    Life::Entity rootB = scene.CreateEntity("RootB");
    Life::Entity rootC = scene.CreateEntity("RootC");
    Life::Entity cameraEntity = scene.CreateEntity("MainCamera");
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
    Life::SpriteRendererComponent spriteRenderer;
    spriteRenderer.SortingLayer = "Foreground";
    spriteRenderer.SortingOrder = 7;
    CHECK(scene.AddSpriteSortingLayer("Foreground"));
    childB.AddComponent<Life::SpriteComponent>(sprite);
    childB.AddComponent<Life::SpriteRendererComponent>(spriteRenderer);
    childB.AddComponent<Life::AnimatorComponent>(Life::AnimatorComponent{
        "Assets/Animations/Hero.animcontroller.json",
        "Assets/Animations/HeroIdle.animationclip.json",
        "Idle",
        0.25f,
        1.5f,
        true,
        true
    });
    childB.AddComponent<Life::AudioSourceComponent>(Life::AudioSourceComponent{
        "Assets/Audio/Step.wav",
        0.1f,
        0.75f,
        true,
        true,
        false
    });

    cameraEntity.GetComponent<Life::TransformComponent>().LocalPosition = { 7.0f, 8.0f, 9.0f };
    Life::CameraComponent camera;
    camera.Projection = Life::ProjectionType::Perspective;
    camera.PerspectiveFieldOfView = 47.5f;
    camera.PerspectiveNearClip = 0.2f;
    camera.PerspectiveFarClip = 250.0f;
    camera.Priority = 4;
    camera.Primary = true;
    camera.ClearMode = Life::CameraClearMode::DepthOnly;
    camera.ClearColor = { 0.15f, 0.20f, 0.25f, 1.0f };
    camera.ViewportRect = { 0.1f, 0.2f, 0.7f, 0.6f, 0.05f, 0.95f };
    cameraEntity.AddComponent<Life::CameraComponent>(camera);

    const std::string rootAId = rootA.GetId();
    const std::string childBId = childB.GetId();
    const std::string cameraEntityId = cameraEntity.GetId();

    REQUIRE(Life::SceneSerializer::Save(scene, scenePath).IsSuccess());

    auto loadResult = Life::SceneSerializer::Load(scenePath);
    REQUIRE(loadResult.IsSuccess());

    Life::Scope<Life::Scene> loadedScene = std::move(loadResult.GetValue());
    REQUIRE(loadedScene != nullptr);
    CHECK(loadedScene->GetName() == "RoundTrip");
    CHECK(loadedScene->GetEntityCount() == 6);
    CHECK(loadedScene->HasCamera());

    const auto roots = loadedScene->GetRootEntities();
    REQUIRE(roots.size() == 4);
    CHECK(roots[0].GetTag() == "RootA");
    CHECK(roots[1].GetTag() == "RootC");
    CHECK(roots[2].GetTag() == "RootB");
    CHECK(roots[3].GetTag() == "MainCamera");

    Life::Entity loadedRootA = loadedScene->FindEntityById(rootAId);
    Life::Entity loadedChildB = loadedScene->FindEntityById(childBId);
    Life::Entity loadedCameraEntity = loadedScene->FindEntityById(cameraEntityId);
    REQUIRE(loadedRootA.IsValid());
    REQUIRE(loadedChildB.IsValid());
    REQUIRE(loadedCameraEntity.IsValid());

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
    REQUIRE(loadedChildB.HasComponent<Life::SpriteRendererComponent>());
    const Life::SpriteRendererComponent& loadedSpriteRenderer = loadedChildB.GetComponent<Life::SpriteRendererComponent>();
    CHECK(loadedSpriteRenderer.SortingLayer == "Foreground");
    CHECK(loadedSpriteRenderer.SortingOrder == 7);
    REQUIRE(loadedScene->GetSpriteSortingLayers().size() == 2);
    CHECK(loadedScene->GetSpriteSortingLayers()[0] == "Default");
    CHECK(loadedScene->GetSpriteSortingLayers()[1] == "Foreground");
    REQUIRE(loadedChildB.HasComponent<Life::AnimatorComponent>());
    const Life::AnimatorComponent& loadedAnimator = loadedChildB.GetComponent<Life::AnimatorComponent>();
    CHECK(loadedAnimator.ControllerAssetKey == "Assets/Animations/Hero.animcontroller.json");
    CHECK(loadedAnimator.ClipAssetKey == "Assets/Animations/HeroIdle.animationclip.json");
    CHECK(loadedAnimator.CurrentStateName == "Idle");
    CHECK(loadedAnimator.PlaybackTimeSeconds == doctest::Approx(0.25f));
    CHECK(loadedAnimator.Speed == doctest::Approx(1.5f));
    REQUIRE(loadedChildB.HasComponent<Life::AudioSourceComponent>());
    const Life::AudioSourceComponent& loadedAudioSource = loadedChildB.GetComponent<Life::AudioSourceComponent>();
    CHECK(loadedAudioSource.ClipAssetKey == "Assets/Audio/Step.wav");
    CHECK(loadedAudioSource.PlaybackTimeSeconds == doctest::Approx(0.1f));
    CHECK(loadedAudioSource.Volume == doctest::Approx(0.75f));
    CHECK(loadedAudioSource.PlayOnStart);
    CHECK(loadedAudioSource.Loop);

    REQUIRE(loadedCameraEntity.HasComponent<Life::CameraComponent>());
    const Life::CameraComponent& loadedCamera = loadedCameraEntity.GetComponent<Life::CameraComponent>();
    CHECK(loadedCamera.Projection == Life::ProjectionType::Perspective);
    CHECK(loadedCamera.PerspectiveFieldOfView == doctest::Approx(47.5f));
    CHECK(loadedCamera.PerspectiveNearClip == doctest::Approx(0.2f));
    CHECK(loadedCamera.PerspectiveFarClip == doctest::Approx(250.0f));
    CHECK(loadedCamera.Priority == 4);
    CHECK(loadedCamera.Primary);
    CHECK(loadedCamera.ClearMode == Life::CameraClearMode::DepthOnly);
    CHECK(loadedCamera.ClearColor.x == doctest::Approx(0.15f));
    CHECK(loadedCamera.ClearColor.y == doctest::Approx(0.20f));
    CHECK(loadedCamera.ClearColor.z == doctest::Approx(0.25f));
    CHECK(loadedCamera.ViewportRect.X == doctest::Approx(0.1f));
    CHECK(loadedCamera.ViewportRect.Y == doctest::Approx(0.2f));
    CHECK(loadedCamera.ViewportRect.Width == doctest::Approx(0.7f));
    CHECK(loadedCamera.ViewportRect.Height == doctest::Approx(0.6f));
    CHECK(loadedCamera.ViewportRect.MinDepth == doctest::Approx(0.05f));
    CHECK(loadedCamera.ViewportRect.MaxDepth == doctest::Approx(0.95f));
}

TEST_CASE("Scene cloning and SceneService save-load do not synthesize missing cameras")
{
    const std::filesystem::path rootDirectory = MakeUniqueSceneTestDirectory("life-scene-no-camera");
    TemporaryDirectoryScope cleanup(rootDirectory);
    const std::filesystem::path scenePath = rootDirectory / "Assets" / "Scenes" / "NoCamera.scene";

    Life::Scene scene("NoCamera");
    scene.SetState(Life::Scene::State::Ready);
    scene.CreateEntity("Marker");
    CHECK_FALSE(scene.HasCamera());

    Life::Scope<Life::Scene> clone = scene.Clone();
    REQUIRE(clone != nullptr);
    CHECK_FALSE(clone->HasCamera());
    CHECK(clone->GetEntityCount() == 1);

    Life::SceneService sceneService;
    Life::Scene& activeScene = sceneService.CreateScene("ActiveNoCamera");
    activeScene.CreateEntity("Marker");
    CHECK_FALSE(sceneService.ActiveSceneHasCamera());
    sceneService.MarkActiveSceneDirty();

    REQUIRE(sceneService.SaveActiveSceneAs(scenePath).IsSuccess());
    CHECK_FALSE(sceneService.ActiveSceneHasCamera());

    REQUIRE(sceneService.LoadScene(scenePath).IsSuccess());
    CHECK(sceneService.HasActiveScene());
    CHECK_FALSE(sceneService.ActiveSceneHasCamera());
    CHECK(sceneService.GetActiveScene().FindEntityByTag("Marker").IsValid());
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
