#include "TestSupport.h"
#include "Assets/GeneratedAssetRuntimeRegistry.h"

#include <array>

using namespace Life::Tests;

TEST_CASE("ServiceRegistry registers and unregisters typed services")
{
    Life::ServiceRegistry registry;
    TestService service{ 42 };

    CHECK_FALSE(registry.Has<TestService>());
    CHECK(registry.TryGet<TestService>() == nullptr);
    CHECK_THROWS_AS(registry.Get<TestService>(), std::logic_error);

    registry.Register<TestService>(service);

    CHECK(registry.Has<TestService>());
    REQUIRE(registry.TryGet<TestService>() != nullptr);
    CHECK(registry.TryGet<TestService>() == &service);
    CHECK(registry.Get<TestService>().Value == 42);

    CHECK(registry.Unregister<TestService>());
    CHECK_FALSE(registry.Has<TestService>());
    CHECK(registry.TryGet<TestService>() == nullptr);
    CHECK_FALSE(registry.Unregister<TestService>());
}

TEST_CASE("ApplicationHost registers built-in and custom services")
{
    auto application = Life::CreateScope<TestApplication>();
    auto* applicationInstance = application.get();
    auto host = Life::CreateScope<Life::ApplicationHost>(std::move(application), Life::CreateScope<TestRuntime>());

    CHECK(host->GetServices().Has<Life::ApplicationHost>());
    CHECK(host->GetServices().Has<Life::Application>());
    CHECK(host->GetServices().Has<Life::ApplicationContext>());
    CHECK(host->GetServices().Has<Life::ApplicationEventRouter>());
    CHECK(host->GetServices().Has<Life::JobSystem>());
    CHECK(host->GetServices().Has<Life::InputSystem>());
    CHECK(host->GetServices().Has<Life::Async::AsyncIO>());
    CHECK(host->GetServices().Has<Life::ApplicationRuntime>());
    CHECK(host->GetServices().Has<Life::Window>());
    CHECK(host->GetServices().Has<Life::Assets::AssetContext>());
    CHECK(host->GetServices().Has<Life::SceneService>());
    CHECK(host->GetServices().Has<Life::SceneRuntime>());
    CHECK(host->GetServices().Has<Life::AudioDevice>());

    CHECK(&host->GetServices().Get<Life::ApplicationHost>() == host.get());
    CHECK(&host->GetServices().Get<Life::Application>() == applicationInstance);
    CHECK(&host->GetServices().Get<Life::ApplicationContext>() == &host->GetContext());
    CHECK(&host->GetServices().Get<Life::JobSystem>() == &Life::GetJobSystem());
    CHECK(&host->GetServices().Get<Life::InputSystem>() == &host->GetInputSystem());
    CHECK(&host->GetServices().Get<Life::Async::AsyncIO>() == &Life::Async::GetAsyncIO());
    CHECK(&host->GetServices().Get<Life::ApplicationRuntime>() == &host->GetRuntime());
    CHECK(&host->GetServices().Get<Life::Assets::AssetContext>() == &host->GetAssetContext());
    CHECK(&host->GetServices().Get<Life::SceneService>() == host->GetSceneService());
    CHECK(&host->GetServices().Get<Life::SceneRuntime>() == host->GetSceneRuntime());
    CHECK(&host->GetServices().Get<Life::AudioDevice>() == host->GetAudioDevice());
    CHECK(&applicationInstance->GetService<Life::Async::AsyncIO>() == &Life::Async::GetAsyncIO());
    CHECK(&applicationInstance->GetService<Life::ApplicationHost>() == host.get());
    CHECK(&applicationInstance->GetService<Life::InputSystem>() == &host->GetInputSystem());
    CHECK(&applicationInstance->GetService<Life::Window>() == &host->GetWindow());
    CHECK(&applicationInstance->GetService<Life::SceneService>() == host->GetSceneService());

    TestService service{ 7 };
    host->GetServices().Register<TestService>(service);

    CHECK(host->GetServices().Has<TestService>());
    CHECK(applicationInstance->HasService<TestService>());
    CHECK(applicationInstance->TryGetService<TestService>() == &service);
    CHECK(applicationInstance->GetService<TestService>().Value == 7);
}

TEST_CASE("Global service registry falls back after host destruction")
{
    {
        auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        CHECK(&Life::GetServices() == &host->GetServices());
    }

    CHECK_FALSE(Life::GetServices().Has<Life::ApplicationHost>());
}

