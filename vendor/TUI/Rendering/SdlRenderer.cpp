#include "Rendering/SdlRenderer.h"

#include "App/StartupConfig.h"
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include "Rendering/Diagnostics/RenderDiagnosticsWriter.h"
#include "Rendering/Objects/Cp437Encoding.h"
#include "Rendering/Styles/ColorMapping.h"
#include "Rendering/Styles/ColorResolver.h"
#include "Rendering/Styles/StylePolicyFactory.h"
#include "Utilities/Unicode/UnicodeConversion.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <map>
#include <utility>

#define NOMINMAX
#include <windows.h>

namespace
{
    constexpr int NominalCellWidth = 8;
    constexpr int AtlasTileWidth = 8;
    constexpr int AtlasTileHeight = 16;
    constexpr int MinimumCellWidth = 6;
    constexpr int MinimumColumns = 120;
    constexpr int MinimumRows = 36;
    constexpr int MaximumColumns = 480;
    constexpr int MaximumRows = 135;

    Input::MouseButton mouseButton(Uint8 button)
    {
        switch (button)
        {
        case SDL_BUTTON_LEFT: return Input::MouseButton::Left;
        case SDL_BUTTON_RIGHT: return Input::MouseButton::Right;
        case SDL_BUTTON_MIDDLE: return Input::MouseButton::Middle;
        default: return Input::MouseButton::None;
        }
    }

    struct Grid
    {
        int windowWidth = 0;
        int windowHeight = 0;
        int pixelWidth = 0;
        int pixelHeight = 0;
        float scale = 1;
        int cellWidth = NominalCellWidth;
        int columns = MinimumColumns;
        int rows = MinimumRows;
        int left = 0;
        int top = 0;
        bool visible = false;
        bool fits = false;

        int cellHeight() const { return cellWidth * 2; }
        int availableCellWidth() const
        {
            return (std::min)(pixelWidth / MinimumColumns, pixelHeight / (MinimumRows * 2));
        }
        int defaultCellWidth() const
        {
            return static_cast<int>(std::lround(NominalCellWidth * scale));
        }

        Point cellAt(float x, float y, bool allowOutside = false) const
        {
            if (!fits)
            {
                return { -1, -1 };
            }
            const double px = static_cast<double>(x) * pixelWidth / windowWidth - left;
            const double py = static_cast<double>(y) * pixelHeight / windowHeight - top;
            if (!allowOutside &&
                !(px >= 0 && px < columns * cellWidth && py >= 0 && py < rows * cellHeight()))
            {
                return { -1, -1 };
            }
            return { static_cast<int>(std::floor(px / cellWidth)),
                static_cast<int>(std::floor(py / cellHeight())) };
        }
    };

    Grid layout(Grid grid, int cellWidthAdjustment = 0)
    {
        const int available = grid.availableCellWidth();
        grid.fits = grid.visible && available >= MinimumCellWidth;
        if (grid.fits)
        {
            grid.cellWidth = std::clamp(grid.defaultCellWidth() + cellWidthAdjustment,
                MinimumCellWidth, available);
            grid.columns = (std::min)(MaximumColumns, grid.pixelWidth / grid.cellWidth);
            grid.rows = (std::min)(MaximumRows, grid.pixelHeight / grid.cellHeight());
            grid.left = (grid.pixelWidth - grid.columns * grid.cellWidth) / 2;
            grid.top = (grid.pixelHeight - grid.rows * grid.cellHeight()) / 2;
        }
        return grid;
    }

    std::filesystem::path executableFolder()
    {
        wchar_t executable[32768];
        const DWORD length = GetModuleFileNameW(nullptr, executable, ARRAYSIZE(executable));
        if (length == 0 || length >= ARRAYSIZE(executable))
        {
            return {};
        }
        return std::filesystem::path(executable).parent_path();
    }

    ColorMapping::RgbValue color(
        const std::optional<Style::StyleColorValue>& authored, Color::Basic fallback)
    {
        const auto resolved = ColorResolver::resolve(authored, ColorSupport::Rgb24);
        return ColorMapping::toRgb(
            resolved && !resolved->isDefault() ? *resolved : Color::FromBasic(fallback));
    }

    Input::RawKeyEvent translateKey(const SDL_KeyboardEvent& event)
    {
        Input::RawKeyEvent key;
        key.pressed = event.down;
        key.modifiers.shift = (event.mod & SDL_KMOD_SHIFT) != 0;
        key.modifiers.ctrl = (event.mod & SDL_KMOD_CTRL) != 0;
        key.modifiers.alt = (event.mod & SDL_KMOD_ALT) != 0;
        switch (event.key)
        {
        case SDLK_ESCAPE: key.virtualKey = VK_ESCAPE; break;
        case SDLK_RETURN: case SDLK_KP_ENTER: key.virtualKey = VK_RETURN; break;
        case SDLK_BACKSPACE: key.virtualKey = VK_BACK; break;
        case SDLK_TAB: key.virtualKey = VK_TAB; break;
        case SDLK_SPACE: key.virtualKey = VK_SPACE; key.character = U' '; break;
        case SDLK_LEFT: key.virtualKey = VK_LEFT; break;
        case SDLK_RIGHT: key.virtualKey = VK_RIGHT; break;
        case SDLK_UP: key.virtualKey = VK_UP; break;
        case SDLK_DOWN: key.virtualKey = VK_DOWN; break;
        case SDLK_HOME: key.virtualKey = VK_HOME; break;
        case SDLK_END: key.virtualKey = VK_END; break;
        case SDLK_PAGEUP: key.virtualKey = VK_PRIOR; break;
        case SDLK_PAGEDOWN: key.virtualKey = VK_NEXT; break;
        case SDLK_INSERT: key.virtualKey = VK_INSERT; break;
        case SDLK_DELETE: key.virtualKey = VK_DELETE; break;
        default:
            if (event.key >= SDLK_F1 && event.key <= SDLK_F12)
                key.virtualKey = static_cast<std::uint16_t>(VK_F1 + event.key - SDLK_F1);
            else if ((key.modifiers.ctrl || key.modifiers.alt) && event.key >= 'a' && event.key <= 'z')
            {
                key.virtualKey = static_cast<std::uint16_t>('A' + event.key - 'a');
                key.character = static_cast<char32_t>(event.key);
            }
            break;
        }
        return key;
    }
}

