# Changelog

## v1.0 — 2026-11-01

- Primeira release pública do port.
- Renderização nativa 960×544 via vitaGL, ~60 FPS.
- Áudio via `AudioTrack` → `sceAudioOut` com resample FIR 44.1→48 kHz.
- Controles físicos e touch:
  - D-pad e analógico esquerdo para movimento.
  - Quadrado/Triângulo soco; Cross/Circle chute.
  - Start pausa; Cross confirma; Circle volta/fecha o pause.
- Save/config em `ux0:data/brucelee/save/`.
- LiveArea com arte neutra (sem assets do jogo).
- VPK somente loader; dados do jogo fornecidos pelo usuário.
- Documentação de instalação, build, troubleshooting, autoria e publicação.
