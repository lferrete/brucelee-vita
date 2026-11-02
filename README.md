<p align="center">
  <img src="extras/livearea/pic0.png" alt="Bruce Lee: Dragon Warrior — PS Vita port" width="640">
</p>

# Bruce Lee: Dragon Warrior — PS Vita (port não oficial)

Port não oficial do jogo Android **Bruce Lee: Dragon Warrior** (Digital
Legends / Sony Ericsson / Indiagames, v1.15.26, `armeabi`) para o **PS Vita**,
usando [vitaGL](https://github.com/Rinnegatamante/vitagl) e o ecossistema
`so_util`/FalsoJNI.

> **Aviso legal:** este é um projeto de fã, não oficial e não afiliado aos
> detentores do jogo. O repositório e o VPK **não contêm** o APK, as `.so`, os
> pacotes de conteúdo, as texturas, os sons ou os saves do jogo. Você precisa
> de uma **cópia legal** do jogo Android. Leia [`LEGAL.md`](LEGAL.md) antes de
> instalar ou redistribuir.

---

## Status

- Testado com a versão Android **1.15.26** (`armeabi`).
- Renderização nativa **960×544** via vitaGL, ~60 FPS.
- Áudio (música + efeitos) via `AudioTrack` → `sceAudioOut`, com resample FIR
  44.1→48 kHz.
- Controles físicos e touch:
  - D-pad e analógico esquerdo movem o personagem.
  - Quadrado/Triângulo soco; Cross/Circle chute.
  - Start pausa.
  - Cross confirma nos menus; Circle volta/fecha o pause.
  - Tela touch do Vita funciona.
- Save/config do jogo funcionam em `ux0:data/brucelee/save/`.
- O VPK é **somente o loader**; os dados do jogo ficam fora dele.

## Requisitos

1. PS Vita/PSTV com CFW (HENkaku/Enso).
2. [kubridge](https://github.com/TheOfficialFloW/kubridge/releases) como plugin
   de kernel.
3. [FdFix](https://github.com/TheOfficialFloW/FdFix/releases) (recomendado).
4. `libshacccg.suprx` em `ur0:data/` — instale via
   [ShaRKBR33D](https://github.com/Rinnegatamante/ShaRKBR33D/releases) ou
   siga o [guia de extração](https://samilops2.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx).
5. Uma **cópia legal** do APK Android e do `data.zpak` do jogo.
6. Espaço livre em `ux0:` para os dados (~220 MB) e o VPK.

## Instalação (jogador)

### 1. Instale os plugins

Copie `kubridge.skprx` e `fd_fix.skprx` para `ux0:tai/` e adicione em
`ux0:tai/config.txt`:

```
*KERNEL
ux0:tai/kubridge.skprx
ux0:tai/fd_fix.skprx
```

> Se você usa `rePatch`, não instale o `fd_fix.skprx`.

Reinicie o console. Instale também o `libshacccg.suprx` em `ur0:data/`.

### 2. Baixe o VPK

Pegue o VPK mais recente na aba **Releases** deste repositório. Ele contém
apenas o loader e a arte neutra da LiveArea — **não** contém o jogo.

### 3. Prepare os dados do jogo

Extraia da sua cópia legal:

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
```

- Do APK: `lib/armeabi/libdefault.so`, `libzplatform.so`,
  `libjava-activity.so`, `assets/BruceLee.zpak`.
- Do `data.zpak`: `Core.pakh`, `Core.pakd000` e o `metadata.xml` do container.
- O `metadata.xml` da raiz deve ser o **manifesto de aplicação** (com
  `<platforms><platform type="android">`), extraído de
  `assets/BruceLee.zpak`.
- O `data.zpak` também deve ser copiado para `content/data.zpak`.

No Windows, o script `tools/prepare_data.ps1` faz essa extração:

```powershell
powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1
```

Ele procura o APK e o `data.zpak` na pasta pai do projeto; também aceita
`-Apk` e `-Zpak` explícitos. O resultado fica em `data/`; copie o conteúdo
para `ux0:data/brucelee/`.

> O repositório **não** inclui o APK, o `data.zpak`, as `.so`, os `.pakh/.pakd`
> nem qualquer asset do jogo. Não peça/post/e link esses arquivos em issues.

### 4. Instale e abra

Instale o VPK pelo VitaShell, abra o jogo e escolha **YES** na tela
“enable sound”. Sem YES, o jogo fica mudo.

## Controles

| Vita | Ação |
|---|---|
| D-pad / analógico esquerdo | mover o personagem |
| Quadrado / Triângulo | soco |
| Cross / Circle | chute |
| Start | pausar |
| Cross | confirmar / entrar em opções |
| Circle | voltar (menus) / fechar o pause |
| Tela touch | HUD, menus e toque original do jogo |

No pause, o Cross confirma e o Circle fecha o menu; nos submenus, o Circle
volta. O Select não é necessário para jogar.

## Áudio

- Escolha **YES** em “enable sound”.
- O volume do jogo fica no save/config (`sound_volume`, `music_volume`).
- O port não altera o volume do sistema.
- Se o áudio estiver mudo, confira o YES, o volume do Vita e o log
  (`AudioTrack`, `[at]`).

## Solução de problemas

- **Tela preta / volta para a LiveArea:** confira kubridge/FdFix, o
  `libshacccg.suprx` e se os dados estão exatamente no layout acima.
- **“Missing shared object”:** falta uma `.so` em `ux0:data/brucelee/lib/armeabi/`.
- **Não encontra pacote:** faltam `Core.pakh`/`Core.pakd000` ou
  `content/data.zpak`.
- **Sem áudio:** escolha YES; veja `ux0:data/brucelee/logs/`.
- **LiveArea antiga em cache:** reinicie o console ou atualize a LiveArea.
- **Controle estranho:** não use plugins de remapeamento que interceptem o
  pad; o port já mapeia D-pad, analógico, face buttons e touch.

Logs:

```
ux0:data/brucelee/logs/brucelee_NNN.log
```

Ao reportar um bug, envie o trecho relevante do log e a versão do jogo/console.
**Nunca** anexe o APK, as `.so`, os pacotes ou saves.

Guia detalhado: [`docs/INSTALL.md`](docs/INSTALL.md) e
[`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md).

## Compilar

Pré-requisitos: VitaSDK com softfp, MSYS2 (make/cmake) e as dependências
vendorizadas em `lib/` (vitaGL, vitaShaRK, kubridge, FalsoJNI, so_util, fios,
sha1).

```bash
cd brucelee-vita
bash build.sh     # configura o CMake
bash make.sh      # compila
```

O VPK é gerado por:

```powershell
powershell -ExecutionPolicy Bypass -File tools\make_vpk.ps1
# build/brucelee.vpk + build/SHA256SUMS.txt
```

A arte da LiveArea do release é gerada por
`tools/make_neutral_livearea.py` (sem assets do jogo). O script
`tools/make_livearea.py` é **pessoal** e usa o ícone do seu APK; não faça
commit da arte gerada por ele.

Guia completo: [`docs/BUILDING.md`](docs/BUILDING.md).

## Dados e privacidade

- `data/`, `build/`, saves, logs e arte derivada do jogo estão no
  `.gitignore`.
- O VPK de release não contém dados do jogo.
- O repositório não coleta dados; não há telemetria.

## Créditos

- **Port, loader, integração, input/áudio, documentação:** Leandro Ferrete.
- **Jogo original:** Digital Legends / Sony Ericsson / Indiagames.
- **so_util:** TheFloW (Andy Nguyen), Rinnegatamante.
- **FalsoJNI:** Volodymyr Atamanenko e contribuidores.
- **vitaGL:** Rinnegatamante e contribuidores.
- **kubridge:** TheFloW.
- **vitaShaRK/SceShaccCg:** Rinnegatamante e contribuidores.
- **VitaSDK, fios, sha1, zlib e newlib:** respectivos autores.

Detalhes em [`AUTHORS.md`](AUTHORS.md) e [`LICENSE`](LICENSE).

## Documentação

- [`docs/INSTALL.md`](docs/INSTALL.md) — instalação passo a passo.
- [`docs/BUILDING.md`](docs/BUILDING.md) — compilar e empacotar.
- [`docs/TROUBLESHOOTING.md`](docs/TROUBLESHOOTING.md) — problemas comuns.
- [`docs/AUTHORSHIP.md`](docs/AUTHORSHIP.md) — autoria e verificação.
- [`docs/PUBLISHING.md`](docs/PUBLISHING.md) — como o repositório/release é
  publicado e protegido.
- [`LEGAL.md`](LEGAL.md) — aviso legal e takedown.
- [`CONTRIBUTING.md`](CONTRIBUTING.md) — regras para contribuições.
