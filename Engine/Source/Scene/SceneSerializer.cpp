#include "Scene/SceneSerializer.h"

#include "Assets/AssetManager.h"
#include "Scene/Scene.h"

#include <nlohmann/json.hpp>

#include <fstream>
#include <unordered_map>
#include <utility>

namespace Life
{
    namespace
    {
        constexpr const char* kVersionField = "version";
        constexpr const char* kNameField = "name";
        constexpr const char* kEntitiesField = "entities";
        constexpr const char* kIdField = "id";
        constexpr const char* kTagField = "tag";
        constexpr const char* kParentIdField = "parentId";
        constexpr const char* kTransformField = "transform";
        constexpr const char* kSpriteField = "sprite";
        constexpr const char* kPositionField = "position";
        constexpr const char* kRotationField = "rotation";
        constexpr const char* kScaleField = "scale";
        constexpr const char* kSizeField = "size";
        constexpr const char* kColorField = "color";
        constexpr const char* kTextureAssetKeyField = "textureAssetKey";

        std::filesystem::path NormalizeAbsolutePath(const std::filesystem::path& path)
        {
            if (path.empty())
                return {};

            std::error_code ec;
            const std::filesystem::path absolutePath = std::filesystem::absolute(path, ec);
            if (!ec)
                return absolutePath.lexically_normal();

            return path.lexically_normal();
        }

        nlohmann::json ToJson(const glm::vec2& value)
        {
            return nlohmann::json::array({ value.x, value.y });
        }

        nlohmann::json ToJson(const glm::vec3& value)
        {
            return nlohmann::json::array({ value.x, value.y, value.z });
        }

        nlohmann::json ToJson(const glm::vec4& value)
        {
            return nlohmann::json::array({ value.x, value.y, value.z, value.w });
        }

        glm::vec2 ReadVec2(const nlohmann::json& value, const glm::vec2& fallback)
        {
            if (!value.is_array() || value.size() != 2)
                return fallback;

            glm::vec2 result = fallback;
            if (value[0].is_number())
                result.x = value[0].get<float>();
            if (value[1].is_number())
                result.y = value[1].get<float>();
            return result;
        }

        glm::vec3 ReadVec3(const nlohmann::json& value, const glm::vec3& fallback)
        {
            if (!value.is_array() || value.size() != 3)
                return fallback;

            glm::vec3 result = fallback;
            if (value[0].is_number())
                result.x = value[0].get<float>();
            if (value[1].is_number())
                result.y = value[1].get<float>();
            if (value[2].is_number())
                result.z = value[2].get<float>();
            return result;
        }

        glm::vec4 ReadVec4(const nlohmann::json& value, const glm::vec4& fallback)
        {
            if (!value.is_array() || value.size() != 4)
                return fallback;

            glm::vec4 result = fallback;
            if (value[0].is_number())
                result.x = value[0].get<float>();
            if (value[1].is_number())
                result.y = value[1].get<float>();
            if (value[2].is_number())
                result.z = value[2].get<float>();
            if (value[3].is_number())
                result.w = value[3].get<float>();
            return result;
        }

        void SerializeEntityRecursive(nlohmann::json& entities, const Entity& entity)
        {
            const TransformComponent& transform = entity.GetComponent<TransformComponent>();

            nlohmann::json record;
            record[kIdField] = entity.GetId();
            record[kTagField] = entity.GetTag();
            if (Entity parent = entity.GetParent(); parent.IsValid())
                record[kParentIdField] = parent.GetId();

            record[kTransformField] = {
                { kPositionField, ToJson(transform.LocalPosition) },
                { kRotationField, ToJson(transform.LocalRotation) },
                { kScaleField, ToJson(transform.LocalScale) }
            };

            if (const SpriteComponent* sprite = entity.TryGetComponent<SpriteComponent>())
            {
                record[kSpriteField] = {
                    { kSizeField, ToJson(sprite->Size) },
                    { kColorField, ToJson(sprite->Color) },
                    { kTextureAssetKeyField, sprite->TextureAssetKey }
                };
            }

            entities.push_back(std::move(record));

            for (const Entity child : entity.GetChildren())
                SerializeEntityRecursive(entities, child);
        }
    }

