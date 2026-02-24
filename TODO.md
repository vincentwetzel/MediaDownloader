# Project TODOs

This file tracks pending tasks, planned features, and known issues for the MediaDownloader project.
Agents should check this file before starting work and update it as tasks are completed or new ones are identified.

## High Priority
- [x] Restore OPUS artwork embedding by skipping incompatible custom ffmpeg attached-pic remux on `.opus` outputs.
- [x] Preserve OPUS artwork during playlist track tagging by switching `.opus` track writes to in-place mutagen updates (no remux).
- [x] Add rotating log files for the application logger to cap disk usage growth from long-running sessions.
- [x] Add a footer Discord icon button next to "Contact Developer" in the main window, with tooltip "Developer Discord" and browser link to the developer invite URL.
- [x] Show per-download thumbnail previews in Active Downloads by wiring worker thumbnail events to the UI and prefetching thumbnails during metadata fetch.
- [x] Apply center-square thumbnail cropping to Active Downloads previews for audio-only items so displayed artwork matches audio embed conversion behavior.
- [x] Preserve playlist order in audio tags by propagating `playlist_index` and writing per-file `track`/`tracknumber` metadata after download completion.
- [x] Fix OPUS playlist track tagging failures when files include embedded cover-art streams (fallback to audio-only Ogg remux for track metadata write).
- [x] Zero-pad single-digit playlist track metadata values (`01`..`09`) for consistent player ordering.
- [x] Prefix audio playlist output filenames with zero-padded playlist indices (`NN - `) during final move.
- [x] Prevent Active Downloads title cleanup from truncating dotted movement names (e.g., `I. Molto allegro...`) when displaying metadata titles.
- [x] Sanitize sorting subfolder token values so illegal path characters (for example `/` in album names) are replaced safely instead of splitting folders.
- [ ] ...

## Medium Priority
- [ ] ...

## Low Priority
- [ ] ...

## Future Ideas / Wishlist
- [ ] ...
