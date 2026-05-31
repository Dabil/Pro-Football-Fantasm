#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

class AsciiBannerFont
{
public:
    enum class Kind
    {
        FIGlet,
        Toilet
    };

    using GlyphRows = std::vector<std::string>;
    using GlyphMap = std::unordered_map<int, GlyphRows>;

public:
    AsciiBannerFont() = default;

    void clear()
    {
        m_name.clear();
        m_sourcePath.clear();
        m_kind = Kind::FIGlet;
        m_hardBlank = ' ';
        m_height = 0;
        m_baseline = 0;
        m_maxLength = 0;
        m_oldLayout = 0;
        m_commentLines = 0;
        m_printDirection = 0;
        m_fullLayout = 0;
        m_codetagCount = 0;
        m_glyphs.clear();
    }

    bool isLoaded() const
    {
        return m_height > 0 && !m_glyphs.empty();
    }

    const std::string& getName() const
    {
        return m_name;
    }

    void setName(std::string name)
    {
        m_name = std::move(name);
    }

    const std::filesystem::path& getSourcePath() const
    {
        return m_sourcePath;
    }

    void setSourcePath(std::filesystem::path sourcePath)
    {
        m_sourcePath = std::move(sourcePath);
    }

    Kind getKind() const
    {
        return m_kind;
    }

    void setKind(const Kind kind)
    {
        m_kind = kind;
    }

    char getHardBlank() const
    {
        return m_hardBlank;
    }

    void setHardBlank(const char hardBlank)
    {
        m_hardBlank = hardBlank;
    }

    int getHeight() const
    {
        return m_height;
    }

    void setHeight(const int height)
    {
        m_height = height;
    }

    int getBaseline() const
    {
        return m_baseline;
    }

    void setBaseline(const int baseline)
    {
        m_baseline = baseline;
    }

    int getMaxLength() const
    {
        return m_maxLength;
    }

    void setMaxLength(const int maxLength)
    {
        m_maxLength = maxLength;
    }

    int getOldLayout() const
    {
        return m_oldLayout;
    }

    void setOldLayout(const int oldLayout)
    {
        m_oldLayout = oldLayout;
    }

    int getCommentLines() const
    {
        return m_commentLines;
    }

    void setCommentLines(const int commentLines)
    {
        m_commentLines = commentLines;
    }

    int getPrintDirection() const
    {
        return m_printDirection;
    }

    void setPrintDirection(const int printDirection)
    {
        m_printDirection = printDirection;
    }

    int getFullLayout() const
    {
        return m_fullLayout;
    }

    void setFullLayout(const int fullLayout)
    {
        m_fullLayout = fullLayout;
    }

    int getCodetagCount() const
    {
        return m_codetagCount;
    }

    void setCodetagCount(const int codetagCount)
    {
        m_codetagCount = codetagCount;
    }

    const GlyphMap& getGlyphs() const
    {
        return m_glyphs;
    }

    GlyphMap& getGlyphs()
    {
        return m_glyphs;
    }

    const GlyphRows* findGlyph(const int codePoint) const
    {
        const auto it = m_glyphs.find(codePoint);
        if (it == m_glyphs.end())
        {
            return nullptr;
        }

        return &it->second;
    }

    bool hasGlyph(const int codePoint) const
    {
        return m_glyphs.find(codePoint) != m_glyphs.end();
    }

    void setGlyph(const int codePoint, GlyphRows glyphRows)
    {
        m_glyphs[codePoint] = std::move(glyphRows);
    }

private:
    std::string m_name;
    std::filesystem::path m_sourcePath;
    Kind m_kind = Kind::FIGlet;

    char m_hardBlank = ' ';

    int m_height = 0;
    int m_baseline = 0;
    int m_maxLength = 0;

    int m_oldLayout = 0;
    int m_commentLines = 0;
    int m_printDirection = 0;
    int m_fullLayout = 0;
    int m_codetagCount = 0;

    GlyphMap m_glyphs;
};
