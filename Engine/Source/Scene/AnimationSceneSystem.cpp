#include "Scene/AnimationSceneSystem.h"

#include "Assets/AnimationClipAsset.h"
#include "Assets/AnimatorControllerAsset.h"
#include "Assets/AssetManager.h"
#include "Core/ServiceRegistry.h"
#include "Scene/Components.h"
#include "Scene/Scene.h"

#include <algorithm>
#include <cmath>

namespace Life
{
    namespace
    {
        using AnimationClip = Assets::AnimationClipAsset;
        using AnimatorController = Assets::AnimatorControllerAsset;

        Assets::AssetManager* TryGetAssetManager()
        {
            return GetServices().TryGet<Assets::AssetManager>();
        }

        Ref<AnimationClip> ResolveClip(const AnimatorComponent& animator)
        {
            Assets::AssetManager* assetManager = TryGetAssetManager();
            if (assetManager == nullptr)
                return nullptr;

            if (!animator.ClipAssetKey.empty())
                return assetManager->GetOrLoad<AnimationClip>(animator.ClipAssetKey);

            if (animator.ControllerAssetKey.empty())
                return nullptr;

            Ref<AnimatorController> controller = assetManager->GetOrLoad<AnimatorController>(animator.ControllerAssetKey);
            if (!controller)
                return nullptr;

            const AnimatorController::Data& controllerData = controller->GetData();
            const std::string& stateName = animator.CurrentStateName.empty()
                ? controllerData.DefaultStateName
                : animator.CurrentStateName;

            for (const AnimatorController::StateDefinition& state : controllerData.States)
            {
                if (state.Name == stateName || (stateName.empty() && state.Name == controllerData.DefaultStateName))
                    return state.ClipKey.empty() ? nullptr : assetManager->GetOrLoad<AnimationClip>(state.ClipKey);
            }

            if (!controllerData.States.empty() && !controllerData.States.front().ClipKey.empty())
                return assetManager->GetOrLoad<AnimationClip>(controllerData.States.front().ClipKey);

            return nullptr;
        }

        float NormalizeClipTime(float timeSeconds, const AnimationClip::Data& clip)
        {
            const float duration = std::max(clip.DurationSeconds, 0.0f);
            if (duration <= 0.0f)
                return 0.0f;

            if (clip.Loop)
            {
                float wrapped = std::fmod(timeSeconds, duration);
                if (wrapped < 0.0f)
                    wrapped += duration;
                return wrapped;
            }

            return std::clamp(timeSeconds, 0.0f, duration);
        }

        glm::vec3 SampleVectorTrack(
            const std::vector<AnimationClip::Vector3Keyframe>& track,
            float timeSeconds,
            const glm::vec3& fallback)
        {
            if (track.empty())
                return fallback;

            if (timeSeconds <= track.front().TimeSeconds)
                return track.front().Value;

            for (std::size_t index = 1; index < track.size(); ++index)
            {
                const AnimationClip::Vector3Keyframe& previous = track[index - 1];
                const AnimationClip::Vector3Keyframe& next = track[index];
                if (timeSeconds > next.TimeSeconds)
                    continue;

                if (previous.Interpolation == AnimationClip::InterpolationMode::Step || next.TimeSeconds <= previous.TimeSeconds)
                    return previous.Value;

                const float alpha = std::clamp(
                    (timeSeconds - previous.TimeSeconds) / (next.TimeSeconds - previous.TimeSeconds),
                    0.0f,
                    1.0f);
                return previous.Value + (next.Value - previous.Value) * alpha;
            }

            return track.back().Value;
        }

        std::string SampleTextureTrack(
            const std::vector<AnimationClip::SpriteTextureKeyframe>& track,
            float timeSeconds,
            const std::string& fallback)
        {
            if (track.empty())
                return fallback;

            const AnimationClip::SpriteTextureKeyframe* selected = &track.front();
            for (const AnimationClip::SpriteTextureKeyframe& keyframe : track)
            {
                if (keyframe.TimeSeconds > timeSeconds)
                    break;
                selected = &keyframe;
            }

            return selected->TextureKey.empty() ? fallback : selected->TextureKey;
        }

        void ApplyClip(Entity entity, AnimatorComponent& animator, const AnimationClip::Data& clip, float timestep)
        {
            if (!animator.Playing)
                return;

            animator.PlaybackTimeSeconds += timestep * animator.Speed;
            const float sampleTime = NormalizeClipTime(animator.PlaybackTimeSeconds, clip);
            TransformComponent& transform = entity.GetComponent<TransformComponent>();
            transform.LocalPosition = SampleVectorTrack(clip.PositionTrack, sampleTime, transform.LocalPosition);
            transform.LocalRotation = SampleVectorTrack(clip.RotationTrack, sampleTime, transform.LocalRotation);
            transform.LocalScale = SampleVectorTrack(clip.ScaleTrack, sampleTime, transform.LocalScale);

            if (!clip.Loop && animator.PlaybackTimeSeconds >= std::max(clip.DurationSeconds, 0.0f))
                animator.Playing = false;

            if (SpriteComponent* sprite = entity.TryGetComponent<SpriteComponent>())
            {
                const std::string textureKey = SampleTextureTrack(clip.SpriteTextureTrack, sampleTime, sprite->TextureAssetKey);
                if (textureKey != sprite->TextureAssetKey)
                {
                    sprite->TextureAssetKey = textureKey;
                    sprite->TextureAsset.reset();
                }
            }
        }
    }

    void AnimationSceneSystem::OnSceneStart(Scene& scene)
    {
        auto view = scene.GetRegistry().view<AnimatorComponent>();
        for (const entt::entity handle : view)
        {
            AnimatorComponent& animator = view.get<AnimatorComponent>(handle);
            animator.PlaybackTimeSeconds = 0.0f;
            animator.Playing = animator.PlayAutomatically;
        }
    }

    void AnimationSceneSystem::OnSceneUpdate(Scene& scene, float timestep)
    {
        auto view = scene.GetRegistry().view<AnimatorComponent, TransformComponent>();
        for (const entt::entity handle : view)
        {
            Entity entity = scene.WrapEntity(handle);
            AnimatorComponent& animator = view.get<AnimatorComponent>(handle);
            Ref<AnimationClip> clip = ResolveClip(animator);
            if (clip)
                ApplyClip(entity, animator, clip->GetData(), timestep);
        }
    }
}
