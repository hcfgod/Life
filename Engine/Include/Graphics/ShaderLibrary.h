#pragma once

#include "Graphics/Shader.h"
#include "Core/Memory.h"

#include <string>
#include <unordered_map>

namespace Life
{
    class GraphicsDevice;

    class ShaderLibrary
    {
    public:
        explicit ShaderLibrary(GraphicsDevice& device);
        ~ShaderLibrary();

        ShaderLibrary(const ShaderLibrary&) = delete;
        ShaderLibrary& operator=(const ShaderLibrary&) = delete;

        Shader* Load(const std::string& name, const ShaderDescription& desc,
                     const void* bytecode, size_t bytecodeSize);

        Shader* LoadFromFile(const std::string& name, const ShaderDescription& desc,
                             const std::string& filePath);

        Shader* Get(const std::string& name) const;
        bool Exists(const std::string& name) const;
        bool Remove(const std::string& name);
        void Clear();

        size_t GetCount() const noexcept { return m_Shaders.size(); }

    private:
        GraphicsDevice& m_Device;
        std::unordered_map<std::string, Scope<Shader>> m_Shaders;
    };
}
