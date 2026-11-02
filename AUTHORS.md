# Autoria e créditos

## Port para PS Vita

**Leandro Ferrete** — adaptação do loader, integração Android/ZSystem, input
(físico, touch, HUD, D-pad), áudio (AudioTrack JNI → `sceAudioOut`, FIR
44.1→48 kHz), diagnóstico no hardware, documentação e testes.

Copyright (C) 2026 Leandro Ferrete. O código original deste port é distribuído
sob a licença MIT (veja `LICENSE`). A LiveArea não leva crédito visual; a
atribuição fica no código, no banner de log e nos metadados do VPK.

## Jogo original

**Bruce Lee: Dragon Warrior** © Digital Legends / Sony Ericsson / Indiagames.
Todos os direitos reservados. Este projeto **não** redistribui o APK, os `.so`,
os pacotes de conteúdo, as texturas, os sons ou os saves do jogo. O usuário
precisa fornecer uma cópia legal.

## Bases e bibliotecas de terceiros

- **so_util** — Andy Nguyen (TheFloW), Rinnegatamante.
- **FalsoJNI** — Volodymyr Atamanenko e contribuidores.
- **vitaGL** — Rinnegatamante e contribuidores.
- **kubridge** — TheFloW.
- **vitaShaRK / SceShaccCg / ShaRKBR33D** — Rinnegatamante e contribuidores.
- **fios / sha1 / zlib / newlib / VitaSDK** — respectivos autores.
- O loader segue o modelo de ports Android do ecossistema Vita (GTA/Asphalt
  e projetos relacionados); os créditos originais permanecem em `LICENSE` e
  nos cabeçalhos dos arquivos derivados.

## Como identificar este port

- Banner de log: `Port by Leandro Ferrete (v1.0)` + copyright em
  `ux0:data/brucelee/logs/`.
- SFO: campos `PORT_AUTHOR=Leandro Ferrete` e `PORT_VERSION=1.0`.
- VPK: `sce_sys/provenance.txt` com autor, versão e aviso legal.
- Release: `build/SHA256SUMS.txt` com o hash do VPK.
- Fonte: cabeçalhos em `source/port_author.h`, `source/main.c`,
  `source/loader.c`, `source/java_shim.c`, `CMakeLists.txt`, `make.sh`,
  `build.sh` e ferramentas, além deste arquivo e do `README.md`.

Veja `docs/AUTHORSHIP.md` para o procedimento de verificação e para a
recomendação de assinatura/versionamento (git/GPG).
