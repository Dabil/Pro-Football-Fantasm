#include "Animation/AnimationBindingResolver.h"

#include <algorithm>
#include <limits>
#include <utility>

#include "Rendering/Objects/XpSequenceAnimationAdapter.h"

namespace
{
    const char* toBindingTargetKindString(Animation::AnimationBindingTargetKind kind)
    {
        using Kind = Animation::AnimationBindingTargetKind;

        switch (kind)
        {
        case Kind::SequenceAssetPlacement:
            return "SequenceAssetPlacement";
        case Kind::FramePlaceholder:
            return "FramePlaceholder";
        case Kind::RegisteredFrameSequence:
            return "RegisteredFrameSequence";
        case Kind::XpSequencePlacement:
            return "XpSequencePlacement";
        default:
            return "Unknown";
        }
    }
}

namespace Animation
{
    AnimationBindingTarget AnimationBindingTarget::sequenceAssetPlacement(
        std::string_view targetNameValue,
        std::string_view controllerNameValue,
        std::string_view assetNameValue,
        const Composition::PageComposer::PlacementSpec& placementValue,
        const Composition::WritePolicy& policyValue)
    {
        AnimationBindingTarget target;
        target.kind = AnimationBindingTargetKind::SequenceAssetPlacement;
        target.targetName = std::string(targetNameValue);
        target.controllerName = std::string(controllerNameValue);
        target.assetName = std::string(assetNameValue);
        target.placement = placementValue;
        target.policy = policyValue;
        return target;
    }

    AnimationBindingTarget AnimationBindingTarget::framePlaceholder(
        std::string_view targetNameValue,
        std::string_view controllerNameValue,
        const std::vector<TextObject>& frames,
        const Composition::PageComposer::PlacementSpec& placementValue,
        const Composition::WritePolicy& policyValue)
    {
        AnimationBindingTarget target;
        target.kind = AnimationBindingTargetKind::FramePlaceholder;
        target.targetName = std::string(targetNameValue);
        target.controllerName = std::string(controllerNameValue);
        target.registeredFrames = &frames;
        target.placement = placementValue;
        target.policy = policyValue;
        return target;
    }

    AnimationBindingTarget AnimationBindingTarget::animatedTextSequence(
        std::string_view targetNameValue,
        std::string_view controllerNameValue,
        const AnimatedTextAssetSequence& sequence,
        const Composition::PageComposer::PlacementSpec& placementValue,
        const Composition::WritePolicy& policyValue)
    {
        AnimationBindingTarget target;
        target.kind = AnimationBindingTargetKind::RegisteredFrameSequence;
        target.targetName = std::string(targetNameValue);
        target.controllerName = std::string(controllerNameValue);
        target.textAssetSequence = &sequence;
        target.placement = placementValue;
        target.policy = policyValue;
        return target;
    }

    AnimationBindingTarget AnimationBindingTarget::xpSequencePlacement(
        std::string_view targetNameValue,
        std::string_view controllerNameValue,
        const XpArtLoader::XpSequence& sequence,
        const Composition::PageComposer::PlacementSpec& placementValue,
        const Composition::WritePolicy& policyValue,
        const XpArtLoader::XpFrameConversionOptions& frameOptions)
    {
        AnimationBindingTarget target;
        target.kind = AnimationBindingTargetKind::XpSequencePlacement;
        target.targetName = std::string(targetNameValue);
        target.controllerName = std::string(controllerNameValue);
        target.xpSequence = &sequence;
        target.xpFrameOptions = frameOptions;
        target.placement = placementValue;
        target.policy = policyValue;
        return target;
    }

    void AnimationBindingResolver::clear()
    {
        m_controllers.clear();
        m_targets.clear();
    }

    void AnimationBindingResolver::bindController(
        std::string_view name,
        Animator& animator)
    {
        if (name.empty())
        {
            return;
        }

        m_controllers[std::string(name)] = &animator;
    }

    void AnimationBindingResolver::unbindController(std::string_view name)
    {
        m_controllers.erase(std::string(name));
    }

    bool AnimationBindingResolver::hasController(std::string_view name) const
    {
        return tryGetController(name) != nullptr;
    }

    Animator* AnimationBindingResolver::tryGetController(std::string_view name)
    {
        const auto found = m_controllers.find(std::string(name));
        return found != m_controllers.end() ? found->second : nullptr;
    }

    const Animator* AnimationBindingResolver::tryGetController(
        std::string_view name) const
    {
        const auto found = m_controllers.find(std::string(name));
        return found != m_controllers.end() ? found->second : nullptr;
    }

    void AnimationBindingResolver::setTargets(
        const std::vector<AnimationBindingTarget>& targetsValue)
    {
        m_targets = targetsValue;
    }

