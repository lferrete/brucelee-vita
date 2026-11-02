# Instalação — Bruce Lee: Dragon Warrior (PS Vita)

> Guia para **jogadores**. O VPK contém apenas o loader; os dados do jogo vêm
> da sua cópia legal do APK Android. Veja `LEGAL.md`.

---

## 1. O que você precisa

- PS Vita ou PSTV com CFW (HENkaku/Enso).
- [kubridge](https://github.com/TheOfficialFloW/kubridge/releases) (plugin de
  kernel).
- [FdFix](https://github.com/TheOfficialFloW/FdFix/releases) (recomendado; não
  use junto com rePatch).
- `libshacccg.suprx` em `ur0:data/` (ShaRKBR33D ou
  [guia de extração](https://samilops2.gitbook.io/vita-troubleshooting-guide/shader-compiler/extract-libshacccg.suprx)).
- Cópia legal do APK de **Bruce Lee: Dragon Warrior 1.15.26** e do
  `data.zpak` do jogo.
- ~220 MB livres em `ux0:`.
- VPK da aba **Releases** deste repositório.

## 2. Plugins

Copie `kubridge.skprx` e `fd_fix.skprx` para `ux0:tai/` e edite
`ux0:tai/config.txt`:

```
*KERNEL
ux0:tai/kubridge.skprx
ux0:tai/fd_fix.skprx
```

Reinicie o console. Instale o `libshacccg.suprx` em `ur0:data/`.

## 3. Preparar os dados do jogo

### 3.1 Layout final

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

### 3.2 De onde vem cada arquivo

Do **APK** (é um ZIP; abra com 7-Zip/WinRAR ou renomeie para `.zip`):

- `lib/armeabi/libdefault.so`
- `lib/armeabi/libzplatform.so`
- `lib/armeabi/libjava-activity.so`
- `assets/BruceLee.zpak`

Do **`data.zpak`** (pacote de conteúdo do jogo):

- `Core.pakh`
- `Core.pakd000`
- `metadata.xml` do container → `content/metadata.xml`

O `metadata.xml` da **raiz** e o `game/metadata.xml` devem ser o **manifesto
de aplicação** (com `<platforms><platform type="android">`), extraído de
`assets/BruceLee.zpak` (entry `metadata.xml`).

Copie também o `data.zpak` original para `content/data.zpak` (o ZSystem pode
abri-lo como ZIP).

### 3.3 Automático no Windows

Na raiz do projeto:

```powershell
powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1
```

O script procura o APK e o `data.zpak` na pasta pai; para caminhos explícitos:

```powershell
powershell -ExecutionPolicy Bypass -File tools\prepare_data.ps1 `
  -Apk "C:\caminho\jogo.apk" `
  -Zpak "C:\caminho\data.zpak"
```

Ele gera `data/` no projeto. Copie **o conteúdo** de `data/` para
`ux0:data/brucelee/`.

> O script também extrai `res/drawable/icon.jpg` para uso pessoal na LiveArea.
> Esse arquivo e a arte derivada dele **não** devem ser commitados; o release
> usa arte neutra.

### 3.4 Manual (Linux/macOS/outro PC)

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

(Ajuste os caminhos; o APK é um ZIP.)

## 4. Instalar o VPK

1. Copie o VPK para o Vita (VitaShell/FTP).
2. Instale pelo VitaShell.
3. Confirme que `ux0:data/brucelee/` está no layout acima.
4. Abra o jogo.
5. Na tela “enable sound”, escolha **YES**.

O VPK não inclui os dados do jogo; se faltar algo, o loader mostra um diálogo
ou grava o erro em `ux0:data/brucelee/logs/`.

## 5. Controles

| Vita | Ação |
|---|---|
| D-pad / analógico esquerdo | mover |
| Quadrado / Triângulo | soco |
| Cross / Circle | chute |
| Start | pausar |
| Cross | confirmar / entrar |
| Circle | voltar / fechar pause |
| Tela touch | HUD e menus |

No pause: Cross confirma; Circle fecha o pause; nos submenus, Circle volta.
Select não é necessário.

## 6. Áudio

- Escolha **YES** em “enable sound”.
- O jogo guarda `sound_volume`/`music_volume` no save.
- O port não muda o volume do sistema; ajuste no Vita se necessário.
- Se estiver mudo, veja `docs/TROUBLESHOOTING.md`.

## 7. Save

- Fica em `ux0:data/brucelee/save/`.
- Faça backup antes de trocar de versão.
- Não edite o save com o jogo aberto.

## 8. Desinstalar

1. Apague o app pela LiveArea/VitaShell.
2. Se quiser remover os dados/save, apague `ux0:data/brucelee/`.

## 9. Privacidade e legalidade

- O port não tem telemetria.
- Não redistribua o APK, as `.so`, os pacotes, os saves ou a arte do jogo.
- Você precisa de uma cópia legal. Veja `LEGAL.md`.
