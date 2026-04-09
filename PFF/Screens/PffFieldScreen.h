#pragma once

#include <string>
#include <vector>

#include "Screens/Screen.h"
#include "Rendering/Objects/TextObject.h"

class ScreenBuffer;
class Surface;

class PffFieldScreen : public Screen
{
public:
    PffFieldScreen();
    ~PffFieldScreen() override = default;

    void onEnter() override;
    void onExit() override;
    void update(double deltaTime) override;
    void draw(Surface& surface) override;

private:
    bool loadXpObject(
        const std::string& path,
        TextObject& outObject,
        bool& outLoaded,
        const std::string& assetLabel);

    bool buildScoreBoardDigits();
    void drawScore(ScreenBuffer& buffer, int x, int y, int score) const;
    void drawNumber(ScreenBuffer& buffer, int x, int y, const std::string& value) const;
    std::string formatScoreValue(int score) const;
    void drawLoadError(Surface& surface) const;

private:
    std::string m_xpFieldPath = "Assets/xp/Field.xp";
    std::string m_xpScoreBoardPath = "Assets/xp/ScoreBoard.xp";
    std::string m_xpScoreBoardFontPath = "Assets/xp/ScoreBoardFont.xp";

    TextObject m_fieldObject;
    TextObject m_scoreBoardObject;
    TextObject m_scoreBoardFontObject;
    std::vector<TextObject> m_scoreBoardDigits;

    bool m_fieldLoaded = false;
    bool m_scoreBoardLoaded = false;
    bool m_scoreBoardFontLoaded = false;

    std::string m_loadErrorMessage;
    double m_elapsedSeconds = 0.0;
    std::string m_statusMessage;

    int m_awayScore = 0;
    int m_homeScore = 0;
};