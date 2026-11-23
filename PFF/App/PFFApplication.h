#pragma once

#include "App/ApplicationHost.h"
#include "App/LoadedWorldContext.h"
#include "Audio/SdlAudio.h"

#include <filesystem>
#include <string_view>
#include <vector>

class PFFApplication : public ApplicationHost
{
public:
    explicit PFFApplication(
        StartupRendererSelection rendererSelection,
        StartupValidationScreenPreference validationScreenSelection,
        const StartupDiagnosticsContext& startupDiagnostics);

    ~PFFApplication() override = default;

    [[nodiscard]] bool hasLoadedWorld() const noexcept;

    [[nodiscard]] LoadedWorldContext& loadedWorldContext() noexcept;
    [[nodiscard]] const LoadedWorldContext& loadedWorldContext() const noexcept;

    void setLoadedWorldContext(LoadedWorldContext context);
    void clearLoadedWorldContext();

protected:
    void configureAssetLibrary() override;
    bool onInitialize() override;
    void onShutdown() override;
    void update(const Animation::TickEvent& event) override;

private:
    // Handles PFF-specific action commands, then falls back to the generic
    // ApplicationHost route/navigation command handler.
    bool handlePffCommandId(std::string_view commandId);

    // First PFF Classic New League vertical slice.
    //
    // Creates a new 1989 GCE World from the compiled PFF Core, validates/loads
    // it, and only then publishes it as the application's LoadedWorldContext.
    bool createNewLeague();
    void openTeamScreen(const LoadedWorldContext&,std::int64_t teamSeasonId);
    void openScheduleScreen(std::int64_t teamSeasonId);
    void openTeamDataScreen(const LoadedWorldContext&,std::int64_t teamSeasonId);

    // Validates/loads an existing World.db and publishes it as the application's
    // active LoadedWorldContext. Used by both New League and Load League.
    bool loadWorldIntoContext(const std::filesystem::path& databasePath);

private:
    LoadedWorldContext m_loadedWorldContext;
    Audio::SdlAudio m_audio;
    std::vector<std::filesystem::path> m_menuMusic;
    std::size_t m_nextMenuTrack = 0;
    bool m_inGameTime = false;

    StartupValidationScreenPreference m_validationScreenStart = StartupValidationScreenPreference::ValidationStartFalse;
};



