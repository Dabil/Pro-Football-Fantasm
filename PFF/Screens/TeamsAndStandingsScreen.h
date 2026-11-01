#pragma once

#include "Screens/Screen.h"
#include "App/LoadedWorldContext.h"
#include "GCE/Database/WorldDatabase.h"
#include "Assets/AssetLibrary.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>
#include <string_view>

class Surface;

namespace Composition
{
	class PageComposer;
}

class TeamsAndStandingsScreen : public Screen
{
public:
	explicit TeamsAndStandingsScreen(
		Assets::AssetLibrary& assetLibrary,
		std::function<bool(std::string_view)> commandDispatcher,
		std::function<LoadedWorldContext()> loadedWorldContextProvider);

	~TeamsAndStandingsScreen();

	void onEnter() override;
	void onExit() override;
	bool handleEvent(const Input::Event& event) override;
	void update(const Animation::TickEvent& event) override;
	void draw(Surface& surface) override;

private:
	void buildButtons();
	void positionButtons(const Rect& footer);
	void handleButtonActivation(const ButtonActivationResult& result);
	void loadTeams();
	void refreshLeagueContext();
	void drawDivisionTeams(
		Composition::PageComposer& page,
		std::string_view regionName,
		std::string_view regionContent,
		std::string_view conference,
		std::string_view division) const;

private:
	Assets::AssetLibrary& m_assetLibrary;

	// Title Text
	std::string m_TitleBannerFontKey = "banner.ansi_shadow";
	TextObject m_TeamsAndStandingsTitleObject;
	Assets::LoadBannerFontResult m_fontResult;

	// buttons
	std::function<bool(std::string_view)> m_commandDispatcher;
	ContainerWidget m_root;
	Button* m_leaguePlayButton = nullptr;

	std::function<LoadedWorldContext()> m_loadedWorldContextProvider;
	LoadedWorldContext m_dataContext;
	std::vector<WorldTeamRecord> m_teams;
	std::string m_teamLoadMessage;
	bool m_teamsLoaded = false;
};

