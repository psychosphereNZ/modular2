@echo off
rem Baut Patchwerk Modular als VST3 (benoetigt Visual Studio 2022 mit C++ und CMake)
cd /d "%~dp0"
where cmake >/dev/null 2>/dev/null || (echo CMake nicht gefunden. Bitte Visual Studio 2022 mit "Desktopentwicklung mit C++" installieren. & pause & exit /b 1)
cmake -B build -G "Visual Studio 17 2022" -A x64 || (pause & exit /b 1)
cmake --build build --config Release --target PatchwerkModular_VST3 || (pause & exit /b 1)
echo.
echo Fertig: build\PatchwerkModular_artefacts\Release\VST3\Patchwerk Modular.vst3
echo Diesen Ordner nach C:\Program Files\Common Files\VST3\ kopieren.
explorer "build\PatchwerkModular_artefacts\Release\VST3"
pause
