#pragma once

#include "Screens/Screen.h"
#include "App/LoadedWorldContext.h"
#include "Database/PFFStandings.h"
#include "Assets/AssetLibrary.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"

#include <filesystem>
#include <functional>
#include <memory>
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
		std::function<LoadedWorldContext()> loadedWorldContextProvider,
        std::function<void(const LoadedWorldContext&,std::int64_t)> openTeam = {});

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
		std::string_view division);
	void drawHeadingTooltip(Surface& surface) const;

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
	std::vector<PffStandingRow> m_teams;
	std::string m_teamLoadMessage;
	std::string m_gameTimeLabel;
	bool m_teamsLoaded = false;
	struct HeadingTooltip
	{
		Rect bounds;
		std::string label;
	};
	std::vector<HeadingTooltip> m_headingTooltips;
	Point m_mousePosition{ -1, -1 };
    // Static standings/header/footer drawing; controls and tooltips stay live.
    std::function<void(const LoadedWorldContext&,std::int64_t)> m_openTeam;
    std::vector<Widget*> m_teamLinks;
    std::unique_ptr<Surface> m_standingsCache;
};

