# Notes and Letters 1.1.0

- Initial registered-cell and active-NPC discovery now runs at save loading. Load/unload events and due checks maintain tracking during play, including known NPCs in other cells.
- Rewrote 204 letters in plain, varied voices with occasional small spelling mistakes. The 20 personal notes remain unsigned.
- Courier inventories carry two to four verified pocket notes independently of deliveries. Removed the accidental 50% empty-bundle roll and retry failed additions.
- Verify unique inventory data after new notes are added.
- Preserve schema 2, all 896 BOOK identities and non-text fields, and existing saved letter bodies and signatures.

Validation: 67,302 automated behavior checks and native build passed. In-game performance and gameplay acceptance remain unverified.