struct SdlRenderer::State
{
    HMODULE library = nullptr;
    HMODULE fontLibrary = nullptr;
    SDL_Window* window = nullptr;
    SDL_Renderer* renderer = nullptr;
    bool videoInitialized = false;
    bool closeRequested = false;
    Grid grid;
    int cellWidthAdjustment = 0;
    int initialCellWidthAdjustment = 0;
    Size publishedSize{ MinimumColumns, MinimumRows };
    bool geometryChanged = false;
    unsigned pressedButtons = 0;
    unsigned cancelledButtons = 0;
    bool textActive = false;
    bool composing = false;
    std::u32string committedText;
    std::size_t textOffset = 0;
    double wheelRemainder = 0;
    int wheelSteps = 0;
    Point wheelPosition;
    SDL_Texture* atlas = nullptr;
    bool batchAtlas = false;
    std::vector<SDL_Vertex> atlasVertices;
    std::vector<int> atlasIndices;
    std::vector<TTF_Font*> fonts;
    bool ttfInitialized = false;
    int fontHeight = 0;
    std::size_t fallbackCount = 0;
    double lastDrawMs = 0;
    double lastPresentMs = 0;
    std::chrono::steady_clock::time_point blinkEpoch = std::chrono::steady_clock::now();

    struct Glyph
    {
        SDL_Texture* texture = nullptr;
        int width = 0;
        int height = 0;
    };
    std::map<std::pair<std::u32string, bool>, Glyph> glyphs;

    decltype(&SDL_GetVersion) getVersion = nullptr;
    decltype(&SDL_GetError) getError = nullptr;
    decltype(&SDL_InitSubSystem) init = nullptr;
    decltype(&SDL_QuitSubSystem) quit = nullptr;
    decltype(&SDL_CreateWindowAndRenderer) create = nullptr;
    decltype(&SDL_DestroyWindow) destroyWindow = nullptr;
    decltype(&SDL_DestroyRenderer) destroyRenderer = nullptr;
    decltype(&SDL_PollEvent) pollEvent = nullptr;
    decltype(&SDL_GetWindowSize) windowSize = nullptr;
    decltype(&SDL_SetWindowSize) setWindowSize = nullptr;
    decltype(&SDL_GetRenderOutputSize) outputSize = nullptr;
    decltype(&SDL_GetWindowDisplayScale) displayScale = nullptr;
    decltype(&SDL_GetWindowFlags) windowFlags = nullptr;
    decltype(&SDL_SetWindowFullscreen) setFullscreen = nullptr;
    decltype(&SDL_GetModState) modState = nullptr;
    decltype(&SDL_StartTextInput) startText = nullptr;
    decltype(&SDL_StopTextInput) stopText = nullptr;
    decltype(&SDL_SetTextInputArea) textArea = nullptr;
    decltype(&SDL_ClearComposition) clearComposition = nullptr;
    decltype(&SDL_SetRenderDrawColor) setColor = nullptr;
    decltype(&SDL_RenderClear) clear = nullptr;
    decltype(&SDL_RenderFillRect) fillRect = nullptr;
    decltype(&SDL_RenderDebugText) text = nullptr;
    decltype(&SDL_RenderPresent) present = nullptr;
    decltype(&SDL_LoadPNG) loadPng = nullptr;
    decltype(&SDL_DestroySurface) destroySurface = nullptr;
    decltype(&SDL_CreateTextureFromSurface) createTexture = nullptr;
    decltype(&SDL_DestroyTexture) destroyTexture = nullptr;
    decltype(&SDL_SetTextureScaleMode) textureScale = nullptr;
    decltype(&SDL_SetTextureColorMod) textureColor = nullptr;
    decltype(&SDL_RenderTexture) drawTexture = nullptr;
    decltype(&SDL_RenderGeometry) drawGeometry = nullptr;
    decltype(&SDL_GetRendererName) rendererName = nullptr;
    decltype(&TTF_Init) fontInit = nullptr;
    decltype(&TTF_Version) fontVersion = nullptr;
    decltype(&TTF_Quit) fontQuit = nullptr;
    decltype(&TTF_OpenFont) openFont = nullptr;
    decltype(&TTF_CloseFont) closeFont = nullptr;
    decltype(&TTF_SetFontSize) setFontSize = nullptr;
    decltype(&TTF_SetFontStyle) setFontStyle = nullptr;
    decltype(&TTF_FontHasGlyph) fontHasGlyph = nullptr;
    decltype(&TTF_RenderText_Blended) renderText = nullptr;

    ~State()
    {
        clearGlyphs();
        if (atlas) destroyTexture(atlas);
        for (auto* font : fonts) closeFont(font);
        if (ttfInitialized) fontQuit();
        if (fontLibrary) FreeLibrary(fontLibrary);
        if (renderer) destroyRenderer(renderer);
        if (textActive) stopText(window);
        if (window) destroyWindow(window);
        if (videoInitialized) quit(SDL_INIT_VIDEO);
        if (library) FreeLibrary(library);
    }

    template <typename Function>
    bool load(Function& function, const char* name, HMODULE module = nullptr)
    {
        function = reinterpret_cast<Function>(GetProcAddress(module ? module : library, name));
        return function != nullptr;
    }

