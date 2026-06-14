#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace Life
{
    class AudioDevice final
    {
    public:
        struct VoiceState
        {
            std::string ClipAssetKey;
            float PlaybackTimeSeconds = 0.0f;
            float Volume = 1.0f;
            bool Loop = false;
            bool Playing = false;
        };

        struct VoiceUpdate
        {
            float PlaybackTimeSeconds = 0.0f;
            float Volume = 1.0f;
            bool Loop = false;
            bool Playing = false;
        };

        void Play(std::string sourceId, std::string clipAssetKey, float volume, bool loop);
        void Stop(const std::string& sourceId);
        void Update(const std::string& sourceId, const VoiceUpdate& update);
        void Clear();

        bool IsPlaying(const std::string& sourceId) const;
        std::size_t GetActiveVoiceCount() const noexcept;

    private:
        std::unordered_map<std::string, VoiceState> m_Voices;
    };
}
