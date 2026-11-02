# Changelog

## v1.0 — 2026-11-01

- First public release of the port.
- Native 960×544 rendering via vitaGL, ~60 FPS.
- Audio through `AudioTrack` → `sceAudioOut` with a 44.1→48 kHz FIR resampler.
- Physical and touch controls:
  - D-pad and left analog stick for movement.
  - Square/Triangle punch; Cross/Circle kick.
  - Start pauses; Cross confirms; Circle goes back/closes the pause menu.
- Save/config in `ux0:data/brucelee/save/`.
- Neutral LiveArea artwork (no game assets).
- Loader-only VPK; game data must be provided by the user.
- Documentation for installation, building, troubleshooting, authorship and
  publishing.
