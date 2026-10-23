# Notes and Letters

Skyrim SE/AE mod — source version **1.0.0-dev**.

## Version 1.0.0

Normal generation is half as likely: NPC chances are now 0.5–1.5%, spouse letters 2.5%, jealousy/hate 0.5%, friendship 1%, and investment letters 10%. Courier bundles have a 50% chance per existing refresh; new house decisions are 42.5% one letter, 5% two, and 52.5% none. Existing notes, timers, category balance and save schema are preserved.

Removed superseded courier instructions and corrected current integration references. Inactive compatibility records remain for older saves. Build, core checks, binary/package validation pass; in-game acceptance remains untested.

## Complete source

[Download all corresponding source and build files](Notes-and-Letters-source.zip). The archive includes the mod implementation, configuration, scripts, dependency source, build instructions, and original license notices. Native mod code is also browsable in [src/](src/).

## Precompiled CommonLib

[Download the matching precompiled CommonLib](CommonLibSSE.lib.xz). This is a development dependency; do not install it into Skyrim.

Extract the complete source archive, then run:

```text
python Restore-CommonLib.py <path-to-extracted-source-folder>
```

Keep [Restore-CommonLib.py](Restore-CommonLib.py) beside CommonLibSSE.lib.xz. The helper decompresses the library to the correct project path and verifies its SHA-256. Python 3.11 or later is required. Follow the original project build instructions for the MSVC compiler, SDK, and Papyrus tools. CMake projects normally compile CommonLib from the included source; the precompiled library is additionally provided for development.

## License

Copyright (C) 2026 Maxmc.

The original mod code is free software: you may redistribute it and/or modify it under the GNU General Public License, version 3 or (at your option) any later version.

This program is distributed WITHOUT ANY WARRANTY, including the implied warranties of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See [LICENSE.txt](LICENSE.txt) for the full terms. Third-party code retains its original copyrights, license terms, and applicable exceptions, preserved in the complete source archive.

---

# Notes and Letters — 1.0.0 development build

An SKSE mod adding small pieces of correspondence to Skyrim. This is an initial development build. Compilation and automated tests have passed; the courier scenes, book rendering, and gameplay triggers still require in-game acceptance testing.

## Requirements and installation

- Skyrim Special Edition or Anniversary Edition, with matching SKSE64 and Address Library for SKSE Plugins.
- Install this archive through your mod manager and enable `NotesAndLetters.esp`. It is ESL-flagged and uses compact local IDs. Keep its filename unchanged.
- Optional and recommended: Courier Delivers to NPCs by Pickysaurus, version 1.0.4 (`WICourierRadiant.esp`), for visible deliveries to supported NPCs: https://www.nexusmods.com/skyrimspecialedition/mods/42928
- Without that optional integration, NPC letters use direct inventory placement. Player letters use Skyrim's vanilla courier.
- Dragonborn supports the Severin Manor letters. Shadowfoot Sanctum must be installed for its house letters; its absence does not block other features.
- No SPID, MCM, or additional dynamic-form framework is used. The DLL supports SE 1.5.97 and the AE runtime versions listed in its runtime guard, including the installed 1.7.104 runtime. Other storefronts/runtimes need separate validation.

No game files or existing companion mods are modified by the build/package tools. Install only when ready to test.

## Authored content

224 original notes have distinct titles: 160 NPC notes, and 64 player letters. The proofreading draft contains their complete text and stable review IDs. `content/notes.json` is the authoritative editable catalog. Run `scripts/build_content.py` and rebuild the DLL after editing it; JSON is build-time source, not a live configuration file. Keep existing note order and IDs stable when updating an existing save.

NPC categories have 20 notes each: romance, condolences, congratulations, gratitude, encouragement, business, welcome, and personal notes/lists. Personal notes and lists never have signatures. Lists contain vanilla item names. Other NPC notes receive varied initials when created; repeated signatures are acceptable. Existing copies keep their chosen text and signature.

Player categories contain 10 spouse letters, 5 jealousy letters, 5 investment letters, 15 friendship letters, 15 hate letters, and 14 house welcomes. The seven houses each have two distinct letters.

## NPC distribution

Living adult NPCs of the ten playable races, their vampire variants, and the elder race are eligible. Children, summons, creatures, dead actors, and the courier are excluded. Active followers remain eligible.

Eligible actor references are discovered without loading their cells. Loaded NPCs and small batches of off-cell references receive an initial roll. Their next refresh is selected once, 3–5 game days later, and checked during loaded or off-cell processing. Loading repeatedly, opening inventory, and reloading a save do not restart a saved timer or grant extra rolls. Missed intervals do not accumulate.

One allowed category is chosen, then a single chance roll is made. The baseline is 1%; occupation modifiers use 0.5% or 1.5%. These are not eight independent rolls. Thus actual overall odds vary modestly by occupation. At most one pocket note from this system is outstanding per NPC.

At refresh, a note still in that NPC's inventory has a 50% chance of being removed. A retained note prevents another roll. If there is no note afterward, the NPC can receive one new chance roll. Taken notes remain intact. Inventory resets do not bypass the saved refresh timer.

Supported recipients receive queued notes through the optional `NotesAndLetters-CourierEvent.esp` and `NL_NPCNoteDelivery.pex`, which attach an event handler to Courier Delivers to NPCs 1.0.4. Enable the patch after its dependency. The event routes the exact queued copy and reports success/failure; pending copies survive failed attempts. Without the optional event handler, notes use direct inventory placement. Existing courier scripts are not replaced, and active external delivery scenes are not redirected. Daytime, recipient availability and vanilla player deliveries still constrain actual arrival.

