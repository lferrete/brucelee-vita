# Compilando o port

> Guia para desenvolvedores. O repositório não inclui dados do jogo.

---

## 1. Requisitos

- **VitaSDK** compilado com softfp (`arm-vita-eabi`).
- **MSYS2** com `make`, `cmake`, `bash`.
- Bibliotecas vendorizadas em `lib/`:
  - `vitaGL` (com softfp)
  - `vitaShaRK`/`SceShaccCg`
  - `kubridge`
  - `FalsoJNI`
  - `so_util`
  - `fios`, `sha1`, `zlib`
- Python 3 + Pillow para a arte neutra da LiveArea (opcional; os PNGs já estão
  no repositório).
- PowerShell no Windows para `tools/make_vpk.ps1`.

## 2. Configurar e compilar

```bash
cd brucelee-vita
bash build.sh
bash make.sh
```

`build.sh` configura o CMake em `build/`; `make.sh` compila e gera
`build/eboot.bin`/SELF.

### Caminho com espaços

O macro `vita_create_vpk` do CMake pode quebrar com espaços no caminho. O
projeto usa `tools/make_vpk.ps1` (PowerShell) para empacotar com quoting
nativo.

## 3. Arte da LiveArea

O release usa **arte neutra**, sem assets do jogo:

```bash
py -3 tools/make_neutral_livearea.py
```

Gera:

```
extras/livearea/icon0.png    128x128
extras/livearea/pic0.png     960x544
extras/livearea/bg0.png      840x500
extras/livearea/startup.png  280x158
```

Esses PNGs são originais (gradiente + texto) e podem ser commitados.

O script `tools/make_livearea.py` é **pessoal**: ele usa o ícone do seu APK
para gerar uma LiveArea com a arte do jogo. Não faça commit do
`source_icon.jpg` nem da arte gerada por ele; o `.gitignore` já os exclui.

## 4. VPK e hash

```powershell
powershell -ExecutionPolicy Bypass -File tools\make_vpk.ps1
```

Gera:

```
build/brucelee.vpk
build/SHA256SUMS.txt
```

O VPK contém `eboot.bin`, `sce_sys/` (LiveArea neutra, SFO) e
`sce_sys/provenance.txt`. Ele **não** contém `.so`, `Core.pakh`,
`Core.pakd000`, `data.zpak`, saves ou APK.

## 5. Dados locais para testar

Use sua cópia legal:

```powershell
powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1 `
  -Apk "C:\caminho\jogo.apk" -Zpak "C:\caminho\data.zpak"
```

O resultado vai para `data/` (ignorado pelo git). Copie para
`ux0:data/brucelee/` no Vita.

## 6. Teste mínimo antes de release

- [ ] Boot limpo, sem dados antigos.
- [ ] Menus, opções, save/load.
- [ ] Luta, pause, retomar.
- [ ] D-pad + analógico + face buttons + touch.
- [ ] Áudio (música/SFX), YES na tela de som.
- [ ] 30+ min sem crash.
- [ ] VPK sem dados do jogo (`unzip -l`).
- [ ] `SHA256SUMS.txt` atualizado.

## 7. Estrutura

```
source/          loader, shims JNI/input/áudio, EGL/I/O/pthread
lib/             dependências vendorizadas
tools/           prepare_data, make_neutral_livearea, make_vpk
extras/livearea  arte neutra + template.xml
docs/            documentação pública
```

As notas internas de engenharia reversa (offsets, EULA, caso Bruce Lee) **não
fazem parte do repositório público** e estão no `.gitignore`.
