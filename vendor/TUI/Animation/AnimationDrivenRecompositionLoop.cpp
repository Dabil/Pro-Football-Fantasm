#include "Animation/AnimationDrivenRecompositionLoop.h"

#include <algorithm>
#include <utility>

namespace Animation
{
    void AnimationDrivenRecompositionLoop::setComposePageCallback(
        ComposePageCallback callback)
    {
        m_composePage = std::move(callback);
        requestRecompose();
    }

    void AnimationDrivenRecompositionLoop::clearComposePageCallback()
    {
        m_composePage = nullptr;
        requestRecompose();
    }

    void AnimationDrivenRecompositionLoop::setBindingResolver(
        AnimationBindingResolver* resolver)
    {
        m_bindingResolver = resolver;
        resetFrameTracking();
        requestRecompose();
    }

    void AnimationDrivenRecompositionLoop::clearBindingResolver()
    {
        m_bindingResolver = nullptr;
        resetFrameTracking();
        requestRecompose();
    }

    void AnimationDrivenRecompositionLoop::setInvalidationRegion(
        Rendering::InvalidationRegion* invalidation)
    {
        m_invalidation = invalidation;
    }

    void AnimationDrivenRecompositionLoop::clearInvalidationRegion()
    {
        m_invalidation = nullptr;
    }

    void AnimationDrivenRecompositionLoop::setClearStyle(const Style& style)
    {
        m_clearStyle = style;
        requestRecompose();
    }

    void AnimationDrivenRecompositionLoop::requestRecompose()
    {
        m_forceRecompose = true;
    }

    void AnimationDrivenRecompositionLoop::resetFrameTracking()
    {
        m_lastFrameState.clear();
        m_lastKnownBindingBounds.clear();
    }

    AnimationDrivenRecompositionLoop::Result
        AnimationDrivenRecompositionLoop::updateAndRecomposeIfNeeded(
            ScreenBuffer& buffer,
            const TickEvent& event)
    {
        if (m_bindingResolver != nullptr)
        {
            m_bindingResolver->updateBoundControllers(event);
        }

        const std::vector<AnimationBindingFrameState> currentFrameState =
            m_bindingResolver != nullptr
            ? m_bindingResolver->captureFrameState()
            : std::vector<AnimationBindingFrameState>{};

        const bool frameStateChanged = shouldRecompose(currentFrameState);

        if (!m_forceRecompose && !frameStateChanged)
        {
            Result result;
            result.frameStateChanged = false;
            result.forced = false;
            result.recomposed = false;

            captureDiagnostics(result);
            return result;
        }

        Result result = composeIntoBuffer(
            buffer,
            m_forceRecompose,
            frameStateChanged);

        applyInvalidation(result, currentFrameState);
        m_lastFrameState = currentFrameState;

        captureDiagnostics(result);
        return result;
    }

    AnimationDrivenRecompositionLoop::Result
        AnimationDrivenRecompositionLoop::recomposeNow(ScreenBuffer& buffer)
    {
        const std::vector<AnimationBindingFrameState> currentFrameState =
            m_bindingResolver != nullptr
            ? m_bindingResolver->captureFrameState()
            : std::vector<AnimationBindingFrameState>{};

        Result result = composeIntoBuffer(
            buffer,
            true,
            true);

        applyInvalidation(result, currentFrameState);
        m_lastFrameState = currentFrameState;

        captureDiagnostics(result);
        return result;
    }

    AnimationDrivenRecompositionLoop::Result
        AnimationDrivenRecompositionLoop::composeIntoBuffer(
            ScreenBuffer& buffer,
            bool forced,
            bool frameStateChanged)
    {
        Result result;
        result.recomposed = true;
        result.forced = forced;
        result.frameStateChanged = frameStateChanged;

        Composition::PageComposer composer(buffer);
        composer.clear(m_clearStyle);

        if (m_composePage)
        {
            m_composePage(composer);
        }

        if (m_bindingResolver != nullptr)
        {
            result.bindingResults = m_bindingResolver->resolveAll(composer);
        }

        result.deterministicSignature =
            composer.computeDeterministicSignature();

        m_forceRecompose = false;
        return result;
    }

