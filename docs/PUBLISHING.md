# Publishing and repository protection

> How this port is published safely, what goes into the repository, and how to
> prevent third parties from pushing/committing to `main`.

---

## 1. What other ports do

Pattern observed in public Android → Vita ports:

- **Rinnegatamante/32secs_vita**: repository with loader, LiveArea and README;
  no game `.so`/assets. The README tells users to obtain the APK legally and
  extract `libgame.so`/assets to `ux0:data/...`. The VPK is published in the
  Releases tab.
- **Rinnegatamante/hrm-vita**: same model; `.gitignore` only excludes `build`
  and the local data folder; the release includes a port `datafiles.zip` (not
  the game) plus the VPK.
- **TheFloW/so_util**: the original loader used by many ports.

Conclusion: **port code + documentation + loader VPK** can be public; **game
data cannot**. The README must state that users need a legal copy.

This project goes further for safety:

- The release LiveArea uses **neutral artwork** (text/gradient), with no
  portrait, logo, icon or screenshots from the game.
- The repository does not include internal reverse-engineering notes (EULA,
  offsets, Bruce Lee case study); they are in `.gitignore`.
- The release VPK contains `eboot.bin`, `sce_sys/` and `provenance.txt`, but
  **no** `.so`, `Core.pakh`, `Core.pakd000`, `data.zpak` or saves.
- The personal LiveArea generator is published, but the copyrighted generated
  art is git-ignored and must not be distributed.

---

## 2. Legal checklist before publishing

- [ ] `git ls-files` contains no `.so`, `.apk`, `.obb`, `.pakh`, `.pakd*`,
      `.zpak`, save, log or game-derived art.
- [ ] `extras/livearea/source_icon.jpg` is ignored.
- [ ] `extras/livearea_personal/` and `extras/loading_personal.rgb565` are
      ignored.
- [ ] `build/` and `data/` are ignored.
- [ ] The release VPK was generated with `tools/make_neutral_livearea.py`.
- [ ] `unzip -l build/brucelee.vpk` shows no game data.
- [ ] `README.md`/`LEGAL.md` state that users need a legal copy.
- [ ] `AUTHORS.md` keeps original and third-party credits.
- [ ] The release does not attach the APK/data/saves.
- [ ] A takedown channel (GitHub Issues) is described in `LEGAL.md`.

---

## 3. Create the public repository

### 3.1 Manual (GitHub web)

1. Create a **public** repository named `brucelee-vita`.
2. Do not initialize with a README/.gitignore (the local repository already
   has them).
3. In **Settings → Collaborators**, add nobody.
4. In **Settings → Features**, keep Issues on; disable Wiki, Projects and
   Discussions (optional).
5. In **Settings → General**, disable auto-merge; allow only squash merge;
   enable "Automatically delete head branches".
6. In **Settings → Actions → General**, use **read-only** permissions and
   disable "Allow GitHub Actions to create and approve pull requests".
7. In **Settings → Branches/Rulesets**, protect `main` (§4).

### 3.2 GitHub CLI

```bash
gh repo create brucelee-vita --public --source=. --remote=origin --push
```

Then apply protections with `tools/setup_github_protection.ps1` (§5).

---

## 4. Protecting `main` (the important part)

Goal: **nobody except you can push/commit directly to `main`**; any external
contribution goes through a pull request and your review.

Branch protection/ruleset for `main`:

- [x] Require a pull request before merging.
- [x] Require approvals: **1**.
- [x] Dismiss stale pull request approvals when new commits are pushed.
- [x] Require conversation resolution before merging.
- [x] Require linear history.
- [x] Do not allow force pushes.
- [x] Do not allow deletions.
- [x] **No collaborators.** On a personal repository, GitHub does not allow
      user/team "restrictions"; the protection against third parties is: no
      collaborators + required PR/approval. If you ever add a collaborator,
      they can only merge through a pull request approved by you.
