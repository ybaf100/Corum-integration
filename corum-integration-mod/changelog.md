# v0.2.36

- Persist End Screen capture metadata before background PNG encoding starts, so a completed local capture survives a game restart even if the post-encode main-thread callback never ran
- Encode into a temporary PNG and atomically rename it into the pending slot only after lossless PNG writing succeeds
- Recover a completed staged capture on the next submission after restarting Geometry Dash
- Recover complete v0.2.35 orphan PNGs whose image reached disk but whose pending metadata was lost during shutdown
- Refuse to silently submit a new 100% clear without evidence when a staged capture is known to have been interrupted
- Keep the v0.2.35 native-size render-target fix and background PNG compression behavior

# v0.2.35

- Fix End Screen captures rendering only in the bottom-left with a large black area by passing Cocos logical dimensions to `CCRenderTexture` and letting Cocos apply the content scale exactly once
- Preserve the real native render-target pixel dimensions in evidence metadata; no screenshot downsampling or lossy conversion was added
- Move lossless PNG compression and file I/O off the Geometry Dash render thread to reduce the End Screen capture hitch
- Prevent asynchronous encodes from letting an older repeated clear overwrite a newer pending capture for the same account and Corum map

# v0.2.34

- Keep End Screen capture on the cross-platform Cocos `CCRenderTexture` path, so Android builds do not require Android `MediaProjection` screen-capture consent
- Add an official-style multi-platform CI build definition for Windows, Android 32-bit, and Android 64-bit, then combine them into one `.geode` artifact
- Restrict `CMAKE_OSX_ARCHITECTURES` configuration to Apple targets so Android cross-compilation is not polluted by Apple-only architecture settings
- Keep the v0.2.33 local-first evidence lifecycle unchanged: clear-time PNGs stay local until Submit or Submit All

# v0.2.33

- Snapshot the clear timestamp, platform, game/mod/Geode versions, and loaded-mod list when the local End Screen PNG is captured, so later submission cannot replace clear-time metadata with a newer session
- Scope the local evidence-complete marker to the current server `ClearEvidence` sheet generation, allowing a clean spreadsheet reset to automatically enable captures again
- Include pending 100% evidence in main-menu Submit All even when the server already has the same 100% record, without recalculating its frozen score
- Replace the batch popup's global top-500 score download with an account-specific record lookup so large player lists cannot hide the current player's existing records
- Persist replacement/completion metadata before deleting pending PNG files to make local evidence state resilient to failed saves or interrupted shutdowns

# v0.2.32

- Stop all End Screen network uploads during gameplay; native lossless PNG captures are staged only in the Geode mod save directory
- Keep only the newest pending capture per Geometry Dash account and canonical Corum map, including primary/alternate-map clears
- Upload pending 100% verification data only after Submit or Submit All is pressed, then send the record request with the returned evidence ID
- Preserve pending PNGs and already-returned evidence IDs when a later upload step fails so a retry does not need another clear or duplicate image upload
- Delete the pending PNG and mark that account/map complete only after the matching record is successfully accepted; later re-clears skip capture once complete
- Keep pre-v0.2.32 records submittable without a pending capture and retain compatibility with v0.2.31 evidence IDs

# v0.2.31

- Capture the currently loaded non-internal Geode mod IDs and versions once when Corum Integration initializes
- Attach that cached loaded-mod snapshot to single record submissions, Submit All, and automatic End Screen evidence uploads
- Keep loaded-mod collection as metadata only; it does not classify records, block submissions, or perform cheat detection

# v0.2.30

- Make the single-level Submit popup compare the primary and alternate saved levels and use whichever has the higher local best
- Submit the actual source level ID and matching 100% evidence ID selected by that comparison
- Add a small `player / map` Geometry Dash-font label to the captured End Level PNG
- Keep evidence capture/upload status out of normal gameplay notifications while retaining the mod setting disclosure
- Store and backfill each record's map title immediately beside its map code with Corum Integration API 2.18

# v0.2.29

- Remove the CheatAPI dependency and all active client-side CheatAPI/integrity detection code
- Automatically capture the completed End Level screen for Corum-listed Normal Mode clears outside Test Mode
- Capture at the game's native physical pixel dimensions and upload a lossless PNG without resizing or downsampling
- Keep evidence upload separate from record registration: records remain manual Submit/Submit All actions
- Attach the latest matching 100% end-screen evidence ID to manual single and batch record submissions
- Add server-side evidence linking support so image-first and record-first races both resolve to the same record

# v0.2.28

- Remove the v0.2.27 load-time AREDL set/read/restore self-test that could run while the Windows DLL loader was still attaching the mod
- Never call CheatAPI `setCheat` or `endCheat` from Corum Integration; Corum is a read-only consumer of CheatAPI state
- Keep `isCheating(AREDL)` as the only CheatAPI verdict input at the existing low-overhead Attempt and external-save boundaries
- Restore the proven v0.2.26 runtime entry point and remove the injected `.corum` startup section from the packaged Windows DLL

