#pragma once

#include "Screens/Screen.h"
#include "Assets/AssetLibrary.h"

class Surface;

class IntroScreen : public Screen
{
public:
	explicit IntroScreen(Assets::AssetLibrary& assetLibrary);
	~IntroScreen();

	void onEnter() override;
	void onExit() override;
	bool handleEvent(const Input::Event& event) override;
	void update(const Animation::TickEvent& event) override;
	void draw(Surface& surface) override;

private:
	Assets::AssetLibrary& m_assetLibrary;
};
