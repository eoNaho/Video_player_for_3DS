# Interface design

## Direction: Trilho de estados

The interface uses a compact state rail to make focus, progress, and active
choices easy to scan on the Nintendo 3DS touch screen. The visual ground is
graphite, with warm white primary text, softer secondary text, and an aqua
accent. A state remains recognizable through position and line weight as well
as color.

## Foundations

- **Background:** deep graphite (`0xFF201B14`).
- **Surface:** dark graphite (`0xFF2A241D`) for resting controls and rows.
- **Raised surface:** lighter graphite (`0xFF3A332A`) for alternation and focus.
- **Accent rail:** aqua (`0xFFCFD964`) for selection, active playback, and progress.
- **Primary text:** warm white (`0xFFE7F1F5`).
- **Secondary text:** muted warm gray (`0xFFC8C4B7`), reserved for metadata.
- **Selected text:** dark charcoal (`0xFF242011`) when an accent fill is used.
- **Typography:** 14 px body, 11.5 px compact captions and controls, 15 px headings.
- **Touch targets:** at least 24 px high where the display allows it.
- In light mode, the library keeps the same layout and rails with warm white
  surfaces, dark text, and a deeper teal accent.

## State language

- A focused library item gets a narrow, solid accent rail at its leading edge.
- A pressed control uses an accent fill and dark text; a persistent active tab
  uses a clear rail along its lower edge.
- Playback progress uses a solid rail. System or informational states keep their
  existing labels and use a distinct rail treatment where space permits.
- Selected and active states retain their text labels; color is never the only
  cue.

## Layout and performance

- Keep all existing player, library, queue, explorer, search, settings, and
  playback actions available.
- Favor larger text and touch areas over showing more rows at once. Scrolling
  remains available for the full library and file lists.
- Use flat opaque fills and small, fixed line marks. Avoid blur, gradients, and
  per-frame effects that burden Old 3DS hardware.
- Place the video-centering action beside the playback time so it can be reached
  without opening settings or changing zoom.
- Keep frame sizing beside playback: **Ajustar** preserves the whole frame and
  **Preencher** crops from the center to remove bars. Centering remains a separate
  action and does not change the current zoom.