    Result<Scope<Scene>> SceneSerializer::Load(const std::filesystem::path& sourcePath, Assets::AssetManager* assetManager)
    {
        if (sourcePath.empty())
        {
            return Result<Scope<Scene>>(ErrorCode::InvalidArgument,
                                        "SceneSerializer::Load requires a source path");
        }

        const std::filesystem::path normalizedPath = NormalizeAbsolutePath(sourcePath);
        std::ifstream stream(normalizedPath, std::ios::in | std::ios::binary);
        if (!stream.is_open())
        {
            return Result<Scope<Scene>>(ErrorCode::FileNotFound,
                                        "Failed to open scene file: " + normalizedPath.string());
        }

        try
        {
            nlohmann::json root;
            stream >> root;
            if (!root.is_object())
            {
                return Result<Scope<Scene>>(ErrorCode::FileCorrupted,
                                            "Scene file root must be a JSON object");
            }

            const uint32_t version = root.contains(kVersionField) && root[kVersionField].is_number_unsigned()
                ? root[kVersionField].get<uint32_t>()
                : 0u;
            if (version != SceneFileCurrentVersion)
            {
                return Result<Scope<Scene>>(ErrorCode::ConfigVersionMismatch,
                                            "Unsupported scene file version: " + std::to_string(version));
            }

            std::string sceneName = normalizedPath.stem().string();
            if (root.contains(kNameField) && root[kNameField].is_string())
                sceneName = root[kNameField].get<std::string>();
            if (sceneName.empty())
                sceneName = "Scene";

            auto scene = CreateScope<Scene>(sceneName);
            scene->SetSourcePath(normalizedPath);
            scene->SetState(Scene::State::Loading);

            std::unordered_map<std::string, Entity> entitiesById;
            std::vector<std::pair<Entity, std::string>> pendingParents;

            if (root.contains(kEntitiesField) && root[kEntitiesField].is_array())
            {
                for (const nlohmann::json& record : root[kEntitiesField])
                {
                    if (!record.is_object())
                        continue;

                    std::string tag = "Entity";
                    if (record.contains(kTagField) && record[kTagField].is_string())
                        tag = record[kTagField].get<std::string>();

                    Entity entity = scene->CreateEntity(tag);

                    if (record.contains(kIdField) && record[kIdField].is_string())
                        entity.GetComponent<IdComponent>().Id = record[kIdField].get<std::string>();

                    if (record.contains(kTransformField) && record[kTransformField].is_object())
                    {
                        TransformComponent& transform = entity.GetComponent<TransformComponent>();
                        const nlohmann::json& transformJson = record[kTransformField];
                        if (transformJson.contains(kPositionField))
                            transform.LocalPosition = ReadVec3(transformJson[kPositionField], transform.LocalPosition);
                        if (transformJson.contains(kRotationField))
                            transform.LocalRotation = ReadVec3(transformJson[kRotationField], transform.LocalRotation);
                        if (transformJson.contains(kScaleField))
                            transform.LocalScale = ReadVec3(transformJson[kScaleField], transform.LocalScale);
                    }

                    if (record.contains(kSpriteField) && record[kSpriteField].is_object())
                    {
                        SpriteComponent sprite;
                        const nlohmann::json& spriteJson = record[kSpriteField];
                        if (spriteJson.contains(kSizeField))
                            sprite.Size = ReadVec2(spriteJson[kSizeField], sprite.Size);
                        if (spriteJson.contains(kColorField))
                            sprite.Color = ReadVec4(spriteJson[kColorField], sprite.Color);
                        if (spriteJson.contains(kTextureAssetKeyField) && spriteJson[kTextureAssetKeyField].is_string())
                            sprite.TextureAssetKey = spriteJson[kTextureAssetKeyField].get<std::string>();
                        if (!sprite.TextureAssetKey.empty() && assetManager != nullptr)
                            sprite.TextureAsset = assetManager->GetOrLoad<Assets::TextureAsset>(sprite.TextureAssetKey);
                        entity.AddComponent<SpriteComponent>(std::move(sprite));
                    }

                    entitiesById[entity.GetId()] = entity;
                    if (record.contains(kParentIdField) && record[kParentIdField].is_string())
                        pendingParents.emplace_back(entity, record[kParentIdField].get<std::string>());
                }
            }

            for (const auto& [storedChild, parentId] : pendingParents)
            {
                Life::Entity child = storedChild;
                if (const auto it = entitiesById.find(parentId); it != entitiesById.end())
                    child.SetParent(it->second);
            }

            scene->SetState(Scene::State::Ready);
            return scene;
        }
        catch (const std::exception& exception)
        {
            return Result<Scope<Scene>>(ErrorCode::FileCorrupted,
                                        std::string("Failed to parse scene JSON: ") + exception.what());
        }
    }

    Result<void> SceneSerializer::Save(const Scene& scene, const std::filesystem::path& sourcePath)
    {
        if (sourcePath.empty())
        {
            return Result<void>(ErrorCode::InvalidArgument,
                                "SceneSerializer::Save requires a source path");
        }

        const std::filesystem::path normalizedPath = NormalizeAbsolutePath(sourcePath);

        try
        {
            std::error_code ec;
            std::filesystem::create_directories(normalizedPath.parent_path(), ec);
            if (ec)
            {
                return Result<void>(ErrorCode::FileAccessDenied,
                                    "Failed to create scene directory: " + ec.message());
            }

            nlohmann::json root;
            root[kVersionField] = SceneFileCurrentVersion;
            root[kNameField] = scene.GetName();
            root[kEntitiesField] = nlohmann::json::array();

            for (const Entity rootEntity : scene.GetRootEntities())
                SerializeEntityRecursive(root[kEntitiesField], rootEntity);

            const std::filesystem::path tempPath = normalizedPath.string() + ".tmp";
            {
                std::ofstream stream(tempPath, std::ios::out | std::ios::binary | std::ios::trunc);
                if (!stream.is_open())
                {
                    return Result<void>(ErrorCode::FileAccessDenied,
                                        "Failed to open temporary scene file for writing: " + tempPath.string());
                }

                stream << root.dump(4);
                stream.flush();
                if (!stream.good())
                {
                    return Result<void>(ErrorCode::FileAccessDenied,
                                        "Failed to flush scene file to disk: " + tempPath.string());
                }
            }

            std::error_code renameError;
            std::filesystem::rename(tempPath, normalizedPath, renameError);
            if (renameError)
            {
                renameError.clear();
                std::filesystem::remove(normalizedPath, renameError);
                renameError.clear();
                std::filesystem::rename(tempPath, normalizedPath, renameError);
                if (renameError)
                {
                    return Result<void>(ErrorCode::FileAccessDenied,
                                        "Failed to replace scene file: " + renameError.message());
                }
            }

            return Result<void>();
        }
        catch (const std::exception& exception)
        {
            return Result<void>(ErrorCode::FileAccessDenied,
                                std::string("Failed to save scene file: ") + exception.what());
        }
    }
}