# v0.2.27

- Keep CheatAPI v1.2.3+ as the required integrity dependency and use the explicit AREDL ruleset API
- Add a one-shot AREDL set/read/restore self-test at session initialization to verify the Corum-to-CheatAPI link without affecting record verdicts
- Keep record verdicts CheatAPI-only: only `isCheating(AREDL)` can mark a new record Hacked
- Keep the existing low-overhead boundary sampling model; no per-frame cheat polling or repeated mod enumeration was added
- Clarify that an AREDL-clean result from a mod that never reports to CheatAPI is a provider-coverage limitation, not a failed Corum API call

# v0.2.26

- Make CheatAPI's AREDL boolean the only integrity-verdict input end to end
- Emit only `Normal` or `Hacked` for new integrity reports; keep mod IDs and lifecycle codes as private observations
- Persist a fresh 100% attestation before Geometry Dash finishes the level-complete callback
- Keep a just-finished Attempt snapshot briefly available for delayed percentage-save callbacks
- Allow a clean 100% re-clear to replace a previous private `Hacked` verdict even when the saved best stays at 100%
- Continue checking map-screen percentage changes at the save boundary without treating an AREDL-clean external save as suspicious
- Update the client for Corum Integration API 2.16 and integrity schema v5

# v0.2.25

- Cache the IDs of loaded Geode mods once when the main menu first opens
- Exclude Geode internal components and Corum Integration itself from the cached list
- Attach the cached list to the private `Observed Mods` record field only
- Never use an observed mod ID to change a `Normal`, `Suspicious`, or `Hacked` verdict
- Keep CheatAPI AREDL as the only mod-assisted verdict input
- Accept up to 128 observed mod IDs with Corum Integration API 2.15

# v0.2.24

- Remove installed-mod scans and every QOLMod/Mega Hack manual verification path
- Treat a positive CheatAPI AREDL result as `Hacked` without a second opinion
- Query AREDL exactly at Attempt start and Attempt end, then latch the result for that Attempt
- Stop querying CheatAPI on pause, resume, practice-toggle, submission, or every frame
- Establish saved-level baselines once at startup so pre-update records remain `Trusted Legacy`
- Watch both `GJGameLevel::savePercentage` and `GameLevelManager::saveLevel` for map-screen Instant Complete changes
- Mark a new normal-mode percentage saved outside an active Attempt as `Suspicious` even when AREDL is clear
- Update the client for Corum Integration API 2.14 and integrity schema v4

# v0.2.23

- Trust every normal-mode best that already exists when this detector first sees a level
- Track new records per level and per attempt instead of taking one submission-time snapshot
- Cache installed detection providers once at startup and avoid per-frame or periodic scans
- Reset the attempt state on start or restart and latch any detected state until that attempt ends
- Sample the environment at gameplay lifecycle boundaries and store the result when Geometry Dash saves progress
- Submit only the stored record attestation, including a unique attestation ID and observed percentage
- Allow a legitimate same-percentage re-clear to replace a previous private Suspicious or Hacked verdict
- Attach a separate integrity attestation to each record in a batch submission
- Update the client for Corum Integration API 2.13 and integrity schema v3

# v0.2.22

- Check dedicated bot and auto-complete mods before calling Cheat API
- Use AREDL only when the direct suspicious-mod scan is clean
- Classify forbidden QOLMod features as `Hacked`
- Classify unresolved AREDL hits as `Suspicious`
- Replace a previous private verdict when a higher clean best is accepted
- Update the client for Corum Integration API 2.12

# v0.2.21

- Add Cheat API v1.2.3 as a required dependency
- Use the AREDL ruleset for private record-integrity metadata
- Attach the same integrity snapshot to single and batch submissions
- Update the client for Corum Integration API 2.8

# v0.2.20

- Choose the Corum rating text color from the actual rating background luminance
- Use the same `0.299R + 0.587G + 0.114B` calculation and `0.58` threshold as the website
- Show near-black text on bright rating colors and white text on dark rating colors

# v0.2.19

- Read `대체 맵 코드` as an alias of each Corum map's primary level ID
- Show Corum information and submission controls on both primary and alternate levels
- Scan both saved levels during main-menu batch review and select only the higher local best
- Submit the actual played level ID while the API stores and scores the canonical primary map
- Prevent primary and alternate clears from producing duplicate records or points
- Require Corum Integration API 2.7 for alternate-map normalization

# v0.2.18

- Send every confirmed main-menu record in one `batchRecords` HTTP request
- Replace per-map sequential uploads with one server-side batch operation
- Keep the animated Geometry Dash loading view visible while the whole batch is processed
- Parse the single response into the existing per-map success and failure result list
- Require Corum Integration API 2.6 for main-menu batch submission