## Occupation modifiers

Roles are assigned from vanilla factions, with conflicts resolved in this order: bandit, guard, beggar, noble, bard, innkeeper, smith, merchant, farmer, hunter, healer/priest, worker, scholar, ordinary. Unmatched NPCs use ordinary odds. Custom faction/class mappings are not yet configurable.

- Romance: beggars and bandits 0.5%; nobles and bards 1.5%; other eligible roles 1%.
- Condolences: beggars and bandits 0.5%; ordinary townspeople, guards, and farmers 1.5%; others 1%.
- Congratulations: beggars, bandits, and guards 0.5%; nobles and bards 1.5%; others 1%.
- Gratitude: beggars and bandits 0.5%; innkeepers, healers, and merchants 1.5%; others 1%.
- Encouragement: beggars and bandits 0.5%; healers, guards, and workers 1.5%; others 1%.
- Business: merchants, smiths, and innkeepers 1.5%; farmers and workers 1%; other roles excluded.
- Welcome: innkeepers and farmers 1.5%; other settled roles 1%; bandits and hunters excluded.
- Personal notes/lists: merchants, scholars, hunters, and workers 1.5%; others 1%.

## Player letters and initial timing defaults

These conservative defaults fill the probabilities that were not explicitly specified:

- Spouse: 2.5% every 3–5 days. Any married spouse qualifies, using the vanilla marriage quest. Signed with their first name.
- Jealousy: 0.5% every 14–21 days. The player must be married; the sender must be a different NPC at Lover relationship rank. Signed with the sender's initial.
- Friendship: 1% every 5–7 days per encountered named friend or better. Signed with a name or initial.
- Hatred: 0.5% every 7–14 days per encountered named Rival or Foe. Every letter begins with the player's name. It is placed in the sender's inventory for 2–3 days before that exact copy is moved to the vanilla courier's delivery container. Theft, removal, destruction, or the sender's death cancels its delivery. An outstanding draft or player delivery prevents another hate draft from that sender.
- Investment: one 10% roll 2–3 days after a new investment observed by this mod. The vanilla investment statistic and current dialogue merchant identify the event. Older investments are not reconstructed. Merchant name and shop context are captured for the letter; unidentified events are logged and skipped rather than assigned to a guessed merchant.
- House welcomes: one lifetime roll per owned supported home: 42.5% one letter, 5% both unique letters, 52.5% none. A named NPC whose editor location places them in the settlement is selected from loaded residents. If no resident is available, the decision waits. Existing owned houses are included on first activation. Letters are signed with the resident's name or initial.

Spouse checks also run when the spouse is away. Other relationship chances are checked when the sender is encountered. Investment delays and staged hate letters continue independently of the sender's loaded state. Courier arrival still follows vanilla delivery conditions.

## Courier's inventory

The courier has a 50% chance to receive a separate 2–4 note bundle on first activation and every 3–5 days. Only bundle copies still carried by the courier are replaced. Stolen bundle notes remain yours. Pending recipient letters are tracked separately and are not removed by bundle refreshes, so a courier can carry more than four notes when deliveries are waiting.

## Saving and updates

The normal save stores inventories and the SKSE co-save stores note metadata, sender/owner references, signatures, timers, unique-ID changes, house decisions, and delivery state. Load-order FormIDs are remapped. Reloading an older save restores its earlier state. Keep the `.skse` co-save with its matching save file. Missing or corrupt existing records disable distribution rather than silently regenerating items.

Individual books use static light-plugin records and saved extra data; no new dynamic BOOK forms are created. The book-menu hook supplies each saved copy's personalized text. Collected notes therefore require the DLL and their co-save for personalized signatures and addresses. Removing the mod is not an uninstall-supported operation.

## Build and validation

Run `scripts/build_content.py` with Python, then `scripts/build.ps1` with the configured MSVC/vcpkg toolchain. The tool paths can be supplied explicitly. `scripts/package.py` validates the built DLL, records, behavior tests, ZIP integrity, and packaged bytes, then creates an install archive and corresponding source archive.

See `VALIDATION.md` for remaining in-game checks. Logs are written to the Skyrim SKSE log folder as `NotesAndLetters.log`.


## Version 1.0.0

Normal note-generation chances are halved across NPC pocket notes, player relationship letters, investments, house welcomes and courier bundles. Fractional percentages remain exact; timers, category balance, signatures, existing notes, queued deliveries and saved house decisions are preserved. Note cleanup retains its existing 50% removal chance.

The documentation now describes the current event-only courier adapter rather than superseded scene/alias/AI implementations. Two inactive compatibility records and save-migration fields are intentionally retained so old saves do not lose their referenced forms. No companion mod is changed.

## Debug commands

`NLDebug` enables session debug output. `NLSpawnNote` forces a valid targeted NPC note, `NLRepeatNotes` repeatedly exercises NPC distribution, and `NLDeliverNote` tests the optional event route. Debug forcing bypasses normal generation chances. Child/dead/creature exclusions and duplicate protection remain. Debug repeat state resets on load/restart.

## Validation limits

Build, automated core tests and package validation are separate from Skyrim runtime acceptance. Courier arrival, reading personalized notes, update/save behavior and rare-event frequencies still require in-game testing. See VALIDATION.md. Keep the matching SKSE co-save; uninstalling this mod from an existing save is not supported.
