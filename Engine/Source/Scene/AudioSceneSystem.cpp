#include "Scene/AudioSceneSystem.h"

#include "Assets/AssetManager.h"
#include "Assets/AudioClipAsset.h"
#include "Audio/AudioDevice.h"
#include "Core/ServiceRegistry.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace Life
{
    namespace
    {
        Assets::AssetManager* TryGetAssetManager()
        {
            return GetServices().TryGet<Assets::AssetManager>();
        }

        AudioDevice* TryGetAudioDevice()
        {
            return GetServices().TryGet<AudioDevice>();
        }

        float GetClipDurationSeconds(const Assets::AudioClipAsset& clip)
        {
            const Assets::AudioClipAsset::DecodedAudio& audio = clip.GetDecodedAudio();
            if (audio.SampleRateHz == 0)
                return 0.0f;

            return static_cast<float>(audio.FrameCount) / static_cast<float>(audio.SampleRateHz);
        }
    }

    void AudioSceneSystem::OnSceneStart(Scene& scene)
    {
        AudioDevice* audioDevice = TryGetAudioDevice();
        auto view = scene.GetRegistry().view<AudioSourceComponent, IdComponent>();
        for (const entt::entity handle : view)
        {
            auto&& [source, id] = view.get<AudioSourceComponent, IdComponent>(handle);
            source.PlaybackTimeSeconds = 0.0f;
            source.Playing = source.PlayOnStart && !source.ClipAssetKey.empty();
            if (source.Playing && audioDevice != nullptr)
                audioDevice->Play(id.Id, source.ClipAssetKey, source.Volume, source.Loop);
        }
    }

    void AudioSceneSystem::OnSceneUpdate(Scene& scene, float timestep)
    {
        Assets::AssetManager* assetManager = TryGetAssetManager();
        AudioDevice* audioDevice = TryGetAudioDevice();
        auto view = scene.GetRegistry().view<AudioSourceComponent, IdComponent>();
        for (const entt::entity handle : view)
        {
            auto&& [source, id] = view.get<AudioSourceComponent, IdComponent>(handle);
            if (!source.Playing)
            {
                if (audioDevice != nullptr)
                    audioDevice->Stop(id.Id);
                continue;
            }

            float durationSeconds = 0.0f;
            if (assetManager != nullptr && !source.ClipAssetKey.empty())
            {
                if (Ref<Assets::AudioClipAsset> clip = assetManager->GetOrLoad<Assets::AudioClipAsset>(source.ClipAssetKey))
                    durationSeconds = GetClipDurationSeconds(*clip);
            }

            source.PlaybackTimeSeconds += std::max(timestep, 0.0f);
            if (durationSeconds > 0.0f && source.PlaybackTimeSeconds >= durationSeconds)
            {
                if (source.Loop)
                {
                    source.PlaybackTimeSeconds = std::fmod(source.PlaybackTimeSeconds, durationSeconds);
                }
                else
                {
                    source.PlaybackTimeSeconds = durationSeconds;
                    source.Playing = false;
                }
            }

            if (audioDevice != nullptr)
            {
                audioDevice->Update(id.Id, AudioDevice::VoiceUpdate{
                    source.PlaybackTimeSeconds,
                    source.Volume,
                    source.Loop,
                    source.Playing
                });
            }
        }
    }

    void AudioSceneSystem::OnSceneStop(Scene& scene)
    {
        AudioDevice* audioDevice = TryGetAudioDevice();
        auto view = scene.GetRegistry().view<AudioSourceComponent, IdComponent>();
        for (const entt::entity handle : view)
        {
            auto&& [source, id] = view.get<AudioSourceComponent, IdComponent>(handle);
            source.Playing = false;
            if (audioDevice != nullptr)
                audioDevice->Stop(id.Id);
        }
    }
}
