#include "App/Application.h"
#include "App/CommandLineOptions.h"
#include "App/StartupConfig.h"
#include "App/StartupOptions.h"
#include "App/TerminalLauncher.h"

#include <iostream>

/*
One very important Windows console rule

The order matters:

1) Set buffer size
2) Set window size

If the window is larger than the buffer, Windows will fail.
*/

int main()
{
    const CommandLineOptions commandLineOptions =
        CommandLineOptionsParser::parseFromProcessCommandLine();

    if (commandLineOptions.showHelp)
    {
        CommandLineOptionsParser::writeHelpText(std::wcout);
        return 0;
    }

    const StartupConfig startupConfig = StartupConfigLoader::loadFromStartupConfig();

    const StartupOptions startupOptions =
        StartupOptionsResolver::resolve(startupConfig, commandLineOptions);

    const StartupLaunchDecision launchDecision =
        TerminalLauncher::prepareStartup(startupOptions);

    if (launchDecision.relaunchPerformed)
    {
        return 0;
    }

    Application app(
        launchDecision.rendererSelection,
        startupOptions.validationScreenPreference,
        launchDecision.diagnosticsContext);

    if (!app.initialize())
    {
        return 1;
    }

    app.run();
    app.shutdown();

    return 0;
}