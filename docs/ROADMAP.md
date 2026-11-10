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
| Visual padrão | Lista com miniatura 48×27 |
| Miniaturas | Sidecar (`<nome>.jpg`, `poster.jpg`, `folder.jpg`) + geração no PC via FTP; extração no console só depois de medir |

## Fases

- **Fase 0 — Fundação (concluída no código):** updater próprio, binários do upstream removidos,
  identidade no `Makefile`/SMDH, telemetria desligada, flags de recursos não usados,
  template de baseline em [`BASELINE.md`](BASELINE.md) (medição ainda pendente no console).
- **Fase 1 — Biblioteca MVP:** índice binário versionado, scan leve, abas
  Todos/Continuar assistindo, fila com os 4 modos, resume por id e migração de `saved_pos/`.
- **Fase 2 — Metadados e miniaturas:** probe em segundo plano com pausa no playback,
  miniaturas sidecar, grade, etiqueta de compatibilidade.
- **Fase 3 — Player e configurações:** tokens de design, alvos de toque ≥ 24 px,
  configurações por categoria, pt-BR.
- **Fase 4 — Otimização guiada por medição:** só o que o baseline justificar.
- **Fase 5 — Extras:** busca, favoritos, próximo episódio, perfil automático completo.

## Integração

Novos módulos em `source/library/` (adicionar ao `SOURCES`):
`lib_index.c`, `lib_scan.c`, `lib_thumb.c`, `lib_queue.c`, `lib_ui.c`.
A lógica pura (índice, hash, parser de nomes, fila, ordenação) deve compilar no PC
com `gcc` para testes unitários.

Pontos de contato em `source/video_player.c`: `Vid_expl_callback`,
`DECODE_THREAD_PLAY_NEXT_REQUEST`, `Vid_hid`, chamada de `Util_expl_draw()`,
leitura/gravação de `saved_pos/` e `vid_settings.txt` (V7 aditiva).

## Regras

1. Explorador atual continua como fallback.
2. Configurações antigas continuam legíveis.
3. Nenhuma alteração na decode thread é necessária para a biblioteca.
4. Toda otimização registra número antes/depois em `BASELINE.md`.
5. Fork permanece GPLv3 com créditos ao projeto original.
