// Rendering/Styles/Color.cpp
#include "Rendering/Styles/Color.h"

Color::Storage::Storage()
    : basic(Basic::White)
{
}

Color::Storage::Storage(Basic value)
    : basic(value)
{
}

Color::Storage::Storage(IndexedValue value)
    : indexed(value)
{
}

Color::Storage::Storage(RgbValue value)
    : rgb(value)
{
}

bool Color::RgbValue::operator==(const RgbValue& other) const
{
    return red == other.red
        && green == other.green
        && blue == other.blue;
}

bool Color::RgbValue::operator!=(const RgbValue& other) const
{
    return !(*this == other);
}

bool Color::IndexedValue::operator==(const IndexedValue& other) const
{
    return index == other.index;
}

bool Color::IndexedValue::operator!=(const IndexedValue& other) const
{
    return !(*this == other);
}

Color::Color()
    : m_kind(Kind::Default)
    , m_storage()
{
}

Color Color::Default()
{
    return Color(Kind::Default);
}

// Next 3 methods are the Main factories

Color Color::FromBasic(Basic basic)
{
    return Color(Kind::Basic16, basic);
}

Color Color::FromIndexed(std::uint8_t index)
{
    return Color(Kind::Indexed256, IndexedValue{ index });
}

Color Color::FromRgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue)
{
    return Color(Kind::Rgb, RgbValue{ red, green, blue });
}

Color::Kind Color::kind() const
{
    return m_kind;
}

bool Color::isDefault() const
{
    return m_kind == Kind::Default;
}

bool Color::isBasic() const
{
    return m_kind == Kind::Basic16;
}

bool Color::isBasic16() const
{
    return m_kind == Kind::Basic16;
}

bool Color::isIndexed() const
{
    return m_kind == Kind::Indexed256;
}

bool Color::isIndexed256() const
{
    return m_kind == Kind::Indexed256;
}

bool Color::isRgb() const
{
    return m_kind == Kind::Rgb;
}

bool Color::isTrueColor() const
{
    return m_kind == Kind::Rgb;
}

Color::Basic Color::basic() const
{
    return m_storage.basic;
}

std::uint8_t Color::index() const
{
    return m_storage.indexed.index;
}

std::uint8_t Color::index256() const
{
    return m_storage.indexed.index;
}

const Color::RgbValue& Color::rgb() const
{
    return m_storage.rgb;
}

std::uint8_t Color::red() const
{
    return m_storage.rgb.red;
}

std::uint8_t Color::green() const
{
    return m_storage.rgb.green;
}

std::uint8_t Color::blue() const
{
    return m_storage.rgb.blue;
}

bool Color::isBrightBasic() const
{
    if (!isBasic16())
    {
        return false;
    }

    switch (basic())
    {
    case Basic::BrightBlack:
    case Basic::BrightRed:
    case Basic::BrightGreen:
    case Basic::BrightYellow:
    case Basic::BrightBlue:
    case Basic::BrightMagenta:
    case Basic::BrightCyan:
    case Basic::BrightWhite:
        return true;

    case Basic::Black:
    case Basic::Red:
    case Basic::Green:
    case Basic::Yellow:
    case Basic::Blue:
    case Basic::Magenta:
    case Basic::Cyan:
    case Basic::White:
    default:
        return false;
    }
}

bool Color::operator==(const Color& other) const
{
    if (m_kind != other.m_kind)
    {
        return false;
    }

    switch (m_kind)
    {
    case Kind::Default:
        return true;

    case Kind::Basic16:
        return m_storage.basic == other.m_storage.basic;

    case Kind::Indexed256:
        return m_storage.indexed == other.m_storage.indexed;

    case Kind::Rgb:
        return m_storage.rgb == other.m_storage.rgb;

    default:
        return false;
    }
}

bool Color::operator!=(const Color& other) const
{
    return !(*this == other);
}

Color::Color(Kind kind)
    : m_kind(kind)
    , m_storage()
{
}

Color::Color(Kind kind, Basic basic)
    : m_kind(kind)
    , m_storage(basic)
{
}

Color::Color(Kind kind, IndexedValue indexed)
    : m_kind(kind)
    , m_storage(indexed)
{
}

Color::Color(Kind kind, RgbValue rgb)
    : m_kind(kind)
    , m_storage(rgb)
{
}