    bool loadFunctions()
    {
        return load(getVersion, "SDL_GetVersion")
            && load(getError, "SDL_GetError")
            && load(init, "SDL_InitSubSystem")
            && load(quit, "SDL_QuitSubSystem")
            && load(create, "SDL_CreateWindowAndRenderer")
            && load(destroyWindow, "SDL_DestroyWindow")
            && load(destroyRenderer, "SDL_DestroyRenderer")
            && load(pollEvent, "SDL_PollEvent")
            && load(windowSize, "SDL_GetWindowSize")
            && load(setWindowSize, "SDL_SetWindowSize")
            && load(outputSize, "SDL_GetRenderOutputSize")
            && load(displayScale, "SDL_GetWindowDisplayScale")
            && load(windowFlags, "SDL_GetWindowFlags")
            && load(setFullscreen, "SDL_SetWindowFullscreen")
            && load(modState, "SDL_GetModState")
            && load(startText, "SDL_StartTextInput")
            && load(stopText, "SDL_StopTextInput")
            && load(textArea, "SDL_SetTextInputArea")
            && load(clearComposition, "SDL_ClearComposition")
            && load(setColor, "SDL_SetRenderDrawColor")
            && load(clear, "SDL_RenderClear")
            && load(fillRect, "SDL_RenderFillRect")
            && load(text, "SDL_RenderDebugText")
            && load(present, "SDL_RenderPresent")
            && load(loadPng, "SDL_LoadPNG")
            && load(destroySurface, "SDL_DestroySurface")
            && load(createTexture, "SDL_CreateTextureFromSurface")
            && load(destroyTexture, "SDL_DestroyTexture")
            && load(textureScale, "SDL_SetTextureScaleMode")
            && load(textureColor, "SDL_SetTextureColorMod")
            && load(drawTexture, "SDL_RenderTexture")
            && load(drawGeometry, "SDL_RenderGeometry")
            && load(rendererName, "SDL_GetRendererName");
    }

    void clearGlyphs()
    {
        for (const auto& entry : glyphs)
        {
            if (entry.second.texture) destroyTexture(entry.second.texture);
        }
        glyphs.clear();
    }

    bool loadAtlas(const std::filesystem::path& folder)
    {
        auto* sheet = loadPng((folder / L"TuiCp437.png").u8string().c_str());
        if (!sheet) return false;
        if (sheet->w == 128 && sheet->h == 256)
        {
            atlas = createTexture(renderer, sheet);
        }
        destroySurface(sheet);
        return atlas && textureScale(atlas, SDL_SCALEMODE_NEAREST);
    }

