# Troubleshooting

> First: the VPK does **not** contain the game. If `.so` files or packages are
> missing, the loader cannot guess them. Check the layout in `docs/INSTALL.md`.

---

## 1. Game does not launch / black screen / returns to LiveArea

1. Check **kubridge** and **FdFix** in `ux0:tai/config.txt` (`*KERNEL`).
2. Check `ur0:data/libshacccg.suprx`.
3. Check `ux0:data/brucelee/lib/armeabi/libdefault.so` and `libzplatform.so`.
4. Check `assets/BruceLee.zpak`, `content/Core.pakh`,
   `content/Core.pakd000`, `content/data.zpak`.
5. Read the last lines of `ux0:data/brucelee/logs/brucelee_NNN.log`.

Common log errors:

- `missing shared object`: a `.so` is missing.
- `Unresolved import`: inconsistent build; use the official VPK.
- Empty `EnumeratePackages`: wrong packages/paths.
- `CreateZdkDisplay`/shader: missing `libshacccg.suprx` or cache.

## 2. "Missing shared object"

Copy the missing `.so` to `ux0:data/brucelee/lib/armeabi/`. The minimum layout
is `libdefault.so` + `libzplatform.so` (+ `libjava-activity.so`).

## 3. Package not found / download screen

`Core.pakh`/`Core.pakd000` or `content/data.zpak` is missing. Extract them from
your legal `data.zpak`. See `docs/INSTALL.md` §3.

## 4. No audio

1. On the first screen, choose **YES** ("enable sound"). Choosing NO mutes the
   game.
2. Check the Vita volume.
3. Check the log: `AudioTrack: sceAudioOut aberto`, `[at] writes=...`.
4. The save may have `sound_volume`/`music_volume` at 0; use the in-game
   options or a fresh save.
5. The port does not change the system volume; do not change it accidentally.

## 5. Strange controls / pause

- The port maps D-pad, analog, face buttons, Start and touch.
- Pad-remapping/overlay plugins can intercept input; disable them while
  testing.
- In the pause menu: Cross confirms; Circle closes; in submenus, Circle goes
  back.
- If input does not respond, check whether the game is transitioning screens;
  wait a moment.

## 6. Save does not load / is corrupted

- Back up `ux0:data/brucelee/save/`.
- Do not edit the save while the game is running.
- If you changed versions, restore the backup.

## 7. Old LiveArea art cached

If the LiveArea artwork does not update after installing a new VPK, reboot the
console or refresh the LiveArea (the Vita caches icons/backgrounds).

## 8. Performance

- The game runs at ~60 FPS on the Vita.
- Optional: PSVshell for overclocking (not bundled).
- Close other apps/heavy plugins.

## 9. Reporting a bug

Open an issue with:

- port version (Release/commit);
- game version (1.15.26) and ABI (`armeabi`);
- CFW/console;
- steps to reproduce;
- the relevant part of `ux0:data/brucelee/logs/brucelee_NNN.log`.

**Do not** attach the APK, `.so` files, packages, saves, or download links.
Issues containing protected material will be closed. See `LEGAL.md`.

## 10. ~110-second "freeze"

The display turned off. Turn it on and continue; during development, use
no-sleep/keep-awake. It is not necessarily a crash.
