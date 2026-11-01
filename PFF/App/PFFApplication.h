#pragma once

#include "App/ApplicationHost.h"
#include "App/LoadedWorldContext.h"

#include <filesystem>
#include <string_view>

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

private:
    // Handles PFF-specific action commands, then falls back to the generic
    // ApplicationHost route/navigation command handler.
    bool handlePffCommandId(std::string_view commandId);

    // First PFF Classic New League vertical slice.
    //
    // Creates a new 1989 GCE World from the compiled PFF Core, validates/loads
    // it, and only then publishes it as the application's LoadedWorldContext.
    bool createNewLeague();

    // Validates/loads an existing World.db and publishes it as the application's
    // active LoadedWorldContext. Used by both New League and Load League.
    bool loadWorldIntoContext(const std::filesystem::path& databasePath);

private:
    LoadedWorldContext m_loadedWorldContext;

    StartupValidationScreenPreference m_validationScreenStart = StartupValidationScreenPreference::ValidationStartFalse;
};



