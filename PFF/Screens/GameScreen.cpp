#include "Screens/GameScreen.h"
#include "Screens/ScoreboardView.h"

#include <algorithm>
#include <sstream>
#include <utility>

#include "Core/Rect.h"
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

}

GameScreen::GameScreen(Assets::AssetLibrary& assetLibrary)
    : m_assetLibrary(assetLibrary)
{
}

bool GameScreen::loadAssetObject(
    const std::string& assetKey,
    TextObject& outObject,
    bool& outLoaded,
    const std::string& assetLabel)
{
    m_lastRequestedAssetKey = assetKey;
    m_lastResolvedAssetPath.clear();

    const Assets::LoadTextAssetResult loadResult = m_assetLibrary.loadTextAsset(assetKey);

    if (!loadResult.success || !loadResult.asset.object)
    {
        std::ostringstream debug;
        debug << assetLabel << " failed to load through AssetLibrary";

        if (!loadResult.errorMessage.empty())
        {
            debug << ": " << loadResult.errorMessage;
        }

        m_loadErrorMessage = debug.str();
        m_statusMessage = debug.str();
        outLoaded = false;
        return false;
    }

    outObject = *loadResult.asset.object;
    outLoaded = outObject.isLoaded() && !outObject.isEmpty();
    m_lastResolvedAssetPath = loadResult.asset.resolvedPath;

    if (!outLoaded)
    {
        m_loadErrorMessage = assetLabel + " loaded, but no drawable content was produced.";
        m_statusMessage = m_loadErrorMessage;
        return false;
    }

    std::ostringstream success;
    success << assetLabel << " loaded via AssetLibrary";
    if (loadResult.fromCache)
    {
        success << " (from cache)";
    }

    if (!loadResult.asset.resolvedPath.empty())
    {
        success << ": " << loadResult.asset.resolvedPath;
    }

    m_statusMessage = success.str();
    return true;
}

bool GameScreen::buildScoreBoardDigits()
{
    m_scoreBoardDigits.clear();

    if (!m_scoreBoardFontLoaded)
    {
        m_loadErrorMessage = "ScoreBoardFont.xp must be loaded before digit reconstruction.";
        m_statusMessage = m_loadErrorMessage;
        return false;
    }

    if (m_scoreBoardFontObject.getWidth() != ExpectedScoreBoardFontWidth ||
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

    m_scoreBoardDigits = ScoreboardView::buildDigits(m_scoreBoardFontObject);
    if (m_scoreBoardDigits.empty())
    {
        m_loadErrorMessage = "ScoreBoardFont.xp digit extraction failed.";
        m_statusMessage = m_loadErrorMessage;
        return false;
    }
    return true;
}

void GameScreen::onEnter()
{
    m_elapsedSeconds = 0.0;

    m_fieldObject.clear();
    m_scoreBoardObject.clear();
    m_scoreBoardFontObject.clear();
    m_scoreBoardDigits.clear();

    m_loadErrorMessage.clear();
    m_statusMessage.clear();
    m_lastRequestedAssetKey.clear();
    m_lastResolvedAssetPath.clear();

    m_fieldLoaded = false;
    m_scoreBoardLoaded = false;
    m_scoreBoardFontLoaded = false;

    if (!loadAssetObject(m_fieldAssetKey, m_fieldObject, m_fieldLoaded, "Field.xp"))
    {
        return;
    }

    if (!loadAssetObject(m_scoreBoardAssetKey, m_scoreBoardObject, m_scoreBoardLoaded, "ScoreBoard.xp"))
    {
        return;
    }

    if (!loadAssetObject(
        m_scoreBoardFontAssetKey,
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

    m_statusMessage = "PFF field, scoreboard, and scoreboard font loaded successfully via AssetLibrary.";
}

void GameScreen::onExit()
{
    m_fieldObject.clear();
    m_scoreBoardObject.clear();
    m_scoreBoardFontObject.clear();
    m_scoreBoardDigits.clear();

    m_loadErrorMessage.clear();
    m_statusMessage.clear();
    m_lastRequestedAssetKey.clear();
    m_lastResolvedAssetPath.clear();

    m_fieldLoaded = false;
    m_scoreBoardLoaded = false;
    m_scoreBoardFontLoaded = false;

    m_elapsedSeconds = 0.0;
}

void GameScreen::update(const Animation::TickEvent& event)
{
    m_elapsedSeconds += event.deltaSeconds;
}

void GameScreen::draw(Surface& surface)
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
    ScoreboardView::drawArtwork(buffer, scoreBoardX, scoreBoardY, m_scoreBoardObject,
                                m_scoreBoardDigits, m_awayScore, m_homeScore);

    const std::string resolutionText =
        std::to_string(buffer.getWidth()) + "x" + std::to_string(buffer.getHeight());

    buffer.writeString(1, 1, resolutionText, Themes::Text);
}

void GameScreen::drawLoadError(Surface& surface) const
{
    ScreenBuffer& buffer = surface.buffer();

    const int screenWidth = buffer.getWidth();
    const int screenHeight = buffer.getHeight();

    const int panelWidth = std::min(screenWidth - 4, 100);
    const int panelHeight = std::min(screenHeight - 4, 10);
    const int panelX = std::max(0, (screenWidth - panelWidth) / 2);
    const int panelY = std::max(0, (screenHeight - panelHeight) / 2);

    buffer.drawFrame(
        Rect{ Point{ panelX, panelY }, Size{ panelWidth, panelHeight } },
        Themes::Frame,
        U'┌', U'┐', U'└', U'┘', U'─', U'│');

    buffer.writeString(panelX + 2, panelY, " PFF Field Asset Load Error ", Themes::PanelTitle);

    const int textWidth = panelWidth - 4;
    int currentY = panelY + 2;

    const std::string assetLine = "Asset key: " + m_lastRequestedAssetKey;
    const std::vector<std::string> assetLines = wrapText(assetLine, textWidth);
    for (const std::string& line : assetLines)
    {
        buffer.writeString(panelX + 2, currentY++, line, Themes::Text);
    }

    if (!m_lastResolvedAssetPath.empty())
    {
        const std::string resolvedLine = "Resolved path: " + m_lastResolvedAssetPath;
        const std::vector<std::string> resolvedLines = wrapText(resolvedLine, textWidth);
        for (const std::string& line : resolvedLines)
        {
            buffer.writeString(panelX + 2, currentY++, line, Themes::Text);
        }
    }

    ++currentY;

    const std::string message = m_loadErrorMessage.empty()
        ? std::string("Unknown asset load error.")
        : m_loadErrorMessage;

    const std::vector<std::string> messageLines = wrapText(message, textWidth);

    for (const std::string& line : messageLines)
    {
        buffer.writeString(panelX + 2, currentY++, line, Themes::Warning);
    }
}