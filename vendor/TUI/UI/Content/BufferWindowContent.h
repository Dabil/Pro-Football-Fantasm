#pragma once

#include <memory>

#include "Core/Rect.h"
#include "Core/Size.h"
#include "Rendering/ScreenBuffer.h"
#include "UI/Content/IWindowContent.h"

/*
    BufferWindowContent supports two distinct operating modes:

    ------------------------------------------------------------------------
    OWNED MODE
    ------------------------------------------------------------------------

    In owned mode, BufferWindowContent internally owns and manages the
    lifetime of its ScreenBuffer.

        auto content =
            UI::BufferWindowContent::createOwned(40, 12);

    The content is responsible for:
        - buffer allocation
        - buffer destruction
        - buffer resizing
        - buffer persistence

    Owned mode is useful when the window content itself is the render target.

    Typical use cases:
        - cached render targets
        - framebuffer-style content
        - telemetry graphs
        - mini-maps
        - internal diagnostics
        - software-rendered panels
        - temporary offscreen rendering
        - composition caches

    Owned buffers automatically resize to match content bounds changes.


    ------------------------------------------------------------------------
    BORROWED MODE
    ------------------------------------------------------------------------

    In borrowed mode, BufferWindowContent references an external
    ScreenBuffer without owning it.

        ScreenBuffer rendererBuffer(...);

        auto content =
            UI::BufferWindowContent::createBorrowed(rendererBuffer);

    The external system remains responsible for:
        - buffer lifetime
        - buffer resizing
        - buffer clearing
        - rendering updates

    BufferWindowContent acts only as a viewport/display adapter.

    Typical use cases:
        - renderer previews
        - live framebuffer inspection
        - debugging external systems
        - render pipeline visualization
        - composition previews
        - emulator/video previews
        - mirroring another render target

    Borrowed buffers are NEVER automatically resized or destroyed by
    BufferWindowContent.

    This explicit owned vs borrowed distinction prevents ambiguity around:
        - lifetime ownership
        - resize authority
        - destruction responsibility
        - render-target synchronization
*/

namespace UI
{
    class BufferWindowContent final : public IWindowContent
    {
    public:
        BufferWindowContent();
        explicit BufferWindowContent(Size size);
        BufferWindowContent(int width, int height);

        explicit BufferWindowContent(ScreenBuffer& borrowedBuffer);

        static std::unique_ptr<BufferWindowContent> createOwned(Size size);
        static std::unique_ptr<BufferWindowContent> createOwned(int width, int height);
        static std::unique_ptr<BufferWindowContent> createBorrowed(ScreenBuffer& borrowedBuffer);

        BufferWindowContent(const BufferWindowContent&) = delete;
        BufferWindowContent& operator=(const BufferWindowContent&) = delete;

        BufferWindowContent(BufferWindowContent&&) noexcept = default;
        BufferWindowContent& operator=(BufferWindowContent&&) noexcept = default;

        ~BufferWindowContent() override;

        std::string_view contentTypeName() const override;

        bool ownsBuffer() const;
        bool hasBuffer() const;

        ScreenBuffer& buffer();
        const ScreenBuffer& buffer() const;

        ScreenBuffer* ownedBuffer();
        const ScreenBuffer* ownedBuffer() const;

        ScreenBuffer& sourceBuffer();
        const ScreenBuffer& sourceBuffer() const;

        void setCopyEmptyCells(bool copyEmptyCells);
        bool copyEmptyCells() const;

        void resizeOwnedBuffer(Size size);
        void resizeOwnedBuffer(int width, int height);

        void update(const Animation::TickEvent& event) override;
        bool handleEvent(const Input::Event& event) override;
        void draw(Surface& surface, const Rect& bounds) override;
        void onBoundsChanged(const Rect& bounds) override;

    private:
        void resizeOwnedBufferToBounds(const Rect& bounds);

    private:
        std::unique_ptr<ScreenBuffer> m_ownedBuffer;
        ScreenBuffer* m_buffer = nullptr;
        bool m_copyEmptyCells = true;
    };
}