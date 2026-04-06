#include "Assets/InputActionsAssetResource.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetLoadCoordinator.h"
#include "Assets/AssetLoadProgress.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetUtils.h"

#include "Core/Log.h"
#include "Core/ServiceRegistry.h"

#include <fstream>
#include <sstream>
#include <string_view>

namespace Life::Assets
{
    static std::shared_ptr<Life::InputActionAsset> DeserializeInputActions(const std::string& jsonText, std::string_view debugName)
    {
        auto asset = std::make_shared<Life::InputActionAsset>();
        InputActionAssetLoadOptions opts;
        opts.DebugName.assign(debugName.begin(), debugName.end());
        const auto result = InputActionAssetSerializer::LoadIntoFromString(*asset, jsonText, opts);
        if (result.IsFailure())
        {
            LOG_CORE_ERROR("InputActionsAssetResource: deserialize failed for '{}': {}",
                           debugName, result.GetError().GetErrorMessage());
            return nullptr;
        }
        return asset;
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

    std::future<InputActionsAssetResource::Ptr> InputActionsAssetResource::LoadAsync(const std::string& key, Settings settings)
    {
        const uint64_t generation = AssetLoadCoordinator::GetGeneration();

        return std::async(std::launch::async, [key, settings, generation]() -> Ptr {
            AssetLoadProgress::SetProgress(key, 0.05f, "Resolving...");

            if (!AssetLoadCoordinator::IsGenerationCurrent(generation))
            {
                AssetLoadProgress::ClearProgress(key);
                return nullptr;
            }

            const auto result = ResolveAndReadJsonAsset(key, "InputActions");
            if (result.IsFailure())
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("InputActionsAssetResource::LoadAsync: {}", result.GetError().GetErrorMessage());
                return nullptr;
            }

            const auto& [guid, fileText] = result.GetValue();
            AssetLoadProgress::SetProgress(key, 0.40f, "Parsing...");

            auto value = DeserializeInputActions(fileText, key);
            if (!value)
            {
                AssetLoadProgress::ClearProgress(key);
                return nullptr;
            }

            auto asset = std::shared_ptr<InputActionsAssetResource>(
                new InputActionsAssetResource(key, guid, std::move(value), settings));

            AssetLoadProgress::ClearProgress(key);
            return asset;
        });
    }

    InputActionsAssetResource::Ptr InputActionsAssetResource::LoadBlocking(const std::string& key, Settings settings)
    {
        auto future = LoadAsync(key, settings);
        future.wait();
        return future.get();
    }

    bool InputActionsAssetResource::Reload()
    {
        const std::string key = GetKey();
        const auto result = ResolveAndReadJsonAsset(key, "InputActions");
        if (result.IsFailure())
        {
            LOG_CORE_ERROR("InputActionsAssetResource::Reload: {}", result.GetError().GetErrorMessage());
            return false;
        }

        auto value = DeserializeInputActions(result.GetValue().second, key);
        if (!value)
            return false;

        m_Value = std::move(value);
        m_Revision.fetch_add(1, std::memory_order_relaxed);
        return true;
    }
}
