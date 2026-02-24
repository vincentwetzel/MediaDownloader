# Project TODOs

This file tracks pending tasks, planned features, and known issues for the MediaDownloader project.
Agents should check this file before starting work and update it as tasks are completed or new ones are identified.

## High Priority
- [x] Show per-download thumbnail previews in Active Downloads by wiring worker thumbnail events to the UI and prefetching thumbnails during metadata fetch.
- [x] Apply center-square thumbnail cropping to Active Downloads previews for audio-only items so displayed artwork matches audio embed conversion behavior.
- [x] Preserve playlist order in audio tags by propagating `playlist_index` and writing per-file `track`/`tracknumber` metadata after download completion.
- [x] Fix OPUS playlist track tagging failures when files include embedded cover-art streams (fallback to audio-only Ogg remux for track metadata write).
- [x] Zero-pad single-digit playlist track metadata values (`01`..`09`) for consistent player ordering.
- [x] Prefix audio playlist output filenames with zero-padded playlist indices (`NN - `) during final move.
- [x] Prevent Active Downloads title cleanup from truncating dotted movement names (e.g., `I. Molto allegro...`) when displaying metadata titles.
- [ ] ...

## Medium Priority
- [ ] ...

## Low Priority
- [ ] ...

## Future Ideas / Wishlist
- [ ] ...
