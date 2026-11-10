# Baseline de desempenho

Preencher **no console real**, antes de mexer em UI ou indexação. Guardar uma cópia
dos números no commit que introduzir cada otimização.

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

| Vídeo | fps efetivo | frames descartados | decode/frame (ms) | "Processing video" (vezes/min) | RAM livre | RAM linear livre |
| --- | --- | --- | --- | --- | --- | --- |
| mpeg4 400×240@30 MKV | | | | | | |
| H.264 256×144@24 | | | | | | |
| H.264 800×240 | | | | | | |
| MP4 longo | | | | | | |
| MP3 | | | | | | |
| SBS 3D | | | | | | |
| Legendas + multi-trilha | | | | | | |

## Tempos de operação

| Operação | Old 3DS | New 3DS |
| --- | --- | --- |
| Abrir o app (cold boot) | | |
| Abrir arquivo | | |
| Seek (média de 10) | | |
| Abrir biblioteca (índice em cache) | | |
| Scan leve de 100 vídeos | | |
| Probe de 1 vídeo | | |

## Metas

- Biblioteca abre sem tela de carregamento perceptível com índice em cache.
- Playback sem queda de fps atribuível a UI/indexação.
- Índice de ~1000 itens em 200–300 KB; cache de 16 miniaturas em ~250 KB.
- Posição salva sobrevive a mover/renomear arquivos.

## Observações

- Medir com o painel de debug existente (CPU/GPU/RAM e gráficos de decodificação).
- Anotar se o Wi-Fi/FTP estava ativo: o README avisa que até FTP afeta o Old 3DS.
