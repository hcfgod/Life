#include "Assets/AnimatorControllerAsset.h"

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

    static AnimatorControllerAsset::Data ParseAnimatorControllerJson(const json& j)
    {
        AnimatorControllerAsset::Data data;
        data.Name = j.value("name", std::string{});
        data.DefaultStateName = j.value("defaultState", std::string{});

        if (j.contains("parameters") && j["parameters"].is_array())
        {
            for (const auto& p : j["parameters"])
            {
                AnimatorControllerAsset::ParameterDefinition param;
                param.Name = p.value("name", std::string{});
                param.Type = static_cast<AnimatorControllerAsset::ParameterType>(p.value("type", 0));
                param.DefaultBool = p.value("defaultBool", false);
                param.DefaultFloat = p.value("defaultFloat", 0.0f);
                param.DefaultInteger = p.value("defaultInteger", 0);
                data.Parameters.push_back(std::move(param));
            }
        }

        if (j.contains("states") && j["states"].is_array())
        {
            for (const auto& s : j["states"])
            {
                AnimatorControllerAsset::StateDefinition state;
                state.Name = s.value("name", std::string{});
                state.ClipKey = s.value("clipKey", std::string{});
                state.SpeedMultiplier = s.value("speedMultiplier", 1.0f);
                state.LoopOverrideEnabled = s.value("loopOverrideEnabled", false);
                state.LoopOverride = s.value("loopOverride", true);

                if (s.contains("transitions") && s["transitions"].is_array())
                {
                    for (const auto& t : s["transitions"])
                    {
                        AnimatorControllerAsset::TransitionDefinition trans;
                        trans.ToState = t.value("toState", std::string{});
                        trans.HasExitTime = t.value("hasExitTime", false);
                        trans.ExitTimeNormalized = t.value("exitTimeNormalized", 1.0f);
                        trans.DurationSeconds = t.value("durationSeconds", 0.1f);
                        trans.CanTransitionToSelf = t.value("canTransitionToSelf", false);

                        if (t.contains("conditions") && t["conditions"].is_array())
                        {
                            for (const auto& c : t["conditions"])
                            {
                                AnimatorControllerAsset::TransitionCondition cond;
                                cond.ParameterName = c.value("parameterName", std::string{});
                                cond.Mode = static_cast<AnimatorControllerAsset::ConditionMode>(c.value("mode", 0));
                                cond.BoolValue = c.value("boolValue", false);
                                cond.FloatThreshold = c.value("floatThreshold", 0.0f);
                                cond.IntegerThreshold = c.value("integerThreshold", 0);
                                trans.Conditions.push_back(std::move(cond));
                            }
                        }

                        state.Transitions.push_back(std::move(trans));
                    }
                }

                data.States.push_back(std::move(state));
            }
        }

        return data;
    }

    static Result<std::pair<std::string, std::string>> ResolveAndReadJsonAsset(
        const std::string& key, const char* typeName)
    {
        auto* bundle = GetServices().TryGet<AssetBundle>();
        if (bundle && bundle->IsEnabled() && bundle->IsLoaded())
        {
            const auto entry = bundle->FindEntryByKey(key);
            if (entry.has_value())
            {
                const auto textResult = bundle->ReadAllTextByKey(key);
                if (textResult.IsSuccess())
                    return std::make_pair(entry->Guid, textResult.GetValue());
            }
        }

        const auto resolvedResult = ResolveAssetKeyToPath(key);
        if (resolvedResult.IsFailure())
            return Result<std::pair<std::string, std::string>>(resolvedResult.GetError());

        const std::string resolvedPath = resolvedResult.GetValue().string();

        const auto guidResult = LoadOrCreateGuid(resolvedPath, {{"key", key}, {"type", typeName}});
        if (guidResult.IsFailure())
            return Result<std::pair<std::string, std::string>>(guidResult.GetError());

        std::ifstream in(resolvedPath, std::ios::in | std::ios::binary);
        if (!in.is_open())
            return Result<std::pair<std::string, std::string>>(ErrorCode::FileNotFound, "Failed to open: " + resolvedPath);

        std::ostringstream ss;
        ss << in.rdbuf();
        return std::make_pair(guidResult.GetValue(), ss.str());
    }

    std::future<AnimatorControllerAsset::Ptr> AnimatorControllerAsset::LoadAsync(const std::string& key)
    {
        Settings s;
        return LoadAsync(key, s);
    }

    std::future<AnimatorControllerAsset::Ptr> AnimatorControllerAsset::LoadAsync(const std::string& key, Settings settings)
    {
        const uint64_t generation = AssetLoadCoordinator::GetGeneration();
        const auto loadKey = CreateRef<std::string>(key);
        const auto loadSettings = CreateRef<Settings>(settings);

        return std::async(std::launch::async, [loadKey, loadSettings, generation]() -> Ptr {
            const std::string& key = *loadKey;
            const Settings& settings = *loadSettings;
            try
            {
                AssetLoadProgress::SetProgress(key, 0.05f, "Resolving...");

                if (!AssetLoadCoordinator::IsGenerationCurrent(generation))
                {
                    AssetLoadProgress::ClearProgress(key);
                    return nullptr;
                }

                const auto result = ResolveAndReadJsonAsset(key, "AnimatorController");
                if (result.IsFailure())
                {
                    AssetLoadProgress::ClearProgress(key);
                    LOG_CORE_ERROR("AnimatorControllerAsset::LoadAsync: {}", result.GetError().GetErrorMessage());
                    return nullptr;
                }

                const auto& [guid, fileText] = result.GetValue();
                AssetLoadProgress::SetProgress(key, 0.40f, "Parsing...");

                try
                {
                    json j = json::parse(fileText);
                    Data data = ParseAnimatorControllerJson(j);

                    auto asset = Ref<AnimatorControllerAsset>(
                        new AnimatorControllerAsset(key, guid, std::move(data), settings));

                    AssetLoadProgress::ClearProgress(key);
                    return asset;
                }
                catch (const std::exception& e)
                {
                    AssetLoadProgress::ClearProgress(key);
                    LOG_CORE_ERROR("AnimatorControllerAsset::LoadAsync: JSON parse failed for '{}': {}", key, e.what());
                    return nullptr;
                }
            }
            catch (const std::exception& e)
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("AnimatorControllerAsset::LoadAsync: unexpected exception for '{}': {}", key, e.what());
                return nullptr;
            }
            catch (...)
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("AnimatorControllerAsset::LoadAsync: unexpected exception for '{}'", key);
                return nullptr;
            }
        });
    }

    AnimatorControllerAsset::Ptr AnimatorControllerAsset::LoadBlocking(const std::string& key)
    {
        Settings s;
        return LoadBlocking(key, s);
    }

    AnimatorControllerAsset::Ptr AnimatorControllerAsset::LoadBlocking(const std::string& key, Settings settings)
    {
        auto future = LoadAsync(key, settings);
        future.wait();
        return future.get();
    }

    bool AnimatorControllerAsset::Reload()
    {
        const std::string key = GetKey();
        const auto result = ResolveAndReadJsonAsset(key, "AnimatorController");
        if (result.IsFailure())
        {
            LOG_CORE_ERROR("AnimatorControllerAsset::Reload: {}", result.GetError().GetErrorMessage());
            return false;
        }

        try
        {
            json j = json::parse(result.GetValue().second);
            m_Data = ParseAnimatorControllerJson(j);
            m_Revision.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        catch (const std::exception& e)
        {
            LOG_CORE_ERROR("AnimatorControllerAsset::Reload: JSON parse failed for '{}': {}", key, e.what());
            return false;
        }
    }
}
