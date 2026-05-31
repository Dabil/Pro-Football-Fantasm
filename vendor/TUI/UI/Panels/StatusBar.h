#pragma once

#include <string>
#include <vector>

#include "UI/Panels/Panel.h"
#include "Rendering/Styles/Style.h"

class Surface;

class StatusBar : public Panel
{
public:
    StatusBar();
    explicit StatusBar(const Rect& bounds);

    const std::string& instructions() const;
    void setInstructions(std::string instructions);
    void clearInstructions();

    const std::vector<std::string>& commandHints() const;
    void setCommandHints(std::vector<std::string> hints);
    void addCommandHint(std::string hint);
    void clearCommandHints();

    const std::string& separator() const;
    void setSeparator(std::string separator);

    const Style& instructionStyle() const;
    void setInstructionStyle(const Style& style);

    const Style& hintStyle() const;
    void setHintStyle(const Style& style);

    void draw(Surface& surface) override;

private:
    std::u32string buildDisplayText(int maxWidth) const;

private:
    std::string m_instructions;
    std::vector<std::string> m_commandHints;
    std::string m_separator = " | ";

    Style m_instructionStyle;
    Style m_hintStyle;
};