#pragma once

#include <entt/entity/entity.hpp>

#include <string>
#include <utility>
#include <vector>

namespace Life
{
    class Scene;

    class Entity
    {
    public:
        Entity() = default;
        Entity(entt::entity handle, Scene* scene) noexcept;

        bool IsValid() const noexcept;
        explicit operator bool() const noexcept { return IsValid(); }

        entt::entity GetHandle() const noexcept { return m_Handle; }
        Scene& GetScene();
        const Scene& GetScene() const;

        const std::string& GetId() const;
        const std::string& GetTag() const;
        void SetTag(std::string tag);

        template<typename TComponent, typename... TArguments>
        TComponent& AddComponent(TArguments&&... arguments);

        template<typename TComponent, typename... TArguments>
        TComponent& AddOrReplaceComponent(TArguments&&... arguments);

        template<typename TComponent>
        bool HasComponent() const;

        template<typename TComponent>
        TComponent& GetComponent();

        template<typename TComponent>
        const TComponent& GetComponent() const;

        template<typename TComponent>
        TComponent* TryGetComponent() noexcept;

        template<typename TComponent>
        const TComponent* TryGetComponent() const noexcept;

        template<typename TComponent>
        bool RemoveComponent();

        Entity GetParent() const;
        bool HasParent() const;
        std::vector<Entity> GetChildren() const;
        bool SetParent(Entity parent);
        void RemoveParent();
        bool IsDescendantOf(Entity ancestor) const;

        bool operator==(const Entity& other) const noexcept = default;

    private:
        friend class Scene;

        entt::entity m_Handle = entt::null;
        Scene* m_Scene = nullptr;
    };
}
