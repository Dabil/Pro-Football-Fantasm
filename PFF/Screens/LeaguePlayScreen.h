#pragma once

#include "Screens/Screen.h"
#include "Assets/AssetLibrary.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"

#include <functional>
#include <string_view>

class Surface;

class LeaguePlayScreen : public Screen
{
public:
	explicit LeaguePlayScreen(
		Assets::AssetLibrary& assetLibrary,
		std::function<bool(std::string_view)> commandDispatcher);

	~LeaguePlayScreen();

	void onEnter() override;
	void onExit() override;
	bool handleEvent(const Input::Event& event) override;
	void update(const Animation::TickEvent& event) override;
	void draw(Surface& surface) override;

private:
	void buildButtons();
	void positionButtons(
		const Rect& leftTopPane, 
		const Rect& leftBottomPane,
		const Rect& rightTopPane,
		const Rect& rightBottomPane,
		const Rect& footer);

	void handleButtonActivation(const ButtonActivationResult& result);

private:
	Assets::AssetLibrary& m_assetLibrary;

	// Title Text
	std::string m_TitleBannerFontKey = "banner.ansi_shadow";
	TextObject m_LeaguePlayTitleObject;
	Assets::LoadBannerFontResult m_fontResult;

	// buttons
	std::function<bool(std::string_view)> m_commandDispatcher;
	ContainerWidget m_root;
	Button* m_mainButton = nullptr;
	Button* m_teamsAndStandingsButton = nullptr;
	Button* m_leagueScheduleButton = nullptr;
	Button* m_leagueLeadersButton = nullptr;
	Button* m_tradeDeskButton = nullptr;
};
