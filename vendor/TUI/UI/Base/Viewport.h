#pragma once

#include "Core/Point.h"
#include "Core/Rect.h"
#include "Core/Size.h"

class Viewport
{
public:
    Viewport();
    Viewport(Size contentSize, Size viewSize);
    Viewport(int contentWidth, int contentHeight, int viewWidth, int viewHeight);

    Size contentSize() const;
    void setContentSize(Size size);
    void setContentSize(int width, int height);

    Size viewSize() const;
    void setViewSize(Size size);
    void setViewSize(int width, int height);

    Point scrollOffset() const;
    int scrollX() const;
    int scrollY() const;

    int maxScrollX() const;
    int maxScrollY() const;

    bool canScrollLeft() const;
    bool canScrollRight() const;
    bool canScrollUp() const;
    bool canScrollDown() const;

    void scrollBy(int deltaX, int deltaY);
    void scrollTo(int x, int y);

    void scrollToTop();
    void scrollToBottom();
    void scrollToLeft();
    void scrollToRight();

    Point contentToView(Point contentPoint) const;
    Point viewToContent(Point viewPoint) const;

    Rect visibleContentRect() const;

private:
    static int clampNonNegative(int value);
    static Size clampSize(Size size);
    static int clampValue(int value, int minimum, int maximum);

    void clampScrollOffset();

private:
    Size m_contentSize;
    Size m_viewSize;
    Point m_scrollOffset;
};