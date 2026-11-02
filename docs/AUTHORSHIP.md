# Authorship, attribution and verification

> How to identify that this port was made by **Leandro Ferrete**, how to prove
> authorship of the port code, and how to publish it without claiming the
> original game.

---

## 1. What you can claim

You can claim **the original port code**: loader, Android/ZSystem integration,
input/audio shims, tools, build scripts and documentation. You **cannot** claim
the game, its `.so` libraries, assets, saves or third-party libraries. The
public LiveArea remains free of visual credits by choice; attribution lives in
the code, logs and metadata.

The correct notice is:

```
Bruce Lee: Dragon Warrior — PS Vita port
Port code Copyright (C) 2026 Leandro Ferrete (MIT).
Original game © Digital Legends / Sony Ericsson / Indiagames.
Third-party libraries retain their own copyrights (see AUTHORS.md/LICENSE).
```

---

## 2. Where authorship appears

### 2.1 Source code

- Headers in `source/port_author.h`, `source/main.c`, `source/loader.c`,
  `source/loader.h`, `source/java_shim.c`, `source/java_shim.h`.
- `CMakeLists.txt`, `make.sh`, `build.sh`, `tools/make_vpk.ps1`,
  `tools/make_neutral_livearea.py` and `tools/make_livearea.py` carry the port
  copyright notice.
- `AUTHORS.md` separates port, original game and third parties.
- `LICENSE` adds the port copyright line to the original authors.
- `README.md` has an "Authorship" section.

### 2.2 Build / runtime

- Log banner:

  ```
  Port by Leandro Ferrete (v1.0)
  Copyright (C) 2026 Leandro Ferrete
  Build: <date> <time>
  ```

  Present in `ux0:data/brucelee/logs/brucelee_NNN.log`.

- LiveArea: **no visual credit** by choice (the public art is neutral and does
  not use game assets).

- SFO (`sce_sys/param.sfo`):
  - `TITLE` = game name;
  - `PORT_AUTHOR` = `Leandro Ferrete`;
  - `PORT_VERSION` = `1.0`;
  - `TITLE_ID` = `BRUCELEE1`.

- VPK:
  - `sce_sys/provenance.txt` with author, version, legal notice and the
    statement that the VPK does not contain the game;
  - `build/SHA256SUMS.txt` with the VPK hash.

### 2.3 Release

- Release file name, for example:
  `brucelee-vita-v1.0.vpk`.
- Release page (GitHub/itch.io) with:
  - your name;
  - original game and third-party credits;
  - SHA-256 hash;
  - date/version;
  - installation instructions.
- Signed tags/commits (see §4).

---

## 3. Quick verification

### 3.1 Log

```bash
grep -a "Port by Leandro Ferrete" brucelee_001.log
```

### 3.2 VPK

```bash
unzip -l brucelee.vpk | grep -i provenance
unzip -p brucelee.vpk sce_sys/provenance.txt
unzip -p brucelee.vpk sce_sys/param.sfo | strings | grep -i -E "PORT_AUTHOR|Leandro"
```

### 3.3 SFO / provenance

Besides `PORT_AUTHOR`, the SFO contains `PORT_VERSION=1.0`. The
`sce_sys/provenance.txt` file repeats the author, version, legal notice and the
statement that the VPK does not contain the game.

### 3.4 Hash

```bash
sha256sum brucelee.vpk
cat build/SHA256SUMS.txt
```

The hash only proves that the file is the one you published; it does not prove
authorship by itself. Combine it with signing (GPG) and git history.

---

## 4. Versioning and signing

The official repository is:

```
https://github.com/lferrete/brucelee-vita
```

The `main` branch is protected: the owner keeps the administrator bypass; any
other collaborator (if one is ever added) must go through an approved pull
request. The initial commit and tag use the GitHub noreply address to avoid
exposing a work email:

```bash
git config user.name "Leandro Ferrete"
git config user.email "147426055+lferrete@users.noreply.github.com"
git commit -m "Bruce Lee: Dragon Warrior - PS Vita port by Leandro Ferrete"
git tag -a v1.0 -m "Port by Leandro Ferrete"
```

To contribute without push permission:

```bash
git clone https://github.com/lferrete/brucelee-vita.git
# fork on GitHub, create a branch, commit and open a pull request
```

Optional (recommended for releases):

```bash
gpg --full-generate-key          # if you do not have a key yet
git config user.signingkey <KEY_ID>
git tag -s v1.0 -m "Port by Leandro Ferrete"
git verify-tag v1.0
```

Publish the repository and release; the signed commit/tag and the VPK hash form
a reasonable proof that the build came from you.

> Do not use another person's identity/email. If you do not want to expose an
> email, use your provider's noreply address (e.g. GitHub `users.noreply`).

---

## 5. What not to do

- Do not put your copyright on the game, its `.so` libraries, assets or saves.
- Do not redistribute the APK, `.so` files, packages or original artwork.
- Do not remove so_util/FalsoJNI/vitaGL/kubridge credits.
- Do not change the MIT license of derived files to a more restrictive one;
  add your copyright line and keep the original notices.

---

## 6. Authorship checklist

- [ ] `AUTHORS.md` separates port/game/third parties.
- [ ] `LICENSE` contains the port copyright plus original authors.
- [ ] `README.md` has an "Authorship" section.
- [ ] Headers in `main.c`, `loader.c`, `java_shim.c`, `port_author.h`.
- [ ] Log banner with name + date.
- [ ] LiveArea **without** visual credit (author's choice).
- [ ] SFO `PORT_AUTHOR`/`PORT_VERSION`.
- [ ] `sce_sys/provenance.txt` inside the VPK.
- [ ] `build/SHA256SUMS.txt`.
- [ ] Release published with credits and hash.
- [ ] (Optional) signed git commit/tag with GPG.