    bool loadFonts(const std::filesystem::path& folder)
    {
        fontLibrary = LoadLibraryExW((folder / L"SDL3_ttf.dll").c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if (!fontLibrary
            || !load(fontInit, "TTF_Init", fontLibrary)
            || !load(fontVersion, "TTF_Version", fontLibrary)
            || fontVersion() != SDL_TTF_VERSION
            || !load(fontQuit, "TTF_Quit", fontLibrary)
            || !load(openFont, "TTF_OpenFont", fontLibrary)
            || !load(closeFont, "TTF_CloseFont", fontLibrary)
            || !load(setFontSize, "TTF_SetFontSize", fontLibrary)
            || !load(setFontStyle, "TTF_SetFontStyle", fontLibrary)
            || !load(fontHasGlyph, "TTF_FontHasGlyph", fontLibrary)
            || !load(renderText, "TTF_RenderText_Blended", fontLibrary)
            || !fontInit())
        {
            return false;
        }
        ttfInitialized = true;
        fontHeight = grid.cellHeight();
        const auto addFont = [&](const std::filesystem::path& path) {
            if (auto* font = openFont(path.u8string().c_str(), static_cast<float>(fontHeight)))
            {
                fonts.push_back(font);
            }
        };
        addFont(folder / L"DejaVuSansMono.ttf");
        if (fonts.empty()) return false;

        wchar_t windows[MAX_PATH];
        const auto length = GetWindowsDirectoryW(windows, ARRAYSIZE(windows));
        if (length > 0 && length < ARRAYSIZE(windows))
        {
            const auto systemFonts = std::filesystem::path(windows) / L"Fonts";
            for (const auto* file : { L"YuGothR.ttc", L"msgothic.ttc", L"msyh.ttc",
                L"malgun.ttf", L"seguisym.ttf", L"seguiemj.ttf" })
            {
                addFont(systemFonts / file);
            }
        }
        return true;
    }

    Glyph glyphTexture(const std::u32string& cluster, bool bold)
    {
        const auto key = std::make_pair(cluster, bold);
        const auto found = glyphs.find(key);
        if (found != glyphs.end()) return found->second;
        constexpr std::size_t maxCachedGlyphs = 512;
        if (glyphs.size() >= maxCachedGlyphs) clearGlyphs();

        Glyph glyph;
        for (auto* font : fonts)
        {
            const bool covered = std::all_of(cluster.begin(), cluster.end(), [&](char32_t cp) {
                return cp == U'\u200d' || (cp >= 0xfe00 && cp <= 0xfe0f) || fontHasGlyph(font, cp);
            });
            if (!covered) continue;
            const auto utf8 = UnicodeConversion::u32ToUtf8(cluster);
            setFontStyle(font, bold ? TTF_STYLE_BOLD : TTF_STYLE_NORMAL);
            auto* surface = renderText(font, utf8.data(), utf8.size(), { 255, 255, 255, 255 });
            if (!surface) continue;
            glyph = { createTexture(renderer, surface), surface->w, surface->h };
            destroySurface(surface);
            if (glyph.texture) break;
        }
        if (!glyph.texture) ++fallbackCount;
        glyphs.emplace(key, glyph);
        return glyph;
    }

    bool refreshGrid()
    {
        Grid next;
        next.visible = windowSize(window, &next.windowWidth, &next.windowHeight)
            && outputSize(renderer, &next.pixelWidth, &next.pixelHeight)
            && next.windowWidth > 0 && next.windowHeight > 0
            && next.pixelWidth > 0 && next.pixelHeight > 0
            && !(windowFlags(window) & SDL_WINDOW_MINIMIZED);
        const float scale = displayScale(window);
        if (std::isfinite(scale) && scale > 0)
        {
            next.scale = (std::min)(scale, 16.0f);
        }
        next = layout(next, cellWidthAdjustment);
        if (next.windowWidth == grid.windowWidth && next.windowHeight == grid.windowHeight
            && next.pixelWidth == grid.pixelWidth && next.pixelHeight == grid.pixelHeight
            && next.scale == grid.scale && next.visible == grid.visible
            && next.cellWidth == grid.cellWidth)
        {
            return false;
        }
        grid = next;
        if (grid.fits && grid.cellHeight() != fontHeight && !fonts.empty())
        {
            clearGlyphs();
            fontHeight = grid.cellHeight();
            for (auto* font : fonts) setFontSize(font, static_cast<float>(fontHeight));
        }
        geometryChanged = true;
        if (!grid.fits)
        {
            cancelPointer();
        }
        return true;
    }

    bool adjustFontSize(int step)
    {
        if (step == 0)
        {
            cellWidthAdjustment = 0;
        }
        else
        {
            if (!grid.fits) return false;
            const int width = std::clamp(grid.cellWidth + step,
                MinimumCellWidth, grid.availableCellWidth());
            if (width == grid.cellWidth) return false;
            cellWidthAdjustment = width - grid.defaultCellWidth();
        }

        if (!refreshGrid()) return false;
        cancelPointer();
        return true;
    }

    void cancelPointer()
    {
        cancelledButtons |= pressedButtons;
        pressedButtons = 0;
        wheelSteps = 0;
        wheelRemainder = 0;
    }

    void endTextInput()
    {
        if (textActive) stopText(window);
        textActive = false;
        composing = false;
        committedText.clear();
        textOffset = 0;
    }

    void syncTextInput(std::optional<Point> caret)
    {
        if (!grid.fits || !caret || caret->x < 0 || caret->x >= grid.columns
            || caret->y < 0 || caret->y >= grid.rows
            || !(windowFlags(window) & SDL_WINDOW_INPUT_FOCUS))
        {
            endTextInput();
            return;
        }

        const auto clientX = [&](int pixel) { return pixel * grid.windowWidth / grid.pixelWidth; };
        const auto clientY = [&](int pixel) { return pixel * grid.windowHeight / grid.pixelHeight; };
        const int x = grid.left + caret->x * grid.cellWidth;
        const int y = grid.top + caret->y * grid.cellHeight();
        const SDL_Rect area{ clientX(x), clientY(y),
            (std::max)(1, clientX(x + grid.cellWidth) - clientX(x)),
            (std::max)(1, clientY(y + grid.cellHeight()) - clientY(y)) };
        if (!textActive) textActive = startText(window);
        if (textActive) textArea(window, &area, 0);
    }

    Input::MouseEvent pointerEvent(Point position) const
    {
        Input::MouseEvent mouse;
        mouse.position = position;
        mouse.buttons = {
            (pressedButtons & SDL_BUTTON_LMASK) != 0,
            (pressedButtons & SDL_BUTTON_RMASK) != 0,
            (pressedButtons & SDL_BUTTON_MMASK) != 0
        };
        const auto mods = modState();
        mouse.modifiers = {
            (mods & SDL_KMOD_SHIFT) != 0,
            (mods & SDL_KMOD_CTRL) != 0,
            (mods & SDL_KMOD_ALT) != 0
        };
        return mouse;
    }

    void fill(float x, float y, float width, float height)
    {
        const SDL_FRect rect{ x, y, width, height };
        fillRect(renderer, &rect);
    }

    void queueAtlasTile(const SDL_FRect& src, const SDL_FRect& dest, ColorMapping::RgbValue fg)
    {
        // Fractional atlas scaling can put samples on a texel boundary.
        // The texture-copy path keeps nearest-neighbor picks consistent across the quad.
        if (!batchAtlas || grid.cellWidth % AtlasTileWidth != 0)
        {
            textureColor(atlas, fg.red, fg.green, fg.blue);
            drawTexture(renderer, atlas, &src, &dest);
            return;
        }
        const int index = static_cast<int>(atlasVertices.size());
        const SDL_FColor tint{ fg.red / 255.0f, fg.green / 255.0f, fg.blue / 255.0f, 1 };
        const float left = src.x / 128;
        const float right = (src.x + src.w) / 128;
        const float top = src.y / 256;
        const float bottom = (src.y + src.h) / 256;
        atlasVertices.push_back({ { dest.x, dest.y }, tint, { left, top } });
        atlasVertices.push_back({ { dest.x + dest.w, dest.y }, tint, { right, top } });
        atlasVertices.push_back({ { dest.x, dest.y + dest.h }, tint, { left, bottom } });
        atlasVertices.push_back({ { dest.x + dest.w, dest.y + dest.h }, tint, { right, bottom } });
        atlasIndices.insert(atlasIndices.end(), { index, index + 1, index + 2,
            index + 1, index + 3, index + 2 });
    }

    void flushAtlas()
    {
        if (atlasVertices.empty()) return;

        if (!drawGeometry(renderer, atlas, atlasVertices.data(), static_cast<int>(atlasVertices.size()),
            atlasIndices.data(), static_cast<int>(atlasIndices.size())))
        {
            batchAtlas = false;
            for (std::size_t i = 0; i < atlasVertices.size(); i += 4)
            {
                const auto& a = atlasVertices[i];
                const auto& b = atlasVertices[i + 3];
                queueAtlasTile(
                    { a.tex_coord.x * 128, a.tex_coord.y * 256,
                        (b.tex_coord.x - a.tex_coord.x) * 128, (b.tex_coord.y - a.tex_coord.y) * 256 },
                    { a.position.x, a.position.y, b.position.x - a.position.x, b.position.y - a.position.y },
                    { static_cast<Uint8>(std::lround(a.color.r * 255)),
                        static_cast<Uint8>(std::lround(a.color.g * 255)),
                        static_cast<Uint8>(std::lround(a.color.b * 255)) });
            }
        }
        atlasVertices.clear();
        atlasIndices.clear();
    }

    void atlasTile(unsigned char index, SDL_FRect dest, ColorMapping::RgbValue fg, bool bold)
    {
        SDL_FRect src{ static_cast<float>(index % 16 * AtlasTileWidth),
            static_cast<float>(index / 16 * AtlasTileHeight), AtlasTileWidth, AtlasTileHeight };
        queueAtlasTile(src, dest, fg);
        if (bold)
        {
            src.w *= (dest.w - 1) / dest.w;
            dest.x += 1;
            dest.w -= 1;
            queueAtlasTile(src, dest, fg);
        }
    }

    void drawGlyph(SDL_FRect dest, const ScreenCell& cell, ColorMapping::RgbValue fg)
    {
        unsigned char index = 0;
        const bool bold = cell.style.bold();
        if (cell.width == CellWidth::One && cell.cluster.size() <= 1 &&
            Cp437Encoding::tryEncodeExact(cell.glyph, index))
        {
            atlasTile(index, dest, fg, bold && (cell.glyph < 0x2500 || cell.glyph > 0x259f));
            return;
        }
        const auto glyph = glyphTexture(cell.cluster.empty() ? std::u32string(1, cell.glyph) : cell.cluster, bold);
        if (!glyph.texture)
        {
            dest.x += (dest.w - grid.cellWidth) / 2;
            dest.w = static_cast<float>(grid.cellWidth);
            atlasTile('?', dest, fg, bold);
            return;
        }
        const float scale = (std::min)({ 1.0f, dest.w / glyph.width, dest.h / glyph.height });
        const float w = std::ceil(glyph.width * scale);
        const float h = std::ceil(glyph.height * scale);
        const SDL_FRect ink{ dest.x + std::floor((dest.w - w) / 2),
            dest.y + std::floor((dest.h - h) / 2), w, h };
        textureColor(glyph.texture, fg.red, fg.green, fg.blue);
        drawTexture(renderer, glyph.texture, nullptr, &ink);
    }
};

class SdlRenderer::KeyboardSource final : public Input::IRawKeyboardSource
{
public:
    explicit KeyboardSource(State& state) : m_state(state) {}

