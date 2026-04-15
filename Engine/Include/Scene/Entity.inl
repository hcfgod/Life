#pragma once

#include <type_traits>
#include <utility>

namespace Life
{
    template<typename TComponent, typename... TArguments>
    TComponent& Entity::AddComponent(TArguments&&... arguments)
    {
        return m_Scene->m_Registry.emplace<TComponent>(m_Handle, std::forward<TArguments>(arguments)...);
    }

    template<typename TComponent, typename... TArguments>
    TComponent& Entity::AddOrReplaceComponent(TArguments&&... arguments)
    {
        return m_Scene->m_Registry.emplace_or_replace<TComponent>(m_Handle, std::forward<TArguments>(arguments)...);
    }

    template<typename TComponent>
    bool Entity::HasComponent() const
    {
        return m_Scene != nullptr && m_Scene->m_Registry.all_of<TComponent>(m_Handle);
    }

    template<typename TComponent>
    TComponent& Entity::GetComponent()
    {
        return m_Scene->m_Registry.get<TComponent>(m_Handle);
    }

    template<typename TComponent>
    const TComponent& Entity::GetComponent() const
    {
        return m_Scene->m_Registry.get<TComponent>(m_Handle);
    }

    template<typename TComponent>
    TComponent* Entity::TryGetComponent() noexcept
    {
        return m_Scene != nullptr ? m_Scene->m_Registry.try_get<TComponent>(m_Handle) : nullptr;
    }

    template<typename TComponent>
    const TComponent* Entity::TryGetComponent() const noexcept
    {
        return m_Scene != nullptr ? m_Scene->m_Registry.try_get<TComponent>(m_Handle) : nullptr;
    }

    template<typename TComponent>
    bool Entity::RemoveComponent()
    {
        if (m_Scene == nullptr)
            return false;

        if constexpr (std::is_same_v<TComponent, IdComponent> ||
                      std::is_same_v<TComponent, TagComponent> ||
                      std::is_same_v<TComponent, TransformComponent> ||
                      std::is_same_v<TComponent, HierarchyComponent>)
        {
            return false;
        }

        if constexpr (std::is_same_v<TComponent, CameraComponent>)
        {
            if (m_Scene->GetCameraCount() <= 1)
                return false;

            const bool removed = m_Scene->m_Registry.remove<TComponent>(m_Handle) > 0;
            if (removed)
                m_Scene->NormalizeCameraPrimaryState();
            return removed;
        }

        return m_Scene->m_Registry.remove<TComponent>(m_Handle) > 0;
    }
}
