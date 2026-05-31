#pragma once

#include "App/ApplicationHost.h"

class PFFApplication : public ApplicationHost
{
public:
    explicit PFFApplication(
        StartupRendererSelection rendererSelection,
        StartupValidationScreenPreference validationScreenSelection,
        const StartupDiagnosticsContext& startupDiagnostics);

    ~PFFApplication() override = default;

protected:
    void configureAssetLibrary() override;
    bool onInitialize() override;

private:
    StartupValidationScreenPreference m_validationScreenStart =
        StartupValidationScreenPreference::ValidationStartFalse;
};

