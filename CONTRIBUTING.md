# Contributing

This repository is maintained by **Leandro Ferrete**. The `main` branch is
protected: direct pushes are restricted to the owner, and external changes are
accepted only through pull requests after review.

## Rules

1. **No game files.** Never submit the APK, `.so` libraries, `Core.pakh`,
   `Core.pakd000`, `data.zpak`, textures, sounds, saves or any other game asset.
2. **No piracy.** Do not link to APK/game-data downloads or request them in
   issues/PRs.
3. **Code and docs only.** Contributions must be original code, documentation,
   build scripts or neutral artwork created by you, compatible with the MIT
   license used by this port.
4. **Keep third-party credits.** Do not remove copyright/license headers from
   so_util, FalsoJNI, vitaGL, kubridge, vitaShaRK or other dependencies.
5. **Build before opening a PR.** State the VitaSDK version, the target
   firmware/CFW, and what you tested on hardware.
6. **No secrets.** Remove tokens, IP addresses, serial numbers and personal
   data from logs and screenshots.

## Pull requests

- Fork the repository and open a PR against `main`.
- The owner reviews and merges; non-owners cannot push to `main`.
- PRs that include game files or copyrighted assets are closed immediately.
- By submitting code/docs, you agree to license your contribution under the
  repository's MIT license (see `LICENSE`).

## Bug reports

Include:

- port version/commit;
- game version and ABI (`armeabi`);
- console/CFW version;
- steps to reproduce;
- the relevant part of `ux0:data/brucelee/logs/brucelee_NNN.log`
  (redact personal data).

Do **not** attach the game or its data. A log plus a description is enough.
