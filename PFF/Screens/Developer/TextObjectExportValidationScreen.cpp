#include "Screens/Developer/TextObjectExportValidationScreen.h"

#include <algorithm>
#include <sstream>
#include <string>

#include "Core/Rect.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/Style.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/Themes.h"
#include "Rendering/Surface.h"

namespace
{
    constexpr int MinimumWidth = 100;
    constexpr int MinimumHeight = 32;

    const Style ValidationBackground =
        style::Bg(Color::FromBasic(Color::Basic::Black));

    const Style GoodCellStyle =
        Themes::Text;

    const Style FailingCellStyle =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(Color::FromBasic(Color::Basic::Red));

    const Style FirstFailingCellStyle =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::Black))
        + style::Bg(Color::FromBasic(Color::Basic::BrightYellow));

    const Style WarningCellStyle =
        style::Bold
        + style::Fg(Color::FromBasic(Color::Basic::BrightWhite))
        + style::Bg(Color::FromBasic(Color::Basic::Magenta));

    const Style ViewportFrameStyle =
        Themes::Frame;

    const Style ViewportFillStyle =
        style::Fg(Color::FromBasic(Color::Basic::White))
        + style::Bg(Color::FromBasic(Color::Basic::Black));

    std::string clipText(const std::string& text, int width)
    {
        if (width <= 0)
        {
            return {};
        }

        if (static_cast<int>(text.size()) <= width)
        {
            return text;
        }

        if (width <= 3)
        {
            return text.substr(0, static_cast<std::size_t>(width));
        }

        return text.substr(0, static_cast<std::size_t>(width - 3)) + "...";
    }

    void writeClipped(
        ScreenBuffer& buffer,
        int x,
        int y,
        int width,
        const std::string& text,
        const Style& style)
    {
        if (width <= 0)
        {
            return;
        }

        buffer.writeString(x, y, clipText(text, width), style);
    }

    void drawPanel(
        ScreenBuffer& buffer,
        int x,
        int y,
        int width,
        int height,
        const std::string& title)
    {
        if (width < 4 || height < 3)
        {
            return;
        }

        buffer.drawFrame(
            Rect{ Point{ x, y }, Size{ width, height } },
            ViewportFrameStyle,
            U'┌', U'┐', U'└', U'┘', U'─', U'│');

        writeClipped(
            buffer,
            x + 2,
            y,
            std::max(0, width - 4),
            " " + title + " ",
            Themes::SectionHeader);
    }

    std::string codePointToString(char32_t codePoint)
    {
        std::ostringstream stream;
        stream << "U+";
        stream << std::hex << std::uppercase;

        const unsigned int value = static_cast<unsigned int>(codePoint);
        if (value <= 0xFFFFu)
        {
            stream.width(4);
        }
        else
        {
            stream.width(6);
        }

        stream.fill('0');
        stream << value;
        return stream.str();
    }

    Style resolveBaseCellStyle(const TextObjectCell& cell)
    {
        if (cell.style.has_value())
        {
            return *cell.style;
        }

        return GoodCellStyle;
    }
}

TextObjectExportValidationScreen::TextObjectExportValidationScreen(
    const TextObject& object,
    const std::string& outputPath,
    const TextObjectExporter::SaveOptions& options)
    : m_object(object)
    , m_outputPath(outputPath)
    , m_options(options)
{
}

void TextObjectExportValidationScreen::onEnter()
{
    m_elapsedSeconds = 0.0;
    revalidate();
}

void TextObjectExportValidationScreen::update(double deltaTime)
{
    m_elapsedSeconds += deltaTime;
}

void TextObjectExportValidationScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    if (screenWidth < MinimumWidth || screenHeight < MinimumHeight)
    {
        buffer.writeString(2, 2, "TextObjectExportValidationScreen needs at least 100x32.", Themes::Warning);
        return;
    }

    buffer.fillRect(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        U' ',
        ValidationBackground);

    buffer.drawFrame(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        Themes::AccentSurface,
        U'╔', U'╗', U'╚', U'╝', U'═', U'║');

    buffer.writeString(3, 0, "[ TextObject Export Validation ]", Themes::PanelTitle);
    buffer.writeString(screenWidth - 37, 0, "[ in-place encoding diagnostics ]", Themes::Info);

    const int topPanelHeight = 9;
    const int warningsPanelHeight = 7;

    const int halfWidth = screenWidth / 2;

    drawSummaryPanel(buffer, 1, 2, halfWidth - 1, topPanelHeight);
    drawLegendPanel(buffer, halfWidth, 2, screenWidth - halfWidth - 1, topPanelHeight);
    drawWarningsPanel(buffer, 1, 2 + topPanelHeight, screenWidth - 2, warningsPanelHeight);
    drawViewportPanel(
        buffer,
        1,
        2 + topPanelHeight + warningsPanelHeight,
        screenWidth - 2,
        screenHeight - (3 + topPanelHeight + warningsPanelHeight));
}