    void AnimationBindingResolver::addTarget(
        const AnimationBindingTarget& target)
    {
        if (target.targetName.empty() || target.controllerName.empty())
        {
            return;
        }

        m_targets.push_back(target);
    }

    void AnimationBindingResolver::clearTargets()
    {
        m_targets.clear();
    }

    const std::vector<AnimationBindingTarget>&
        AnimationBindingResolver::targets() const
    {
        return m_targets;
    }

    std::vector<AnimationBindingResolutionResult>
        AnimationBindingResolver::resolveAll(
            Composition::PageComposer& composer) const
    {
        std::vector<AnimationBindingResolutionResult> results;
        results.reserve(m_targets.size());

        for (const AnimationBindingTarget& target : m_targets)
        {
            const Animator* animator = tryGetController(target.controllerName);

            if (animator == nullptr)
            {
                AnimationBindingResolutionResult result;
                result.targetName = target.targetName;
                result.controllerName = target.controllerName;
                result.resolved = false;
                result.placed = false;
                result.message = "Animation controller reference could not be resolved.";

                results.push_back(result);
                continue;
            }

            results.push_back(placeResolvedFrame(composer, target, *animator));
        }

        return results;
    }

    AnimationBindingResolutionResult
        AnimationBindingResolver::placeResolvedFrame(
            Composition::PageComposer& composer,
            const AnimationBindingTarget& target,
            const Animator& animator) const
    {
        AnimationBindingResolutionResult result;
        result.targetName = target.targetName;
        result.controllerName = target.controllerName;
        result.resolved = true;

        const std::size_t animatorFrameIndex = animator.currentFrameIndex();
        result.frameIndex = animatorFrameIndex;

        switch (target.kind)
        {
        case AnimationBindingTargetKind::SequenceAssetPlacement:
        {
            if (target.assetName.empty())
            {
                result.message = "Sequence asset placement has no asset name.";
                return result;
            }

            composer.placeSource(
                Composition::ObjectSource::fromAssetFrame(
                    target.assetName,
                    toPlacementFrameIndex(animatorFrameIndex)),
                target.placement,
                target.policy);

            result.placed = true;
            result.message = "Placed sequence asset frame.";
            return result;
        }

        case AnimationBindingTargetKind::FramePlaceholder:
        {
            if (target.registeredFrames == nullptr)
            {
                result.message = "Frame placeholder has no registered frame list.";
                return result;
            }

            if (!isValidFrameIndex(animatorFrameIndex, target.registeredFrames->size()))
            {
                result.message = "Frame placeholder resolved frame is out of range.";
                return result;
            }

            const TextObject& frame = (*target.registeredFrames)[animatorFrameIndex];

            result.placementBounds = composer.resolvePlacementBounds(
                target.placement,
                Size{ frame.getWidth(), frame.getHeight() });

            composer.placeSource(
                Composition::ObjectSource::fromRegisteredFrame(
                    *target.registeredFrames,
                    toPlacementFrameIndex(animatorFrameIndex)),
                target.placement,
                target.policy);

            result.placed = true;
            result.message = "Placed registered frame placeholder.";
            return result;
        }

        case AnimationBindingTargetKind::RegisteredFrameSequence:
        {
            if (target.textAssetSequence == nullptr)
            {
                result.message = "Animated text sequence target has no sequence.";
                result.frameIndex.reset();
                return result;
            }

            const AnimatedTextAssetSequencePlaybackResult playbackResult =
                buildTextObjectForAnimatorWithResult(
                    *target.textAssetSequence,
                    animator);

            if (playbackResult.hasFrameIndex)
            {
                result.frameIndex = playbackResult.frameIndex;
            }
            else
            {
                result.frameIndex.reset();
            }

            if (!playbackResult.success() || !playbackResult.hasObject())
            {
                result.message = playbackResult.buildResult.errorMessage.empty()
                    ? "Animated text sequence did not produce a loaded frame."
                    : playbackResult.buildResult.errorMessage;
                return result;
            }

            result.placementBounds = composer.resolvePlacementBounds(
                target.placement,
                Size{
                    playbackResult.buildResult.object.getWidth(),
                    playbackResult.buildResult.object.getHeight()
                });

            composer.placeSource(
                Composition::ObjectSource::fromTextObject(playbackResult.buildResult.object),
                target.placement,
                target.policy);

            result.placed = true;
            result.message = "Placed duration-aware animated text sequence frame.";
            return result;
        }

        case AnimationBindingTargetKind::XpSequencePlacement:
        {
            if (target.xpSequence == nullptr || !target.xpSequence->isValid())
            {
                result.message = "XP sequence placement has no valid XP sequence.";
                return result;
            }

            if (!isValidFrameIndex(
                animatorFrameIndex,
                static_cast<std::size_t>(target.xpSequence->getFrameCount())))
            {
                result.message = "XP sequence resolved frame ordinal is out of range.";
                return result;
            }

            const XpSequenceAnimationAdapter::TextObjectFrameResult frameResult =
                XpSequenceAnimationAdapter::buildTextObjectForFrame(
                    *target.xpSequence,
                    toPlacementFrameIndex(animatorFrameIndex),
                    target.xpFrameOptions);

            if (!frameResult.success || !frameResult.buildResult.object.isLoaded())
            {
                result.message = frameResult.errorMessage.empty()
                    ? "XP sequence frame conversion failed."
                    : frameResult.errorMessage;
                return result;
            }

            result.placementBounds = composer.resolvePlacementBounds(
                target.placement,
                Size{
                    frameResult.buildResult.object.getWidth(),
                    frameResult.buildResult.object.getHeight()
                });

            composer.placeSource(
                Composition::ObjectSource::fromXpSequence(
                    *target.xpSequence,
                    toPlacementFrameIndex(animatorFrameIndex),
                    target.xpFrameOptions),
                target.placement,
                target.policy);

            result.placed = true;
            result.message = "Placed XP sequence frame.";
            return result;
        }

        default:
            result.message = "Unknown animation binding target kind.";
            return result;
        }
    }

