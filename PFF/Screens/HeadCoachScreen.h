#pragma once

#include "App/LoadedWorldContext.h"
#include "Assets/AssetLibrary.h"
#include "Screens/Screen.h"
#include "UI/Widgets/Button.h"
#include "UI/Widgets/ContainerWidget.h"
#include <array>
#include <functional>

class HeadCoachScreen final : public Screen
{
public:
    HeadCoachScreen(Assets::AssetLibrary& assets,
        std::function<bool(std::string_view)> dispatch,
        std::function<LoadedWorldContext()> context,
        LoadedWorldContext selectedWorld, std::int64_t teamSeasonId,
        std::function<void(std::int64_t)> openFormationEditor);

    void onEnter() override;
    bool handleEvent(const Input::Event& event) override;
    void update(const Animation::TickEvent& event) override;
    void draw(Surface& surface) override;

private:
    LoadedWorldContext currentWorld() const;
    bool matchesSelection(const LoadedWorldContext& world) const;

    std::function<bool(std::string_view)> m_dispatch;
    std::function<LoadedWorldContext()> m_context;
    LoadedWorldContext m_selectedWorld;
    std::int64_t m_teamSeasonId;
    std::function<void(std::int64_t)> m_openFormationEditor;
    TextObject m_banner;
    ContainerWidget m_root;
    std::array<Button*, 5> m_controls{};
    Button* m_return = nullptr;
    bool m_formationPending = false;
    bool m_returnPending = false;
};
