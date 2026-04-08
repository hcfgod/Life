#include "Assets/AnimationClipAsset.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetLoadCoordinator.h"
#include "Assets/AssetLoadProgress.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetUtils.h"

#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <sstream>

namespace Life::Assets
{
    using json = nlohmann::json;

    static AnimationClipAsset::Data ParseAnimationClipJson(const json& j)
    {
        AnimationClipAsset::Data data;
        data.Name = j.value("name", std::string{});
        data.Loop = j.value("loop", true);
        data.DurationSeconds = j.value("durationSeconds", 1.0f);
        data.SamplesPerSecond = j.value("samplesPerSecond", 30.0f);

        if (j.contains("spriteSubRectTrack") && j["spriteSubRectTrack"].is_array())
        {
            for (const auto& kf : j["spriteSubRectTrack"])
            {
                AnimationClipAsset::SpriteSubRectKeyframe k;
                k.TimeSeconds = kf.value("time", 0.0f);
                k.UvMin.x = kf.value("uMin", 0.0f);
                k.UvMin.y = kf.value("vMin", 0.0f);
                k.UvMax.x = kf.value("uMax", 1.0f);
                k.UvMax.y = kf.value("vMax", 1.0f);
                data.SpriteSubRectTrack.push_back(k);
            }
        }

        if (j.contains("spriteTextureTrack") && j["spriteTextureTrack"].is_array())
        {
            for (const auto& kf : j["spriteTextureTrack"])
            {
                AnimationClipAsset::SpriteTextureKeyframe k;
                k.TimeSeconds = kf.value("time", 0.0f);
                k.TextureKey = kf.value("textureKey", std::string{});
                data.SpriteTextureTrack.push_back(k);
            }
        }

        auto parseVec3Track = [](const json& arr) -> std::vector<AnimationClipAsset::Vector3Keyframe> {
            std::vector<AnimationClipAsset::Vector3Keyframe> track;
            for (const auto& kf : arr)
            {
                AnimationClipAsset::Vector3Keyframe k;
                k.TimeSeconds = kf.value("time", 0.0f);
                k.Value.x = kf.value("x", 0.0f);
                k.Value.y = kf.value("y", 0.0f);
                k.Value.z = kf.value("z", 0.0f);
                k.Interpolation = static_cast<AnimationClipAsset::InterpolationMode>(kf.value("interpolation", 1));
                track.push_back(k);
            }
            return track;
        };

        if (j.contains("positionTrack") && j["positionTrack"].is_array())
            data.PositionTrack = parseVec3Track(j["positionTrack"]);
        if (j.contains("scaleTrack") && j["scaleTrack"].is_array())
            data.ScaleTrack = parseVec3Track(j["scaleTrack"]);
        if (j.contains("rotationTrack") && j["rotationTrack"].is_array())
            data.RotationTrack = parseVec3Track(j["rotationTrack"]);

        if (j.contains("eventTrack") && j["eventTrack"].is_array())
        {
            for (const auto& kf : j["eventTrack"])
            {
                AnimationClipAsset::EventKeyframe k;
                k.TimeSeconds = kf.value("time", 0.0f);
                k.Name = kf.value("name", std::string{});
                k.StringPayload = kf.value("stringPayload", std::string{});
                k.FloatPayload = kf.value("floatPayload", 0.0f);
                k.IntegerPayload = kf.value("integerPayload", 0);
                k.BooleanPayload = kf.value("booleanPayload", false);
                data.EventTrack.push_back(k);
            }
        }

        return data;
    }

    static Result<std::pair<std::string, std::string>> ResolveAndReadJsonAsset(
        const std::string& key, const char* typeName)
    {
        std::string guid;
        std::string fileText;

        auto* bundle = GetServices().TryGet<AssetBundle>();
        if (bundle && bundle->IsEnabled() && bundle->IsLoaded())
        {
            const auto entry = bundle->FindEntryByKey(key);
            if (entry.has_value())
            {
                const auto textResult = bundle->ReadAllTextByKey(key);
                if (textResult.IsSuccess())
                {
                    return std::make_pair(entry->Guid, textResult.GetValue());
                }
            }
        }

        const auto resolvedResult = ResolveAssetKeyToPath(key);
        if (resolvedResult.IsFailure())
        {
            return Result<std::pair<std::string, std::string>>(resolvedResult.GetError());
        }

        const std::string resolvedPath = resolvedResult.GetValue().string();

        const auto guidResult = LoadOrCreateGuid(resolvedPath, {{"key", key}, {"type", typeName}});
        if (guidResult.IsFailure())
        {
            return Result<std::pair<std::string, std::string>>(guidResult.GetError());
        }
        guid = guidResult.GetValue();

        std::ifstream in(resolvedPath, std::ios::in | std::ios::binary);
        if (!in.is_open())
        {
            return Result<std::pair<std::string, std::string>>(
                ErrorCode::FileNotFound, std::string("Failed to open: ") + resolvedPath);
        }

        std::ostringstream ss;
        ss << in.rdbuf();
        fileText = ss.str();

        return std::make_pair(guid, fileText);
    }

    std::future<AnimationClipAsset::Ptr> AnimationClipAsset::LoadAsync(const std::string& key)
    {
        Settings s;
        return LoadAsync(key, s);
    }

    std::future<AnimationClipAsset::Ptr> AnimationClipAsset::LoadAsync(const std::string& key, Settings settings)
    {
        const uint64_t generation = AssetLoadCoordinator::GetGeneration();

        return std::async(std::launch::async, [key, settings, generation]() -> Ptr {
            AssetLoadProgress::SetProgress(key, 0.05f, "Resolving...");

            if (!AssetLoadCoordinator::IsGenerationCurrent(generation))
            {
                AssetLoadProgress::ClearProgress(key);
                return nullptr;
            }

            const auto result = ResolveAndReadJsonAsset(key, "AnimationClip");
            if (result.IsFailure())
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("AnimationClipAsset::LoadAsync: {}", result.GetError().GetErrorMessage());
                return nullptr;
            }

            const auto& [guid, fileText] = result.GetValue();

            AssetLoadProgress::SetProgress(key, 0.40f, "Parsing...");

            try
            {
                json j = json::parse(fileText);
                Data data = ParseAnimationClipJson(j);

                auto asset = Ref<AnimationClipAsset>(
                    new AnimationClipAsset(key, guid, std::move(data), settings));

                AssetLoadProgress::ClearProgress(key);
                return asset;
            }
            catch (const std::exception& e)
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("AnimationClipAsset::LoadAsync: JSON parse failed for '{}': {}", key, e.what());
                return nullptr;
            }
        });
    }

    AnimationClipAsset::Ptr AnimationClipAsset::LoadBlocking(const std::string& key)
    {
        Settings s;
        return LoadBlocking(key, s);
    }

    AnimationClipAsset::Ptr AnimationClipAsset::LoadBlocking(const std::string& key, Settings settings)
    {
        auto future = LoadAsync(key, settings);
        future.wait();
        return future.get();
    }

    bool AnimationClipAsset::Reload()
    {
        const std::string key = GetKey();
        const auto result = ResolveAndReadJsonAsset(key, "AnimationClip");
        if (result.IsFailure())
        {
            LOG_CORE_ERROR("AnimationClipAsset::Reload: {}", result.GetError().GetErrorMessage());
            return false;
        }

        try
        {
            json j = json::parse(result.GetValue().second);
            m_Data = ParseAnimationClipJson(j);
            m_Revision.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_CORE_ERROR("AnimationClipAsset::Reload: JSON parse failed for '{}': {}", key, e.what());
            return false;
        }
    }
}
