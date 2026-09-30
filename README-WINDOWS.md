# SonicDNA SuperAmp 0.7.1 — Windows ASIO Source

This matches the corrected 0.7.1 Mac source.

Included:
- Windows x64 Standalone
- VST3
- JUCE ASIO enabled for the Standalone build
- Static Microsoft C/C++ runtime for portability
- Inno Setup installer
- Cleanup of older single-file or folder-style VST3 installs
- GitHub Actions workflow with a Standalone launch smoke test

Local Windows build requirements:
- Visual Studio 2022 with Desktop development with C++
- CMake
- Inno Setup 6

PowerShell:

    Set-ExecutionPolicy -Scope Process Bypass
    .\build-windows.ps1

Close all DAWs before installing so Windows does not lock an older VST3 bundle.