# v0.2.17

- Added a Corum batch record button to the Geometry Dash main menu
- Refresh the complete Corum map catalog and score table when the batch window opens
- Scan saved local bests and include only records that meet the minimum and improve the server best
- Preview every submitted map, local percentage, awarded points, maximum points, and expected player total
- Require a final `Submit All` confirmation before sending any batch records
- Submit records sequentially and keep the popup open with live progress
- Show per-map Geometry Dash success or failure icons and error details after the batch finishes

# v0.2.16

- Resolve the Apps Script endpoint from the website endpoint manifest at startup
- Download the complete Corum map catalog only once per game session
- Show `C Integration is ready` after startup data is fully prepared
- Show an English error notification when endpoint or catalog loading fails
- Stop per-level background refreshes during the active game session
- Reload the selected map, existing record, current points, and maximum points when the submission popup opens
- Keep the submission form hidden behind a loading view until the fresh data is ready
- Changed the mod ID and generated package filename to `hwanhee1.corum_integration`

# v0.2.15

- Recalculate awarded points only when a submitted percentage improves the server best
- Preview the new score as `Updated Points` before an improved record is submitted
- Keep the existing score locked when map data changes without a best-record improvement
- Updated the client for the best-improvement scoring API 2.5 response

# v0.2.14

- Locked each map's awarded points to the player's first eligible submission
- Added account-aware map lookups so the popup shows an existing locked score
- Kept later best-record updates from changing the originally awarded points
- Updated the client for the frozen-score API 2.4 response

# v0.2.13

- Changed the score beside the Corum rank to the map's maximum 100% score
- Kept `Estimated Points` as separate current and maximum values
- Updated the client for the restored tokenless API 2.3 deployment

# v0.2.12

- Added the current record score beside the Corum rank on the level screen
- Split `Estimated Points` into current and maximum point values
- Kept both displays synchronized with the website's `corum-v1` scoring formula

# v0.2.11

- Added an estimated point preview to the record submission popup
- Matched the preview to the website's `corum-v1` scoring formula
- Changed the displayed developer name to `hwanhee1`

# v0.2.10

- Changed Corum UI text to Geometry Dash bitmap fonts
- Matched the rating card border and fill to the exact same Corum difficulty color
- Replaced the custom result symbols with Geometry Dash's native complete and delete icons

# v0.2.9

- Replaced the single-line Corum text with a color-coded rating card beside the Geometry Dash difficulty
- Added the current Corum rank above the Geometry Dash difficulty icon
- Used black rating text below 18 and white rating text from 18 upward
- Kept the submission popup open while the request is running
- Added in-popup loading, success checkmark, and failure details

# v0.2.8

- Replaced the percentage text button with a paper-plane button in the upper-left
- Added a Geometry Dash-style `Submit Record` popup with required and current percentages
- Kept the paper-plane button visible on every Corum-listed level, even below the minimum
- Disabled the popup's Submit button and highlighted the record in red when the minimum is not met

# v0.2.7

- Changed all runtime labels and notifications from Korean to English
- Changed the record button label to `Submit n%`
- Added English client messages for every server error code
- Translated the in-game About and Changelog pages

# v0.2.6

- Follow the redirect returned after an Apps Script POST request with GET
- Fixed false HTTP 405 errors after a record had already been saved

# v0.2.5

- Built the Google Apps Script `/exec` endpoint into the mod
- Fixed the inability to store the URL because of the Geometry Dash input filter
- Removed the obsolete `Corum API URL` setting

# v0.2.4

- Removed personal token issuance, copying, and input
- Automatically register the current Geometry Dash account ID and username on first submission
- Save the best record in the same request as automatic player registration
- Block submissions only when the player's `Active` value is set to `FALSE`
- Automatically detect the map sheet from the title and level ID columns

# v0.2.3

- Changed the private source-record sheet to `Records`
- Migrated unique records from the legacy `CorumClears` sheet
- Kept the legacy sheet as a backup instead of deleting it

# v0.2.2

- Fixed the `Continue` button not opening the custom-level menu
- Opened `CreatorLayer` directly instead of invoking the original menu callback again

# v0.2.1

- Added a C Integration warning before entering the custom-level menu
- Required `Continue` to proceed to custom levels
- Clarified that records are sent only when the manual submission button is pressed

# v0.2.0

- Replaced automatic clear uploads with a manual button on the left
- Added per-level minimum record support
- Submitted the current Geometry Dash username and saved best
- Allowed improved best records to update the server record
- Hid all Corum UI on levels that are not listed

# v0.1.0

- Displayed Corum difficulty and list rank
- Automatically submitted normal-mode 100% clears
- Excluded practice, test, and replay records
- Added map lookup caching and submission notifications
- Configured Windows, Android, macOS, and iOS targets
