#include "Assets/TextureAsset.h"

#include "Assets/AssetBundle.h"
#include "Assets/AssetLoadCoordinator.h"
#include "Assets/AssetLoadProgress.h"
#include "Assets/AssetManager.h"
#include "Assets/AssetPaths.h"
#include "Assets/AssetUtils.h"
#include "Assets/GeneratedAssetRuntimeRegistry.h"
#include "Assets/ImageDecode.h"

#include "Core/Log.h"
#include "Core/ServiceRegistry.h"
#include "Graphics/GraphicsDevice.h"

#include <fstream>
#include <thread>

namespace Life::Assets
{
    namespace
    {
        TextureFilterMode ToTextureFilterMode(TextureFilter filter) noexcept
        {
            switch (filter)
            {
            case TextureFilter::Nearest:
                return TextureFilterMode::Nearest;
            case TextureFilter::NearestMipmapNearest:
                return TextureFilterMode::NearestMipmapNearest;
            case TextureFilter::LinearMipmapLinear:
                return TextureFilterMode::LinearMipmapLinear;
            case TextureFilter::Linear:
            default:
                return TextureFilterMode::Linear;
            }
        }

        TextureWrapMode ToTextureWrapMode(TextureWrap wrap) noexcept
        {
            switch (wrap)
            {
            case TextureWrap::ClampToEdge:
                return TextureWrapMode::ClampToEdge;
            case TextureWrap::MirroredRepeat:
                return TextureWrapMode::MirroredRepeat;
            case TextureWrap::Repeat:
            default:
                return TextureWrapMode::Repeat;
            }
        }

        TextureSamplerDescription ToTextureSamplerDescription(const TextureSpecification& specification) noexcept
        {
            TextureSamplerDescription samplerDescription;
            samplerDescription.MinFilter = ToTextureFilterMode(specification.MinFilter);
            samplerDescription.MagFilter = ToTextureFilterMode(specification.MagFilter);
            samplerDescription.WrapU = ToTextureWrapMode(specification.WrapU);
            samplerDescription.WrapV = ToTextureWrapMode(specification.WrapV);
            samplerDescription.WrapW = TextureWrapMode::Repeat;
            return samplerDescription;
        }
    }

