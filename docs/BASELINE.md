# Baseline de desempenho

Preencher **no console real** para quantificar UI, indexação e as otimizações de
consumo. Os ajustes atuais ainda não têm medição comparativa; registrar os números
antes de afirmar ganho de bateria ou fixar metas.

## Ambiente

| Campo | Old 3DS | New 3DS |
| --- | --- | --- |
| Modelo exato | | |
| Versão do app | | |
| Cartão SD (classe/marca) | | |
| build (commit) | | |

## Vídeos de referência

Do README: mpeg4 400×240@30 em MKV; H.264 256×144@24; H.264 800×240 (New 3DS, HW);
um MP4 longo (teste de RAM); um MP3; um SBS 3D; um com legendas e múltiplas trilhas.

| Vídeo | fps efetivo | frames descartados | decode/frame (ms) | "Processing video" (vezes/min) | RAM livre | RAM linear livre | bateria (%/30 min) |
| --- | --- | --- | --- | --- | --- | --- | --- |
| mpeg4 400×240@30 MKV | | | | | | | |
| H.264 256×144@24 | | | | | | | |
| H.264 800×240 | | | | | | | |
| MP4 longo | | | | | | | |
| MP3 | | | | | | | |
| SBS 3D | | | | | | | |
| Legendas + multi-trilha | | | | | | | |

## Tempos de operação

| Operação | Old 3DS | New 3DS |
| --- | --- | --- |
| Abrir o app (cold boot) | | |
| Abrir arquivo | | |
| Seek (média de 10) | | |
| Abrir biblioteca (índice em cache) | | |
| Scan leve de 100 vídeos | | |
| Probe de 1 vídeo | | |

## Comparação de energia

Fazer duas sessões de 30 minutos com a mesma carga de bateria, brilho, volume e
arquivo; desligar Wi-Fi/FTP e anotar o modelo e a temperatura inicial. Comparar os
perfis Auto e Economia durante reprodução e durante uma tela pausada. Registrar a
queda percentual e qualquer diferença de fluidez, áudio ou quadros descartados.

As otimizações já presentes no código são: limite de CPU de 10% em idle/pausa,
limites de 40% (Old) / 50% (New) no perfil Economia e em bateria baixa no Auto,
redesenho sob demanda da biblioteca, espera de VBlank em tela estática e pausa de
250 ms entre tarefas ociosas da biblioteca. Esses valores descrevem a política do
código; ainda não medem consumo real nem comprovam ganho de autonomia.

## Otimizações de renderização (commit atual, medição pendente)

Mudanças no código ainda sem números no console; registrar antes/depois com a
tabela de vídeos de referência:

| Mudança | O que deveria melhorar | Antes | Depois |
| --- | --- | --- | --- |
| Player 100% dirigido a eventos (sem `render_every_frame`): redraw só em quadro novo, entrada ou watch | Bateria e engasgo fora do eco; antes o loop redesenhava as duas telas sem esperar VBlank | fps do vídeo, %/30 min | |
| Garantia de `refresh_needed` na entrada (`Vid_hid`, `Util_expl_main`) | UI responde ao toque no modo Economia (antes dependia do loop desenfreado) | lag percebido | |
| Correção: checar flag de redraw antes de limpar (`Vid_main`, biblioteca) | Regressão da iteração anterior: limpar antes de checar engolia o flag de todo input e a UI só atualizava 1×/s (timer de 1000 ms). Padrão correto: checa → limpa → desenha, igual a `sem.c`/`menu.c`/`ftp_daemon.c` | lag percebido | |
| Espera de decodificador 1 ms → 5 ms no decode thread | Menos churn de contexto no núcleo 0 (Old 3DS) | fps, bateria | |
| Convert/decode threads dormem 100 ms em pausa/idle | Bateria com vídeo pausado e no menu | %/30 min em pausa | |
| Handoff do pacote de vídeo por condição em vez de polling fixo de 5 ms | Evita que o decode thread perca até ~150 ms por segundo em vídeos de 30 fps esperando a cópia; reduz starvation do cache de pacotes | frames descartados, eventos de buffering | |
| Convert thread cede 10 ms durante BUFFERING sem imagens prontas | Libera o core 1 para o read-packet thread reabastecer o cache; evita busy-loop concorrente durante buffering | duração/frequência de buffering | |

## Metas

- Biblioteca abre sem tela de carregamento perceptível com índice em cache.
- Playback sem queda de fps atribuível a UI/indexação.
- Medir o índice de ~1000 itens e o cache atual de até 8 miniaturas sidecar; registrar
  o uso de RAM/linear e a fluidez da UI com 0, 1 e 8 imagens visíveis antes de fixar metas.
- Posição salva sobrevive a mover/renomear arquivos.

## Observações

- Medir com o painel de debug existente (CPU/GPU/RAM e gráficos de decodificação).
- Anotar se o Wi-Fi/FTP estava ativo: o README avisa que até FTP afeta o Old 3DS.
