#pragma once

#include "Scene/Entity.h"
#include "Scene/Components.h"

#include <entt/entt.hpp>
#include <glm/mat4x4.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Life
{
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
        const std::vector<std::string>& GetSpriteSortingLayers() const noexcept;
        bool AddSpriteSortingLayer(std::string name);
        bool RenameSpriteSortingLayer(std::string_view oldName, std::string name);
        bool RemoveSpriteSortingLayer(std::string_view name);
        bool MoveSpriteSortingLayer(std::string_view name, std::size_t index);
        std::size_t ResolveSpriteSortingLayerIndex(std::string_view name) const noexcept;
        void SetSpriteSortingLayers(std::vector<std::string> layers);

        glm::mat4 GetLocalTransformMatrix(Entity entity) const;
        glm::mat4 GetWorldTransformMatrix(Entity entity) const;
        Entity CreateDefaultCameraEntity(std::string tag = "Main Camera");
        bool HasCamera() const noexcept;
        std::size_t GetCameraCount() const noexcept;
        bool EnsureAtLeastOneCamera();
        Entity FindPrimaryCameraEntity();
        Entity FindPrimaryCameraEntity() const;
        Entity ResolveRenderCameraEntity();
        Entity ResolveRenderCameraEntity() const;
        bool BuildCameraFromEntity(Entity entity, float aspectRatio, Camera& camera) const;
        bool BuildPrimaryCamera(float aspectRatio, Camera& camera) const;
        Scope<Scene> Clone() const;
        Entity InstantiatePrefab(const Scene& prefabScene, Entity parent = {}, std::string prefabGuid = {});

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
        void NormalizeCameraPrimaryState();
        static glm::mat4 ComposeTransform(const TransformComponent& transform);
        static std::string GenerateEntityId();
        static std::string SanitizeSortingLayerName(std::string_view name);

        std::string m_Name;
        std::filesystem::path m_SourcePath;
        State m_State = State::Unloaded;
        entt::registry m_Registry;
        std::vector<entt::entity> m_RootEntities;
        std::vector<std::string> m_SpriteSortingLayers{ "Default" };
    };

    glm::mat4 ComposeTransform(const TransformComponent& transform);
    bool DecomposeTransform(const glm::mat4& matrix, TransformComponent& transform);
    bool SetEntityWorldTransform(Scene& scene, Entity entity, const glm::mat4& worldTransform);

}

#include "Scene/Entity.inl"