    bool initialize() override { return true; }
    void shutdown() override {}

    std::optional<Input::RawInputEvent> pollRawInput() override
    {
        if (m_state.textOffset < m_state.committedText.size())
        {
            Input::RawKeyEvent key;
            key.virtualKey = VK_PACKET;
            key.character = m_state.committedText[m_state.textOffset++];
            key.pressed = true;
            return key;
        }
        if (m_state.wheelSteps != 0)
        {
            const bool up = m_state.wheelSteps > 0;
            auto mouse = m_state.pointerEvent(m_state.wheelPosition);
            mouse.action = Input::MouseAction::Wheel;
            mouse.button = up ? Input::MouseButton::WheelUp : Input::MouseButton::WheelDown;
            mouse.wheelDelta = up ? 120 : -120;
            m_state.wheelSteps -= up ? 1 : -1;
            return mouse;
        }
        for (const Uint8 button : { SDL_BUTTON_LEFT, SDL_BUTTON_RIGHT, SDL_BUTTON_MIDDLE })
        {
            if (m_state.cancelledButtons & SDL_BUTTON_MASK(button))
            {
                m_state.cancelledButtons &= ~SDL_BUTTON_MASK(button);
                auto mouse = m_state.pointerEvent({ -1, -1 });
                mouse.action = Input::MouseAction::Released;
                mouse.button = mouseButton(button);
                return mouse;
            }
        }

        SDL_Event event;
        while (m_state.pollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_EVENT_QUIT:
            case SDL_EVENT_WINDOW_CLOSE_REQUESTED:
                m_state.closeRequested = true;
                return std::nullopt;
            case SDL_EVENT_WINDOW_RESIZED:
            case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
            case SDL_EVENT_WINDOW_DISPLAY_CHANGED:
            case SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED:
            case SDL_EVENT_WINDOW_MINIMIZED:
            case SDL_EVENT_WINDOW_MAXIMIZED:
            case SDL_EVENT_WINDOW_RESTORED:
            case SDL_EVENT_WINDOW_ENTER_FULLSCREEN:
            case SDL_EVENT_WINDOW_LEAVE_FULLSCREEN:
                // Publish the new logical size before consuming subsequent input.
                if (m_state.refreshGrid()) return std::nullopt;
                break;
            case SDL_EVENT_WINDOW_FOCUS_LOST:
                m_state.cancelPointer();
                m_state.endTextInput();
                return std::nullopt;
            case SDL_EVENT_RENDER_DEVICE_RESET:
                m_state.clearGlyphs();
                m_state.destroyTexture(m_state.atlas);
                m_state.atlas = nullptr;
                if (!m_state.loadAtlas(executableFolder()))
                {
                    std::cerr << "TUI SDL could not restore its glyph atlas after a render device reset.\n";
                    m_state.closeRequested = true;
                    return std::nullopt;
                }
                break;
            case SDL_EVENT_TEXT_EDITING:
                m_state.composing = m_state.textActive && event.edit.text && *event.edit.text;
                break;
            case SDL_EVENT_TEXT_INPUT:
                if (m_state.textActive && event.text.text)
                {
                    m_state.composing = false;
                    m_state.committedText = UnicodeConversion::utf8ToU32(event.text.text);
                    m_state.textOffset = 0;
                    if (!m_state.committedText.empty()) return pollRawInput();
                }
                break;
            case SDL_EVENT_MOUSE_WHEEL:
            {
                const auto position = m_state.grid.cellAt(event.wheel.mouse_x, event.wheel.mouse_y);
                if (position.x < 0)
                {
                    m_state.wheelRemainder = 0;
                    break;
                }
                if (!std::isfinite(event.wheel.y)) break;
                const double direction = event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -1 : 1;
                m_state.wheelRemainder = std::clamp(m_state.wheelRemainder + event.wheel.y * direction, -32.0, 32.0);
                m_state.wheelSteps = static_cast<int>(m_state.wheelRemainder);
                m_state.wheelRemainder -= m_state.wheelSteps;
                m_state.wheelPosition = position;
                if (m_state.wheelSteps) return pollRawInput();
                break;
            }
            case SDL_EVENT_WINDOW_MOUSE_LEAVE:
                return m_state.pointerEvent({ -1, -1 });
            case SDL_EVENT_MOUSE_MOTION:
            {
                auto mouse = m_state.pointerEvent(
                    m_state.grid.cellAt(event.motion.x, event.motion.y, m_state.pressedButtons != 0));
                mouse.action = mouse.buttons.anyPressed() ? Input::MouseAction::Dragged
                    : Input::MouseAction::Moved;
                return mouse;
            }
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
            case SDL_EVENT_MOUSE_BUTTON_UP:
            {
                const auto button = mouseButton(event.button.button);
                if (button == Input::MouseButton::None) break;
                const unsigned mask = SDL_BUTTON_MASK(event.button.button);
                const Point position = m_state.grid.cellAt(event.button.x, event.button.y);
                const bool down = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
                if (down)
                {
                    if (position.x < 0) break;
                    m_state.pressedButtons |= mask;
                }
                else
                {
                    if (!(m_state.pressedButtons & mask)) break;
                    m_state.pressedButtons &= ~mask;
                }
                auto mouse = m_state.pointerEvent(position);
                mouse.button = button;
                mouse.clickCount = event.button.clicks;
                mouse.action = down ? Input::MouseAction::Pressed : Input::MouseAction::Released;
                return mouse;
            }
            case SDL_EVENT_KEY_DOWN:
            case SDL_EVENT_KEY_UP:
            {
                if ((event.key.mod & SDL_KMOD_CTRL) && !(event.key.mod & (SDL_KMOD_ALT | SDL_KMOD_GUI)))
                {
                    std::optional<int> step;
                    switch (event.key.key)
                    {
                    case SDLK_EQUALS: case SDLK_PLUS: case SDLK_KP_PLUS: step = 1; break;
                    case SDLK_MINUS: case SDLK_KP_MINUS: step = -1; break;
                    case SDLK_0: case SDLK_KP_0: step = 0; break;
                    default: break;
                    }
                    if (step)
                    {
                        // Let the host publish the resized grid before routing later pointer events.
                        if (event.key.down && m_state.adjustFontSize(*step)) return std::nullopt;
                        break;
                    }
                }
                if (event.key.key == SDLK_F11)
                {
                    if (event.key.down && !event.key.repeat)
                    {
                        const bool fullscreen = (m_state.windowFlags(m_state.window) & SDL_WINDOW_FULLSCREEN) != 0;
                        if (!m_state.setFullscreen(m_state.window, !fullscreen))
                        {
                            std::cerr << "TUI SDL fullscreen failed: " << m_state.getError() << '\n';
                        }
                        m_state.refreshGrid();
                        return std::nullopt;
                    }
                    break;
                }
                const auto key = translateKey(event.key);
                if (m_state.textActive && key.virtualKey == VK_SPACE &&
                    !(key.modifiers.ctrl || key.modifiers.alt))
                {
                    break;
                }
                if (m_state.composing && (key.virtualKey == VK_RETURN || key.virtualKey == VK_ESCAPE))
                {
                    if (event.key.down && key.virtualKey == VK_ESCAPE)
                    {
                        m_state.clearComposition(m_state.window);
                        m_state.composing = false;
                    }
                    break;
                }
                if (key.virtualKey != 0 && (m_state.grid.fits || key.virtualKey == VK_ESCAPE))
                {
                    return key;
                }
                break;
            }
            default: break;
            }
        }
        return std::nullopt;
    }

private:
    State& m_state;
};

