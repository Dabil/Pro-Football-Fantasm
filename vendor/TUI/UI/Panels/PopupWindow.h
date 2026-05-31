#pragma once

#include <string>

#include "Core/Rect.h"
#include "UI/Panels/Window.h"

class Surface;

class PopupWindow : public Window
{
public:
    PopupWindow();
    explicit PopupWindow(const Rect& bounds);
    PopupWindow(const Rect& bounds, std::string title);
    PopupWindow(const Rect& bounds, std::string title, bool modal);

    bool isOpen() const;
    void setOpen(bool open);

    void open();
    void close();
    void toggle();

    void draw(Surface& surface) override;

private:
    bool m_open = false;
};