void TextObjectExportValidationScreen::revalidate()
{
    m_failingCells.clear();
    m_warningCells.clear();
    m_encodingError.clear();

    m_resolvedFileType = (m_options.fileType == TextObjectExporter::FileType::Auto)
        ? TextObjectExporter::detectFileType(m_outputPath)
        : m_options.fileType;

    TextObjectExporter::SaveOptions previewOptions = m_options;
    previewOptions.fileType = m_resolvedFileType;

    m_resolvedEncoding = TextObjectExporter::resolveEffectiveEncoding(
        m_resolvedFileType,
        previewOptions,
        &m_encodingError);

    m_previewResult = TextObjectExporter::exportToBytes(m_object, previewOptions);
    m_previewResult.outputPath = m_outputPath;
    m_previewResult.resolvedFileType = m_resolvedFileType;

    for (const TextObjectExporter::SaveWarning& warning : m_previewResult.warnings)
    {
        if (warning.sourcePosition.isValid())
        {
            m_warningCells.push_back(warning.sourcePosition);
        }
    }

    if (m_resolvedEncoding == TextObjectExporter::Encoding::Auto)
    {
        return;
    }

    for (int y = 0; y < m_object.getHeight(); ++y)
    {
        for (int x = 0; x < m_object.getWidth(); ++x)
        {
            const TextObjectCell& cell = m_object.getCell(x, y);

            if (cell.kind != CellKind::Glyph)
            {
                continue;
            }

            if (!TextObjectExporter::canEncodeCodePoint(cell.glyph, m_resolvedEncoding))
            {
                FailingCell failingCell;
                failingCell.x = x;
                failingCell.y = y;
                failingCell.glyph = cell.glyph;
                m_failingCells.push_back(failingCell);
            }
        }
    }
}

void TextObjectExportValidationScreen::drawSummaryPanel(
    ScreenBuffer& buffer,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Export Summary");

    const std::string statusLine = m_previewResult.success
        ? TextObjectExporter::formatSaveSuccess(m_previewResult)
        : TextObjectExporter::formatSaveError(m_previewResult);

    writeClipped(buffer, x + 2, y + 1, width - 4, "Path: " + m_outputPath, Themes::Text);
    writeClipped(buffer, x + 2, y + 2, width - 4, "File type: " + std::string(TextObjectExporter::toString(m_resolvedFileType)), Themes::Text);
    writeClipped(buffer, x + 2, y + 3, width - 4, "Encoding: " + std::string(TextObjectExporter::toString(m_resolvedEncoding)), Themes::Text);
    writeClipped(
        buffer,
        x + 2,
        y + 4,
        width - 4,
        "Object size: " + std::to_string(m_object.getWidth()) + "x" + std::to_string(m_object.getHeight()),
        Themes::Text);

    writeClipped(
        buffer,
        x + 2,
        y + 5,
        width - 4,
        "Failing glyph cells: " + std::to_string(m_failingCells.size()),
        m_failingCells.empty() ? Themes::Success : Themes::Warning);

    if (m_previewResult.hasEncodingFailure && m_previewResult.firstFailingPosition.isValid())
    {
        std::ostringstream firstFail;
        firstFail
            << "First failure: "
            << codePointToString(m_previewResult.firstFailingCodePoint)
            << " at ("
            << m_previewResult.firstFailingPosition.x
            << ", "
            << m_previewResult.firstFailingPosition.y
            << ")";

        writeClipped(buffer, x + 2, y + 6, width - 4, firstFail.str(), Themes::Warning);
    }
    else
    {
        writeClipped(buffer, x + 2, y + 6, width - 4, statusLine, m_previewResult.success ? Themes::Success : Themes::Warning);
    }

    writeClipped(
        buffer,
        x + 2,
        y + 7,
        width - 4,
        "Preview mode only: no file is written by this screen.",
        Themes::MutedText);
}

void TextObjectExportValidationScreen::drawLegendPanel(
    ScreenBuffer& buffer,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Legend");

    buffer.writeString(x + 2, y + 1, "A", GoodCellStyle);
    writeClipped(buffer, x + 6, y + 1, width - 8, "Normal encodable glyph", Themes::Text);

    buffer.writeString(x + 2, y + 2, "A", FailingCellStyle);
    writeClipped(buffer, x + 6, y + 2, width - 8, "Glyph cannot be represented in the selected encoding", Themes::Text);

    buffer.writeString(x + 2, y + 3, "A", FirstFailingCellStyle);
    writeClipped(buffer, x + 6, y + 3, width - 8, "First failing glyph reported by exporter", Themes::Text);

    buffer.writeString(x + 2, y + 4, "A", WarningCellStyle);
    writeClipped(buffer, x + 6, y + 4, width - 8, "Warning hotspot from structured save diagnostics", Themes::Text);

    writeClipped(buffer, x + 2, y + 5, width - 4, "Policy notes", Themes::SectionHeader);
    writeClipped(buffer, x + 2, y + 6, width - 4, "The same encoding policy as TextObjectExporter is used here.", Themes::MutedText);
    writeClipped(buffer, x + 2, y + 7, width - 4, "Viewport shows the top-left crop of the TextObject.", Themes::MutedText);
}