    void AnimationDrivenRecompositionLoop::applyInvalidation(
        Result& result,
        const std::vector<AnimationBindingFrameState>& currentFrameState)
    {
        if (m_invalidation == nullptr || !result.recomposed)
        {
            return;
        }

        if (result.forced)
        {
            m_invalidation->invalidateAll();
            result.invalidated = true;
            result.wholePageInvalidated = true;
            result.dirtyRegionCount = 0;

            m_lastKnownBindingBounds.clear();
            for (const AnimationBindingResolutionResult& bindingResult : result.bindingResults)
            {
                if (bindingResult.placementBounds.has_value())
                {
                    m_lastKnownBindingBounds[makeBindingKey(
                        bindingResult.targetName,
                        bindingResult.controllerName)] = *bindingResult.placementBounds;
                }
            }
            return;
        }

        if (!result.frameStateChanged)
        {
            return;
        }

        std::unordered_map<std::string, Rect> nextKnownBounds;
        nextKnownBounds.reserve(result.bindingResults.size());

        for (const AnimationBindingResolutionResult& bindingResult : result.bindingResults)
        {
            const std::string key = makeBindingKey(
                bindingResult.targetName,
                bindingResult.controllerName);

            const auto oldBounds = m_lastKnownBindingBounds.find(key);
            const bool hadOldBounds = oldBounds != m_lastKnownBindingBounds.end();

            if (bindingResult.placementBounds.has_value())
            {
                m_invalidation->invalidateRect(*bindingResult.placementBounds);
                nextKnownBounds[key] = *bindingResult.placementBounds;
            }

            if (hadOldBounds)
            {
                m_invalidation->invalidateRect(oldBounds->second);
            }

            if (!bindingResult.placementBounds.has_value() && !hadOldBounds)
            {
                m_invalidation->invalidateAll();
                result.invalidated = true;
                result.wholePageInvalidated = true;
                result.dirtyRegionCount = 0;
                m_lastKnownBindingBounds = std::move(nextKnownBounds);
                return;
            }
        }

        for (const auto& oldEntry : m_lastKnownBindingBounds)
        {
            const bool stillPresent = std::any_of(
                currentFrameState.begin(),
                currentFrameState.end(),
                [&oldEntry](const AnimationBindingFrameState& state)
                {
                    return makeBindingKey(
                        state.targetName,
                        state.controllerName) == oldEntry.first;
                });

            if (!stillPresent)
            {
                m_invalidation->invalidateRect(oldEntry.second);
            }
        }

        result.invalidated = !m_invalidation->isEmpty();
        result.wholePageInvalidated = m_invalidation->isWholePageInvalidated();
        result.dirtyRegionCount = m_invalidation->dirtyRegions().size();
        m_lastKnownBindingBounds = std::move(nextKnownBounds);
    }

    void AnimationDrivenRecompositionLoop::captureDiagnostics(
        const Result& result)
    {
        if (m_diagnostics == nullptr || !m_diagnostics->enabled)
        {
            return;
        }

        m_diagnostics->clearRuntimeData();

        if (m_bindingResolver != nullptr)
        {
            AnimationDiagnosticsReport bindingReport =
                m_bindingResolver->diagnosticsReport();

            m_diagnostics->controllers = std::move(bindingReport.controllers);
            m_diagnostics->bindings = std::move(bindingReport.bindings);
        }

        m_diagnostics->lastRecomposition.recomposed = result.recomposed;
        m_diagnostics->lastRecomposition.frameStateChanged = result.frameStateChanged;
        m_diagnostics->lastRecomposition.forced = result.forced;
        m_diagnostics->lastRecomposition.invalidated = result.invalidated;
        m_diagnostics->lastRecomposition.wholePageInvalidated = result.wholePageInvalidated;
        m_diagnostics->lastRecomposition.dirtyRegionCount = result.dirtyRegionCount;
        m_diagnostics->lastRecomposition.bindingResultCount = result.bindingResults.size();
        m_diagnostics->lastRecomposition.deterministicSignature =
            result.deterministicSignature;
    }

    std::string AnimationDrivenRecompositionLoop::makeBindingKey(
        const std::string& targetName,
        const std::string& controllerName)
    {
        return targetName + "\x1f" + controllerName;
    }

    bool AnimationDrivenRecompositionLoop::shouldRecompose(
        const std::vector<AnimationBindingFrameState>& currentFrameState) const
    {
        if (m_forceRecompose)
        {
            return true;
        }

        return currentFrameState != m_lastFrameState;
    }

    void AnimationDrivenRecompositionLoop::setDiagnosticsReport(
        AnimationDiagnosticsReport* diagnostics)
    {
        m_diagnostics = diagnostics;
    }

    void AnimationDrivenRecompositionLoop::clearDiagnosticsReport()
    {
        m_diagnostics = nullptr;
    }
}