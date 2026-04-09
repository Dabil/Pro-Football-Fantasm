#include "Screens/PffFieldScreen.h"

#include <algorithm>
#include <filesystem>
#include <sstream>
#include <utility>

#include "Core/Rect.h"
#include "Rendering/Objects/TextObjectBuilder.h"
#include "Rendering/Objects/XpArtLoader.h"
#include "Rendering/ScreenBuffer.h"
#include "Rendering/Styles/Color.h"
#include "Rendering/Styles/StyleBuilder.h"
#include "Rendering/Styles/Themes.h"
#include "Rendering/Surface.h"
#include "Utilities/Text/TextWrap.h"

namespace
{
    constexpr int MinimumWidth = 20;
    constexpr int MinimumHeight = 8;

    constexpr int ScoreBoardFontDigitCount = 10;
    constexpr int ScoreBoardFontDigitWidth = 8;
    constexpr int ScoreBoardFontDigitHeight = 7;
    constexpr int ScoreBoardFontSeparatorWidth = 1;

    constexpr int ExpectedScoreBoardFontWidth =
        (ScoreBoardFontDigitCount * ScoreBoardFontDigitWidth) +
        ((ScoreBoardFontDigitCount - 1) * ScoreBoardFontSeparatorWidth);

    constexpr int ExpectedScoreBoardFontHeight = ScoreBoardFontDigitHeight;

    constexpr int ScoreDigitSpacing = 1;

    constexpr int AwayScoreOffsetX = 2;
    constexpr int HomeScoreOffsetX = 27;
    constexpr int ScoreOffsetY = 2;
}

PffFieldScreen::PffFieldScreen()
{
}

bool PffFieldScreen::loadXpObject(
    const std::string& path,
    TextObject& outObject,
    bool& outLoaded,
    const std::string& assetLabel)
{
    XpArtLoader::LoadOptions options;
    options.fileType = XpArtLoader::FileType::Xp;
    options.flattenLayers = true;
    options.treatMagentaBackgroundAsTransparent = true;
    options.visibleTransparentBaseCellsUseBlackBackground = true;
    options.strictLayerSizeValidation = true;

    const XpArtLoader::LoadResult loadResult = XpArtLoader::loadFromFile(path, options);

    if (!loadResult.success)
    {
        std::ostringstream debug;
        debug << XpArtLoader::formatLoadError(loadResult)
              << " | cwd=" << std::filesystem::current_path().string()
              << " | path=" << path;

        m_loadErrorMessage = debug.str();
        m_statusMessage = debug.str();
        outLoaded = false;
        return false;
    }

    outObject = loadResult.object;
    outLoaded = outObject.isLoaded() && !outObject.isEmpty();

    if (!outLoaded)
    {
        m_loadErrorMessage = assetLabel + " loaded, but no drawable content was produced.";
        m_statusMessage = m_loadErrorMessage;
        return false;
    }

    m_statusMessage = XpArtLoader::formatLoadSuccess(loadResult);
    return true;
}

bool PffFieldScreen::buildScoreBoardDigits()
{
    m_scoreBoardDigits.clear();

    if (!m_scoreBoardFontLoaded)
    {
        m_loadErrorMessage = "ScoreBoardFont.xp must be loaded before digit reconstruction.";
        m_statusMessage = m_loadErrorMessage;
        return false;
    }

    if (m_scoreBoardFontObject.getWidth()  != ExpectedScoreBoardFontWidth ||
        m_scoreBoardFontObject.getHeight() != ExpectedScoreBoardFontHeight)
    {
        std::ostringstream error;
        error << "ScoreBoardFont.xp has unexpected dimensions. Expected "
              << ExpectedScoreBoardFontWidth << "x" << ExpectedScoreBoardFontHeight
              << ", but got "
              << m_scoreBoardFontObject.getWidth() << "x" << m_scoreBoardFontObject.getHeight()
              << ".";

        m_loadErrorMessage = error.str();
        m_statusMessage = error.str();
        return false;
    }

    m_scoreBoardDigits.reserve(ScoreBoardFontDigitCount);

    for (int digitIndex = 0; digitIndex < ScoreBoardFontDigitCount; ++digitIndex)
    {
        TextObjectBuilder builder(ScoreBoardFontDigitWidth, ScoreBoardFontDigitHeight);

        const int sourceStartX =
            digitIndex * (ScoreBoardFontDigitWidth + ScoreBoardFontSeparatorWidth);

        for (int y = 0; y < ScoreBoardFontDigitHeight; ++y)
        {
            for (int x = 0; x < ScoreBoardFontDigitWidth; ++x)
            {
                const TextObjectCell* sourceCell =
                    m_scoreBoardFontObject.tryGetCell(sourceStartX + x, y);

                if (sourceCell == nullptr)
                {
                    std::ostringstream error;
                    error << "ScoreBoardFont.xp digit extraction failed at digit "
                        << digitIndex << ", source cell ("
                        << (sourceStartX + x) << ", " << y << ").";

                    m_loadErrorMessage = error.str();
                    m_statusMessage = error.str();
                    m_scoreBoardDigits.clear();
                    return false;
                }

                builder.setCell(
                    x,
                    y,
                    sourceCell->glyph,
                    sourceCell->kind,
                    sourceCell->width,
                    sourceCell->style);
            }
        }

        m_scoreBoardDigits.push_back(builder.build());
    }

    return static_cast<int>(m_scoreBoardDigits.size()) == ScoreBoardFontDigitCount;
}

