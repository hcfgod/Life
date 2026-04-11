#pragma once

#include "Graphics/Detail/ImGuiRendererBackend.h"

namespace Life
{
    class VulkanGraphicsDevice;

    namespace Detail
    {
#if LIFE_HAS_IMGUI_VULKAN
        class ImGuiVulkanRendererBackend final : public ImGuiRendererBackend
        {
        public:
            explicit ImGuiVulkanRendererBackend(VulkanGraphicsDevice& graphicsDevice);
            ~ImGuiVulkanRendererBackend() override;

            bool Initialize() override;
            void Shutdown() noexcept override;
            void NewFrame() override;
            void RenderDrawData(ImDrawData* drawData) override;
            void* GetTextureHandle(TextureResource& texture) override;
            void ReleaseTextureHandle(TextureResource& texture) noexcept override;

        private:
            struct Impl;

            VulkanGraphicsDevice& m_GraphicsDevice;
            Scope<Impl> m_Impl;
            bool m_Initialized = false;
        };
#endif
    }
}
