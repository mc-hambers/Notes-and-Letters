# Notes and Letters 1.1.1

- Fix signature tracking when Skyrim changes a note instance ID during inventory insertion. Apply pending ID changes before verification and identify an unambiguous newly inserted instance when necessary.
- Apply pending ID changes before rendering a note.
- Recover an untracked courier/player-held note when opened without adding another item. Recovery assigns a new saved signature; personal lists remain unsigned.
- Preserve existing tracked text/signatures, save schema 2, runtime filenames and all BOOK record identities.

Validation: native build, Windows loader, package checks and 67,311 behavior assertions passed. In-game signature display remains unverified. Not installed automatically.