SdlRenderer::SdlRenderer(const StartupDiagnosticsContext& startup)
    : m_startup(startup)
{
    m_diagnostics.setEnabled(true);
}

SdlRenderer::~SdlRenderer() = default;

bool SdlRenderer::initialize()
{
    if (m_state)
    {
        return true;
    }

    auto state = std::make_unique<State>();
    const auto folder = executableFolder();
    if (!folder.empty())
    {
        state->library = LoadLibraryExW((folder / L"SDL3.dll").c_str(), nullptr,
            LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    }
    if (!state->library)
    {
        reportStartup("Could not load SDL3.dll beside the application. Rebuild or restore the bundled x64 SDL3.dll.");
        return false;
    }
    if (!state->loadFunctions() || state->getVersion() != SDL_VERSION)
    {
        reportStartup("SDL3.dll does not match the bundled SDL 3.4.20 interfaces.");
        return false;
    }
    if (!state->init(SDL_INIT_VIDEO))
    {
        reportStartup(std::string("Could not initialize SDL video: ") + state->getError());
        return false;
    }
    state->videoInitialized = true;
    if (!state->create("TUI (SDL)", 1024, 720,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_MAXIMIZED,
        &state->window, &state->renderer))
    {
        reportStartup(std::string("Could not create the SDL window and renderer: ") + state->getError());
        return false;
    }

    state->initialCellWidthAdjustment = StartupConfigLoader::loadSdlFontSizeAdjustment();
    state->cellWidthAdjustment = state->initialCellWidthAdjustment;
    state->refreshGrid();
    if (!state->loadAtlas(folder))
    {
        reportStartup("Could not load the 128 x 256 CP437 atlas TuiCp437.png beside the application.");
        return false;
    }
    if (!state->loadFonts(folder))
    {
        reportStartup("Could not initialize SDL3_ttf.dll and DejaVuSansMono.ttf beside the application.");
        return false;
    }
    const char* driver = state->rendererName(state->renderer);
    state->batchAtlas = driver && std::string_view(driver) != "software";
    state->publishedSize = { state->grid.columns, state->grid.rows };
    state->geometryChanged = false;
    m_state = std::move(state);
    reportStartup({});
    return true;
}

void SdlRenderer::shutdown()
{
    if (m_state)
    {
        if (m_state->cellWidthAdjustment != m_state->initialCellWidthAdjustment &&
            !StartupConfigLoader::saveSdlFontSizeAdjustment(m_state->cellWidthAdjustment))
        {
            std::cerr << "TUI SDL could not save the font size in startup.config.\n";
        }
        reportStartup({});
    }
    m_state.reset();
}

void SdlRenderer::present(const ScreenBuffer& buffer)
{
    if (!m_state)
    {
        return;
    }
    auto& state = *m_state;
    const auto& grid = state.grid;
    if (!grid.visible)
    {
        return;
    }
    const auto drawStart = std::chrono::steady_clock::now();
    state.setColor(state.renderer, 0, 0, 0, 255);
    state.clear(state.renderer);
    if (!grid.fits)
    {
        constexpr char message[] = "Resize the window or press F11";
        constexpr int messageWidth = static_cast<int>(sizeof(message) - 1) * NominalCellWidth;
        state.setColor(state.renderer, 255, 255, 255, 255);
        state.text(state.renderer,
            static_cast<float>((std::max)(0, (grid.pixelWidth - messageWidth) / 2)),
            static_cast<float>((std::max)(0, (grid.pixelHeight - 8) / 2)), message);
        state.present(state.renderer);
        return;
    }
    const int columns = (std::min)(buffer.getWidth(), grid.columns);
    const int rows = (std::min)(buffer.getHeight(), grid.rows);
    const double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - state.blinkEpoch).count();
    const bool slowVisible = std::fmod(elapsed, 1.0) < 0.5;
    const bool fastVisible = std::fmod(elapsed, 0.5) < 0.25;
    const auto colors = [](const Style& style) {
        auto fg = color(style.foregroundColorValue(), Color::Basic::BrightWhite);
        auto bg = color(style.backgroundColorValue(), Color::Basic::Black);
        if (style.reverse()) std::swap(fg, bg);
        if (style.dim())
        {
            fg = { static_cast<Uint8>(fg.red / 2), static_cast<Uint8>(fg.green / 2),
                static_cast<Uint8>(fg.blue / 2) };
        }
        return std::make_pair(fg, bg);
    };
    for (int y = 0; y < rows; ++y)
    {
        const float py = static_cast<float>(grid.top + y * grid.cellHeight());
        for (int x = 0; x < columns;)
        {
            const auto& style = buffer.getLogicalCell(x, y).style;
            if (!style.backgroundColorValue() && !style.reverse())
            {
                ++x;
                continue;
            }
            const auto bg = colors(style).second;
            int end = x + 1;
            while (end < columns)
            {
                const auto& next = buffer.getLogicalCell(end, y).style;
                if (style.backgroundColorValue() != next.backgroundColorValue() ||
                    style.reverse() != next.reverse() ||
                    (style.reverse() && style.foregroundColorValue() != next.foregroundColorValue()))
                {
                    break;
                }
                ++end;
            }
            const float px = static_cast<float>(grid.left + x * grid.cellWidth);
            state.setColor(state.renderer, bg.red, bg.green, bg.blue, 255);
            state.fill(px, py, static_cast<float>((end - x) * grid.cellWidth),
                static_cast<float>(grid.cellHeight()));
            x = end;
        }

        for (int x = 0; x < columns; ++x)
        {
            const auto& cell = buffer.getCell(x, y);
            const Style& style = cell.style;
            if (cell.kind != CellKind::Glyph || style.invisible() ||
                !(style.fastBlink() ? fastVisible : !style.slowBlink() || slowVisible)) continue;

            const bool ink = cell.glyph != U' ' || cell.cluster.size() > 1;
            if (!ink && !style.underline() && !style.strike()) continue;
            const bool wide = cell.width == CellWidth::Two;
            if (wide && x + 1 >= columns) continue;
            const auto fg = colors(style).first;
            const float px = static_cast<float>(grid.left + x * grid.cellWidth);
            const float w = static_cast<float>(grid.cellWidth * (wide ? 2 : 1));
            const float h = static_cast<float>(grid.cellHeight());
            if (ink) state.drawGlyph({ px, py, w, h }, cell, fg);
            if (style.underline() || style.strike())
            {
                state.flushAtlas();
                state.setColor(state.renderer, fg.red, fg.green, fg.blue, 255);
                if (style.underline()) state.fill(px, py + h - 2, w, 1);
                if (style.strike()) state.fill(px, py + h / 2, w, 1);
            }
        }
    }
    state.flushAtlas();
    const auto presentStart = std::chrono::steady_clock::now();
    state.present(state.renderer);
    const auto end = std::chrono::steady_clock::now();
    state.lastDrawMs = std::chrono::duration<double, std::milli>(presentStart - drawStart).count();
    state.lastPresentMs = std::chrono::duration<double, std::milli>(end - presentStart).count();
}

