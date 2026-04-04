#include "Core/LifePCH.h"
#include "Graphics/ShaderLibrary.h"
#include "Core/Log.h"

namespace Life
{
    ShaderLibrary::ShaderLibrary(GraphicsDevice& device)
        : m_Device(device)
    {
    }

    ShaderLibrary::~ShaderLibrary() = default;

    Shader* ShaderLibrary::Load(const std::string& name, const ShaderDescription& desc,
                                 const void* bytecode, size_t bytecodeSize)
    {
        if (Exists(name))
        {
            LOG_CORE_WARN("ShaderLibrary::Load: Shader '{}' already exists, replacing.", name);
            Remove(name);
        }

        auto shader = Shader::CreateFromBytecode(m_Device, desc, bytecode, bytecodeSize);
        if (!shader)
        {
            LOG_CORE_ERROR("ShaderLibrary::Load: Failed to create shader '{}'.", name);
            return nullptr;
        }

        Shader* rawPtr = shader.get();
        m_Shaders[name] = std::move(shader);
        return rawPtr;
    }

    Shader* ShaderLibrary::LoadFromFile(const std::string& name, const ShaderDescription& desc,
                                         const std::string& filePath)
    {
        if (Exists(name))
        {
            LOG_CORE_WARN("ShaderLibrary::LoadFromFile: Shader '{}' already exists, replacing.", name);
            Remove(name);
        }

        auto shader = Shader::CreateFromFile(m_Device, desc, filePath);
        if (!shader)
        {
            LOG_CORE_ERROR("ShaderLibrary::LoadFromFile: Failed to load shader '{}' from '{}'.", name, filePath);
            return nullptr;
        }

        Shader* rawPtr = shader.get();
        m_Shaders[name] = std::move(shader);
        return rawPtr;
    }

    Shader* ShaderLibrary::Get(const std::string& name) const
    {
        auto it = m_Shaders.find(name);
        if (it == m_Shaders.end())
            return nullptr;
        return it->second.get();
    }

    bool ShaderLibrary::Exists(const std::string& name) const
    {
        return m_Shaders.find(name) != m_Shaders.end();
    }

    bool ShaderLibrary::Remove(const std::string& name)
    {
        return m_Shaders.erase(name) > 0;
    }

    void ShaderLibrary::Clear()
    {
        m_Shaders.clear();
    }
}
