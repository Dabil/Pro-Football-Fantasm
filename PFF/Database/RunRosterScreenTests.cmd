@echo off
setlocal
cd /d "%~dp0..\.."
if not exist PFF.sln exit /b 1
where cl >nul 2>nul
if errorlevel 1 (
  for /f "usebackq tokens=*" %%i in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "PFF_ROSTER_VS=%%i"
)
if defined PFF_ROSTER_VS call "%PFF_ROSTER_VS%\Common7\Tools\VsDevCmd.bat" -no_logo -arch=x64 -host_arch=x64
where cl >nul 2>nul
if errorlevel 1 exit /b 1
if not exist bin\RosterBuild mkdir bin\RosterBuild
msbuild vendor\GCE\GCE.vcxproj /p:Configuration=Debug /p:Platform=x64 "/p:SolutionDir=%CD%/" "/p:IntDir=%CD%/bin/RosterBuild/obj/GCE/" "/p:OutDir=%CD%/bin/RosterBuild/Debug/" /nologo /v:minimal
if errorlevel 1 exit /b %errorlevel%
msbuild vendor\TUI\TUI.vcxproj /p:Configuration=Debug /p:Platform=x64 "/p:SolutionDir=%CD%/" "/p:IntDir=%CD%/bin/RosterBuild/obj/TUI/" "/p:OutDir=%CD%/bin/RosterBuild/Debug/" /nologo /v:minimal
if errorlevel 1 exit /b %errorlevel%
cl /nologo /MDd /Zi /D_DEBUG /c vendor\sqlite\sqlite3.c /Fo:bin\RosterBuild\RosterSqlite.obj /Fd:bin\RosterBuild\SqliteCompile.pdb
if errorlevel 1 exit /b %errorlevel%
cl /nologo /utf-8 /std:c++17 /EHsc /MDd /Zi /D_DEBUG /I PFF /I vendor /I vendor/TUI /I vendor/sqlite /Fd:bin\RosterBuild\RosterTestsCompile.pdb PFF\Database\RosterScreenTests.cpp PFF\Database\PFFDatabase.cpp PFF\Screens\RosterScreen.cpp PFF\Screens\TeamScreen.cpp PFF\Screens\FrameView.cpp PFF\Screens\HeaderView.cpp PFF\Screens\FooterView.cpp PFF\Screens\ContentControlView.cpp /Fo:bin\RosterBuild\ /Fe:bin\RosterBuild\RosterScreenTests.exe bin\RosterBuild\Debug\GCE.lib bin\RosterBuild\Debug\TUI.lib bin\RosterBuild\RosterSqlite.obj /link kernel32.lib user32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib /DEBUG /PDB:bin\RosterBuild\RosterScreenTests.pdb
if errorlevel 1 exit /b %errorlevel%
bin\RosterBuild\RosterScreenTests.exe
