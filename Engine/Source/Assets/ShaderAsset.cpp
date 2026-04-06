#include "Assets/ShaderAsset.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetLoadCoordinator.h"
#include "Assets/AssetLoadProgress.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetUtils.h"
#include "Assets/GeneratedAssetRuntimeRegistry.h"

#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

#include <fstream>
#include <sstream>

namespace Life::Assets
{
    std::future<ShaderAsset::Ptr> ShaderAsset::LoadAsync(const std::string& key, const Settings& settings)
    {
        const uint64_t generation = AssetLoadCoordinator::GetGeneration();

        return std::async(std::launch::async, [key, settings, generation]() -> Ptr {
            AssetLoadProgress::SetProgress(key, 0.05f, "Resolving...");

            if (!AssetLoadCoordinator::IsGenerationCurrent(generation))
            {
                AssetLoadProgress::ClearProgress(key);
                return nullptr;
            }

            bool fromBundle = false;
            std::string resolvedPath;
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
                        fromBundle = true;
                        guid = entry->Guid;
                        resolvedPath = "<AssetBundle>";
                        fileText = textResult.GetValue();
                        AssetLoadProgress::SetProgress(key, 0.20f, "Reading from bundle...");
                    }
                }
            }

            if (!fromBundle)
            {
                const auto resolvedResult = ResolveAssetKeyToPath(key);
                if (resolvedResult.IsFailure())
                {
                    AssetLoadProgress::ClearProgress(key);
                    LOG_CORE_ERROR("ShaderAsset::LoadAsync: failed to resolve key '{}': {}",
                                   key, resolvedResult.GetError().GetErrorMessage());
                    return nullptr;
                }

                resolvedPath = resolvedResult.GetValue().string();
                AssetLoadProgress::SetProgress(key, 0.12f, "Reading source...");

                const auto guidResult = LoadOrCreateGuid(resolvedPath, {{"key", key}, {"type", "Shader"}});
                if (guidResult.IsFailure())
                {
                    AssetLoadProgress::ClearProgress(key);
                    LOG_CORE_ERROR("ShaderAsset::LoadAsync: meta GUID failed for '{}': {}",
                                   resolvedPath, guidResult.GetError().GetErrorMessage());
                    return nullptr;
                }
                guid = guidResult.GetValue();

                std::ifstream in(resolvedPath, std::ios::in | std::ios::binary);
                if (!in.is_open())
                {
                    AssetLoadProgress::ClearProgress(key);
                    LOG_CORE_ERROR("ShaderAsset::LoadAsync: failed to open '{}'", resolvedPath);
                    return nullptr;
                }

                std::ostringstream ss;
                ss << in.rdbuf();
                fileText = ss.str();
            }

            AssetLoadProgress::SetProgress(key, 0.30f, "Parsing...");

            const ParseCombinedGlslInput parseInput{ key, resolvedPath, fileText, settings.Name };
            const auto parsedResult = ParseCombinedGlsl(parseInput);
            if (parsedResult.IsFailure())
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("ShaderAsset::LoadAsync: parse failed for '{}': {}",
                               resolvedPath, parsedResult.GetError().GetErrorMessage());
                return nullptr;
            }

            auto parsed = parsedResult.GetValue();

            AssetLoadProgress::SetProgress(key, 0.50f, "Validating...");

            const auto preparedResult = PrepareShaderStagesForActiveGraphicsAPI(std::move(parsed), resolvedPath);
            if (preparedResult.IsFailure())
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("ShaderAsset::LoadAsync: preparation failed for '{}': {}",
                               resolvedPath, preparedResult.GetError().GetErrorMessage());
                return nullptr;
            }

            parsed = preparedResult.GetValue();

            auto asset = std::shared_ptr<ShaderAsset>(
                new ShaderAsset(key, guid, std::move(parsed), settings));

            AssetLoadProgress::ClearProgress(key);
            return asset;
        });
    }

    ShaderAsset::Ptr ShaderAsset::LoadBlocking(const std::string& key, const Settings& settings)
    {
        auto future = LoadAsync(key, settings);
        future.wait();
        return future.get();
    }

    bool ShaderAsset::Reload()
    {
        const std::string key = GetKey();

        const auto resolvedResult = ResolveAssetKeyToPath(key);
        if (resolvedResult.IsFailure())
        {
            LOG_CORE_ERROR("ShaderAsset::Reload: failed to resolve key '{}': {}", key, resolvedResult.GetError().GetErrorMessage());
            return false;
        }

        const std::string resolvedPath = resolvedResult.GetValue().string();

        std::ifstream in(resolvedPath, std::ios::in | std::ios::binary);
        if (!in.is_open())
        {
            LOG_CORE_ERROR("ShaderAsset::Reload: failed to open '{}'", resolvedPath);
            return false;
        }

        std::ostringstream ss;
        ss << in.rdbuf();
        const std::string fileText = ss.str();

        const ParseCombinedGlslInput parseInput{ key, resolvedPath, fileText, m_Settings.Name };
        const auto parsedResult = ParseCombinedGlsl(parseInput);
        if (parsedResult.IsFailure())
        {
            LOG_CORE_ERROR("ShaderAsset::Reload: parse failed for '{}': {}", resolvedPath, parsedResult.GetError().GetErrorMessage());
            return false;
        }

        auto parsed = parsedResult.GetValue();
        const auto preparedResult = PrepareShaderStagesForActiveGraphicsAPI(std::move(parsed), resolvedPath);
        if (preparedResult.IsFailure())
        {
            LOG_CORE_ERROR("ShaderAsset::Reload: preparation failed for '{}': {}", resolvedPath, preparedResult.GetError().GetErrorMessage());
            return false;
        }

        m_Stages = preparedResult.GetValue();
        return true;
    }
}
