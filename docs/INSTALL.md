# Installation — Bruce Lee: Dragon Warrior (PS Vita)

> Player guide. The VPK contains only the loader; the game data comes from
> your legally obtained Android APK. See `LEGAL.md`.

---

## 1. What you need

- PS Vita or PSTV with CFW (HENkaku/Enso).
- [kubridge](https://github.com/TheOfficialFloW/kubridge/releases) kernel
  plugin.
- [FdFix](https://github.com/TheOfficialFloW/FdFix/releases) (recommended; do
  not use it together with rePatch).
- `libshacccg.suprx` in `ur0:data/` (ShaRKBR33D or the
  [extraction guide](https://samilops2.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx)).
- A legal copy of **Bruce Lee: Dragon Warrior 1.15.26** and its `data.zpak`.
- ~220 MB free in `ux0:`.
- The VPK from the repository's **Releases**:
  <https://github.com/lferrete/brucelee-vita/releases/latest>.

## 2. Plugins

Copy `kubridge.skprx` and `fd_fix.skprx` to `ux0:tai/` and edit
`ux0:tai/config.txt`:

```
*KERNEL
ux0:tai/kubridge.skprx
ux0:tai/fd_fix.skprx
```

Reboot the console. Install `libshacccg.suprx` in `ur0:data/`.

## 3. Prepare the game data

### 3.1 Final layout

```
ux0:data/brucelee/
  lib/armeabi/libdefault.so
  lib/armeabi/libzplatform.so
  lib/armeabi/libjava-activity.so
  assets/BruceLee.zpak
  content/Core.pakh
  content/Core.pakd000
  content/data.zpak
  metadata.xml
  game/metadata.xml
  logs/
  shader_cache/
  save/
```

### 3.2 Where each file comes from

From the **APK** (a ZIP; open it with 7-Zip/WinRAR or rename it to `.zip`):

- `lib/armeabi/libdefault.so`
- `lib/armeabi/libzplatform.so`
- `lib/armeabi/libjava-activity.so`
- `assets/BruceLee.zpak`

From the game's **`data.zpak`**:

- `Core.pakh`
- `Core.pakd000`
- the container `metadata.xml` → `content/metadata.xml`

The root `metadata.xml` and `game/metadata.xml` must be the **application
manifest** (with `<platforms><platform type="android">`) from
`assets/BruceLee.zpak` (entry `metadata.xml`).

Also copy the original `data.zpak` to `content/data.zpak` (ZSystem may open it
as a ZIP).

### 3.3 Windows helper

From the project root:

```powershell
powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1
```

The script looks for the APK and `data.zpak` in the parent folder. For explicit
paths:

```powershell
powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1 `
  -Apk "C:\path\game.apk" `
  -Zpak "C:\path\data.zpak"
```

It writes `data/` in the project. Copy the **contents** of `data/` to
`ux0:data/brucelee/`.

> The script also extracts `res/drawable/icon.jpg` for optional personal
> LiveArea art. That file and the generated art must **not** be committed; the
> public release uses neutral art.

### 3.4 Manual (Linux/macOS/other)

Use `unzip`:

```bash
mkdir -p ux0data/brucelee/{lib/armeabi,assets,content,logs,shader_cache,save}
unzip game.apk 'lib/armeabi/*' 'assets/BruceLee.zpak' -d ux0data/brucelee
unzip data.zpak 'Core.pakh' 'Core.pakd000' 'metadata.xml' -d ux0data/brucelee/content
unzip -p game.apk assets/BruceLee.zpak > /tmp/brucelee.zpak
unzip -p /tmp/brucelee.zpak metadata.xml > ux0data/brucelee/metadata.xml
cp ux0data/brucelee/metadata.xml ux0data/brucelee/game/metadata.xml
cp data.zpak ux0data/brucelee/content/data.zpak
```

(Adjust paths; the APK is a ZIP.)

## 4. Install the VPK

1. Copy the VPK to the Vita (VitaShell/FTP).
2. Install it with VitaShell.
3. Confirm `ux0:data/brucelee/` matches the layout above.
4. Launch the game.
5. On the "enable sound" screen, choose **YES**.

The VPK does not include game data. If something is missing, the loader shows a
dialog or writes the error to `ux0:data/brucelee/logs/`.

## 5. Controls

| Vita | Action |
|---|---|
| D-pad / left analog stick | move |
| Square / Triangle | punch |
| Cross / Circle | kick |
| Start | pause |
| Cross | confirm / enter |
| Circle | back / close pause |
| Touch screen | HUD and menus |

In the pause menu, Cross confirms and Circle closes it; in submenus, Circle
goes back. Select is not required.

## 6. Audio

- Choose **YES** on "enable sound".
- The game stores `sound_volume`/`music_volume` in its save.
- The port does not change the system volume; adjust it on the Vita if needed.
- If muted, see `docs/TROUBLESHOOTING.md`.

## 7. Saves

- Located in `ux0:data/brucelee/save/`.
- Back up before changing versions.
- Do not edit the save while the game is running.

## 8. Uninstall

1. Delete the app from the LiveArea/VitaShell.
2. To remove data/saves, delete `ux0:data/brucelee/`.

## 9. Privacy and legal

- The port has no telemetry.
- Do not redistribute the APK, `.so` files, packages, saves or game art.
- You need a legal copy. See `LEGAL.md`.