- [x] Allow the owner/admin to bypass (so you are not locked out). If you want
      even your own changes to require a PR, disable the bypass — but then you
      need another reviewer to approve.
- [ ] Require signed commits (optional; requires GPG/SSH).
- [ ] Require deployments/status checks (there is no mandatory CI).

Tag protection for `v*` (ruleset):

- [x] Restrict deletions/updates/non-fast-forward for `v*` tags.
- [x] Only the owner can create/update releases.

Extra settings:

- No collaborators.
- Issues open for bugs (no game attachments).
- PRs from forks are welcome, but **only you merge**.
- Forks: keep them enabled if you want PRs; they cannot push to your
  repository. To close contributions entirely, disable "Allow forking".
- Wiki/Projects/Discussions off to reduce surface.
- No write-capable `GITHUB_TOKEN` workflows; disable untrusted Actions.

> The branch protection API requires repository administration permission. If
> the token does not have it, use the UI; the configuration is the same.

---

## 5. Protection script

`tools/setup_github_protection.ps1` applies:

- repository settings (issues, merge, auto-delete, signoff);
- `main` branch protection (required PR, 1 approval, linear history, no
  force/deletion, owner admin bypass; no user restrictions);
- read-only Actions permissions and no automatic PR approval;
- a `v*` tag ruleset.

Usage (never commit the token):

```powershell
$env:GITHUB_TOKEN = "ghp_..."   # token with repository administration
powershell -ExecutionPolicy Bypass -File tools\setup_github_protection.ps1 `
  -Owner lferrete -Repo brucelee-vita -Branch main
```

The script never prints the token. If the API returns 403, use the GitHub UI
with the checklist in §4.

---

## 6. Release

1. Generate neutral artwork:
   `py -3 tools/make_neutral_livearea.py`
2. Build and package:
   `bash make.sh` and `powershell -File tools/make_vpk.ps1`.
3. Verify the VPK:
   ```bash
   unzip -l build/brucelee.vpk
   # must not list .so/.pakh/.pakd/.zpak/.apk/save
   ```
4. Create the `v1.0` tag and the GitHub release.
5. Attach **only**:
   - `brucelee.vpk`
   - `SHA256SUMS.txt`
6. In the release body, include:
   - unofficial port notice;
   - requirements and a link to `docs/INSTALL.md`;
   - controls;
   - credits;
   - SHA-256 hash;
   - "game data not included; use your own legal copy".
7. Do not attach game screenshots or data. The neutral LiveArea art is already
   inside the VPK.

---

## 7. Takedown

If a rights holder complains:

1. Do not argue; remove/restrict the contested material as soon as possible.
2. If it is an asset/artwork, replace it with neutral art.
3. If it is the port itself, archive the repository/release and notify.
4. Record the decision in the issue/commit.

The public channel is the repository's **Issues** tab (see `LEGAL.md`).

---

## 8. What stays out of the public repository

`.gitignore` excludes:

- `data/`, `build/`, `.so`, `.apk`, `.obb`, `.pakh`, `.pakd*`, `.zpak`,
  saves/logs;
- `extras/livearea/source_icon.jpg`, `extras/livearea_personal/`,
  `extras/loading_personal.rgb565`;
- `PORTING_PLAN.md`, `README VITAGL.md`;
- `docs/ANDROID_TO_VITA_PORTING.md`, `docs/AUDIO_PIPELINE.md`,
  `docs/CASE_STUDY_BRUCE_LEE.md`, `docs/CHECKLISTS.md`,
  `docs/DEBUGGING_VITA.md`, `docs/INPUT_AND_HUD.md`,
  `docs/VITAMCP_WORKFLOW.md`;
- `.opencode/`, `tools/sync_port_skill.ps1`.

Those files remain on your machine for development but are not published. If
you ever want to publish technical notes, sanitize EULA/offsets and secrets
first.
