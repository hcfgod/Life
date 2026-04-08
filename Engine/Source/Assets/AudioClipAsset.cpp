#include "Assets/AudioClipAsset.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetLoadCoordinator.h"
#include "Assets/AssetLoadProgress.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetUtils.h"

#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

#include <fstream>

namespace Life::Assets
{
    std::future<AudioClipAsset::Ptr> AudioClipAsset::LoadAsync(const std::string& assetPath)
    {
        Settings defaultSettings;
        return LoadAsync(assetPath, defaultSettings);
    }

    std::future<AudioClipAsset::Ptr> AudioClipAsset::LoadAsync(const std::string& assetPath, const Settings& settings)
    {
        const uint64_t generation = AssetLoadCoordinator::GetGeneration();

        return std::async(std::launch::async, [assetPath, settings, generation]() -> Ptr {
            AssetLoadProgress::SetProgress(assetPath, 0.05f, "Resolving...");

            if (!AssetLoadCoordinator::IsGenerationCurrent(generation))
            {
                AssetLoadProgress::ClearProgress(assetPath);
                return nullptr;
            }

            bool fromBundle = false;
            std::vector<uint8_t> rawBytes;
            std::string guid;
            std::string resolvedPath;

            auto* bundle = GetServices().TryGet<AssetBundle>();
            if (bundle && bundle->IsEnabled() && bundle->IsLoaded())
            {
                const auto entry = bundle->FindEntryByKey(assetPath);
                if (entry.has_value())
                {
                    const auto bytesResult = bundle->ReadAllBytesByKey(assetPath);
                    if (bytesResult.IsSuccess())
                    {
                        fromBundle = true;
                        rawBytes = bytesResult.GetValue();
                        guid = entry->Guid;
                        AssetLoadProgress::SetProgress(assetPath, 0.15f, "Reading from bundle...");
                    }
                }
            }

            if (!fromBundle)
            {
                const auto resolvedResult = ResolveAssetKeyToPath(assetPath);
                if (resolvedResult.IsFailure())
                {
                    AssetLoadProgress::ClearProgress(assetPath);
                    LOG_CORE_ERROR("AudioClipAsset::LoadAsync: failed to resolve key '{}': {}",
                                   assetPath, resolvedResult.GetError().GetErrorMessage());
                    return nullptr;
                }

                resolvedPath = resolvedResult.GetValue().string();

                const auto guidResult = LoadOrCreateGuid(resolvedPath, {{"key", assetPath}, {"type", "AudioClip"}});
                if (guidResult.IsFailure())
                {
                    AssetLoadProgress::ClearProgress(assetPath);
                    LOG_CORE_ERROR("AudioClipAsset::LoadAsync: meta GUID failed for '{}': {}",
                                   resolvedPath, guidResult.GetError().GetErrorMessage());
                    return nullptr;
                }
                guid = guidResult.GetValue();

                std::ifstream in(resolvedPath, std::ios::in | std::ios::binary | std::ios::ate);
                if (!in.is_open())
                {
                    AssetLoadProgress::ClearProgress(assetPath);
                    LOG_CORE_ERROR("AudioClipAsset::LoadAsync: failed to open '{}'", resolvedPath);
                    return nullptr;
                }

                const auto size = in.tellg();
                in.seekg(0, std::ios::beg);
                rawBytes.resize(static_cast<size_t>(size));
                in.read(reinterpret_cast<char*>(rawBytes.data()), size);
            }

            AssetLoadProgress::SetProgress(assetPath, 0.40f, "Decoding audio...");

            // TODO: Integrate actual audio decoding (WAV/OGG/MP3/FLAC).
            // For now, create a stub decoded audio with silence.
            DecodedAudio audio;
            audio.SampleRateHz = settings.TargetSampleRateHz;
            audio.ChannelCount = settings.TargetChannelCount;
            audio.FrameCount = 0;

            LOG_CORE_WARN("AudioClipAsset::LoadAsync: audio decoding not yet implemented for '{}' ({} bytes raw)",
                          assetPath, rawBytes.size());

            auto asset = Ref<AudioClipAsset>(
                new AudioClipAsset(assetPath, guid, std::move(audio), settings));

            AssetLoadProgress::ClearProgress(assetPath);
            return asset;
        });
    }

    AudioClipAsset::Ptr AudioClipAsset::LoadBlocking(const std::string& assetPath)
    {
        Settings defaultSettings;
        return LoadBlocking(assetPath, defaultSettings);
    }

    AudioClipAsset::Ptr AudioClipAsset::LoadBlocking(const std::string& assetPath, const Settings& settings)
    {
        auto future = LoadAsync(assetPath, settings);
        future.wait();
        return future.get();
    }

    bool AudioClipAsset::Reload()
    {
        LOG_CORE_WARN("AudioClipAsset::Reload: not yet implemented for '{}'", GetKey());
        return false;
    }
}
