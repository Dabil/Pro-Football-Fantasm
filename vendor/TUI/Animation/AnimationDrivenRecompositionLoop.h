#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

#include "Animation/AnimationBindingResolver.h"
#include "Animation/AnimationDiagnostics.h"
#include "Animation/TickEvent.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/InvalidationRegion.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Style.h"

namespace Animation
{
    class AnimationDrivenRecompositionLoop
    {
    public:
        using ComposePageCallback =
            std::function<void(Composition::PageComposer& composer)>;

        struct Result
        {
            bool recomposed = false;
            bool frameStateChanged = false;
            bool forced = false;
            std::uint64_t deterministicSignature = 0;
            bool invalidated = false;
            bool wholePageInvalidated = false;
            std::size_t dirtyRegionCount = 0;
            std::vector<AnimationBindingResolutionResult> bindingResults;
        };

        void setComposePageCallback(ComposePageCallback callback);
        void clearComposePageCallback();

        void setBindingResolver(AnimationBindingResolver* resolver);
        void clearBindingResolver();

        void setInvalidationRegion(Rendering::InvalidationRegion* invalidation);
        void clearInvalidationRegion();

        void setClearStyle(const Style& style);
        void requestRecompose();
        void resetFrameTracking();

        Result updateAndRecomposeIfNeeded(
            ScreenBuffer& buffer,
            const TickEvent& event);

        Result recomposeNow(ScreenBuffer& buffer);

        void setDiagnosticsReport(AnimationDiagnosticsReport* diagnostics);
        void clearDiagnosticsReport();

    private:
        Result composeIntoBuffer(
            ScreenBuffer& buffer,
            bool forced,
            bool frameStateChanged);

        bool shouldRecompose(
            const std::vector<AnimationBindingFrameState>& currentFrameState) const;

        void applyInvalidation(
            Result& result,
            const std::vector<AnimationBindingFrameState>& currentFrameState);

        void captureDiagnostics(const Result& result);

        static std::string makeBindingKey(
            const std::string& targetName,
            const std::string& controllerName);

    private:
        ComposePageCallback m_composePage;
        AnimationBindingResolver* m_bindingResolver = nullptr;
        Rendering::InvalidationRegion* m_invalidation = nullptr;

        Style m_clearStyle;
        bool m_forceRecompose = true;

        std::vector<AnimationBindingFrameState> m_lastFrameState;
        std::unordered_map<std::string, Rect> m_lastKnownBindingBounds;

        AnimationDiagnosticsReport* m_diagnostics = nullptr;
    };
}