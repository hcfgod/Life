#pragma once


#include "Scene/Components.h"

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace Life
{
    class Entity;

    class Scene
    {
    public:
        enum class State : uint8_t
        {
            Unloaded = 0,
            Loading = 1,
            Ready = 2
        };

        explicit Scene(std::string name = "Untitled");
        ~Scene() = default;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) noexcept = default;
        Scene& operator=(Scene&&) noexcept = default;

        const std::string& GetName() const noexcept;
        void SetName(std::string name);

        const std::filesystem::path& GetSourcePath() const noexcept;
        void SetSourcePath(std::filesystem::path sourcePath);
        bool HasSourcePath() const noexcept;

        State GetState() const noexcept { return m_State; }
        bool IsLoading() const noexcept { return m_State == State::Loading; }
        bool IsReady() const noexcept { return m_State == State::Ready; }
        void SetState(State state) noexcept;

        Entity CreateEntity(std::string tag = "Entity");
        Entity CreateChildEntity(Entity parent, std::string tag = "Entity");
        bool DestroyEntity(Entity entity);
        void Clear();

        bool IsValid(Entity entity) const noexcept;
        std::size_t GetEntityCount() const noexcept;

        Entity WrapEntity(entt::entity handle) noexcept;
        Entity WrapEntity(entt::entity handle) const noexcept;
        Entity FindEntityById(std::string_view id);
        Entity FindEntityById(std::string_view id) const;
        Entity FindEntityByTag(std::string_view tag);
        Entity FindEntityByTag(std::string_view tag) const;

        bool SetParent(Entity child, Entity parent);
        void RemoveParent(Entity child);
        Entity GetParent(Entity entity) const;
        bool HasParent(Entity entity) const;
        std::vector<Entity> GetChildren(Entity entity) const;
        std::size_t GetSiblingIndex(Entity entity) const;
        bool SetSiblingIndex(Entity entity, std::size_t index);
        std::vector<Entity> GetRootEntities() const;
        std::vector<Entity> GetEntities() const;
        bool IsDescendantOf(Entity entity, Entity ancestor) const;

        glm::mat4 GetLocalTransformMatrix(Entity entity) const;
        glm::mat4 GetWorldTransformMatrix(Entity entity) const;

        entt::registry& GetRegistry() noexcept;
        const entt::registry& GetRegistry() const noexcept;

    private:
        friend class Entity;

        struct ParentRelation
        {
            entt::entity Child = entt::null;
            entt::entity Parent = entt::null;
        };

        void InitializeEntity(entt::entity handle, std::string tag);
        void DetachFromParent(entt::entity child, bool makeRoot = false);
        void RemoveFromRootOrder(entt::entity handle);
        bool WouldCreateCycle(ParentRelation relation) const;
        static glm::mat4 ComposeTransform(const TransformComponent& transform);
        static std::string GenerateEntityId();

        std::string m_Name;
        std::filesystem::path m_SourcePath;
        State m_State = State::Unloaded;
        entt::registry m_Registry;
        std::vector<entt::entity> m_RootEntities;
    };

}

#include "Scene/Entity.h"

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

        return m_Scene->m_Registry.remove<TComponent>(m_Handle) > 0;
    }
}