TEST_CASE("Creating a second ApplicationHost while one is live is rejected")
{
    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    CHECK(&Life::GetServices() == &host->GetServices());

    try
    {
        [[maybe_unused]] auto secondHost = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
        FAIL("Expected second host creation to throw");
    }
    catch (const Life::Error& error)
    {
        CHECK(error.GetCode() == Life::ErrorCode::InvalidState);
    }

    CHECK(&Life::GetServices() == &host->GetServices());

    host.reset();
    CHECK_FALSE(Life::GetServices().Has<Life::ApplicationHost>());
}

namespace
{
    class ReloadTrackingAsset final : public Life::Asset
    {
    public:
        ReloadTrackingAsset(std::string key, std::string guid)
            : Life::Asset(std::move(key), std::move(guid))
        {
        }

        bool Reload() override
        {
            ++ReloadCount;
            return ReloadResult;
        }

        int ReloadCount = 0;
        bool ReloadResult = true;
    };

    class GeneratedReloadTrackingAsset final : public Life::Asset
    {
    public:
        GeneratedReloadTrackingAsset(std::string key, std::string guid)
            : Life::Asset(std::move(key), std::move(guid))
        {
        }

        bool Reload() override
        {
            ++ReloadCount;
            return true;
        }

        int ReloadCount = 0;
    };

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

TEST_CASE("AssetManager exposes cached assets and reloads live cached instances")
{
    Life::Assets::AssetManager assetManager;
    auto asset = Life::Ref<ReloadTrackingAsset>(new ReloadTrackingAsset("Assets/Test.asset", "guid-test-asset"));
    assetManager.Cache(asset->GetKey(), asset->GetGuid(), asset);

    REQUIRE(assetManager.GetCachedByKey<ReloadTrackingAsset>(asset->GetKey()) == asset);
    REQUIRE(assetManager.GetByGuid<ReloadTrackingAsset>(asset->GetGuid()) == asset);

    CHECK(assetManager.ReloadCachedAssetByKey(asset->GetKey()));
    CHECK(assetManager.ReloadCachedAssetByGuid(asset->GetGuid()));
    CHECK(asset->ReloadCount == 2);

    asset->ReloadResult = false;
    CHECK_FALSE(assetManager.ReloadCachedAssetByKey(asset->GetKey()));
    CHECK(asset->ReloadCount == 3);

    asset.reset();
    CHECK_FALSE(assetManager.ReloadCachedAssetByKey("Assets/Test.asset"));
    CHECK_FALSE(assetManager.ReloadCachedAssetByGuid("guid-test-asset"));
}

TEST_CASE("AssetHandle truthiness is GUID based and explicit manager resolution caches assets")
{
    Life::Assets::AssetManager assetManager;
    auto asset = Life::Ref<ReloadTrackingAsset>(new ReloadTrackingAsset("Assets/Test.asset", "guid-explicit-handle"));
    assetManager.Cache(asset->GetKey(), asset->GetGuid(), asset);

    Life::AssetHandle<ReloadTrackingAsset> handle("guid-explicit-handle");
    CHECK(handle);
    CHECK(handle.HasGuid());
    CHECK_FALSE(handle.IsResolved());

    CHECK(handle.Resolve(assetManager) == asset);
    CHECK(handle.IsResolved());

    handle.SetGuid("guid-only");
    CHECK(handle);
    CHECK_FALSE(handle.IsResolved());
    CHECK(handle.Resolve(assetManager) == nullptr);
}

TEST_CASE("AssetContext instances keep project roots isolated")
{
    const std::filesystem::path base = std::filesystem::temp_directory_path() / ("life-asset-context-" + Life::Assets::GenerateGuid());
    TemporaryDirectoryScope cleanup(base);

    const std::filesystem::path firstRoot = base / "First";
    const std::filesystem::path secondRoot = base / "Second";
    std::filesystem::create_directories(firstRoot / "Assets");
    std::filesystem::create_directories(secondRoot / "Assets");

    Life::Assets::AssetContext firstContext;
    Life::Assets::AssetContext secondContext;
    firstContext.SetActiveProjectRootDirectory(firstRoot);
    secondContext.SetActiveProjectRootDirectory(secondRoot);

    REQUIRE(firstContext.TryGetActiveProjectRootDirectory().has_value());
    REQUIRE(secondContext.TryGetActiveProjectRootDirectory().has_value());
    CHECK(firstContext.TryGetActiveProjectRootDirectory().value() == std::filesystem::absolute(firstRoot).lexically_normal());
    CHECK(secondContext.TryGetActiveProjectRootDirectory().value() == std::filesystem::absolute(secondRoot).lexically_normal());

    const auto firstResolved = firstContext.ResolveAssetKeyToPath("Assets/Test.asset");
    const auto secondResolved = secondContext.ResolveAssetKeyToPath("Assets/Test.asset");
    REQUIRE(firstResolved.IsSuccess());
    REQUIRE(secondResolved.IsSuccess());
    CHECK(firstResolved.GetValue() == std::filesystem::absolute(firstRoot / "Assets/Test.asset").lexically_normal());
    CHECK(secondResolved.GetValue() == std::filesystem::absolute(secondRoot / "Assets/Test.asset").lexically_normal());
}

TEST_CASE("AudioClipAsset decodes generated PCM WAV metadata and samples")
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / ("life-audio-clip-" + Life::Assets::GenerateGuid());
    TemporaryDirectoryScope cleanup(root);
    std::filesystem::create_directories(root);
    const std::filesystem::path wavPath = root / "test.wav";