    std::future<TextureAsset::Ptr> TextureAsset::LoadAsync(const std::string& assetPath, const TextureSpecification& specification)
    {
        const uint64_t generation = AssetLoadCoordinator::GetGeneration();
        const auto loadAssetPath = CreateRef<std::string>(assetPath);
        const auto loadSpecification = CreateRef<TextureSpecification>(specification);

        return std::async(std::launch::async, [loadAssetPath, loadSpecification, generation]() -> Ptr {
            const std::string& assetPath = *loadAssetPath;
            const TextureSpecification& specification = *loadSpecification;
            try
            {
                AssetLoadProgress::SetProgress(assetPath, 0.05f, "Resolving...");

                if (!AssetLoadCoordinator::IsGenerationCurrent(generation))
                {
                    AssetLoadProgress::ClearProgress(assetPath);
                    return nullptr;
                }

                bool fromBundle = false;
                std::vector<uint8_t> bundleBytes;
                std::string guid;
                std::string resolvedPath;
                std::string debugName = assetPath;

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
                            bundleBytes = bytesResult.GetValue();
                            guid = entry->Guid;
                            AssetLoadProgress::SetProgress(assetPath, 0.15f, "Reading from bundle...");
                        }
                    }
                }

                if (!fromBundle)
                {
                    const auto resolvedPathResult = ResolveAssetKeyToPath(assetPath);
                    if (resolvedPathResult.IsFailure())
                    {
                        AssetLoadProgress::ClearProgress(assetPath);
                        LOG_CORE_ERROR("TextureAsset::LoadAsync: failed to resolve key '{}': {}",
                                       assetPath, resolvedPathResult.GetError().GetErrorMessage());
                        return nullptr;
                    }

                    resolvedPath = resolvedPathResult.GetValue().string();
                    debugName = resolvedPath;
                    AssetLoadProgress::SetProgress(assetPath, 0.15f, "Reading...");

                    auto guidResult = LoadOrCreateGuid(resolvedPath);
                    if (guidResult.IsFailure())
                    {
                        AssetLoadProgress::ClearProgress(assetPath);
                        LOG_CORE_ERROR("TextureAsset::LoadAsync: failed GUID/meta for '{}': {}",
                                       resolvedPath, guidResult.GetError().GetErrorMessage());
                        return nullptr;
                    }
                    guid = guidResult.GetValue();
                }

                AssetLoadProgress::SetProgress(assetPath, 0.35f, "Decoding...");

                auto decodedResult = fromBundle
                    ? DecodeToRGBA8FromMemory(bundleBytes.data(), bundleBytes.size(), debugName, specification.FlipVerticallyOnLoad)
                    : DecodeToRGBA8(resolvedPath, specification.FlipVerticallyOnLoad);

                if (decodedResult.IsFailure())
                {
                    auto ppmFallback = fromBundle
                        ? TryDecodePpmP3ToRGBA8FromMemory(bundleBytes.data(), bundleBytes.size(), debugName)
                        : TryDecodePpmP3ToRGBA8(resolvedPath);
                    if (ppmFallback.IsSuccess())
                    {
                        if (specification.FlipVerticallyOnLoad)
                        {
                            auto img = ppmFallback.GetValue();
                            FlipVerticalRGBA8(img);
                            ppmFallback = img;
                        }
                        decodedResult = ppmFallback;
                    }
                }

                if (decodedResult.IsFailure())
                {
                    AssetLoadProgress::ClearProgress(assetPath);
                    LOG_CORE_ERROR("TextureAsset::LoadAsync: decode failed for '{}': {}",
                                   debugName, decodedResult.GetError().GetErrorMessage());
                    return nullptr;
                }

                const auto& decoded = decodedResult.GetValue();
                AssetLoadProgress::SetProgress(assetPath, 0.75f, "Uploading to GPU...");

                auto* device = GetServices().TryGet<GraphicsDevice>();
                if (!device)
                {
                    AssetLoadProgress::ClearProgress(assetPath);
                    LOG_CORE_ERROR("TextureAsset::LoadAsync: GraphicsDevice not available for '{}'", assetPath);
                    return nullptr;
                }

                TextureDescription desc;
                desc.DebugName = assetPath;
                desc.Width = decoded.Width;
                desc.Height = decoded.Height;
                desc.Format = TextureFormat::RGBA8_UNORM;
                desc.MipLevels = 1;
                desc.Sampler = ToTextureSamplerDescription(specification);

                auto texture = TextureResource::Create2D(*device, desc, decoded.Pixels.data());
                if (!texture || !texture->IsValid())
                {
                    AssetLoadProgress::ClearProgress(assetPath);
                    LOG_CORE_ERROR("TextureAsset::LoadAsync: GPU upload failed for '{}'", assetPath);
                    return nullptr;
                }

                auto asset = Ref<TextureAsset>(
                    new TextureAsset(assetPath, guid, std::move(texture), specification));

                AssetLoadProgress::ClearProgress(assetPath);
                return asset;
            }
            catch (const std::exception& e)
            {
                AssetLoadProgress::ClearProgress(assetPath);
                LOG_CORE_ERROR("TextureAsset::LoadAsync: unexpected exception for '{}': {}", assetPath, e.what());
                return nullptr;
            }
            catch (...)
            {
                AssetLoadProgress::ClearProgress(assetPath);
                LOG_CORE_ERROR("TextureAsset::LoadAsync: unexpected exception for '{}'", assetPath);
                return nullptr;
            }
        });
    }

    TextureAsset::Ptr TextureAsset::LoadBlocking(const std::string& assetPath, const TextureSpecification& specification)
    {
        auto future = LoadAsync(assetPath, specification);
        future.wait();
        return future.get();
    }

    void TextureAsset::ApplySpecification(const TextureSpecification& spec)
    {
        m_Specification = spec;
        if (m_Texture)
            m_Texture->SetSamplerDescription(ToTextureSamplerDescription(spec));
    }

    bool TextureAsset::Reload()
    {
        const std::string key = GetKey();
        const auto resolvedResult = ResolveAssetKeyToPath(key);
        if (resolvedResult.IsFailure())
        {
            LOG_CORE_ERROR("TextureAsset::Reload: failed to resolve '{}'", key);
            return false;
        }

        const std::string resolvedPath = resolvedResult.GetValue().string();
        auto decodedResult = DecodeToRGBA8(resolvedPath, m_Specification.FlipVerticallyOnLoad);
        if (decodedResult.IsFailure())
        {
            LOG_CORE_ERROR("TextureAsset::Reload: decode failed for '{}': {}",
                           resolvedPath, decodedResult.GetError().GetErrorMessage());
            return false;
        }

        const auto& decoded = decodedResult.GetValue();

        auto* device = GetServices().TryGet<GraphicsDevice>();
        if (!device)
        {
            LOG_CORE_ERROR("TextureAsset::Reload: GraphicsDevice not available");
            return false;
        }

        TextureDescription desc;
        desc.DebugName = key;
        desc.Width = decoded.Width;
        desc.Height = decoded.Height;
        desc.Format = TextureFormat::RGBA8_UNORM;
        desc.MipLevels = 1;
        desc.Sampler = ToTextureSamplerDescription(m_Specification);

        auto newTexture = TextureResource::Create2D(*device, desc, decoded.Pixels.data());
        if (!newTexture || !newTexture->IsValid())
        {
            LOG_CORE_ERROR("TextureAsset::Reload: GPU upload failed for '{}'", key);
            return false;
        }

        m_Texture = std::move(newTexture);
        return true;
    }
}
