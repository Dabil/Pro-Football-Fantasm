#include "UI/Content/ComposedWindowContent.h"

#include <algorithm>
#include <utility>

#include "Input/Event.h"
#include "Rendering/Composition/PageComposer.h"
#include "Rendering/ScreenBuffer.h"

namespace
{
    int positiveOrZero(int value)
    {
        return std::max(0, value);
    }

    Rect makeLocalBoundsFromContentBounds(const Rect& bounds)
    {
        Rect localBounds;
        localBounds.position = { 0, 0 };
        localBounds.size.width = positiveOrZero(bounds.size.width);
        localBounds.size.height = positiveOrZero(bounds.size.height);
        return localBounds;
    }

    bool areSameSize(const Rect& lhs, const Rect& rhs)
    {
        return lhs.size.width == rhs.size.width &&
            lhs.size.height == rhs.size.height;
    }

    void blitVisibleCells(
        const ScreenBuffer& source,
        ScreenBuffer& destination,
        const Rect& destinationBounds)
    {
        const int width = std::min(source.getWidth(), positiveOrZero(destinationBounds.size.width));
        const int height = std::min(source.getHeight(), positiveOrZero(destinationBounds.size.height));

        for (int y = 0; y < height; ++y)
        {
            const int destinationY = destinationBounds.position.y + y;

            for (int x = 0; x < width; ++x)
            {
                const int destinationX = destinationBounds.position.x + x;

                if (!destination.inBounds(destinationX, destinationY))
                {
                    continue;
                }

                const ScreenCell& cell = source.getCell(x, y);

                if (cell.isEmpty())
                {
                    continue;
                }

                destination.setCell(destinationX, destinationY, cell);
            }
        }
    }
}

namespace UI
{
    ComposedWindowContent::ComposedWindowContent(ComposeCallback compose)
        : m_compose(std::move(compose))
    {
    }

    ComposedWindowContent::~ComposedWindowContent() = default;

    std::string_view  ComposedWindowContent::contentTypeName() const
    {
        return "UI::ComposedWindowContent";
    }

    void ComposedWindowContent::setComposeCallback(ComposeCallback compose)
    {
        m_compose = std::move(compose);
        requestRecompose();
    }

    void ComposedWindowContent::requestRecompose()
    {
        m_dirty = true;
    }

    void ComposedWindowContent::update(const Animation::TickEvent& event)
    {
        (void)event;
    }

    bool ComposedWindowContent::handleEvent(const Input::Event& event)
    {
        (void)event;
        return false;
    }

    void ComposedWindowContent::draw(Surface& surface, const Rect& bounds)
    {
        resizeLocalSurface(bounds);
        rebuildIfDirty();

        if (m_localBounds.size.width <= 0 || m_localBounds.size.height <= 0)
        {
            return;
        }

        blitVisibleCells(m_surface.buffer(), surface.buffer(), bounds);
    }

    void ComposedWindowContent::onAttached()
    {
        m_attached = true;
        requestRecompose();
    }

    void ComposedWindowContent::onDetached()
    {
        m_attached = false;
    }

    void ComposedWindowContent::onBoundsChanged(const Rect& bounds)
    {
        resizeLocalSurface(bounds);
    }

    void ComposedWindowContent::resizeLocalSurface(const Rect& bounds)
    {
        const Rect newLocalBounds = makeLocalBoundsFromContentBounds(bounds);

        if (areSameSize(newLocalBounds, m_localBounds))
        {
            return;
        }

        m_localBounds = newLocalBounds;
        m_surface.resize(m_localBounds.size.width, m_localBounds.size.height);
        requestRecompose();
    }

    void ComposedWindowContent::rebuildIfDirty()
    {
        if (!m_dirty)
        {
            return;
        }

        m_dirty = false;

        m_surface.clear(Style());

        if (!m_attached)
        {
            return;
        }

        if (m_localBounds.size.width <= 0 || m_localBounds.size.height <= 0)
        {
            return;
        }

        if (!m_compose)
        {
            return;
        }

        Composition::PageComposer composer(m_surface.buffer());
        composer.clear(Style());
        m_compose(composer, m_localBounds);
    }
}