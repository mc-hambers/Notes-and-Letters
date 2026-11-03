# Notes and Letters 1.0.1 — hitch reduction

Removes periodic whole-form-table scans and sorts from gameplay. NPC visits are deduplicated and spread over short jobs; relationship checks are throttled and normal note logs no longer flush on every entry. Slow jobs (5 ms or longer) report diagnostics in NotesAndLetters.log.

Save compatible: unchanged schema 2, serialization IDs, all 896 ESP book records, note identities, probabilities and game-day refresh timers. Existing queued NPC recovery and player courier delivery are retained. Install over the previous version with Skyrim closed; no new game or save cleaning required.

Validation: 66,902 behavior checks, native Release build, DLL loader/metadata/export checks and ZIP integrity checks passed. ESP is byte-identical to 1.0.0. Real gameplay/hitch improvement remains unverified until a comparable fresh Hitch Hunter run.

Download Notes-and-Letters-v1.0.1-dev.zip for installation; the source archive is for development.
