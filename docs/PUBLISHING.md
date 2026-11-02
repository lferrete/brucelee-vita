# Publicação e proteção do repositório

> Como este port é publicado de forma segura, o que entra no repositório e como
> impedir que terceiros façam push/commit na `main`.

---

## 1. O que os outros ports fazem

Padrão observado em ports Android → Vita (exemplos públicos):

- **Rinnegatamante/32secs_vita**: repositório com loader, LiveArea e README;
  nenhum `.so`/asset do jogo. O README manda o usuário obter o APK legalmente
  e extrair `libgame.so`/assets para `ux0:data/...`. O VPK fica na aba
  Releases.
- **Rinnegatamante/hrm-vita**: mesmo modelo; o `.gitignore` só exclui `build`
  e a pasta local de dados; o release inclui um `datafiles.zip` do port (não
  do jogo) e o VPK.
- **TheFloW/so_util**: loader original; usado como base por vários ports.

Conclusão: **código do port + documentação + VPK de loader** podem ser
públicos; **dados do jogo não**. O README deve dizer explicitamente que o
usuário precisa de uma cópia legal.

Este projeto vai um passo além por segurança:

- A LiveArea do release usa **arte neutra** (texto/gradiente), sem rosto,
  logo, ícone ou screenshots do jogo.
- O repositório não inclui notas internas de engenharia reversa (EULA,
  offsets, caso Bruce Lee); elas ficam no `.gitignore`.
- O VPK de release contém `eboot.bin`, `sce_sys/` e `provenance.txt`, mas
  **nenhum** `.so`, `Core.pakh`, `Core.pakd000`, `data.zpak` ou save.

---

## 2. Checklist legal antes de publicar

- [ ] `git ls-files` não contém `.so`, `.apk`, `.obb`, `.pakh`, `.pakd*`,
      `.zpak`, save, log ou arte derivada do jogo.
- [ ] `extras/livearea/source_icon.jpg` está ignorado.
- [ ] `build/` e `data/` estão ignorados.
- [ ] O VPK de release foi gerado com `tools/make_neutral_livearea.py`.
- [ ] `unzip -l build/brucelee.vpk` não mostra dados do jogo.
- [ ] `README.md`/`LEGAL.md` deixam claro que o usuário precisa da cópia legal.
- [ ] `AUTHORS.md` mantém os créditos originais e de terceiros.
- [ ] O release não anexa APK/dados/saves.
- [ ] Há um canal de takedown (issues do GitHub) em `LEGAL.md`.

---

## 3. Criar o repositório público

### 3.1 Manual (GitHub web)

1. Crie um repositório **público** chamado `brucelee-vita`.
2. Não inicialize com README/.gitignore (o repositório local já tem).
3. Em **Settings → Collaborators**, não adicione ninguém.
4. Em **Settings → Features**, deixe Issues ligado; desligue Wiki, Projects e
   Discussions (opcional).
5. Em **Settings → General**, desligue Auto-merge; permita apenas Squash
   merge; habilite “Automatically delete head branches”.
6. Em **Settings → Actions → General**, use permissões **read-only** e desligue
   “Allow GitHub Actions to create and approve pull requests”.
7. Em **Settings → Branches/Rulesets**, crie a proteção da `main` (§4).

### 3.2 Com GitHub CLI

```bash
gh repo create brucelee-vita --public --source=. --remote=origin --push
```

Depois aplique as proteções com `tools/setup_github_protection.ps1` (§5).

---

## 4. Proteção da `main` (o importante)

Objetivo: **ninguém além de você faz push/commit direto na `main`**; qualquer
contribuição externa passa por PR e sua revisão.

Configuração de branch protection/ruleset para `main`:

- [x] Require a pull request before merging.
- [x] Require approvals: **1**.
- [x] Dismiss stale pull request approvals when new commits are pushed.
- [x] Require conversation resolution before merging.
- [x] Require linear history.
- [x] Do not allow force pushes.
- [x] Do not allow deletions.
- [x] **Sem colaboradores** — em repositório pessoal, só o owner pode fazer
      push por padrão. O GitHub não permite “restrict users” em repo pessoal;
      a proteção contra terceiros é: nenhum collaborator + PR obrigatório com
      aprovação. Se um dia adicionar um collaborator, ele só entra via PR
      aprovado por você.
