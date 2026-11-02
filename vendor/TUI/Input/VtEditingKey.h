#pragma once
#include <cstdint>
#include <string_view>

namespace Input::Detail
{
    // Private shared decoder for VT editing keys (no public widget API change).
    inline std::uint16_t vtEditingKey(std::wstring_view value)
    {
        if (value == L"\x1B[3~") return 0x2E;
        if (value == L"\x1B[2~") return 0x2D;
        if (value == L"\x1B[H" || value == L"\x1B[1~" || value == L"\x1B[7~") return 0x24;
        if (value == L"\x1B[F" || value == L"\x1B[4~" || value == L"\x1B[8~") return 0x23;
        if (value == L"\x1B[5~") return 0x21;
        if (value == L"\x1B[6~") return 0x22;
        return 0;
    }
}
