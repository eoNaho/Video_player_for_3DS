# Product

<!-- impeccable:product-schema 1 -->

## Platform

Nintendo 3DS homebrew application (native C)

## Users

Owners of Nintendo 3DS systems who watch locally stored videos and use the console's buttons or touch screen to browse and control playback.

## Product Purpose

Play local video and audio files on a Nintendo 3DS, with a library for finding titles, resuming playback, and organizing a queue.

## Positioning

The player combines local media playback with an indexed, searchable video library and persistent playback progress on the 3DS.

## Operating Context

The app runs on the 3DS's top and bottom displays. The bottom display is 320 by 240 pixels and is operated with physical buttons and touch. Media is stored locally, commonly on the SD card. Users switch between watching a video, browsing files or the library, and changing playback or system settings.

## Capabilities and Constraints

- Preserve the existing player, file explorer, library, search, filters, sorting, favorites, continue and recent views, queue, audio and subtitle track selection, and settings.
- Keep a direct action for centering the video without changing its zoom, and make it easy to find during playback.
- Let viewers switch between fitting the full frame and filling the screen with a centered crop.
- Library metadata, thumbnails, queue behavior, and saved playback positions are tied to local media.
- The display area and available resources are limited, especially on Old 3DS hardware; interface rendering uses the project's native 3DS drawing code.

## Brand Commitments

Product name: Video Player for 3DS. The project is a homebrew application for Nintendo 3DS systems.

## Evidence on Hand

- `README.md` documents local media playback, video/audio/subtitle support, controls, and supported file formats.
- `docs/ROADMAP.md` documents the library, queue, resume, sorting, filters, and Old 3DS performance priorities.
- The source implements playback, local file browsing, library, and settings screens.

## Product Principles

- Keep existing playback and library capabilities available.
- Make it easy to browse and resume local media on the handheld.
- Keep common actions reachable with the console's buttons and touch screen.
- Respect the display and performance limits of 3DS hardware.

## Accessibility & Inclusion

The user asked for text that is easier to read. Prioritize clear contrast, a stronger text hierarchy, and readable text sizing within the small bottom display.
