#pragma once

#include <string>

/*
	Purpose

	Represents a grapheme - like display unit for layout without 
	forcing that complexity into ScreenCell.
*/

struct TextCluster
{
	std::u32string codePoints;
	int displayWidth = 1;
};