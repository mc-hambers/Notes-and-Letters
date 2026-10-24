# Validation — Notes and Letters 1.0.0

Automated checks cover stable save schema 2, old pending-pocket recipient recovery, one-copy/idempotent migration, preserving text/signature/model/IDs, excluding player letters/bundles/hate drafts, regular-vs-repeat-combat chances, old-save decoding, timers, gifts of existing notes, duplicate prevention, native allocation and book identity. Binary/package checks validate x64 exports/version, all 896 BOOK records, stable IDs, inert helper records, exact package payloads and archive integrity.

## Required Skyrim acceptance (unrun)

1. Load a matching ESS/SKSE save with queued NPC notes, both with the old addon absent and left installed. Confirm each original note moves once to the correct NPC; reloading/repeated scans must not create another copy or change collected text/signatures/models.
2. Check unloaded, dead and disabled recipients, already-delivered copies, stolen/missing queued copies and deleted/unresolvable recipients. Never regenerate a missing physical copy.
3. Generate ordinary NPC notes and use NLSpawnNote/NLRepeatNotes. Confirm direct inventories without external event requests or NPC-courier movement.
4. Remove the obsolete event ESP/PEX, retaining NotesAndLetters.esp and the matched co-save. Confirm update compatibility and original player-letter behavior.
5. Run NLDebug then NLDeliverPlayerNote (or NLDeliverNote), without any qualifying relationship/investment/home. Confirm one test player letter, native courier arrival and dialogue, no crosshair requirement and no lasting bypass. Repeat during an existing player delivery; do not reset or teleport that active courier.
6. Test failed quest startup, indoors/outdoors, travel, paused console, player combat, normal later spouse/friend/hate/investment/home letters and reading/transfer/save persistence.

All live inventory, VM, alias, courier pathing and dialogue assertions require Skyrim. Pure tests and compilation cannot prove them.
