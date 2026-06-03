#include "Assets/AudioClipAsset.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetLoadCoordinator.h"
#include "Assets/AssetLoadProgress.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetUtils.h"

#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

#include <algorithm>
#include <cstring>
#include <fstream>

namespace Life::Assets
{
    namespace
    {
        uint16_t ReadU16(const std::vector<uint8_t>& bytes, std::size_t offset)
        {
            return static_cast<uint16_t>(bytes[offset] | (bytes[offset + 1] << 8));
        }

        uint32_t ReadU32(const std::vector<uint8_t>& bytes, std::size_t offset)
        {
            return static_cast<uint32_t>(bytes[offset] |
                (bytes[offset + 1] << 8) |
                (bytes[offset + 2] << 16) |
                (bytes[offset + 3] << 24));
        }

        float ClampSample(float value)
        {
            return std::clamp(value, -1.0f, 1.0f);
        }

        Result<AudioClipAsset::DecodedAudio> DecodeWavPcm(const std::vector<uint8_t>& bytes)
        {
            if (bytes.size() < 44 ||
                std::memcmp(bytes.data(), "RIFF", 4) != 0 ||
                std::memcmp(bytes.data() + 8, "WAVE", 4) != 0)
            {
                return Result<AudioClipAsset::DecodedAudio>(ErrorCode::FileCorrupted, "Audio file is not a RIFF/WAVE file");
            }

            uint16_t audioFormat = 0;
            uint16_t channelCount = 0;
            uint32_t sampleRate = 0;
            uint16_t bitsPerSample = 0;
            std::size_t dataOffset = 0;
            uint32_t dataSize = 0;

            std::size_t offset = 12;
            while (offset + 8 <= bytes.size())
            {
                const char* chunkId = reinterpret_cast<const char*>(bytes.data() + offset);
                const uint32_t chunkSize = ReadU32(bytes, offset + 4);
                const std::size_t chunkDataOffset = offset + 8;
                if (chunkDataOffset + chunkSize > bytes.size())
                    return Result<AudioClipAsset::DecodedAudio>(ErrorCode::FileCorrupted, "WAV chunk extends past end of file");

                if (std::memcmp(chunkId, "fmt ", 4) == 0)
                {
                    if (chunkSize < 16)
                        return Result<AudioClipAsset::DecodedAudio>(ErrorCode::FileCorrupted, "WAV fmt chunk is too small");

                    audioFormat = ReadU16(bytes, chunkDataOffset);
                    channelCount = ReadU16(bytes, chunkDataOffset + 2);
                    sampleRate = ReadU32(bytes, chunkDataOffset + 4);
                    bitsPerSample = ReadU16(bytes, chunkDataOffset + 14);
                }
                else if (std::memcmp(chunkId, "data", 4) == 0)
                {
                    dataOffset = chunkDataOffset;
                    dataSize = chunkSize;
                }

                offset = chunkDataOffset + chunkSize + (chunkSize & 1u);
            }

            if (channelCount == 0 || sampleRate == 0 || bitsPerSample == 0 || dataOffset == 0 || dataSize == 0)
                return Result<AudioClipAsset::DecodedAudio>(ErrorCode::FileCorrupted, "WAV file is missing fmt or data chunks");

            if (audioFormat != 1 && audioFormat != 3)
                return Result<AudioClipAsset::DecodedAudio>(ErrorCode::NotSupported, "Only PCM and IEEE float WAV files are supported");

            if (audioFormat == 3 && bitsPerSample != 32)
                return Result<AudioClipAsset::DecodedAudio>(ErrorCode::NotSupported, "Only 32-bit IEEE float WAV files are supported");

            const uint32_t bytesPerSample = bitsPerSample / 8u;
            if (bytesPerSample == 0 || dataSize % bytesPerSample != 0)
                return Result<AudioClipAsset::DecodedAudio>(ErrorCode::FileCorrupted, "Invalid WAV sample size");

            const uint64_t sampleCount = dataSize / bytesPerSample;
            if (sampleCount % channelCount != 0)
                return Result<AudioClipAsset::DecodedAudio>(ErrorCode::FileCorrupted, "WAV data is not frame aligned");

            AudioClipAsset::DecodedAudio audio;
            audio.SampleRateHz = sampleRate;
            audio.ChannelCount = channelCount;
            audio.FrameCount = sampleCount / channelCount;
            audio.Samples.reserve(static_cast<std::size_t>(sampleCount));

            for (uint64_t sampleIndex = 0; sampleIndex < sampleCount; ++sampleIndex)
            {
                const std::size_t sampleOffset = dataOffset + static_cast<std::size_t>(sampleIndex * bytesPerSample);
                float sample = 0.0f;
                if (audioFormat == 3)
                {
                    std::memcpy(&sample, bytes.data() + sampleOffset, sizeof(float));
                    sample = ClampSample(sample);
                }
                else if (bitsPerSample == 8)
                {
                    sample = (static_cast<float>(bytes[sampleOffset]) - 128.0f) / 128.0f;
                }
                else if (bitsPerSample == 16)
                {
                    const int16_t value = static_cast<int16_t>(ReadU16(bytes, sampleOffset));
                    sample = static_cast<float>(value) / 32768.0f;
                }
                else if (bitsPerSample == 24)
                {
                    int32_t value = bytes[sampleOffset] | (bytes[sampleOffset + 1] << 8) | (bytes[sampleOffset + 2] << 16);
                    if ((value & 0x00800000) != 0)
                        value |= static_cast<int32_t>(0xFF000000);
                    sample = static_cast<float>(value) / 8388608.0f;
                }
                else if (bitsPerSample == 32)
                {
                    const int32_t value = static_cast<int32_t>(ReadU32(bytes, sampleOffset));
                    sample = static_cast<float>(value) / 2147483648.0f;
                }
                else
                {
                    return Result<AudioClipAsset::DecodedAudio>(ErrorCode::NotSupported, "Unsupported PCM WAV bit depth");
                }

                audio.Samples.push_back(ClampSample(sample));
            }

            return audio;
        }
    }

