#include "Scene/Scene.h"

#include "Assets/AssetUtils.h"

#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace Life
{
    namespace
    {
        Entity MakeEntity(entt::entity handle, Scene* scene) noexcept
        {
            return Entity(handle, scene);
        }
    }

    Scene::Scene(std::string name)
        : m_Name(std::move(name))
    {
    }

    const std::string& Scene::GetName() const noexcept
    {
        return m_Name;
    }

    void Scene::SetName(std::string name)
    {
        m_Name = std::move(name);
    }

    const std::filesystem::path& Scene::GetSourcePath() const noexcept
    {
        return m_SourcePath;
    }

    void Scene::SetSourcePath(std::filesystem::path sourcePath)
    {
        m_SourcePath = std::move(sourcePath);
    }

    bool Scene::HasSourcePath() const noexcept
    {
        return !m_SourcePath.empty();
    }

    void Scene::SetState(State state) noexcept
    {
        m_State = state;
    }

    Entity Scene::CreateEntity(std::string tag)
    {
        entt::entity handle = m_Registry.create();
        InitializeEntity(handle, std::move(tag));
        m_RootEntities.push_back(handle);
        return MakeEntity(handle, this);
    }

    Entity Scene::CreateChildEntity(Entity parent, std::string tag)
    {
        Entity child = CreateEntity(std::move(tag));
        SetParent(child, parent);
        return child;
    }

    bool Scene::DestroyEntity(Entity entity)
    {
        if (!IsValid(entity))
            return false;

        std::vector<Entity> children = GetChildren(entity);
        for (Entity child : children)
            DestroyEntity(child);

        DetachFromParent(entity.GetHandle());
        RemoveFromRootOrder(entity.GetHandle());
        m_Registry.destroy(entity.GetHandle());
        return true;
    }

    void Scene::Clear()
    {
        m_Registry.clear();
        m_RootEntities.clear();
        m_State = State::Unloaded;
    }

    bool Scene::IsValid(Entity entity) const noexcept
    {
        return entity.m_Scene == this && entity.GetHandle() != entt::null && m_Registry.valid(entity.GetHandle());
    }

    std::size_t Scene::GetEntityCount() const noexcept
    {
        std::size_t count = 0;
        for ([[maybe_unused]] const entt::entity handle : m_Registry.view<IdComponent>())
            ++count;
        return count;
    }

    Entity Scene::WrapEntity(entt::entity handle) noexcept
    {
        return MakeEntity(handle, this);
    }

    Entity Scene::WrapEntity(entt::entity handle) const noexcept
    {
        return MakeEntity(handle, const_cast<Scene*>(this));
    }

    Entity Scene::FindEntityById(std::string_view id)
    {
        auto view = m_Registry.view<IdComponent>();
        for (entt::entity handle : view)
        {
            const IdComponent& component = view.get<IdComponent>(handle);
            if (component.Id == id)
                return WrapEntity(handle);
        }

        return {};
    }

    Entity Scene::FindEntityById(std::string_view id) const
    {
        auto view = m_Registry.view<IdComponent>();
        for (entt::entity handle : view)
        {
            const IdComponent& component = view.get<IdComponent>(handle);
            if (component.Id == id)
                return WrapEntity(handle);
        }

        return {};
    }

    Entity Scene::FindEntityByTag(std::string_view tag)
    {
        auto view = m_Registry.view<TagComponent>();
        for (entt::entity handle : view)
        {
            const TagComponent& component = view.get<TagComponent>(handle);
            if (component.Tag == tag)
                return WrapEntity(handle);
        }

        return {};
    }

    Entity Scene::FindEntityByTag(std::string_view tag) const
    {
        auto view = m_Registry.view<TagComponent>();
        for (entt::entity handle : view)
        {
            const TagComponent& component = view.get<TagComponent>(handle);
            if (component.Tag == tag)
                return WrapEntity(handle);
        }

        return {};
    }

    bool Scene::SetParent(Entity child, Entity parent)
    {
        if (!IsValid(child) || !IsValid(parent) || child == parent)
            return false;

        if (WouldCreateCycle(child.GetHandle(), parent.GetHandle()))
            return false;

        DetachFromParent(child.GetHandle());
        RemoveFromRootOrder(child.GetHandle());

        HierarchyComponent& childHierarchy = m_Registry.get<HierarchyComponent>(child.GetHandle());
        HierarchyComponent& parentHierarchy = m_Registry.get<HierarchyComponent>(parent.GetHandle());
        childHierarchy.Parent = parent.GetHandle();
        parentHierarchy.Children.push_back(child.GetHandle());
        return true;
    }

    void Scene::RemoveParent(Entity child)
    {
        if (!IsValid(child))
            return;

        DetachFromParent(child.GetHandle(), true);
    }

    Entity Scene::GetParent(Entity entity) const
    {
        if (!IsValid(entity))
            return {};

        const HierarchyComponent& hierarchy = m_Registry.get<HierarchyComponent>(entity.GetHandle());
        if (hierarchy.Parent == entt::null || !m_Registry.valid(hierarchy.Parent))
            return {};

        return WrapEntity(hierarchy.Parent);
    }

    bool Scene::HasParent(Entity entity) const
    {
        if (!IsValid(entity))
            return false;

        const HierarchyComponent& hierarchy = m_Registry.get<HierarchyComponent>(entity.GetHandle());
        return hierarchy.Parent != entt::null && m_Registry.valid(hierarchy.Parent);
    }

    std::vector<Entity> Scene::GetChildren(Entity entity) const
    {
        std::vector<Entity> children;
        if (!IsValid(entity))
            return children;

        const HierarchyComponent& hierarchy = m_Registry.get<HierarchyComponent>(entity.GetHandle());
        children.reserve(hierarchy.Children.size());
        for (entt::entity childHandle : hierarchy.Children)
        {
            if (m_Registry.valid(childHandle))
                children.push_back(WrapEntity(childHandle));
        }

        return children;
    }

    std::size_t Scene::GetSiblingIndex(Entity entity) const
    {
        if (!IsValid(entity))
            return 0;

        const HierarchyComponent& hierarchy = m_Registry.get<HierarchyComponent>(entity.GetHandle());
        if (hierarchy.Parent != entt::null && m_Registry.valid(hierarchy.Parent))
        {
            const HierarchyComponent& parentHierarchy = m_Registry.get<HierarchyComponent>(hierarchy.Parent);
            const auto it = std::find(parentHierarchy.Children.begin(), parentHierarchy.Children.end(), entity.GetHandle());
            return it != parentHierarchy.Children.end()
                ? static_cast<std::size_t>(std::distance(parentHierarchy.Children.begin(), it))
                : parentHierarchy.Children.size();
        }

        const auto it = std::find(m_RootEntities.begin(), m_RootEntities.end(), entity.GetHandle());
        return it != m_RootEntities.end()
            ? static_cast<std::size_t>(std::distance(m_RootEntities.begin(), it))
            : m_RootEntities.size();
    }

    bool Scene::SetSiblingIndex(Entity entity, std::size_t index)
    {
        if (!IsValid(entity))
            return false;

        HierarchyComponent& hierarchy = m_Registry.get<HierarchyComponent>(entity.GetHandle());
        if (hierarchy.Parent != entt::null && m_Registry.valid(hierarchy.Parent))
        {
            HierarchyComponent& parentHierarchy = m_Registry.get<HierarchyComponent>(hierarchy.Parent);
            auto it = std::find(parentHierarchy.Children.begin(), parentHierarchy.Children.end(), entity.GetHandle());
            if (it == parentHierarchy.Children.end())
                return false;

            const entt::entity handle = *it;
            parentHierarchy.Children.erase(it);
            index = std::min(index, parentHierarchy.Children.size());
            parentHierarchy.Children.insert(parentHierarchy.Children.begin() + static_cast<std::ptrdiff_t>(index), handle);
            return true;
        }

        auto it = std::find(m_RootEntities.begin(), m_RootEntities.end(), entity.GetHandle());
        if (it == m_RootEntities.end())
            return false;

        const entt::entity handle = *it;
        m_RootEntities.erase(it);
        index = std::min(index, m_RootEntities.size());
        m_RootEntities.insert(m_RootEntities.begin() + static_cast<std::ptrdiff_t>(index), handle);
        return true;
    }

    std::vector<Entity> Scene::GetRootEntities() const
    {
        std::vector<Entity> roots;
        roots.reserve(m_RootEntities.size());
        for (const entt::entity handle : m_RootEntities)
        {
            if (m_Registry.valid(handle))
                roots.push_back(WrapEntity(handle));
        }

        return roots;
    }

    std::vector<Entity> Scene::GetEntities() const
    {
        std::vector<Entity> entities;
        const auto view = m_Registry.view<IdComponent>();
        for (const entt::entity handle : view)
            entities.push_back(WrapEntity(handle));

        return entities;
    }

    bool Scene::IsDescendantOf(Entity entity, Entity ancestor) const
    {
        if (!IsValid(entity) || !IsValid(ancestor))
            return false;

        entt::entity current = m_Registry.get<HierarchyComponent>(entity.GetHandle()).Parent;
        while (current != entt::null && m_Registry.valid(current))
        {
            if (current == ancestor.GetHandle())
                return true;

            current = m_Registry.get<HierarchyComponent>(current).Parent;
        }

        return false;
    }

    glm::mat4 Scene::GetLocalTransformMatrix(Entity entity) const
    {
        if (!IsValid(entity))
            return glm::mat4(1.0f);

        return ComposeTransform(m_Registry.get<TransformComponent>(entity.GetHandle()));
    }

    glm::mat4 Scene::GetWorldTransformMatrix(Entity entity) const
    {
        if (!IsValid(entity))
            return glm::mat4(1.0f);

        glm::mat4 transform = GetLocalTransformMatrix(entity);
        entt::entity parentHandle = m_Registry.get<HierarchyComponent>(entity.GetHandle()).Parent;
        while (parentHandle != entt::null && m_Registry.valid(parentHandle))
        {
            transform = ComposeTransform(m_Registry.get<TransformComponent>(parentHandle)) * transform;
            parentHandle = m_Registry.get<HierarchyComponent>(parentHandle).Parent;
        }

        return transform;
    }

    entt::registry& Scene::GetRegistry() noexcept
    {
        return m_Registry;
    }

    const entt::registry& Scene::GetRegistry() const noexcept
    {
        return m_Registry;
    }

    void Scene::InitializeEntity(entt::entity handle, std::string tag)
    {
        m_Registry.emplace<IdComponent>(handle, IdComponent{ GenerateEntityId() });
        m_Registry.emplace<TagComponent>(handle, TagComponent{ tag.empty() ? std::string("Entity") : std::move(tag) });
        m_Registry.emplace<TransformComponent>(handle);
        m_Registry.emplace<HierarchyComponent>(handle);
    }

    void Scene::DetachFromParent(entt::entity child, bool makeRoot)
    {
        if (child == entt::null || !m_Registry.valid(child))
            return;

        HierarchyComponent& childHierarchy = m_Registry.get<HierarchyComponent>(child);
        if (childHierarchy.Parent == entt::null || !m_Registry.valid(childHierarchy.Parent))
        {
            childHierarchy.Parent = entt::null;
            if (makeRoot && std::find(m_RootEntities.begin(), m_RootEntities.end(), child) == m_RootEntities.end())
                m_RootEntities.push_back(child);
            return;
        }

        HierarchyComponent& parentHierarchy = m_Registry.get<HierarchyComponent>(childHierarchy.Parent);
        parentHierarchy.Children.erase(
            std::remove(parentHierarchy.Children.begin(), parentHierarchy.Children.end(), child),
            parentHierarchy.Children.end());
        childHierarchy.Parent = entt::null;
        if (makeRoot && std::find(m_RootEntities.begin(), m_RootEntities.end(), child) == m_RootEntities.end())
            m_RootEntities.push_back(child);
    }

    void Scene::RemoveFromRootOrder(entt::entity handle)
    {
        m_RootEntities.erase(
            std::remove(m_RootEntities.begin(), m_RootEntities.end(), handle),
            m_RootEntities.end());
    }

    bool Scene::WouldCreateCycle(entt::entity child, entt::entity parent) const
    {
        entt::entity current = parent;
        while (current != entt::null && m_Registry.valid(current))
        {
            if (current == child)
                return true;

            current = m_Registry.get<HierarchyComponent>(current).Parent;
        }

        return false;
    }

    glm::mat4 Scene::ComposeTransform(const TransformComponent& transform)
    {
        const glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.LocalPosition);
        glm::mat4 rotation = glm::mat4(1.0f);
        rotation = glm::rotate(rotation, transform.LocalRotation.y, glm::vec3(0.0f, 1.0f, 0.0f));
        rotation = glm::rotate(rotation, transform.LocalRotation.x, glm::vec3(1.0f, 0.0f, 0.0f));
        rotation = glm::rotate(rotation, transform.LocalRotation.z, glm::vec3(0.0f, 0.0f, 1.0f));
        const glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.LocalScale);
        return translation * rotation * scale;
    }

    std::string Scene::GenerateEntityId()
    {
        return Assets::GenerateGuid();
    }

    Entity::Entity(entt::entity handle, Scene* scene) noexcept
        : m_Handle(handle)
        , m_Scene(scene)
    {
    }

    bool Entity::IsValid() const noexcept
    {
        return m_Scene != nullptr && m_Scene->IsValid(*this);
    }

    Scene& Entity::GetScene()
    {
        if (m_Scene == nullptr)
            throw std::logic_error("Entity is not bound to a scene.");

        return *m_Scene;
    }

    const Scene& Entity::GetScene() const
    {
        if (m_Scene == nullptr)
            throw std::logic_error("Entity is not bound to a scene.");

        return *m_Scene;
    }

    const std::string& Entity::GetId() const
    {
        return m_Scene->m_Registry.get<IdComponent>(m_Handle).Id;
    }

    const std::string& Entity::GetTag() const
    {
        return m_Scene->m_Registry.get<TagComponent>(m_Handle).Tag;
    }

    void Entity::SetTag(std::string tag)
    {
        m_Scene->m_Registry.get<TagComponent>(m_Handle).Tag = std::move(tag);
    }

    Entity Entity::GetParent() const
    {
        return GetScene().GetParent(*this);
    }

    bool Entity::HasParent() const
    {
        return GetScene().HasParent(*this);
    }

    std::vector<Entity> Entity::GetChildren() const
    {
        return GetScene().GetChildren(*this);
    }

    bool Entity::SetParent(Entity parent)
    {
        return GetScene().SetParent(*this, parent);
    }

    void Entity::RemoveParent()
    {
        GetScene().RemoveParent(*this);
    }

    bool Entity::IsDescendantOf(Entity ancestor) const
    {
        return GetScene().IsDescendantOf(*this, ancestor);
    }
}
