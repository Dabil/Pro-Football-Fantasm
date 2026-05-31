#include "UI/Panels/PopupWindow.h"

#include <utility>

PopupWindow::PopupWindow()
    : Window()
{
    close();
}

PopupWindow::PopupWindow(const Rect& bounds)
    : Window(bounds)
{
    close();
}

PopupWindow::PopupWindow(const Rect& bounds, std::string title)
    : Window(bounds, std::move(title))
{
    close();
}

PopupWindow::PopupWindow(const Rect& bounds, std::string title, bool modal)
    : Window(bounds, std::move(title), modal)
{
    close();
}

bool PopupWindow::isOpen() const
{
    return m_open;
}

void PopupWindow::setOpen(bool open)
{
    if (open)
    {
        this->open();
    }
    else
    {
        close();
    }
}

void PopupWindow::open()
{
    m_open = true;
    show();
}

void PopupWindow::close()
{
    m_open = false;
    hide();
}

void PopupWindow::toggle()
{
    setOpen(!m_open);
}

void PopupWindow::draw(Surface& surface)
{
    if (!m_open || !isVisible())
    {
        return;
    }

    Window::draw(surface);
}