#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "Core/Rect.h"
#include "Rendering/Styles/Style.h"
#include "UI/Panels/ScrollablePanel.h"
#include "UI/Widgets/WidgetStyle.h"

class Surface;

namespace Input
{
    class Event;
}

class TextView : public ScrollablePanel
{
public:
    TextView();
    explicit TextView(std::string title);
    TextView(const Rect& bounds, std::string title = {});

    void setText(std::string_view text);
    void setLines(const std::vector<std::string>& lines);
    void setLines(std::vector<std::string>&& lines);
    void appendLine(std::string line);
    void clear();

    const std::vector<std::string>& lines() const;
    bool empty() const;
    std::size_t lineCount() const;

    void setTextStyle(const Style& style);
    const Style& textStyle() const;

    const WidgetStyles::StyleSet& textStyleSet() const;
    void setTextStyleSet(const WidgetStyles::StyleSet& styleSet);

    bool handleEvent(const Input::Event& event) override;

protected:
    void drawScrollableContent(
        Surface& surface,
        const Rect& visibleContentRect) override;

private:
    void updateContentSizeFromLines();
    static std::vector<std::string> splitLines(std::string_view text);
    static std::string sliceUtf8ByDisplayColumns(
        std::string_view text,
        int startColumn,
        int maxColumns);

private:
    std::vector<std::string> m_lines;
    WidgetStyles::StyleSet m_textStyleSet;
};