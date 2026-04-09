#pragma once

#include <cstdint>

class Color
{
public:
    enum class Kind : std::uint8_t
    {
        Default,
        Basic16,
        Indexed256,
        Rgb
    };

    // Backward-compatibility alias for older code that still refers to Basic.
    enum class Basic : std::uint8_t
    {
        Black,
        Red,
        Green,
        Yellow,
        Blue,
        Magenta,
        Cyan,
        White,

        BrightBlack,
        BrightRed,
        BrightGreen,
        BrightYellow,
        BrightBlue,
        BrightMagenta,
        BrightCyan,
        BrightWhite
    };

    struct RgbValue
    {
        std::uint8_t red = 0;
        std::uint8_t green = 0;
        std::uint8_t blue = 0;

        bool operator==(const RgbValue& other) const;
        bool operator!=(const RgbValue& other) const;
    };

private:
    struct IndexedValue
    {
        std::uint8_t index = 0;

        bool operator==(const IndexedValue& other) const;
        bool operator!=(const IndexedValue& other) const;
    };

    union Storage
    {
        Basic basic;
        IndexedValue indexed;
        RgbValue rgb;

        Storage();
        explicit Storage(Basic value);
        explicit Storage(IndexedValue value);
        explicit Storage(RgbValue value);
    };

public:
    Color();

    static Color Default();

    static Color FromBasic(Basic basic);
    static Color FromIndexed(std::uint8_t index);
    static Color FromRgb(std::uint8_t red, std::uint8_t green, std::uint8_t blue);

    Kind kind() const;

    bool isDefault() const;
    bool isBasic() const;
    bool isBasic16() const;
    bool isIndexed() const;
    bool isIndexed256() const;
    bool isRgb() const;
    bool isTrueColor() const;

    Basic basic() const;
    std::uint8_t index() const;
    std::uint8_t index256() const;

    const RgbValue& rgb() const;
    std::uint8_t red() const;
    std::uint8_t green() const;
    std::uint8_t blue() const;

    bool isBrightBasic() const;

    bool operator==(const Color& other) const;
    bool operator!=(const Color& other) const;

private:
    explicit Color(Kind kind);
    Color(Kind kind, Basic basic);
    Color(Kind kind, IndexedValue indexed);
    Color(Kind kind, RgbValue rgb);

private:
    Kind m_kind = Kind::Default;
    Storage m_storage;
};