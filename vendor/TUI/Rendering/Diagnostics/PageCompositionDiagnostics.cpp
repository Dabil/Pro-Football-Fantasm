#include "Rendering/Diagnostics/PageCompositionDiagnostics.h"

namespace
{
    constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ull;
    constexpr std::uint64_t FnvPrime = 1099511628211ull;

    void fnvAppend(std::uint64_t& hash, std::uint64_t value)
    {
        for (int i = 0; i < 8; ++i)
        {
            const std::uint8_t byte =
                static_cast<std::uint8_t>((value >> (i * 8)) & 0xFFu);
            hash ^= static_cast<std::uint64_t>(byte);
            hash *= FnvPrime;
        }
    }

    void fnvAppendString(std::uint64_t& hash, const std::string& value)
    {
        for (unsigned char ch : value)
        {
            hash ^= static_cast<std::uint64_t>(ch);
            hash *= FnvPrime;
        }

        hash ^= 0xFFu;
        hash *= FnvPrime;
    }

    void fnvAppendU32String(std::uint64_t& hash, const std::u32string& value)
    {
        for (char32_t ch : value)
        {
            fnvAppend(hash, static_cast<std::uint64_t>(ch));
        }

        hash ^= 0xFEu;
        hash *= FnvPrime;
    }
}

void PageCompositionDiagnostics::clear()
{
    m_entries.clear();
    m_summary = Summary{};
    m_nextSequence = 0;
    m_lastDeterministicSignature = 0;
}

void PageCompositionDiagnostics::record(Entry entry)
{
    entry.sequence = m_nextSequence++;
    m_entries.push_back(std::move(entry));

    ++m_summary.totalEntries;

    if (m_entries.back().success)
    {
        ++m_summary.successfulEntries;
    }
    else
    {
        ++m_summary.failedEntries;
    }

    if (m_entries.back().clamped)
    {
        ++m_summary.clampedEntries;
    }
}

const std::vector<PageCompositionDiagnostics::Entry>&
PageCompositionDiagnostics::entries() const
{
    return m_entries;
}

const PageCompositionDiagnostics::Summary&
PageCompositionDiagnostics::summary() const
{
    return m_summary;
}

void PageCompositionDiagnostics::setLastDeterministicSignature(
    std::uint64_t signature)
{
    m_lastDeterministicSignature = signature;
}

std::uint64_t PageCompositionDiagnostics::lastDeterministicSignature() const
{
    return m_lastDeterministicSignature;
}

std::uint64_t PageCompositionDiagnostics::computeStableSignature(
    int width,
    int height,
    const std::u32string& renderedText,
    const FrameContext& frameContext)
{
    std::uint64_t hash = FnvOffsetBasis;

    fnvAppend(hash, static_cast<std::uint64_t>(width));
    fnvAppend(hash, static_cast<std::uint64_t>(height));
    fnvAppend(hash, static_cast<std::uint64_t>(frameContext.frameIndex));
    fnvAppendString(hash, frameContext.channel);
    fnvAppendString(hash, frameContext.tag);
    fnvAppendU32String(hash, renderedText);

    return hash;
}