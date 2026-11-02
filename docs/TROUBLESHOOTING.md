# Solução de problemas

> Antes de tudo: o VPK **não** contém o jogo. Se faltar `.so`/pacote, o loader
> não tem como adivinhar. Confira o layout em `docs/INSTALL.md`.

---

## 1. O jogo não abre / tela preta / volta para a LiveArea

1. Confira **kubridge** e **FdFix** no `ux0:tai/config.txt` (`*KERNEL`).
2. Confira `ur0:data/libshacccg.suprx`.
3. Confira `ux0:data/brucelee/lib/armeabi/libdefault.so` e `libzplatform.so`.
4. Confira `assets/BruceLee.zpak`, `content/Core.pakh`,
   `content/Core.pakd000`, `content/data.zpak`.
5. Veja a última linha de `ux0:data/brucelee/logs/brucelee_NNN.log`.

Erros comuns no log:

- `missing shared object`: falta uma `.so`.
- `Unresolved import`: build/loader inconsistente; use o VPK oficial.
- `EnumeratePackages` vazio: pacotes/paths errados.
- `CreateZdkDisplay`/shader: falta `libshacccg.suprx` ou cache.

## 2. “Missing shared object”

Copie a `.so` que falta para `ux0:data/brucelee/lib/armeabi/`. O layout mínimo
é `libdefault.so` + `libzplatform.so` (+ `libjava-activity.so`).

## 3. Não encontra o pacote / tela de download

Faltam `Core.pakh`/`Core.pakd000` ou `content/data.zpak`. Extraia do
`data.zpak` da sua cópia legal. Veja `docs/INSTALL.md` §3.

## 4. Sem áudio

1. Na primeira tela, escolha **YES** (“enable sound”). Se escolheu NO, o jogo
   fica mudo.
2. Confira o volume do Vita.
3. Veja o log: `AudioTrack: sceAudioOut aberto`, `[at] writes=...`.
4. O save pode ter `sound_volume`/`music_volume` em 0; abra as opções do jogo
   ou use um save novo.
5. O port não altera o volume do sistema; não mexa nele sem querer.

## 5. Controles estranhos / pause

- O port mapeia D-pad, analógico, face buttons, Start e touch.
- Plugins de remapeamento/overlay podem interceptar o pad; desative-os para
  testar.
- No pause: Cross confirma; Circle fecha; nos submenus, Circle volta.
- Se o input não responde, veja se o jogo está em transição de tela; aguarde.

## 6. Save não carrega / corrompeu

- Faça backup de `ux0:data/brucelee/save/`.
- Não edite o save com o jogo aberto.
- Se trocou de versão, volte o backup.

## 7. LiveArea antiga em cache

Se a arte da LiveArea não atualizar após instalar um VPK novo, reinicie o
console ou atualize a LiveArea (o Vita cacheia ícones/fundos).

## 8. Performance

- O jogo roda ~60 FPS no Vita.
- Opcional: PSVshell para overclock (não incluso).
- Feche outros apps/plugins pesados.

## 9. Como reportar um bug

Abra uma issue com:

- versão do port (Release/commit);
- versão do jogo (1.15.26) e ABI (`armeabi`);
- CFW/console;
- passos para reproduzir;
- trecho de `ux0:data/brucelee/logs/brucelee_NNN.log`.

**Não** anexe APK, `.so`, pacotes, saves ou links de download. Issues com
material protegido serão fechadas. Veja `LEGAL.md`.

## 10. “Freeze” de ~110 segundos

A tela desligou. Ligue a tela e continue; durante o desenvolvimento, use
no-sleep/keep-awake. Não é necessariamente um crash.
