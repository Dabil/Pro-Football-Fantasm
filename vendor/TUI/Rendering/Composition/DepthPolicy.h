#pragma once

namespace Composition
{
    /*
        DepthPolicy answers:

            "How should this write interact with future depth-aware composition?"

        This tier keeps the enum backend-agnostic and policy-only.

        Semantics:
        - Ignore:
            Do not consult or affect depth ordering.
        - Respect:
            Participate in normal depth-aware composition.
        - BehindOnly:
            Only write when the source is behind existing depth ownership.
            Exact depth comparison mechanics belong to later implementation tiers.
    */
    enum class DepthPolicy
    {
        Ignore,
        Respect,
        BehindOnly
    };
}