    {
        const std::vector<uint8_t> wavBytes = CreateTinyPcm16Wav();
        std::ofstream stream(wavPath, std::ios::out | std::ios::binary | std::ios::trunc);
        REQUIRE(stream.is_open());
        stream.write(reinterpret_cast<const char*>(wavBytes.data()), static_cast<std::streamsize>(wavBytes.size()));
    }

    Life::Assets::AudioClipAsset::Ptr clip = Life::Assets::AudioClipAsset::LoadBlocking(wavPath.string());
    REQUIRE(clip != nullptr);
    const Life::Assets::AudioClipAsset::DecodedAudio& audio = clip->GetDecodedAudio();
    CHECK(audio.SampleRateHz == 8000);
    CHECK(audio.ChannelCount == 1);
    CHECK(audio.FrameCount == 4);
    REQUIRE(audio.Samples.size() == 4);
    CHECK(audio.Samples[0] == doctest::Approx(0.0f));
    CHECK(audio.Samples[1] == doctest::Approx(0.5f).epsilon(0.01f));
    CHECK(audio.Samples[2] == doctest::Approx(-0.5f).epsilon(0.01f));
    CHECK(audio.Samples[3] == doctest::Approx(0.999f).epsilon(0.01f));
}

TEST_CASE("AssetHotReloadManager defers generated reload execution until Pump runs on the host thread")
{
    Life::Log::Init();

    auto host = Life::CreateScope<Life::ApplicationHost>(Life::CreateScope<TestApplication>(), Life::CreateScope<TestRuntime>());
    Life::Assets::AssetHotReloadManager& hotReloadManager = host->GetAssetContext().GetHotReloadManager();
    hotReloadManager.SetDebounceWindow(std::chrono::milliseconds(0));

    const std::string key = "Generated/TestHotReload.asset";
    const std::string guid = "generated-test-hot-reload-guid";

    auto asset = Life::Ref<GeneratedReloadTrackingAsset>(new GeneratedReloadTrackingAsset(key, guid));
    host->GetAssetManager().Cache(key, guid, asset);

    auto registerResult = host->GetAssetDatabase().RegisterGeneratedAsset(guid, key, Life::Assets::AssetType::Texture2D);
    REQUIRE(registerResult.IsSuccess());

    int generatedReloadCount = 0;
    host->GetAssetContext().GetGeneratedAssetRegistry().Register(
        key,
        []() -> Life::Ref<Life::Asset> { return nullptr; },
        [&](const std::string& virtualKey)
        {
            CHECK(virtualKey == key);
            ++generatedReloadCount;
            return true;
        });

    hotReloadManager.RequestReload(key, guid);

    CHECK(generatedReloadCount == 0);
    CHECK(asset->ReloadCount == 0);

    hotReloadManager.Pump();

    CHECK(generatedReloadCount == 1);
    CHECK(asset->ReloadCount == 1);

    host->GetAssetContext().GetGeneratedAssetRegistry().Unregister(key);
    hotReloadManager.SetDebounceWindow(std::chrono::milliseconds(300));
}