    AnimationBindingResolutionResult AnimationBindingResolver::resolveTarget(
        Composition::PageComposer& composer,
        const AnimationBindingTarget& target) const
    {
        if (target.targetName.empty())
        {
            return makeSkippedResult(target, "Animation target name is empty.");
        }

        if (target.controllerName.empty())
        {
            return makeSkippedResult(
                target,
                "Animation target has no controller reference.");
        }

        const Animator* animator = tryGetController(target.controllerName);
        if (animator == nullptr)
        {
            return makeSkippedResult(
                target,
                "Animation controller reference could not be resolved.");
        }

        return placeResolvedFrame(composer, target, *animator);
    }

    void AnimationBindingResolver::updateBoundControllers(const TickEvent& event)
    {
        std::vector<Animator*> updatedControllers;
        updatedControllers.reserve(m_controllers.size());

        for (const auto& entry : m_controllers)
        {
            Animator* animator = entry.second;
            if (animator == nullptr)
            {
                continue;
            }

            const bool alreadyUpdated =
                std::find(
                    updatedControllers.begin(),
                    updatedControllers.end(),
                    animator) != updatedControllers.end();

            if (alreadyUpdated)
            {
                continue;
            }

            animator->update(event);
            updatedControllers.push_back(animator);
        }
    }

    std::vector<AnimationBindingFrameState>
        AnimationBindingResolver::captureFrameState() const
    {
        std::vector<AnimationBindingFrameState> states;
        states.reserve(m_targets.size());

        for (const AnimationBindingTarget& target : m_targets)
        {
            AnimationBindingFrameState state;
            state.targetName = target.targetName;
            state.controllerName = target.controllerName;

            const Animator* animator = tryGetController(target.controllerName);
            if (animator != nullptr)
            {
                state.resolved = true;
                state.frameIndex = resolveFrameIndexForTarget(target, *animator);
            }

            states.push_back(state);
        }

        return states;
    }

    bool AnimationBindingResolver::isValidFrameIndex(
        std::size_t frameIndex,
        std::size_t frameCount)
    {
        return frameCount > 0 && frameIndex < frameCount;
    }

    int AnimationBindingResolver::toPlacementFrameIndex(std::size_t frameIndex)
    {
        if (frameIndex > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return std::numeric_limits<int>::max();
        }

        return static_cast<int>(frameIndex);
    }

    AnimationBindingResolutionResult
        AnimationBindingResolver::makeSkippedResult(
            const AnimationBindingTarget& target,
            std::string message)
    {
        AnimationBindingResolutionResult result;
        result.targetName = target.targetName;
        result.controllerName = target.controllerName;
        result.resolved = false;
        result.placed = false;
        result.message = std::move(message);
        return result;
    }

