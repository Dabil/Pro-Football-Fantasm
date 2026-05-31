#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "Core/Point.h"
#include "Core/Rect.h"
#include "Core/Size.h"
#include "Rendering/Composition/Alignment.h"
#include "Rendering/Composition/WritePolicy.h"

namespace Composition
{
    enum class ObjectSourceKind;
}

class PageCompositionDiagnostics
{
public:
    enum class OperationKind
    {
        Clear,
        LoadScreen,
        PlaceSourceAtPoint,
        PlaceSourceInRegion,
        WriteObjectAtPoint,
        WriteObjectInRegion,
        WriteTextLine,
        WriteTextBlock,
        CreateRegion,
        ClearRegions,
        Resize,
        SetFrames,
        ClearFrames,
        BeginFrame,
        EndFrame
    };

    struct FrameContext
    {
        int frameIndex = -1;
        std::string channel;
        std::string tag;

        bool isActive() const
        {
            return frameIndex >= 0 || !channel.empty() || !tag.empty();
        }
    };

    struct Entry
    {
        std::size_t sequence = 0;
        OperationKind operation = OperationKind::Clear;

        std::string operationName;
        std::string sourceKind;
        std::string sourceName;
        std::string regionName;
        std::string detail;

        Rect requestedRegion;
        Rect resolvedRegion;
        Point origin;
        Size contentSize;
        Composition::Alignment alignment;

        Composition::WritePolicy writePolicy;

        bool usedAlignment = false;
        bool usedOverrideStyle = false;
        bool usedAssetLibrary = false;
        bool usedXpConversion = false;
        bool clampRequested = false;
        bool clamped = false;
        bool success = false;

        int resolvedFrameIndex = -1;
        std::string errorMessage;

        FrameContext frameContext;
    };

    struct Summary
    {
        std::size_t totalEntries = 0;
        std::size_t successfulEntries = 0;
        std::size_t failedEntries = 0;
        std::size_t clampedEntries = 0;
    };

public:
    void clear();
    void record(Entry entry);

    const std::vector<Entry>& entries() const;
    const Summary& summary() const;

    void setLastDeterministicSignature(std::uint64_t signature);
    std::uint64_t lastDeterministicSignature() const;

    static std::uint64_t computeStableSignature(
        int width,
        int height,
        const std::u32string& renderedText,
        const FrameContext& frameContext);

private:
    std::vector<Entry> m_entries;
    Summary m_summary;
    std::size_t m_nextSequence = 0;
    std::uint64_t m_lastDeterministicSignature = 0;
};