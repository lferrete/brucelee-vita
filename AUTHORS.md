# Authors and credits

## PS Vita port

**Leandro Ferrete** — loader adaptation, Android/ZSystem integration, input
(physical, touch, HUD, D-pad), audio (AudioTrack JNI → `sceAudioOut`, 44.1→48
kHz FIR), hardware diagnostics, documentation and testing.

Copyright (C) 2026 Leandro Ferrete. The original port code is distributed under
the MIT license (see `LICENSE`). The LiveArea artwork is derived from the
original game and remains the property of its rights holders; the port
attribution lives in the source, log banner and VPK metadata.

## Original game

**Bruce Lee: Dragon Warrior** © Digital Legends / Sony Ericsson / Indiagames.
All rights reserved. This project does **not** redistribute the APK, `.so`
libraries, content packages, game data or save files. The LiveArea artwork is
derived from the original game; users must provide a legally obtained copy to
play.

## Third-party components

- **so_util** — Andy Nguyen (TheFloW), Rinnegatamante.
- **FalsoJNI** — Volodymyr Atamanenko and contributors.
- **vitaGL** — Rinnegatamante and contributors.
- **kubridge** — TheFloW.
- **vitaShaRK / SceShaccCg / ShaRKBR33D** — Rinnegatamante and contributors.
- **fios / sha1 / zlib / newlib / VitaSDK** — their respective authors.
- The loader follows the Android-port model of the Vita ecosystem (GTA/Asphalt
  and related projects); the original credits remain in `LICENSE` and in the
  headers of derived files.

## How to identify this port

- Log banner: `Port by Leandro Ferrete (v1.0)` and the copyright line in
  `ux0:data/brucelee/logs/`.
- SFO: `PORT_AUTHOR=Leandro Ferrete` and `PORT_VERSION=1.0`.
- VPK: `sce_sys/provenance.txt` with author, version and legal notice.
- Release: `build/SHA256SUMS.txt` with the VPK hash.
- Source: headers in `source/port_author.h`, `source/main.c`,
  `source/loader.c`, `source/java_shim.c`, `CMakeLists.txt`, `make.sh`,
  `build.sh` and the tools.
- The LiveArea has no author watermark by choice.

See `docs/AUTHORSHIP.md` for the verification procedure and git/GPG guidance.
