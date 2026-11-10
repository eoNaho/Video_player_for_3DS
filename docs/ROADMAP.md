# Roadmap do fork eoNaho

Base: Core-2-Extreme/Video_player_for_3DS v1.8.0 (GPLv3).
Alvo de desempenho: **Old 3DS primeiro**. Nada de blur/transparência em tempo real;
segundo plano (scan, probe, miniaturas) cede CPU ao player.

## Decisões já tomadas

| Tema | Decisão |
| --- | --- |
| Identidade | `UNIQUE_ID = 0xEC600`, `PRODUCT_CODE = CTR-Vid`, pasta `/3ds/Video_player/` |
| Versão | `2.0.0` (`APP_VER = 2000`) |
| Atualizador | Aponta para `github.com/eoNaho/Video_player_for_3DS` |
| Telemetria | Desligada (`DEF_SEM_ENABLE_SEND_APP_INFO = false`) |
| Câmera / mic / encoder A/V / muxer | Desligados no build |
| Visual padrão | Lista compacta ou grade 2×4; miniaturas sidecar limitadas a 8 visíveis |
| Miniaturas | Sidecar (`<nome>.jpg`, `poster.jpg`, `folder.jpg`) + geração no PC via FTP; extração no console só depois de medir |

## Fases

- **Fase 0 — Fundação (concluída no código):** updater próprio, binários do upstream removidos,
  identidade no `Makefile`/SMDH, telemetria desligada, flags de recursos não usados,
  template de baseline em [`BASELINE.md`](BASELINE.md) (medição ainda pendente no console).
- **Fase 1 — Biblioteca MVP:** núcleo portável concluído (`lib_index`, `lib_name`,
  `lib_queue`, `lib_scan`, `lib_sort`, `lib_view`, `lib_migrate`, `lib_thumb`, `lib_ui`
  + testes no PC), com adaptadores 3DS de varredura (`lib_scan_3ds`), armazenamento
   (`lib_store_3ds`) e migração de `saved_pos` (`lib_migrate_3ds`). Probe, integração
   com `video_player.c`, retomada persistente, abas e tela 3DS leve já estão implementados;
   falta validar o alvo no Old 3DS e no New 3DS.
- **Fase 2 — Metadados e miniaturas:** probe, etiquetas de codec/resolução/tracks,
  miniaturas sidecar preguiçosas, grade 2×4, busca por título e atalhos de ordenação
  estão implementados no código. Ainda falta documentar/fornecer o fluxo de geração
  das sidecars no PC e transferi-las por FTP, além da validação no console.
- **Fase 3 — Player e configurações:** as telas de configurações existentes já têm
  páginas por categoria. Tokens visuais compartilhados, botões da biblioteca com
  áreas de toque de pelo menos 24 px e catálogos pt-BR para configurações, player,
  menu e FTP estão implementados.
- **Fase 4 — Otimização guiada por medição:** há uma primeira redução de trabalho
  ocioso: a biblioteca dorme por 250 ms entre tarefas, telas pausadas só redesenham
  quando há mudanças e o perfil de energia oferece Auto/Economia/Desempenho, com
  limite menor de CPU em Economia e em bateria baixa no modo Auto. O Old 3DS ainda
  precisa de medições de consumo, fluidez, RAM e tempos em [`BASELINE.md`](BASELINE.md);
  não há ganho de bateria quantificado.
- **Fase 5 — Extras:** a fila visível aceita itens da biblioteca e permite remover,
  limpar e escolher modo de reprodução. A seleção de episódios cria uma sequência
  automática por série, temporada e episódio e avança ao próximo item ao terminar;
  filas alteradas manualmente mantêm a ordem escolhida. Busca e favoritos também
  estão disponíveis.

## Integração

Novos módulos em `source/library/` (adicionar ao `SOURCES`):
`lib_index.c`, `lib_scan.c`, `lib_scan_3ds.c`, `lib_sort.c`, `lib_view.c`,
`lib_migrate.c`, `lib_migrate_3ds.c`, `lib_store_3ds.c`, `lib_thumb.c`,
`lib_ui.c`, `lib_ui_3ds.c`, `lib_library_3ds.c`, `lib_probe.c`. A lógica pura (índice, hash, parser de nomes, fila,
ordenação, varredura/diff, abas, migração, navegação) deve compilar no PC com
`gcc` para testes unitários.

Pontos de contato em `source/video_player.c`: `Vid_expl_callback`,
`DECODE_THREAD_PLAY_NEXT_REQUEST`, `Vid_hid`, chamada de `Util_expl_draw()`,
leitura/gravação de `saved_pos/` e `vid_settings.txt` (V7 aditiva).

## Regras

1. Explorador atual continua como fallback.
2. Configurações antigas continuam legíveis.
3. A ponte com a decode thread deve permanecer limitada a pausa cooperativa,
   metadados, retomada e gravação de progresso; o algoritmo interno de decode não muda.
4. Toda otimização registra número antes/depois em `BASELINE.md`.
5. Fork permanece GPLv3 com créditos ao projeto original.

## Testes no PC

O núcleo da biblioteca (`source/library/`) não depende do 3DS e roda no PC:

```sh
make -C tests run
```

Inclui busca de título sem diferenciar caixa ASCII/acentos latinos e navegação em
grade 2-colunas. Também cobre hash de id, parser de título/temporada/episódio/ano, migração do nome de
`saved_pos`, formato binário do índice (round-trip, checksum, versão), diff de
varredura (renome/carência), ordenação natural, abas da biblioteca, navegação
da tela, posições legadas e nomes de miniatura, além dos modos da fila
(incluindo baralho sem repetição).
