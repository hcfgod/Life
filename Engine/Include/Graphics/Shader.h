#pragma once

#include "Graphics/GraphicsTypes.h"
#include "Core/Memory.h"

#include <cstdint>
#include <string>
#include <vector>

namespace nvrhi
{
    class IShader;
}

namespace Life
{
    class GraphicsDevice;

    struct ShaderDescription
    {
        std::string DebugName;
        ShaderStage Stage = ShaderStage::None;
        std::string EntryPoint = "main";
    };

    class Shader
    {
    public:
        ~Shader();

        Shader(const Shader&) = delete;
        Shader& operator=(const Shader&) = delete;
        Shader(Shader&&) noexcept;
        Shader& operator=(Shader&&) noexcept;

        const ShaderDescription& GetDescription() const noexcept { return m_Description; }
        ShaderStage GetStage() const noexcept { return m_Description.Stage; }
        const std::string& GetDebugName() const noexcept { return m_Description.DebugName; }
        bool IsValid() const noexcept;

        nvrhi::IShader* GetNativeHandle() const;

        static Scope<Shader> CreateFromBytecode(GraphicsDevice& device, const ShaderDescription& desc,
                                                 const void* bytecode, size_t bytecodeSize);

        static Scope<Shader> CreateFromFile(GraphicsDevice& device, const ShaderDescription& desc,
                                             const std::string& filePath);

    private:
        friend class Renderer;

        Shader() = default;

        struct Impl;
        Scope<Impl> m_Impl;
        ShaderDescription m_Description;
    };
}