- [x] Allow the owner/admin to bypass (para você não ficar trancado fora).
      Se quiser exigir PR até de você, desligue o bypass — mas aí você precisa
      de outro revisor para aprovar.
- [ ] Require signed commits (opcional; exige GPG/SSH configurado).
- [ ] Require deployments/status checks (não há CI obrigatório).

Proteção de tags `v*` (ruleset):

- [x] Restrict deletions/updates/non-fast-forward para tags `v*`.
- [x] Só o owner pode criar/atualizar releases.

Configurações extras:

- Sem colaboradores.
- Issues abertas para bugs (sem anexos de jogo).
- PRs de forks são bem-vindos, mas **só você mergeia**.
- Forks: você pode manter habilitado para permitir PRs; eles não têm push no
  seu repositório. Se quiser fechar totalmente, desabilite “Allow forking”,
  mas aí ninguém consegue abrir PR.
- Wiki/Projects/Discussions desligados para reduzir superfície.
- Sem `GITHUB_TOKEN` com write em workflows; sem Actions não revisadas.

> A API de branch protection exige permissão de administração no repositório.
> Se o token não tiver, faça pela UI; a configuração é a mesma.

---

## 5. Script de proteção

`tools/setup_github_protection.ps1` aplica:

- configurações do repositório (issues, merge, auto-delete, signoff);
- branch protection da `main` (PR obrigatório, 1 aprovação, linear history,
  sem force/deletion, push restrito ao owner, bypass do admin);
- permissões read-only do Actions e desabilita aprovação automática de PRs.

Uso (nunca commite o token):

```powershell
$env:GITHUB_TOKEN = "ghp_..."   # token com admin do repo
powershell -ExecutionPolicy Bypass -File tools\setup_github_protection.ps1 `
  -Owner lferrete -Repo brucelee-vita -Branch main
```

O script não imprime o token. Se a API recusar (403), use a UI do GitHub com o
checklist do §4.

---

## 6. Release

1. Gere a arte neutra:
   `py -3 tools/make_neutral_livearea.py`
2. Compile e empacote:
   `bash make.sh` e `powershell -File tools/make_vpk.ps1`.
3. Verifique o VPK:
   ```bash
   unzip -l build/brucelee.vpk
   # não deve listar .so/.pakh/.pakd/.zpak/.apk/save
   ```
4. Crie a tag `v1.0` e o release no GitHub.
5. Anexe **somente**:
   - `brucelee.vpk`
   - `SHA256SUMS.txt`
6. No corpo do release, inclua:
   - aviso de port não oficial;
   - requisitos e link para `docs/INSTALL.md`;
   - controles;
   - créditos;
   - hash SHA-256;
   - “game data not included; use your own legal copy”.
7. Não anexe screenshots do jogo nem dados. A imagem neutra da LiveArea já
   está no VPK.

---

## 7. Takedown

Se um detentor de direitos reclamar:

1. Não discuta; remova/restrinja o material contestado o quanto antes.
2. Se for um asset/arte, substitua por arte neutra.
3. Se for o próprio port, arquive o repositório/release e comunique.
4. Registre a decisão no issue/commit.

O canal público é a aba **Issues** do repositório (ver `LEGAL.md`).

---

## 8. O que fica fora do repositório público

O `.gitignore` exclui:

- `data/`, `build/`, `.so`, `.apk`, `.obb`, `.pakh`, `.pakd*`, `.zpak`,
  saves/logs;
- `extras/livearea/source_icon.jpg` e a arte pessoal gerada a partir do APK;
- `PORTING_PLAN.md`, `README VITAGL.md`;
- `docs/ANDROID_TO_VITA_PORTING.md`, `docs/AUDIO_PIPELINE.md`,
  `docs/CASE_STUDY_BRUCE_LEE.md`, `docs/CHECKLISTS.md`,
  `docs/DEBUGGING_VITA.md`, `docs/INPUT_AND_HUD.md`,
  `docs/VITAMCP_WORKFLOW.md`;
- `.opencode/`, `tools/make_livearea.py`, `tools/sync_port_skill.ps1`.

Esses arquivos continuam na sua máquina para desenvolvimento, mas não vão para
o GitHub. Se um dia quiser publicar notas técnicas, sanitize EULA/offsets e
segredos antes.
