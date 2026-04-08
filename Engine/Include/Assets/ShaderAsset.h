#pragma once

#include "Assets/Asset.h"
#include "Assets/ShaderStageParsing.h"

#include <future>
#include <memory>
#include <string>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // ShaderAsset
    // Wraps parsed shader stages loaded from the asset pipeline.
    // Life uses pre-compiled SPIR-V via nvrhi, so this asset stores the parsed
    // GLSL source stages for pipeline creation.
    // -----------------------------------------------------------------------------
    class ShaderAsset final : public Life::Asset
    {
    public:
        using Ptr = Ref<ShaderAsset>;

        struct Settings
        {
            std::string Name;
        };

        static std::future<Ptr> LoadAsync(const std::string& key, const Settings& settings = {});
        static Ptr LoadBlocking(const std::string& key, const Settings& settings = {});

        const ParsedShaderStages& GetStages() const { return m_Stages; }
        const std::string& GetName() const { return m_Stages.Name; }

        bool Reload() override;

    private:
        friend class ShaderAssetFactory;

        ShaderAsset(std::string key, std::string guid, ParsedShaderStages stages, Settings settings)
            : Asset(std::move(key), std::move(guid))
            , m_Stages(std::move(stages))
            , m_Settings(std::move(settings))
        {
        }

        ParsedShaderStages m_Stages;
        Settings m_Settings{};
    };
}