void PffFieldScreen::onEnter()
{
    m_elapsedSeconds = 0.0;

    m_fieldObject.clear();
    m_scoreBoardObject.clear();
    m_scoreBoardFontObject.clear();
    m_scoreBoardDigits.clear();

    m_loadErrorMessage.clear();
    m_statusMessage.clear();

    m_fieldLoaded = false;
    m_scoreBoardLoaded = false;
    m_scoreBoardFontLoaded = false;

    if (!loadXpObject(m_xpFieldPath, m_fieldObject, m_fieldLoaded, "Field.xp"))
    {
        return;
    }

    if (!loadXpObject(m_xpScoreBoardPath, m_scoreBoardObject, m_scoreBoardLoaded, "ScoreBoard.xp"))
    {
        return;
    }

    if (!loadXpObject(
        m_xpScoreBoardFontPath,
        m_scoreBoardFontObject,
        m_scoreBoardFontLoaded,
        "ScoreBoardFont.xp"))
    {
        return;
    }

    if (!buildScoreBoardDigits())
    {
        m_scoreBoardFontLoaded = false;
        return;
    }

    m_statusMessage = "PFF field, scoreboard, and scoreboard font loaded successfully.";
}

void PffFieldScreen::onExit()
{
    m_fieldObject.clear();
    m_scoreBoardObject.clear();
    m_scoreBoardFontObject.clear();
    m_scoreBoardDigits.clear();

    m_loadErrorMessage.clear();
    m_statusMessage.clear();

    m_fieldLoaded = false;
    m_scoreBoardLoaded = false;
    m_scoreBoardFontLoaded = false;

    m_elapsedSeconds = 0.0;
}

void PffFieldScreen::update(double deltaTime)
{
    m_elapsedSeconds += deltaTime;
}

std::string PffFieldScreen::formatScoreValue(int score) const
{
    score = std::clamp(score, 0, 99);

    if (score < 10)
    {
        return "0" + std::to_string(score);
    }

    return std::to_string(score);
}

void PffFieldScreen::drawNumber(ScreenBuffer& buffer, int x, int y, const std::string& value) const
{
    if (m_scoreBoardDigits.size() != ScoreBoardFontDigitCount)
    {
        return;
    }

    int cursorX = x;

    for (char ch : value)
    {
        if (ch < '0' || ch > '9')
        {
            continue;
        }

        const int digitIndex = ch - '0';
        const TextObject& digitObject = m_scoreBoardDigits[static_cast<std::size_t>(digitIndex)];

        digitObject.draw(buffer, cursorX, y);
        cursorX += digitObject.getWidth() + ScoreDigitSpacing;
    }
}

void PffFieldScreen::drawScore(ScreenBuffer& buffer, int x, int y, int score) const
{
    drawNumber(buffer, x, y, formatScoreValue(score));
}

void PffFieldScreen::draw(Surface& surface)
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    buffer.fillRect(
        Rect{ Point{ 0, 0 }, Size{ screenWidth, screenHeight } },
        U' ',
        style::Bg(Color::FromBasic(Color::Basic::Black)));

    if (screenWidth < MinimumWidth || screenHeight < MinimumHeight)
    {
        buffer.writeString(1, 1, "Pro Football Fantasm needs a larger window.", Themes::Warning);
        return;
    }

    if (!m_fieldLoaded || !m_scoreBoardLoaded || !m_scoreBoardFontLoaded)
    {
        drawLoadError(surface);
        return;
    }

    const int fieldX = std::max(0, (screenWidth - m_fieldObject.getWidth()) / 2);
    const int fieldY = 0;
    m_fieldObject.draw(buffer, fieldX, fieldY);

    const int scoreBoardX = std::max(0, (screenWidth - m_scoreBoardObject.getWidth()) / 2);
    const int scoreBoardY = m_fieldObject.getHeight();
    m_scoreBoardObject.draw(buffer, scoreBoardX, scoreBoardY);

    drawScore(
        buffer,
        scoreBoardX + AwayScoreOffsetX,
        scoreBoardY + ScoreOffsetY,
        m_awayScore);

    drawScore(
        buffer,
        scoreBoardX + HomeScoreOffsetX,
        scoreBoardY + ScoreOffsetY,
        m_homeScore);

    const std::string resolutionText =
        std::to_string(buffer.getWidth()) + "x" + std::to_string(buffer.getHeight());

    buffer.writeString(1, 1, resolutionText, Themes::Text);
}

void PffFieldScreen::drawLoadError(Surface& surface) const
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    const int panelWidth = std::min(screenWidth - 4, 100);
    const int panelHeight = std::min(screenHeight - 4, 8);
    const int panelX = std::max(0, (screenWidth - panelWidth) / 2);
    const int panelY = std::max(0, (screenHeight - panelHeight) / 2);

    buffer.drawFrame(
        Rect{ Point{ panelX, panelY }, Size{ panelWidth, panelHeight } },
        Themes::Frame,
        U'┌', U'┐', U'└', U'┘', U'─', U'│');

    buffer.writeString(panelX + 2, panelY, " PFF Field XP Load Error ", Themes::PanelTitle);

    const int textWidth = panelWidth - 4;
    int currentY = panelY + 2;

    const std::vector<std::string> pathLines =
        wrapText("Path: " + m_xpFieldPath, textWidth);

    for (const std::string& line : pathLines)
    {
        buffer.writeString(panelX + 2, currentY++, line, Themes::Text);
    }

    ++currentY;

    const std::string message = m_loadErrorMessage.empty()
        ? std::string("Unknown XP load error.")
        : m_loadErrorMessage;

    const std::vector<std::string> messageLines =
        wrapText(message, textWidth);

    for (const std::string& line : messageLines)
    {
        buffer.writeString(panelX + 2, currentY++, line, Themes::Warning);
    }
}