    std::future<AudioClipAsset::Ptr> AudioClipAsset::LoadAsync(const std::string& assetPath)
    {
        Settings defaultSettings;
        return LoadAsync(assetPath, defaultSettings);
    }

    std::future<AudioClipAsset::Ptr> AudioClipAsset::LoadAsync(const std::string& assetPath, const Settings& settings)
    {
        const uint64_t generation = AssetLoadCoordinator::GetGeneration();
        const auto loadAssetPath = CreateRef<std::string>(assetPath);
        const auto loadSettings = CreateRef<Settings>(settings);

        return std::async(std::launch::async, [loadAssetPath, loadSettings, generation]() -> Ptr {
            const std::string& assetPath = *loadAssetPath;
            const Settings& settings = *loadSettings;
            try
            {
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

                const auto decodeResult = DecodeWavPcm(rawBytes);
                if (decodeResult.IsFailure())
                {
                    AssetLoadProgress::ClearProgress(assetPath);
                    LOG_CORE_ERROR("AudioClipAsset::LoadAsync: failed to decode '{}': {}",
                                   assetPath, decodeResult.GetError().GetErrorMessage());
                    return nullptr;
                }

                DecodedAudio audio = decodeResult.GetValue();

                auto asset = Ref<AudioClipAsset>(
                    new AudioClipAsset(assetPath, guid, std::move(audio), settings));

                AssetLoadProgress::ClearProgress(assetPath);
                return asset;
            }
            catch (const std::exception& e)
            {
                AssetLoadProgress::ClearProgress(assetPath);
                LOG_CORE_ERROR("AudioClipAsset::LoadAsync: unexpected exception for '{}': {}", assetPath, e.what());
                return nullptr;
            }
            catch (...)
            {
                AssetLoadProgress::ClearProgress(assetPath);
                LOG_CORE_ERROR("AudioClipAsset::LoadAsync: unexpected exception for '{}'", assetPath);
                return nullptr;
            }
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
        Ptr reloaded = LoadBlocking(GetKey(), m_Settings);
        if (!reloaded || reloaded->GetGuid() != GetGuid())
            return false;

        m_Audio = std::move(reloaded->m_Audio);
        return true;
    }
}