void TextObjectExportValidationScreen::drawWarningsPanel(
    ScreenBuffer& buffer,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Warnings / Notes");

    int row = y + 1;

    if (!m_encodingError.empty() && row < y + height - 1)
    {
        writeClipped(buffer, x + 2, row++, width - 4, m_encodingError, Themes::Warning);
    }

    if (!m_previewResult.warnings.empty())
    {
        for (const TextObjectExporter::SaveWarning& warning : m_previewResult.warnings)
        {
            if (row >= y + height - 1)
            {
                break;
            }

            const std::string line =
                std::string("[") +
                TextObjectExporter::toString(warning.code) +
                "] " +
                warning.message;

            writeClipped(buffer, x + 2, row++, width - 4, line, Themes::Warning);
        }
    }

    if (row == y + 1)
    {
        writeClipped(buffer, x + 2, row++, width - 4, "No warnings recorded for this export preview.", Themes::Success);
    }

    if (TextObjectExporter::shouldRecommendXpForFidelity(m_previewResult) && row < y + height - 1)
    {
        writeClipped(
            buffer,
            x + 2,
            row++,
            width - 4,
            "Recommendation: prefer .xp when RGB color or authored style fidelity matters.",
            Themes::Info);
    }

    if (row < y + height - 1)
    {
        writeClipped(
            buffer,
            x + 2,
            row,
            width - 4,
            "Use the highlighted viewport below to locate failing or warning-producing cells quickly.",
            Themes::MutedText);
    }
}

void TextObjectExportValidationScreen::drawViewportPanel(
    ScreenBuffer& buffer,
    int x,
    int y,
    int width,
    int height) const
{
    drawPanel(buffer, x, y, width, height, "Object Viewport");

    if (width < 6 || height < 5)
    {
        return;
    }

    const int innerX = x + 2;
    const int innerY = y + 2;
    const int innerWidth = width - 4;
    const int innerHeight = height - 3;

    buffer.fillRect(
        Rect{ Point{ innerX, innerY }, Size{ innerWidth, innerHeight } },
        U' ',
        ViewportFillStyle);

    const int visibleWidth = std::min(innerWidth, m_object.getWidth());
    const int visibleHeight = std::min(innerHeight, m_object.getHeight());

    for (int row = 0; row < visibleHeight; ++row)
    {
        for (int col = 0; col < visibleWidth; ++col)
        {
            const TextObjectCell& cell = m_object.getCell(col, row);

            if (cell.kind == CellKind::Empty)
            {
                continue;
            }

            if (cell.kind == CellKind::WideTrailing)
            {
                continue;
            }

            if (cell.kind == CellKind::CombiningContinuation)
            {
                continue;
            }

            Style drawStyle = resolveBaseCellStyle(cell);

            if (isFirstFailingCell(col, row))
            {
                drawStyle = FirstFailingCellStyle;
            }
            else if (isFailingCell(col, row))
            {
                drawStyle = FailingCellStyle;
            }
            else if (isWarningCell(col, row))
            {
                drawStyle = WarningCellStyle;
            }

            buffer.writeCodePoint(innerX + col, innerY + row, cell.glyph, drawStyle);
        }
    }

    const std::string footer =
        "Visible crop: " +
        std::to_string(visibleWidth) +
        "x" +
        std::to_string(visibleHeight) +
        " of " +
        std::to_string(m_object.getWidth()) +
        "x" +
        std::to_string(m_object.getHeight());

    writeClipped(
        buffer,
        x + 2,
        y + 1,
        width - 4,
        footer,
        Themes::MutedText);
}

bool TextObjectExportValidationScreen::isFailingCell(int x, int y) const
{
    for (const FailingCell& failingCell : m_failingCells)
    {
        if (failingCell.x == x && failingCell.y == y)
        {
            return true;
        }
    }

    return false;
}

bool TextObjectExportValidationScreen::isFirstFailingCell(int x, int y) const
{
    if (!m_previewResult.hasEncodingFailure || !m_previewResult.firstFailingPosition.isValid())
    {
        return false;
    }

    return m_previewResult.firstFailingPosition.x == x
        && m_previewResult.firstFailingPosition.y == y;
}

bool TextObjectExportValidationScreen::isWarningCell(int x, int y) const
{
    for (const TextObjectExporter::SourcePosition& warningCell : m_warningCells)
    {
        if (warningCell.x == x && warningCell.y == y)
        {
            return true;
        }
    }

    return false;
}
