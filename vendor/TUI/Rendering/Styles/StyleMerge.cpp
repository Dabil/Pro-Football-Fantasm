#include "Rendering/Styles/StyleMerge.h"

/*

Logical style merge rules:

Replace:
    source completely replaces destination

PreserveDestination:
    destination completely wins

MergePreserveDestination:
    destination is the base
    source overlays only the fields it explicitly authored

Important:
    color merge here is purely logical authored-intent merge
    ThemeColor is not resolved here
    renderer capability or downgrade logic does not belong here

*/

namespace
{
    void mergeOptionalColor(
        Style& result,
        const Style& source,
        bool foreground)
    {
        if (foreground)
        {
            if (!source.hasForegroundColorValue())
            {
                return;
            }

            const Style::StyleColorValue& value = *source.foregroundColorValue();

            if (value.hasConcreteColor())
            {
                result = result.withForeground(*value.concreteColor());
            }
            else if (value.hasThemeColor())
            {
                result = result.withForeground(*value.themeColor());
            }
        }
        else
        {
            if (!source.hasBackgroundColorValue())
            {
                return;
            }

            const Style::StyleColorValue& value = *source.backgroundColorValue();

            if (value.hasConcreteColor())
            {
                result = result.withBackground(*value.concreteColor());
            }
            else if (value.hasThemeColor())
            {
                result = result.withBackground(*value.themeColor());
            }
        }
    }

    void mergeOptionalAttribute(
        Style& result,
        const Style::AttributeState& state,
        Style(Style::* setter)(bool) const)
    {
        if (state.has_value())
        {
            result = (result.*setter)(*state);
        }
    }
}

Style StyleMerge::merge(
    const Style& destination,
    const Style& source,
    StyleMergeMode mode)
{
    switch (mode)
    {
    case StyleMergeMode::Replace:
        return source;

    case StyleMergeMode::PreserveDestination:
        return destination;

    case StyleMergeMode::MergePreserveDestination:
        return mergePreserveDestination(destination, source);

    default:
        return destination;
    }
}

Style StyleMerge::mergePreserveDestination(
    const Style& destination,
    const Style& source)
{
    Style result = destination;

    mergeOptionalColor(result, source, true);
    mergeOptionalColor(result, source, false);

    mergeOptionalAttribute(result, source.boldState(), &Style::withBold);
    mergeOptionalAttribute(result, source.dimState(), &Style::withDim);
    mergeOptionalAttribute(result, source.underlineState(), &Style::withUnderline);
    mergeOptionalAttribute(result, source.slowBlinkState(), &Style::withSlowBlink);
    mergeOptionalAttribute(result, source.fastBlinkState(), &Style::withFastBlink);
    mergeOptionalAttribute(result, source.reverseState(), &Style::withReverse);
    mergeOptionalAttribute(result, source.invisibleState(), &Style::withInvisible);
    mergeOptionalAttribute(result, source.strikeState(), &Style::withStrike);

    return result;
}