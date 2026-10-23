# Validation — 1.0.0 development build

The x64 DLL, native core tests and package/binary checks must pass before release. Core checks cover half-rate generation (including fractional 0.5% chances), house outcomes, category restrictions, persistence, old-save decoding, refresh timers, book variants, event callback IDs, debug forcing and duplicate prevention. Package checks cover all 896 BOOK records, stable FormIDs, inert compatibility records, optional event patch structure, binary metadata, licensing and exact archived payloads.

## Required in-game checks (not performed)

1. Update an existing save with its matching SKSE co-save; verify existing notes, signatures, paper styles, timers and house decisions survive.
2. Preview, open, transfer, drop and reload personalized notes without crashes or lost text.
3. Verify normal NPC odds are 0.5/1/1.5% per existing refresh; player spouse/jealousy/friend/hate chances are 2.5/0.5/1/0.5%; investment chance is 10%. Confirm no extra rolls from inventory or cell loading.
4. Verify courier bundle refresh has a 50% generation chance, producing 2–4 notes on success; pending and stolen copies remain protected.
5. Verify new house decisions use 42.5% one, 5% two and 52.5% no letters, with no rerolls of saved decisions.
6. Test direct placement without the optional courier patch and exact-copy event delivery with Courier Delivers to NPCs 1.0.4 plus the patch. Test busy/failure retry, vanilla delivery priority, save/load and interrupted travel.
7. Test hate draft theft/death cancellation, investment attribution, spouse/relationship eligibility, and all debug commands.

Old helper QUST/PACK records and serialized migration fields are deliberately retained, inactive, for save compatibility. The old scene/alias/AI adapter is not used. No companion mod files are edited or redistributed.
