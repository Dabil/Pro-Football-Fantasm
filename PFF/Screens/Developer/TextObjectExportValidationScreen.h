#pragma once

#include <string>
#include <vector>

#include "Rendering/Objects/TextObject.h"
#include "Rendering/Objects/TextObjectExporter.h"
#include "Screens/Screen.h"

class Surface;
class ScreenBuffer;

class TextObjectExportValidationScreen : public Screen
{
public:
    TextObjectExportValidationScreen(
        const TextObject& object,
        const std::string& outputPath,
        const TextObjectExporter::SaveOptions& options = {});

    ~TextObjectExportValidationScreen() override = default;

    void onEnter() override;
    void update(double deltaTime) override;
    void draw(Surface& surface) override;

private:
    struct FailingCell
    {
        int x = 0;
        int y = 0;
        char32_t glyph = U' ';
    };

private:
    void revalidate();

    void drawSummaryPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;
    void drawLegendPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;
    void drawWarningsPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;
    void drawViewportPanel(ScreenBuffer& buffer, int x, int y, int width, int height) const;

    bool isFailingCell(int x, int y) const;
    bool isFirstFailingCell(int x, int y) const;
    bool isWarningCell(int x, int y) const;

private:
    TextObject m_object;
    std::string m_outputPath;
    TextObjectExporter::SaveOptions m_options;

    TextObjectExporter::FileType m_resolvedFileType = TextObjectExporter::FileType::Unknown;
    TextObjectExporter::Encoding m_resolvedEncoding = TextObjectExporter::Encoding::Auto;
    std::string m_encodingError;

    TextObjectExporter::SaveResult m_previewResult;
    std::vector<FailingCell> m_failingCells;
    std::vector<TextObjectExporter::SourcePosition> m_warningCells;

    double m_elapsedSeconds = 0.0;
};