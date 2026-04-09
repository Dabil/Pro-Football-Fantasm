#pragma once

#include <string>
#include <vector>
#include <sstream>

inline std::vector<std::string> wrapText(const std::string& text, int maxWidth)
{
    std::vector<std::string> lines;

    if (maxWidth <= 0)
    {
        return lines;
    }

    std::istringstream input(text);
    std::string rawLine;

    // Process existing newline-separated lines first
    while (std::getline(input, rawLine))
    {
        std::string line;
        std::istringstream words(rawLine);
        std::string word;

        while (words >> word)
        {
            // If word itself is longer than maxWidth → hard split
            if (static_cast<int>(word.size()) > maxWidth)
            {
                if (!line.empty())
                {
                    lines.push_back(line);
                    line.clear();
                }

                for (size_t i = 0; i < word.size(); i += maxWidth)
                {
                    lines.push_back(word.substr(i, maxWidth));
                }

                continue;
            }

            if (line.empty())
            {
                line = word;
            }
            else if (static_cast<int>(line.size() + 1 + word.size()) <= maxWidth)
            {
                line += ' ';
                line += word;
            }
            else
            {
                lines.push_back(line);
                line = word;
            }
        }

        if (!line.empty())
        {
            lines.push_back(line);
        }

        // Preserve blank lines
        if (rawLine.empty())
        {
            lines.emplace_back();
        }
    }

    return lines;
}