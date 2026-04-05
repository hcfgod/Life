#pragma once

#include "Assets/Asset.h"

#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // AudioClipAsset
    // CPU-decoded audio clip asset. Decode happens on background thread.
    // Stores PCM float32 samples in the engine mixer format.
    // -----------------------------------------------------------------------------
    class AudioClipAsset final : public Life::Asset
    {
    public:
        using Ptr = std::shared_ptr<AudioClipAsset>;

        struct Settings
        {
            uint32_t TargetSampleRateHz = 48000;
            uint32_t TargetChannelCount = 2;
        };

        struct DecodedAudio
        {
            uint32_t SampleRateHz = 0;
            uint32_t ChannelCount = 0;
            uint64_t FrameCount = 0;
            std::vector<float> Samples;
        };

        static std::future<Ptr> LoadAsync(const std::string& assetPath);
        static std::future<Ptr> LoadAsync(const std::string& assetPath, const Settings& settings);
        static Ptr LoadBlocking(const std::string& assetPath);
        static Ptr LoadBlocking(const std::string& assetPath, const Settings& settings);

        const DecodedAudio& GetDecodedAudio() const { return m_Audio; }
        const Settings& GetSettings() const { return m_Settings; }

        bool Reload() override;

    private:
        AudioClipAsset(std::string key, std::string guid, DecodedAudio audio, Settings settings)
            : Asset(std::move(key), std::move(guid))
            , m_Audio(std::move(audio))
            , m_Settings(settings)
        {
        }

        DecodedAudio m_Audio{};
        Settings m_Settings{};
    };
}
