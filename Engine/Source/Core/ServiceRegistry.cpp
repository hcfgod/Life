#include "Core/ServiceRegistry.h"

namespace Life
{
    namespace
    {
        ServiceRegistry s_FallbackRegistry;
        std::mutex s_GlobalRegistryMutex;
        ServiceRegistry* s_GlobalRegistry = nullptr;
    }

    void ServiceRegistry::Clear()
    {
        std::scoped_lock lock(m_Mutex);
        m_Services.clear();
    }

    ServiceRegistry& GetServices()
    {
        std::scoped_lock lock(s_GlobalRegistryMutex);
        ServiceRegistry* registry = s_GlobalRegistry != nullptr ? s_GlobalRegistry : &s_FallbackRegistry;
        return *registry;
    }

    void SetGlobalServiceRegistry(ServiceRegistry* registry)
    {
        std::scoped_lock lock(s_GlobalRegistryMutex);
        s_GlobalRegistry = registry;
    }
}
