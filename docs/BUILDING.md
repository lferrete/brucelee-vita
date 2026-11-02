# Building the port

> Developer guide. The repository does not include game data.

---

## 1. Requirements

- **VitaSDK** built with softfp (`arm-vita-eabi`).
- **MSYS2** with `make`, `cmake`, `bash`.
- Vendored libraries in `lib/`:
  - `vitaGL` (softfp)
  - `vitaShaRK`/`SceShaccCg`
  - `kubridge`
  - `FalsoJNI`
  - `so_util`
  - `fios`, `sha1`, `zlib`
- Python 3 + Pillow for LiveArea art (optional; the PNGs are already in the
  repository).
- PowerShell on Windows for `tools/make_vpk.ps1`.

## 2. Configure and build

```bash
cd brucelee-vita
bash build.sh
bash make.sh
```

`build.sh` configures CMake in `build/`; `make.sh` compiles and produces
`build/eboot.bin`/SELF.

### Paths with spaces

CMake's `vita_create_vpk` macro can break on paths containing spaces. This
project uses `tools/make_vpk.ps1` (PowerShell) to pack with native Windows
quoting.

## 3. LiveArea artwork

The LiveArea art is tracked in the repository and used by default:

```
extras/livearea/icon0.png    128x128
extras/livearea/pic0.png     960x544
extras/livearea/bg0.png      840x500
extras/livearea/startup.png  280x158
extras/loading.rgb565        960x544 RGB565
```

The art is derived from the original game and remains the property of its
rights holders. The README uses a separate gameplay screenshot
(`docs/fight-screenshot.png`); it is not part of the VPK.

### Regenerating the art

`tools/make_livearea.py` uses the icon from **your own legally obtained APK**
to regenerate the LiveArea. The script reads
`extras/livearea/source_icon.jpg` and writes the tracked files above.

```powershell
py -3 tools\make_livearea.py
```

Review the image diff before committing. The VPK can also be packed with a
custom art folder/loading screen:

```powershell
powershell -ExecutionPolicy Bypass -File tools\make_vpk.ps1 `
  -ArtDir extras\livearea `
  -LoadingFile extras\loading.rgb565
```

## 4. VPK and hash

```powershell
powershell -ExecutionPolicy Bypass -File tools\make_vpk.ps1
```

Output:

```
build/brucelee.vpk
build/SHA256SUMS.txt
```

The VPK contains `eboot.bin`, `sce_sys/` (LiveArea art and SFO),
`sce_sys/provenance.txt` and `loading.rgb565`. It does **not** contain `.so`
files, `Core.pakh`, `Core.pakd000`, `data.zpak`, saves or the APK.

## 5. Local game data for testing

Use your legal copy:

```powershell
powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1 `
  -Apk "C:\path\game.apk" -Zpak "C:\path\data.zpak"
```

The result goes to `data/` (git-ignored). Copy it to `ux0:data/brucelee/` on
the Vita.

## 6. Minimum release test

- [ ] Clean boot without old data.
- [ ] Menus, options, save/load.
- [ ] Fight, pause, resume.
- [ ] D-pad + analog + face buttons + touch.
- [ ] Audio (music/SFX), YES on the sound prompt.
- [ ] 30+ min without crashes.
- [ ] VPK contains no game data (`unzip -l`).
- [ ] `SHA256SUMS.txt` updated.

## 7. Layout

```
source/          loader, JNI/input/audio shims, EGL/I/O/pthread
lib/             vendored dependencies
tools/           prepare_data, make_livearea, make_vpk
extras/livearea  LiveArea art + template.xml
docs/            public documentation
```

Internal reverse-engineering notes (offsets, EULA, Bruce Lee case study) are
**not part of the public repository** and are in `.gitignore`.
