#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Life::Audio
{
    // -----------------------------------------------------------------------------
    // AudioClip
    // Skeleton placeholder for the audio subsystem. This provides enough surface
    // area for AudioClipAsset to compile. A real implementation will replace this
    // once the audio system is built out.
    // -----------------------------------------------------------------------------
    class AudioClip
    {
    public:
        AudioClip() = default;

        uint32_t GetSampleRate() const { return m_SampleRate; }
        uint32_t GetChannelCount() const { return m_ChannelCount; }
        uint64_t GetFrameCount() const { return m_FrameCount; }
        float GetDurationSeconds() const { return m_DurationSeconds; }
        const std::string& GetDebugName() const { return m_DebugName; }

        const std::vector<float>& GetSamples() const { return m_Samples; }

        void SetSampleRate(uint32_t rate) { m_SampleRate = rate; }
        void SetChannelCount(uint32_t channels) { m_ChannelCount = channels; }
        void SetFrameCount(uint64_t frames) { m_FrameCount = frames; }
        void SetDurationSeconds(float duration) { m_DurationSeconds = duration; }
        void SetDebugName(const std::string& name) { m_DebugName = name; }
        void SetSamples(std::vector<float> samples) { m_Samples = std::move(samples); }

    private:
        uint32_t m_SampleRate = 48000;
        uint32_t m_ChannelCount = 2;
        uint64_t m_FrameCount = 0;
        float m_DurationSeconds = 0.0f;
        std::string m_DebugName;
        std::vector<float> m_Samples;
    };
}
