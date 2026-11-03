# Notes and Letters 1.0.1 validation

Native Release build and NoteBehavior suite: passed. Tests cover save serialization, legacy note recovery and deduplicated actor queue ordering, reset and repeat admission.

Save schema remains 2. ESP/catalog hashes are compared against the prior 1.0.0 release at packaging time. Plugin metadata is 1.0.1.

The global form-table traversal and sort have been removed from runtime work. Queued work is capped at four loaded actors per job with a two-ms admission budget; discovery visits one registered cell per slice. One engine call can exceed the budget, so slices at least five ms are logged. A full game restart and a comparable Hitch Hunter run are required to verify improvement. Gameplay validation is pending.
