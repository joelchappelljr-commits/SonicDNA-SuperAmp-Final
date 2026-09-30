# SonicDNA SuperAmp 0.7.1 — Mac Source

This source package is for local macOS Terminal builds.

Key architecture:
- Paired NAM mode: user preamp NAM -> user poweramp NAM
- Single NAM mode: user full-amp NAM, poweramp stage bypassed
- User cabinet IR only; no built-in amps or cabinets
- IR level, low cut and high cut
- Tube Screamer, Klon and Timmy drives
- 10-band graphic EQ
- Studio compressor
- Delay
- SDNA Space reverbs
- Stable tuner
- CPU / input / output metering
- Two-level modular interface with draggable signal-path icons
- AMP panel opens by default
- Startup crash fix: initial setSize happens only after module icons exist

Build:
    chmod +x build-mac.sh
    ./build-mac.sh

Universal Apple Silicon + Intel:
    ./build-mac.sh --universal

The script prefers:
    /Applications/CMake.app/Contents/bin/cmake
