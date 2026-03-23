#pragma once

#include <mutex>
#include <stdexcept>
#include <type_traits>
#include <typeindex>
#include <unordered_map>

namespace Life
{
    class ServiceRegistry
    {
    public:
        ServiceRegistry() = default;
        ~ServiceRegistry() = default;

        ServiceRegistry(const ServiceRegistry&) = delete;
        ServiceRegistry& operator=(const ServiceRegistry&) = delete;
        ServiceRegistry(ServiceRegistry&&) = delete;
        ServiceRegistry& operator=(ServiceRegistry&&) = delete;

        template<typename TService>
        void Register(TService& service)
        {
            using TStoredService = std::remove_cv_t<std::remove_reference_t<TService>>;
            static_assert(!std::is_pointer_v<TStoredService>);

            std::scoped_lock lock(m_Mutex);
            m_Services[std::type_index(typeid(TStoredService))] = &service;
        }

        template<typename TService>
        bool Unregister()
        {
            using TStoredService = std::remove_cv_t<std::remove_reference_t<TService>>;
            std::scoped_lock lock(m_Mutex);
            return m_Services.erase(std::type_index(typeid(TStoredService))) > 0;
        }

        template<typename TService>
        TService& Get()
        {
            if (TService* service = TryGet<TService>())
                return *service;

            throw std::logic_error("ServiceRegistry does not contain the requested service.");
        }

        template<typename TService>
        const TService& Get() const
        {
            if (const TService* service = TryGet<TService>())
                return *service;

            throw std::logic_error("ServiceRegistry does not contain the requested service.");
        }

        template<typename TService>
        TService* TryGet()
        {
            using TStoredService = std::remove_cv_t<std::remove_reference_t<TService>>;
            std::scoped_lock lock(m_Mutex);
            const auto it = m_Services.find(std::type_index(typeid(TStoredService)));
            return it != m_Services.end() ? static_cast<TStoredService*>(it->second) : nullptr;
        }

        template<typename TService>
        const TService* TryGet() const
        {
            using TStoredService = std::remove_cv_t<std::remove_reference_t<TService>>;
            std::scoped_lock lock(m_Mutex);
            const auto it = m_Services.find(std::type_index(typeid(TStoredService)));
            return it != m_Services.end() ? static_cast<const TStoredService*>(it->second) : nullptr;
        }

        template<typename TService>
        bool Has() const
        {
            using TStoredService = std::remove_cv_t<std::remove_reference_t<TService>>;
            std::scoped_lock lock(m_Mutex);
            return m_Services.find(std::type_index(typeid(TStoredService))) != m_Services.end();
        }

        void Clear();

    private:
        mutable std::mutex m_Mutex;
        std::unordered_map<std::type_index, void*> m_Services;
    };

    ServiceRegistry& GetServices();
    void PushGlobalServiceRegistry(ServiceRegistry& registry);
    bool PopGlobalServiceRegistry(ServiceRegistry& registry);
    void SetGlobalServiceRegistry(ServiceRegistry* registry);
}
