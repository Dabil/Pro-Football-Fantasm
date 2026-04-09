#pragma once

#include "Rendering/ScreenBuffer.h"

class Surface
{
public:
    Surface();
    Surface(int width, int height);

    void resize(int width, int height);
    void clear(const Style& style);

    ScreenBuffer& buffer();
    const ScreenBuffer& buffer() const;

private:
    ScreenBuffer m_buffer;
};