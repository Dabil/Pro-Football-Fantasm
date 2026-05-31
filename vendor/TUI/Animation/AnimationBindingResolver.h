#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "Animation/AnimatedTextAssetSequence.h"
#include "Animation/Animator.h"
#include "Animation/TickEvent.h"
#include "Animation/AnimationDiagnostics.h"
#include "Rendering/Composition/ObjectSource.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/Composition/WritePolicy.h"
#include "Rendering/Composition/WritePresets.h"
#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/XpArtLoader.h"

namespace Animation
{
    enum class AnimationBindingTargetKind
    {
        SequenceAssetPlacement,
        FramePlaceholder,
        RegisteredFrameSequence,
        XpSequencePlacement
    };

    struct AnimationBindingTarget
    {
        std::string targetName;
        std::string controllerName;

        AnimationBindingTargetKind kind =
            AnimationBindingTargetKind::SequenceAssetPlacement;

        Composition::PageComposer::PlacementSpec placement =
            Composition::PageComposer::PlacementSpec::at(0, 0);

        Composition::WritePolicy policy =
            Composition::WritePresets::authoredObject();

        std::string assetName;

        const std::vector<TextObject>* registeredFrames = nullptr;
        const AnimatedTextAssetSequence* textAssetSequence = nullptr;
        const XpArtLoader::XpSequence* xpSequence = nullptr;

        XpArtLoader::XpFrameConversionOptions xpFrameOptions;

        static AnimationBindingTarget sequenceAssetPlacement(
            std::string_view targetName,
            std::string_view controllerName,
            std::string_view assetName,
            const Composition::PageComposer::PlacementSpec& placement,
            const Composition::WritePolicy& policy =
            Composition::WritePresets::authoredObject());

        static AnimationBindingTarget framePlaceholder(
            std::string_view targetName,
            std::string_view controllerName,
            const std::vector<TextObject>& frames,
            const Composition::PageComposer::PlacementSpec& placement,
            const Composition::WritePolicy& policy =
            Composition::WritePresets::authoredObject());

        static AnimationBindingTarget animatedTextSequence(
            std::string_view targetName,
            std::string_view controllerName,
            const AnimatedTextAssetSequence& sequence,
            const Composition::PageComposer::PlacementSpec& placement,
            const Composition::WritePolicy& policy =
            Composition::WritePresets::authoredObject());

        static AnimationBindingTarget xpSequencePlacement(
            std::string_view targetName,
            std::string_view controllerName,
            const XpArtLoader::XpSequence& sequence,
            const Composition::PageComposer::PlacementSpec& placement,
            const Composition::WritePolicy& policy =
            Composition::WritePresets::authoredObject(),
            const XpArtLoader::XpFrameConversionOptions& frameOptions = {});
    };

    struct AnimationBindingResolutionResult
    {
        std::string targetName;
        std::string controllerName;

        bool resolved = false;
        bool placed = false;

        std::optional<std::size_t> frameIndex;
        std::optional<Rect> placementBounds;
        std::string message;
    };

    struct AnimationBindingFrameState
    {
        std::string targetName;
        std::string controllerName;
        bool resolved = false;
        std::optional<std::size_t> frameIndex;

        bool operator==(const AnimationBindingFrameState& other) const
        {
            return targetName == other.targetName
                && controllerName == other.controllerName
                && resolved == other.resolved
                && frameIndex == other.frameIndex;
        }

        bool operator!=(const AnimationBindingFrameState& other) const
        {
            return !(*this == other);
        }
    };

    class AnimationBindingResolver
    {
    public:
        void clear();

        void bindController(std::string_view name, Animator& animator);
        void unbindController(std::string_view name);

        bool hasController(std::string_view name) const;
        Animator* tryGetController(std::string_view name);
        const Animator* tryGetController(std::string_view name) const;

        void setTargets(const std::vector<AnimationBindingTarget>& targets);
        void addTarget(const AnimationBindingTarget& target);
        void clearTargets();

        const std::vector<AnimationBindingTarget>& targets() const;

        std::vector<AnimationBindingResolutionResult> resolveAll(
            Composition::PageComposer& composer) const;

        AnimationBindingResolutionResult resolveTarget(
            Composition::PageComposer& composer,
            const AnimationBindingTarget& target) const;

        void updateBoundControllers(const TickEvent& event);

        std::vector<AnimationBindingFrameState> captureFrameState() const;

        AnimationDiagnosticsReport diagnosticsReport() const;

    private:
        static bool isValidFrameIndex(
            std::size_t frameIndex,
            std::size_t frameCount);

        static int toPlacementFrameIndex(std::size_t frameIndex);

        static AnimationBindingResolutionResult makeSkippedResult(
            const AnimationBindingTarget& target,
            std::string message);

        AnimationBindingResolutionResult placeResolvedFrame(
            Composition::PageComposer& composer,
            const AnimationBindingTarget& target,
            const Animator& animator) const;

        static std::optional<std::size_t> resolveFrameIndexForTarget(
            const AnimationBindingTarget& target,
            const Animator& animator);

    private:
        std::unordered_map<std::string, Animator*> m_controllers;
        std::vector<AnimationBindingTarget> m_targets;
    };
}