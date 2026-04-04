#include "Core/LifePCH.h"
#include "Graphics/Shader.h"
#include "Graphics/GraphicsDevice.h"
#include "Core/Log.h"

#include <nvrhi/nvrhi.h>
#include <fstream>

namespace Life
{
    struct Shader::Impl
    {
        nvrhi::ShaderHandle Handle;
    };

    Shader::~Shader() = default;

    Shader::Shader(Shader&& other) noexcept
        : m_Impl(std::move(other.m_Impl))
        , m_Description(std::move(other.m_Description))
    {
    }

    Shader& Shader::operator=(Shader&& other) noexcept
    {
        if (this != &other)
        {
            m_Impl = std::move(other.m_Impl);
            m_Description = std::move(other.m_Description);
        }
        return *this;
    }

    bool Shader::IsValid() const noexcept
    {
        return m_Impl && m_Impl->Handle;
    }

    nvrhi::IShader* Shader::GetNativeHandle() const
    {
        return m_Impl ? m_Impl->Handle.Get() : nullptr;
    }

    static nvrhi::ShaderType ToNvrhiShaderType(ShaderStage stage)
    {
        switch (stage)
        {
        case ShaderStage::Vertex:   return nvrhi::ShaderType::Vertex;
        case ShaderStage::Pixel:    return nvrhi::ShaderType::Pixel;
        case ShaderStage::Geometry: return nvrhi::ShaderType::Geometry;
        case ShaderStage::Hull:     return nvrhi::ShaderType::Hull;
        case ShaderStage::Domain:   return nvrhi::ShaderType::Domain;
        case ShaderStage::Compute:  return nvrhi::ShaderType::Compute;
        default:                    return nvrhi::ShaderType::None;
        }
    }

    Scope<Shader> Shader::CreateFromBytecode(GraphicsDevice& device, const ShaderDescription& desc,
                                              const void* bytecode, size_t bytecodeSize)
    {
        nvrhi::IDevice* nvrhiDevice = device.GetNvrhiDevice();
        if (!nvrhiDevice)
            return nullptr;

        if (!bytecode || bytecodeSize == 0)
        {
            LOG_CORE_ERROR("Shader::CreateFromBytecode: No bytecode provided for '{}'.", desc.DebugName);
            return nullptr;
        }

        nvrhi::ShaderType shaderType = ToNvrhiShaderType(desc.Stage);
        if (shaderType == nvrhi::ShaderType::None)
        {
            LOG_CORE_ERROR("Shader::CreateFromBytecode: Invalid shader stage for '{}'.", desc.DebugName);
            return nullptr;
        }

        nvrhi::ShaderDesc shaderDesc;
        shaderDesc.shaderType = shaderType;
        shaderDesc.debugName = desc.DebugName.c_str();
        shaderDesc.entryName = desc.EntryPoint.c_str();

        nvrhi::ShaderHandle handle = nvrhiDevice->createShader(shaderDesc, bytecode, bytecodeSize);
        if (!handle)
        {
            LOG_CORE_ERROR("Shader::CreateFromBytecode: Failed to create shader '{}'.", desc.DebugName);
            return nullptr;
        }

        Scope<Shader> shader(new Shader());
        shader->m_Impl = CreateScope<Impl>();
        shader->m_Impl->Handle = handle;
        shader->m_Description = desc;
        return shader;
    }

    Scope<Shader> Shader::CreateFromFile(GraphicsDevice& device, const ShaderDescription& desc,
                                          const std::string& filePath)
    {
        std::ifstream file(filePath, std::ios::binary | std::ios::ate);
        if (!file.is_open())
        {
            LOG_CORE_ERROR("Shader::CreateFromFile: Failed to open shader file '{}'.", filePath);
            return nullptr;
        }

        const auto fileSize = file.tellg();
        if (fileSize <= 0)
        {
            LOG_CORE_ERROR("Shader::CreateFromFile: Shader file '{}' is empty.", filePath);
            return nullptr;
        }

        std::vector<char> bytecode(static_cast<size_t>(fileSize));
        file.seekg(0);
        file.read(bytecode.data(), fileSize);

        return CreateFromBytecode(device, desc, bytecode.data(), bytecode.size());
    }
}
