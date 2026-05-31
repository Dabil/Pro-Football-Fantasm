#pragma once

#include <string>
#include <vector>

#include "Assets/AssetLibrary.h"
#include "Rendering/Objects/TextObject.h"
#include "Screens/Screen.h"

class ScreenBuffer;
class Surface;

class PffGameScreen : public Screen
{
public:
    explicit PffGameScreen(Assets::AssetLibrary& assetLibrary);
    ~PffGameScreen() override = default;

    void onEnter() override;
    void onExit() override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface) override;

private:
    bool loadAssetObject(
        const std::string& assetKey,
        TextObject& outObject,
        bool& outLoaded,
        const std::string& assetLabel);

    bool buildScoreBoardDigits();
    void drawScore(ScreenBuffer& buffer, int x, int y, int score) const;
    void drawNumber(ScreenBuffer& buffer, int x, int y, const std::string& value) const;
    std::string formatScoreValue(int score) const;
    void drawLoadError(Surface& surface) const;

private:
    Assets::AssetLibrary& m_assetLibrary;

    std::string m_fieldAssetKey = "pff.field";
    std::string m_scoreBoardAssetKey = "pff.scoreboard";
    std::string m_scoreBoardFontAssetKey = "pff.scoreboard_font";

    std::string m_lastRequestedAssetKey;
    std::string m_lastResolvedAssetPath;

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