#include "Assets/MaterialAsset.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetDatabase.h"
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

    std::future<MaterialAsset::Ptr> MaterialAsset::LoadAsync(const std::string& key, Settings settings)
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
                        }
                    }
                }

                if (!fromBundle)
                {
                    const auto resolvedResult = ResolveAssetKeyToPath(key);
                    if (resolvedResult.IsFailure())
                    {
                        AssetLoadProgress::ClearProgress(key);
                        LOG_CORE_ERROR("MaterialAsset::LoadAsync: failed to resolve key '{}': {}",
                                       key, resolvedResult.GetError().GetErrorMessage());
                        return nullptr;
                    }

                    resolvedPath = resolvedResult.GetValue().string();

                    const auto guidResult = LoadOrCreateGuid(resolvedPath, {{"key", key}, {"type", "Material"}});
                    if (guidResult.IsFailure())
                    {
                        AssetLoadProgress::ClearProgress(key);
                        LOG_CORE_ERROR("MaterialAsset::LoadAsync: meta GUID failed for '{}': {}",
                                       resolvedPath, guidResult.GetError().GetErrorMessage());
                        return nullptr;
                    }
                    guid = guidResult.GetValue();

                    std::ifstream in(resolvedPath, std::ios::in | std::ios::binary);
                    if (!in.is_open())
                    {
                        AssetLoadProgress::ClearProgress(key);
                        LOG_CORE_ERROR("MaterialAsset::LoadAsync: failed to open '{}'", resolvedPath);
                        return nullptr;
                    }

                    std::ostringstream ss;
                    ss << in.rdbuf();
                    fileText = ss.str();
                }

                AssetLoadProgress::SetProgress(key, 0.40f, "Parsing material JSON...");

                auto asset = Ref<MaterialAsset>(
                    new MaterialAsset(key, guid, settings));
                asset->m_ResolvedPath = resolvedPath;

                try
                {
                    json j = json::parse(fileText);

                    if (j.contains("shader") && j["shader"].is_string())
                        asset->m_Shader = AssetHandle<ShaderAsset>(j["shader"].get<std::string>());

                    if (j.contains("mainTexture") && j["mainTexture"].is_string())
                        asset->m_MainTexture = AssetHandle<TextureAsset>(j["mainTexture"].get<std::string>());

                    if (j.contains("normalTexture") && j["normalTexture"].is_string())
                        asset->m_NormalTexture = AssetHandle<TextureAsset>(j["normalTexture"].get<std::string>());

                    asset->m_NormalStrength = j.value("normalStrength", 1.0f);
                    asset->m_Roughness = j.value("roughness", 0.5f);
                    asset->m_SpecularIntensity = j.value("specularIntensity", 0.5f);

                    if (j.contains("mainTextureSubRect") && j["mainTextureSubRect"].is_object())
                    {
                        const auto& sub = j["mainTextureSubRect"];
                        asset->m_HasMainTextureSubRect = true;
                        asset->m_MainTextureUvMin.x = sub.value("uMin", 0.0f);
                        asset->m_MainTextureUvMin.y = sub.value("vMin", 0.0f);
                        asset->m_MainTextureUvMax.x = sub.value("uMax", 1.0f);
                        asset->m_MainTextureUvMax.y = sub.value("vMax", 1.0f);
                    }

                    // Register dependencies in AssetDatabase
                    auto* db = GetServices().TryGet<AssetDatabase>();
                    if (db)
                    {
                        std::vector<std::string> deps;
                        if (!asset->m_Shader.GetGuid().empty())
                            deps.push_back(asset->m_Shader.GetGuid());
                        if (!asset->m_MainTexture.GetGuid().empty())
                            deps.push_back(asset->m_MainTexture.GetGuid());
                        if (!asset->m_NormalTexture.GetGuid().empty())
                            deps.push_back(asset->m_NormalTexture.GetGuid());
                        db->SetDependencies(guid, deps);
                    }
                }
                catch (const std::exception& e)
                {
                    AssetLoadProgress::ClearProgress(key);
                    LOG_CORE_ERROR("MaterialAsset::LoadAsync: JSON parse failed for '{}': {}", key, e.what());
                    return nullptr;
                }

                AssetLoadProgress::ClearProgress(key);
                return asset;
            }
            catch (const std::exception& e)
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("MaterialAsset::LoadAsync: unexpected exception for '{}': {}", key, e.what());
                return nullptr;
            }
            catch (...)
            {
                AssetLoadProgress::ClearProgress(key);
                LOG_CORE_ERROR("MaterialAsset::LoadAsync: unexpected exception for '{}'", key);
                return nullptr;
            }
        });
    }

    MaterialAsset::Ptr MaterialAsset::LoadBlocking(const std::string& key, Settings settings)
    {
        auto future = LoadAsync(key, settings);
        future.wait();
        return future.get();
    }

    bool MaterialAsset::Reload()
    {
        const std::string key = GetKey();

        std::string fileText;
        if (!m_ResolvedPath.empty() && m_ResolvedPath[0] != '<')
        {
            std::ifstream in(m_ResolvedPath, std::ios::in | std::ios::binary);
            if (!in.is_open())
            {
                LOG_CORE_ERROR("MaterialAsset::Reload: failed to open '{}'", m_ResolvedPath);
                return false;
            }
            std::ostringstream ss;
            ss << in.rdbuf();
            fileText = ss.str();
        }
        else
        {
            auto* bundle = GetServices().TryGet<AssetBundle>();
            if (bundle && bundle->IsEnabled() && bundle->IsLoaded())
            {
                const auto textResult = bundle->ReadAllTextByKey(key);
                if (textResult.IsSuccess())
                    fileText = textResult.GetValue();
            }
        }

        if (fileText.empty())
        {
            LOG_CORE_ERROR("MaterialAsset::Reload: no source for '{}'", key);
            return false;
        }

        try
        {
            json j = json::parse(fileText);

            AssetHandle<ShaderAsset> shader = m_Shader;
            AssetHandle<TextureAsset> mainTexture = m_MainTexture;
            AssetHandle<TextureAsset> normalTexture = m_NormalTexture;
            bool hasMainTextureSubRect = false;
            glm::vec2 mainTextureUvMin = glm::vec2(0.0f, 0.0f);
            glm::vec2 mainTextureUvMax = glm::vec2(1.0f, 1.0f);
            float normalStrength = j.value("normalStrength", 1.0f);
            float roughness = j.value("roughness", 0.5f);
            float specularIntensity = j.value("specularIntensity", 0.5f);

            if (j.contains("shader") && j["shader"].is_string())
                shader = AssetHandle<ShaderAsset>(j["shader"].get<std::string>());
            if (j.contains("mainTexture") && j["mainTexture"].is_string())
                mainTexture = AssetHandle<TextureAsset>(j["mainTexture"].get<std::string>());
            if (j.contains("normalTexture") && j["normalTexture"].is_string())
                normalTexture = AssetHandle<TextureAsset>(j["normalTexture"].get<std::string>());

            if (j.contains("mainTextureSubRect") && j["mainTextureSubRect"].is_object())
            {
                const auto& sub = j["mainTextureSubRect"];
                hasMainTextureSubRect = true;
                mainTextureUvMin.x = sub.value("uMin", 0.0f);
                mainTextureUvMin.y = sub.value("vMin", 0.0f);
                mainTextureUvMax.x = sub.value("uMax", 1.0f);
                mainTextureUvMax.y = sub.value("vMax", 1.0f);
            }

            m_Shader = std::move(shader);
            m_MainTexture = std::move(mainTexture);
            m_NormalTexture = std::move(normalTexture);
            m_HasMainTextureSubRect = hasMainTextureSubRect;
            m_MainTextureUvMin = mainTextureUvMin;
            m_MainTextureUvMax = mainTextureUvMax;
            m_NormalStrength = normalStrength;
            m_Roughness = roughness;
            m_SpecularIntensity = specularIntensity;

            if (auto* db = GetServices().TryGet<AssetDatabase>())
            {
                std::vector<std::string> deps;
                if (!m_Shader.GetGuid().empty())
                    deps.push_back(m_Shader.GetGuid());
                if (!m_MainTexture.GetGuid().empty())
                    deps.push_back(m_MainTexture.GetGuid());
                if (!m_NormalTexture.GetGuid().empty())
                    deps.push_back(m_NormalTexture.GetGuid());
                (void)db->SetDependencies(GetGuid(), deps);
            }
        }
        catch (const std::exception& e)
        {
            LOG_CORE_ERROR("MaterialAsset::Reload: JSON parse failed for '{}': {}", key, e.what());
            return false;
        }

        return true;
    }

    bool MaterialAsset::LoadFromJsonFile()
    {
        return Reload();
    }
}