void SdlRenderer::resize(int width, int height)
{
    if (!m_state || !m_state->grid.visible || width <= 0 || height <= 0)
    {
        return;
    }
    const auto& grid = m_state->grid;
    const int w = (std::min)(width, MaximumColumns) * grid.cellWidth;
    const int h = (std::min)(height, MaximumRows) * grid.cellHeight();
    m_state->setWindowSize(m_state->window,
        (std::max)(1, static_cast<int>(std::lround(static_cast<double>(w) * grid.windowWidth / grid.pixelWidth))),
        (std::max)(1, static_cast<int>(std::lround(static_cast<double>(h) * grid.windowHeight / grid.pixelHeight))));
}

bool SdlRenderer::pollResize()
{
    if (!m_state) return false;

    auto& state = *m_state;
    state.refreshGrid();
    if (std::exchange(state.geometryChanged, false))
    {
        reportStartup({});
    }
    if (!state.grid.fits || (state.grid.columns == state.publishedSize.width &&
        state.grid.rows == state.publishedSize.height))
    {
        return false;
    }
    state.publishedSize = { state.grid.columns, state.grid.rows };
    return true;
}

int SdlRenderer::getConsoleWidth() const
{
    return m_state ? m_state->publishedSize.width : MinimumColumns;
}

int SdlRenderer::getConsoleHeight() const
{
    return m_state ? m_state->publishedSize.height : MinimumRows;
}

