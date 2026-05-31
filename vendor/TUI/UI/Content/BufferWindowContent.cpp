#include "UI/Content/BufferWindowContent.h"

#include <algorithm>
#include <stdexcept>

#include "Input/Event.h"
#include "Rendering/Surface.h"

namespace
{
    int positiveOrZero(int value)
    {
        return std::max(0, value);
    }

    Size safeSize(Size size)
    {
        size.width = positiveOrZero(size.width);
        size.height = positiveOrZero(size.height);
        return size;
    }

    Size sizeFromBounds(const Rect& bounds)
    {
        return safeSize(bounds.size);
    }

    void blitBuffer(
        const ScreenBuffer& source,
        ScreenBuffer& destination,
        const Rect& destinationBounds,
        bool copyEmptyCells)
    {
        const int width = std::min(
            source.getWidth(),
            positiveOrZero(destinationBounds.size.width));

        const int height = std::min(
            source.getHeight(),
            positiveOrZero(destinationBounds.size.height));

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

                if (!copyEmptyCells && cell.isEmpty())
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
    BufferWindowContent::BufferWindowContent()
        : BufferWindowContent(Size{ 0, 0 })
    {
    }

    BufferWindowContent::BufferWindowContent(Size size)
        : m_ownedBuffer(std::make_unique<ScreenBuffer>(
            positiveOrZero(size.width),
            positiveOrZero(size.height)))
        , m_buffer(m_ownedBuffer.get())
    {
    }

    BufferWindowContent::BufferWindowContent(int width, int height)
        : BufferWindowContent(Size{ width, height })
    {
    }

    BufferWindowContent::BufferWindowContent(ScreenBuffer& borrowedBuffer)
        : m_buffer(&borrowedBuffer)
    {
    }

    std::unique_ptr<BufferWindowContent> BufferWindowContent::createOwned(Size size)
    {
        return std::make_unique<BufferWindowContent>(size);
    }

    std::unique_ptr<BufferWindowContent> BufferWindowContent::createOwned(int width, int height)
    {
        return std::make_unique<BufferWindowContent>(width, height);
    }

    std::unique_ptr<BufferWindowContent> BufferWindowContent::createBorrowed(ScreenBuffer& borrowedBuffer)
    {
        return std::make_unique<BufferWindowContent>(borrowedBuffer);
    }

    BufferWindowContent::~BufferWindowContent() = default;

    std::string_view UI::BufferWindowContent::contentTypeName() const
    {
        return "UI::BufferWindowContent";
    }

    bool BufferWindowContent::ownsBuffer() const
    {
        return m_ownedBuffer != nullptr;
    }

    bool BufferWindowContent::hasBuffer() const
    {
        return m_buffer != nullptr;
    }

    ScreenBuffer& BufferWindowContent::buffer()
    {
        return sourceBuffer();
    }

    const ScreenBuffer& BufferWindowContent::buffer() const
    {
        return sourceBuffer();
    }

    ScreenBuffer* BufferWindowContent::ownedBuffer()
    {
        return m_ownedBuffer.get();
    }

    const ScreenBuffer* BufferWindowContent::ownedBuffer() const
    {
        return m_ownedBuffer.get();
    }

    ScreenBuffer& BufferWindowContent::sourceBuffer()
    {
        if (m_buffer == nullptr)
        {
            throw std::logic_error("BufferWindowContent has no source buffer.");
        }

        return *m_buffer;
    }

    const ScreenBuffer& BufferWindowContent::sourceBuffer() const
    {
        if (m_buffer == nullptr)
        {
            throw std::logic_error("BufferWindowContent has no source buffer.");
        }

        return *m_buffer;
    }

    void BufferWindowContent::setCopyEmptyCells(bool copyEmptyCells)
    {
        m_copyEmptyCells = copyEmptyCells;
    }

    bool BufferWindowContent::copyEmptyCells() const
    {
        return m_copyEmptyCells;
    }

    void BufferWindowContent::resizeOwnedBuffer(Size size)
    {
        if (m_ownedBuffer == nullptr)
        {
            return;
        }

        const Size safe = safeSize(size);
        m_ownedBuffer->resize(safe.width, safe.height);
    }

    void BufferWindowContent::resizeOwnedBuffer(int width, int height)
    {
        resizeOwnedBuffer(Size{ width, height });
    }

    void BufferWindowContent::update(const Animation::TickEvent& event)
    {
        (void)event;
    }

    bool BufferWindowContent::handleEvent(const Input::Event& event)
    {
        (void)event;
        return false;
    }

    void BufferWindowContent::draw(Surface& surface, const Rect& bounds)
    {
        if (m_buffer == nullptr)
        {
            return;
        }

        if (bounds.size.width <= 0 || bounds.size.height <= 0)
        {
            return;
        }

        blitBuffer(*m_buffer, surface.buffer(), bounds, m_copyEmptyCells);
    }

    void BufferWindowContent::onBoundsChanged(const Rect& bounds)
    {
        resizeOwnedBufferToBounds(bounds);
    }

    void BufferWindowContent::resizeOwnedBufferToBounds(const Rect& bounds)
    {
        if (m_ownedBuffer == nullptr)
        {
            return;
        }

        const Size size = sizeFromBounds(bounds);

        if (m_ownedBuffer->getWidth() == size.width &&
            m_ownedBuffer->getHeight() == size.height)
        {
            return;
        }

        m_ownedBuffer->resize(size.width, size.height);
    }
}