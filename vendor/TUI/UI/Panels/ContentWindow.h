#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "UI/Content/IWindowContent.h"
#include "UI/Panels/Window.h"

namespace UI
{
    class ContentWindow final : public Window
    {
    public:
        ContentWindow(const Rect& bounds, std::string title, std::unique_ptr<IWindowContent> content);
        ~ContentWindow() override;

        IWindowContent* content();
        const IWindowContent* content() const;
        std::string_view contentTypeName() const;

        bool hasTransferableContent() const override;
        std::unique_ptr<IWindowContent> releaseContent() override;

        void update(const Animation::TickEvent& event) override;
        bool handleEvent(const Input::Event& event) override;
        void draw(Surface& surface) override;

    private:
        void detachContent();
        void notifyContentBoundsChanged();

    private:
        std::unique_ptr<IWindowContent> m_content;
        Rect m_lastContentBounds{};
        bool m_hasLastContentBounds = false;
    };
}