TextBackendCapabilities SdlRenderer::textCapabilities() const
{
    return { false, true, true, m_state && m_state->fonts.size() > 1,
        m_state && m_state->fonts.size() > 1 };
}

const RenderDiagnostics* SdlRenderer::renderDiagnostics() const
{
    return &m_diagnostics;
}

std::unique_ptr<Input::IRawKeyboardSource> SdlRenderer::createKeyboardSource()
{
    return std::make_unique<KeyboardSource>(*m_state);
}

bool SdlRenderer::closeRequested() const
{
    return m_state && m_state->closeRequested;
}

void SdlRenderer::setTextInputCaret(std::optional<Point> position)
{
    if (m_state) m_state->syncTextInput(position);
}

void SdlRenderer::reportStartup(const std::string& error)
{
    auto& report = m_diagnostics.report();
    RendererCapabilities capabilities{};
    capabilities.colorTier = error.empty() ? ColorSupport::Rgb24 : ColorSupport::None;
    capabilities.unicodeOutput = error.empty();
    capabilities.slowBlink = RendererFeatureSupport::Unsupported;
    capabilities.fastBlink = RendererFeatureSupport::Unsupported;
    if (error.empty())
    {
        capabilities.reverse = RendererFeatureSupport::Supported;
        capabilities.invisible = RendererFeatureSupport::Supported;
        capabilities.bold = RendererFeatureSupport::Emulated;
        capabilities.dim = RendererFeatureSupport::Emulated;
        capabilities.underline = RendererFeatureSupport::Supported;
        capabilities.strike = RendererFeatureSupport::Supported;
        capabilities.slowBlink = RendererFeatureSupport::Emulated;
        capabilities.fastBlink = RendererFeatureSupport::Emulated;
        capabilities.graphemeClusters = RendererFeatureSupport::Supported;
        capabilities.combiningMarks = RendererFeatureSupport::Supported;
        capabilities.eastAsianWide = RendererFeatureSupport::Supported;
    }
    report.setCapabilities(capabilities);
    report.setPolicy(StylePolicyFactory::buildFromCapabilities(
        capabilities, RendererStylePolicyProfile::DirectPresentationBackend));

    BackendStateSnapshot backend;
    backend.startupConfigSource = m_startup.startupConfigSource;
    backend.startupConfigFileFound = m_startup.startupConfigFileFound;
    backend.configuredHostPreference = toString(m_startup.configuredHostPreference);
    backend.configuredRendererPreference = toString(m_startup.configuredRendererPreference);
    backend.requestedHostKind = toString(m_startup.requestedHost);
    backend.requestedRendererIdentity = toString(m_startup.requestedRenderer);
    if (error.empty())
    {
        backend.actualHostKind = toString(TerminalHostKind::SdlWindow);
        backend.rendererIdentity = toString(RendererKind::SdlRenderer);
        const char* driver = m_state->rendererName(m_state->renderer);
        backend.activeRenderPath = driver ? std::string("SDL 2D (") + driver + ")" : "SDL 2D";
    }
    else
    {
        std::cerr << "TUI SDL startup failed: " << error << '\n';
    }
    report.setBackendState(backend);

    auto trace = m_startup.selectionTrace;
    if (error.empty())
    {
        const auto& grid = m_state->grid;
        const auto dimensions = [](int w, int h) {
            return std::to_string(w) + "x" + std::to_string(h);
        };
        trace.addEntry(RendererSelectionStage::FinalSelection, "Selected SDL renderer",
            "SDL 3.4.20, client=" + dimensions(grid.windowWidth, grid.windowHeight)
            + ", drawable=" + dimensions(grid.pixelWidth, grid.pixelHeight)
            + ", display scale=" + std::to_string(grid.scale)
            + ", grid=" + (grid.fits ? dimensions(grid.columns, grid.rows) : "suspended")
            + ", cell=" + dimensions(grid.cellWidth, grid.cellHeight())
            + ", origin=" + std::to_string(grid.left) + "," + std::to_string(grid.top)
            + ", fonts=" + std::to_string(m_state->fonts.size())
            + ", cached glyphs=" + std::to_string(m_state->glyphs.size())
            + ", fallback cache misses=" + std::to_string(m_state->fallbackCount)
            + ", last draw ms=" + std::to_string(m_state->lastDrawMs)
            + ", last present ms=" + std::to_string(m_state->lastPresentMs));
    }
    else
    {
        trace.addEntry(RendererSelectionStage::FinalSelection, "SDL initialization failed", error);
    }
    report.setRendererSelectionTrace(trace);
    RenderDiagnosticsWriter::write(m_diagnostics);
}
