#pragma once

#include "Assets/Asset.h"
#include "Core/Input/InputAction.h"

#include <atomic>
#include <future>
#include <memory>
#include <string>

namespace Life::Assets
{
    // -----------------------------------------------------------------------------
    // InputActionsAssetResource
    // Unity-style Input Actions asset as a first-class engine Asset.
    // Stored as JSON in Assets/, loaded via InputActionAsset serialization.
    // -----------------------------------------------------------------------------
    class InputActionsAssetResource final : public Life::Asset
    {
    public:
        using Ptr = Ref<InputActionsAssetResource>;

        struct Settings
        {
            // reserved for importer settings
        };

        static std::future<Ptr> LoadAsync(const std::string& key, Settings settings = {});
        static Ptr LoadBlocking(const std::string& key, Settings settings = {});

        bool Reload() override;

        const Ref<Life::InputActionAsset>& GetValue() const { return m_Value; }
        uint64_t GetRevision() const { return m_Revision.load(std::memory_order_relaxed); }

    private:
        InputActionsAssetResource(std::string key, std::string guid, Ref<Life::InputActionAsset> value, Settings settings)
            : Asset(std::move(key), std::move(guid))
            , m_Value(std::move(value))
            , m_Settings(std::move(settings))
        {
        }

        std::string m_ResolvedPath;
        Ref<Life::InputActionAsset> m_Value;
        Settings m_Settings{};
        std::atomic<uint64_t> m_Revision{0};
    };
}
