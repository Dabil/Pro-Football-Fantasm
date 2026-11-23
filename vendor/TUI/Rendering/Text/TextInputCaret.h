#pragma once

#include <optional>

#include "Core/Point.h"

// Temporary capture for the focused editor drawn by the active screen.
// Renderers that use native text input can place their candidate window here.
namespace TextInputCaret
{
    inline thread_local std::optional<Point>* activeCapture = nullptr;

    class Capture
    {
    public:
        Capture() : previous(activeCapture) { activeCapture = &position; }
        ~Capture() { activeCapture = previous; }
        Capture(const Capture&) = delete;
        Capture& operator=(const Capture&) = delete;

        std::optional<Point> position;

    private:
        std::optional<Point>* previous;
    };

    inline void record(Point point)
    {
        if (activeCapture) *activeCapture = point;
    }
}
