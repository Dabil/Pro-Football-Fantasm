#pragma once

#include "Rendering/Text/TextTypes.h"

/*
    Purpose:

    Central width policy for code points.
*/

namespace UnicodeWidth
{
    bool isCombiningMark(char32_t codePoint);
    bool isEastAsianWide(char32_t codePoint);
    bool isEmojiPresentation(char32_t codePoint);

    CellWidth measureCodePointWidth(char32_t codePoint);
}