    AnimationDiagnosticsReport AnimationBindingResolver::diagnosticsReport() const
    {
        AnimationDiagnosticsReport report;
        report.enabled = true;

        report.controllers.reserve(m_controllers.size());
        for (const auto& entry : m_controllers)
        {
            const std::string& controllerName = entry.first;
            const Animator* animator = entry.second;

            if (animator == nullptr)
            {
                continue;
            }

            AnimatorDiagnostics controller;
            controller.controllerName = controllerName;
            controller.frameCount = animator->frameCount();
            controller.currentFrameIndex = animator->currentFrameIndex();
            controller.framesPerSecond = animator->framesPerSecond();
            controller.playbackRate = animator->playbackRate();
            controller.elapsedSeconds = animator->elapsedSeconds();
            controller.durationSeconds = animator->durationSeconds();
            controller.progress = animator->progress();
            controller.playbackState = animator->playbackState();
            controller.playbackMode = animator->playbackMode();
            controller.hasExplicitFrameIndex = animator->hasExplicitFrameIndex();
            controller.explicitFrameIndex = animator->explicitFrameIndex();

            report.controllers.push_back(controller);
        }

        report.bindings.reserve(m_targets.size());
        for (const AnimationBindingTarget& target : m_targets)
        {
            AnimationBindingDiagnostics binding;
            binding.targetName = target.targetName;
            binding.controllerName = target.controllerName;
            binding.targetKind = toBindingTargetKindString(target.kind);

            const Animator* animator = tryGetController(target.controllerName);
            binding.controllerResolved = animator != nullptr;

            if (animator != nullptr)
            {
                binding.animatorFrameIndex = animator->currentFrameIndex();
                binding.currentFrameIndex = resolveFrameIndexForTarget(target, *animator);
            }

            switch (target.kind)
            {
            case AnimationBindingTargetKind::SequenceAssetPlacement:
                binding.targetReferenceValid = !target.assetName.empty();
                binding.frameReferenceValid = binding.targetReferenceValid &&
                    binding.currentFrameIndex.has_value();
                binding.message = binding.targetReferenceValid
                    ? "Sequence asset reference is present."
                    : "Sequence asset placement has no asset name.";
                break;

            case AnimationBindingTargetKind::FramePlaceholder:
                binding.targetReferenceValid = target.registeredFrames != nullptr;
                binding.availableFrameCount = target.registeredFrames != nullptr
                    ? std::optional<std::size_t>(target.registeredFrames->size())
                    : std::nullopt;
                binding.frameReferenceValid =
                    target.registeredFrames != nullptr &&
                    binding.currentFrameIndex.has_value() &&
                    binding.currentFrameIndex.value() < target.registeredFrames->size();
                binding.message = binding.frameReferenceValid
                    ? "Registered frame binding is valid."
                    : "Registered frame binding is missing frames or resolved outside range.";
                break;

            case AnimationBindingTargetKind::RegisteredFrameSequence:
                binding.usesDurationAwareSequenceTiming = true;
                binding.targetReferenceValid = target.textAssetSequence != nullptr;
                binding.availableFrameCount = target.textAssetSequence != nullptr
                    ? std::optional<std::size_t>(target.textAssetSequence->frameCount())
                    : std::nullopt;
                binding.frameReferenceValid =
                    target.textAssetSequence != nullptr &&
                    binding.currentFrameIndex.has_value() &&
                    binding.currentFrameIndex.value() < target.textAssetSequence->frameCount();
                binding.message = binding.frameReferenceValid
                    ? "Animated text sequence binding is valid using duration-aware sequence timing."
                    : "Animated text sequence binding is missing a sequence or resolved outside range.";
                break;

            case AnimationBindingTargetKind::XpSequencePlacement:
                binding.targetReferenceValid =
                    target.xpSequence != nullptr &&
                    target.xpSequence->isValid();
                binding.availableFrameCount =
                    target.xpSequence != nullptr
                    ? std::optional<std::size_t>(
                        static_cast<std::size_t>(target.xpSequence->getFrameCount()))
                    : std::nullopt;
                binding.frameReferenceValid =
                    target.xpSequence != nullptr &&
                    target.xpSequence->isValid() &&
                    binding.currentFrameIndex.has_value() &&
                    binding.currentFrameIndex.value() <
                    static_cast<std::size_t>(target.xpSequence->getFrameCount());
                binding.message = binding.frameReferenceValid
                    ? "XP sequence binding is valid."
                    : "XP sequence binding is invalid, missing, or resolved outside range.";
                break;

            default:
                binding.message = "Unknown animation binding target kind.";
                break;
            }

            if (!binding.controllerResolved)
            {
                binding.message = "Animation controller reference could not be resolved.";
            }

            report.bindings.push_back(binding);
        }

        return report;
    }

    std::optional<std::size_t>
        AnimationBindingResolver::resolveFrameIndexForTarget(
            const AnimationBindingTarget& target,
            const Animator& animator)
    {
        switch (target.kind)
        {
        case AnimationBindingTargetKind::RegisteredFrameSequence:
            if (target.textAssetSequence == nullptr ||
                target.textAssetSequence->isEmpty())
            {
                return std::nullopt;
            }

            return frameIndexForAnimator(*target.textAssetSequence, animator);

        case AnimationBindingTargetKind::SequenceAssetPlacement:
        case AnimationBindingTargetKind::FramePlaceholder:
        case AnimationBindingTargetKind::XpSequencePlacement:
            return animator.currentFrameIndex();

        default:
            return std::nullopt;
        }
    }
}