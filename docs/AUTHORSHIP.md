# Autoria, atribuição e verificação

> Como identificar que este port foi feito por **Leandro Ferrete**, como
> provar a autoria do código do port e como publicá-lo sem se apropriar do
> jogo original.

---

## 1. O que você pode reivindicar

Você pode reivindicar **o código original do port**: loader, integração
Android/ZSystem, shims de input/áudio, ferramentas, scripts de build e
documentação. Você **não** pode reivindicar o jogo, os `.so`, os assets, os
saves ou as bibliotecas de terceiros. A LiveArea permanece sem crédito visual
(por escolha); a atribuição fica no código, logs e metadados.

A forma correta é:

```
Bruce Lee: Dragon Warrior — PS Vita port
Port code Copyright (C) 2026 Leandro Ferrete (MIT).
Original game © Digital Legends / Sony Ericsson / Indiagames.
Third-party libraries retain their own copyrights (see AUTHORS.md/LICENSE).
```

---

## 2. Onde a autoria aparece

### 2.1 No código-fonte

- Cabeçalhos em `source/port_author.h`, `source/main.c`, `source/loader.c`,
  `source/loader.h`, `source/java_shim.c`, `source/java_shim.h`.
- `CMakeLists.txt`, `make.sh`, `build.sh`, `tools/make_vpk.ps1` e
  `tools/make_livearea.py` com o aviso de copyright do port.
- `AUTHORS.md` com a divisão entre port, jogo original e terceiros.
- `LICENSE` com a linha de copyright do port (somada aos autores originais).
- `README.md` com a seção “Autor do port”.

### 2.2 No build/execução

- Banner no log:

  ```
  Port by Leandro Ferrete (v1.0)
  Copyright (C) 2026 Leandro Ferrete
  Build: <data> <hora>
  ```

  Presente em todo `ux0:data/brucelee/logs/brucelee_NNN.log`.

- LiveArea: **sem crédito visual** (a arte é derivada do ícone do jogo e não
  leva texto de autoria).

- SFO (`sce_sys/param.sfo`):
  - `TITLE` = nome do jogo;
  - `PORT_AUTHOR` = `Leandro Ferrete`;
  - `PORT_VERSION` = `1.0`;
  - `TITLE_ID` = `BRUCELEE1`.

- VPK:
  - `sce_sys/provenance.txt` com autor, versão, aviso legal e instrução de
    que o VPK não contém o jogo;
  - `build/SHA256SUMS.txt` com o hash do VPK.

### 2.3 Na release

- Nome do arquivo de release, por exemplo:
  `brucelee-dragon-warrior-vita-leandroferrete-v1.0.vpk`.
- Página de release (GitHub/itch.io) com:
  - seu nome;
  - créditos do jogo original e das libs;
  - hash SHA-256;
  - screenshots/vídeo;
  - data/versão.
- Tags/commits assinados (ver §4).

---

## 3. Como verificar rapidamente

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

### 3.3 SFO / proveniência

Além do `PORT_AUTHOR`, o SFO contém `PORT_VERSION=1.0`. O arquivo
`sce_sys/provenance.txt` dentro do VPK repete autor, versão, aviso legal e a
declaração de que o VPK não contém o jogo.

### 3.4 Hash

```bash
sha256sum brucelee.vpk
cat build/SHA256SUMS.txt
```

O hash só prova que o arquivo é o mesmo que você publicou; não prova autoria
por si só. Combine com assinatura (GPG) e histórico git.

---

## 4. Versionamento e assinatura

O repositório oficial é:

```
https://github.com/lferrete/brucelee-vita
```

A branch `main` é protegida: o owner mantém o bypass de administrador; qualquer
outro colaborador (se um dia houver) só entra via pull request aprovado. O
commit inicial e a tag usam o e-mail noreply do GitHub para não expor o e-mail
de trabalho:

```bash
git config user.name "Leandro Ferrete"
git config user.email "147426055+lferrete@users.noreply.github.com"
git commit -m "Bruce Lee: Dragon Warrior - PS Vita port by Leandro Ferrete"
git tag -a v1.0 -m "Port by Leandro Ferrete"
```

Para contribuir sem permissão de push:

```bash
git clone https://github.com/lferrete/brucelee-vita.git
# fork no GitHub, branch, commit, pull request
```

Opcional (recomendado para release):

```bash
gpg --full-generate-key          # se ainda não tiver chave
git config user.signingkey <KEY_ID>
git tag -s v1.0 -m "Port by Leandro Ferrete"
git verify-tag v1.0
```

Publique o repositório e a release; o commit/tag assinado e o hash do VPK
formam uma prova razoável de que aquela build saiu de você.

> Não use e-mail/identidade de outra pessoa. Se não quiser expor e-mail,
> use o e-mail noreply do seu provedor (ex.: GitHub `users.noreply`).

---

## 5. O que não fazer

- Não coloque seu copyright no jogo, nos `.so`, nos assets ou nos saves.
- Não redistribua o APK, os `.so`, os pacotes ou a arte original.
- Não remova os créditos de so_util/FalsoJNI/vitaGL/kubridge.
- Não altere a licença MIT dos arquivos derivados para uma licença mais
  restritiva; adicione sua linha de copyright e mantenha os avisos originais.

---

## 6. Checklist de autoria

- [ ] `AUTHORS.md` com port/jogo/terceiros.
- [ ] `LICENSE` com copyright do port + autores originais.
- [ ] `README.md` com “Autor do port”.
- [ ] Cabeçalhos em `main.c`, `loader.c`, `java_shim.c`.
- [ ] Banner de log com nome + data.
- [ ] LiveArea **sem** crédito visual (escolha do autor).
- [ ] SFO `PORT_AUTHOR`/`PORT_VERSION`.
- [ ] `sce_sys/provenance.txt` dentro do VPK.
- [ ] `build/SHA256SUMS.txt`.
- [ ] Release publicada com créditos e hash.
- [ ] (Opcional) git commit/tag assinado com GPG.
