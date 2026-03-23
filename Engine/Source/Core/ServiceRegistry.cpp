#include "Core/ServiceRegistry.h"

#include <vector>

namespace Life
{
    namespace
    {
        ServiceRegistry s_FallbackRegistry;
        std::mutex s_GlobalRegistryMutex;
        std::vector<ServiceRegistry*> s_GlobalRegistryStack;
    }

    void ServiceRegistry::Clear()
    {
        std::scoped_lock lock(m_Mutex);
        m_Services.clear();
    }

    ServiceRegistry& GetServices()
    {
        std::scoped_lock lock(s_GlobalRegistryMutex);
        ServiceRegistry* registry = s_GlobalRegistryStack.empty() ? &s_FallbackRegistry : s_GlobalRegistryStack.back();
        return *registry;
    }

    void PushGlobalServiceRegistry(ServiceRegistry& registry)
    {
        std::scoped_lock lock(s_GlobalRegistryMutex);
        s_GlobalRegistryStack.push_back(&registry);
    }

    bool PopGlobalServiceRegistry(ServiceRegistry& registry)
    {
        std::scoped_lock lock(s_GlobalRegistryMutex);

        for (auto it = s_GlobalRegistryStack.end(); it != s_GlobalRegistryStack.begin();)
        {
            --it;
            if (*it == &registry)
            {
                s_GlobalRegistryStack.erase(it);
                return true;
            }
        }

        return false;
    }

    void SetGlobalServiceRegistry(ServiceRegistry* registry)
    {
        std::scoped_lock lock(s_GlobalRegistryMutex);
        s_GlobalRegistryStack.clear();

        if (registry != nullptr)
            s_GlobalRegistryStack.push_back(registry);
    }
}
