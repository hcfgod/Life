#include "Audio/AudioDevice.h"

#include <algorithm>
#include <utility>

namespace Life
{
    void AudioDevice::Play(std::string sourceId, std::string clipAssetKey, float volume, bool loop)
    {
        if (sourceId.empty() || clipAssetKey.empty())
            return;

        VoiceState& voice = m_Voices[std::move(sourceId)];
        voice.ClipAssetKey = std::move(clipAssetKey);
        voice.PlaybackTimeSeconds = 0.0f;
        voice.Volume = std::clamp(volume, 0.0f, 1.0f);
        voice.Loop = loop;
        voice.Playing = true;
    }

    void AudioDevice::Stop(const std::string& sourceId)
    {
        if (sourceId.empty())
            return;

        if (auto it = m_Voices.find(sourceId); it != m_Voices.end())
            it->second.Playing = false;
    }

    void AudioDevice::Update(const std::string& sourceId, const VoiceUpdate& update)
    {
        if (sourceId.empty())
            return;

        auto it = m_Voices.find(sourceId);
        if (it == m_Voices.end())
            return;

        it->second.PlaybackTimeSeconds = update.PlaybackTimeSeconds;
        it->second.Volume = std::clamp(update.Volume, 0.0f, 1.0f);
        it->second.Loop = update.Loop;
        it->second.Playing = update.Playing;
    }

    void AudioDevice::Clear()
    {
        m_Voices.clear();
    }

    bool AudioDevice::IsPlaying(const std::string& sourceId) const
    {
        const auto it = m_Voices.find(sourceId);
        return it != m_Voices.end() && it->second.Playing;
    }

    std::size_t AudioDevice::GetActiveVoiceCount() const noexcept
    {
        std::size_t count = 0;
        for (const auto& [sourceId, voice] : m_Voices)
        {
            (void)sourceId;
            if (voice.Playing)
                ++count;
        }
        return count;
    }
}
