# Corum Integration

Displays the Corum difficulty as a color-coded card beside the Geometry Dash difficulty, with the current Corum rank and maximum points for a 100% record above it. The card uses Geometry Dash text and one matching color for both its fill and border. Its text automatically switches between near-black and white using the rating background luminance.

A paper-plane button appears in the upper-left on every Corum-listed level. Before your first submission, the `Submit Record` popup shows estimated `CURRENT` and `MAX` point values using the website's `corum-v1` formula. After a record exists, its awarded score stays locked while the saved best is unchanged. When your local best is higher than the server best, the popup previews the `NEW` score that will be awarded using the current rank, minimum, and improved percentage. Rank or minimum changes alone do not change a locked score. The Submit button is disabled until your best meets the requirement.

The popup remains open during submission. It shows a spinner while sending, Geometry Dash's complete icon after success, or its delete icon with the error details after failure.

Your current Geometry Dash username and best record are sent when you submit a record. You can submit again after improving your best.

The Geometry Dash main menu also has a Corum paper-plane button for batch
submission. Opening it refreshes the full Corum catalog and this account's server records, scans
all saved local bests, and shows only records that meet the current minimum and
beat the server best. The review list shows each map's percentage, expected
award, 100% maximum, and the expected player total. Nothing is sent until
`Submit All` is pressed. Every confirmed record is then sent in one batch
request. The popup keeps its animated loading view open until the server returns
one response, which is expanded into a final per-map success or failure list.

Your current Geometry Dash account ID and username are registered automatically on the first submission. No personal token is required.

At game startup, the mod reads the current Google Apps Script endpoint from the
website endpoint manifest and downloads the complete Corum map catalog once.
`C Integration is ready` appears only after both steps finish. A startup error
notification appears if either step fails.

The startup map snapshot is kept for the rest of the game session, so opening
level screens does not trigger background refreshes. Opening the paper-plane
popup is the one exception: it first shows a loading view, then fetches that
map's latest rank, minimum, existing record, current points, and maximum points
before showing the submission form.

When a map has a different `대체 맵 코드`, Corum information and submission
work on both level IDs. The main-menu scan compares both saved levels and keeps
only the higher local best. The server normalizes either submission to the
primary map so records and points are never counted twice.

No URL input or mod rebuild is required when the Apps Script deployment changes.
Update `public/corum-endpoint.json` on the Corum website and redeploy the site.

Corum information and the paper-plane button stay hidden on levels that are not listed on Corum.

When you enter the custom-level menu from the main menu, a warning explains that C Integration is active. Press `Continue` to proceed or `Cancel` to stay on the main menu.

Submitted records are stored as `unverified`. Corum Integration v0.2.29 no longer depends on or calls CheatAPI.

At startup, v0.2.31 snapshots the IDs and versions of currently loaded non-internal Geode mods once, excluding Corum Integration itself. That cached list is attached to manual record metadata. Starting with v0.2.33, the capture timestamp, platform, game/mod/Geode versions, and loaded-mod list used by End Screen evidence are persisted when the PNG is captured, so submitting it in a later session cannot change the clear-time metadata. These fields are not used to classify records or block submissions.

When a Corum-listed level is completed in Normal Mode outside Test Mode, v0.2.32 captures the completed End Level screen at the game's native physical pixel resolution and stages one lossless PNG in the Geode mod save directory. No network upload happens at clear time. A newer clear replaces the previous pending capture for the same Geometry Dash account and canonical Corum map, including primary/alternate-map pairs.

On Android, v0.2.35 keeps this capture inside Geometry Dash's Cocos renderer using `CCRenderTexture`; it does not invoke Android `MediaProjection` or attempt to capture other apps/system UI. The mod therefore does not request Android's screen-recording consent prompt for End Screen evidence. The render target uses Cocos logical dimensions so the engine applies its content scale exactly once, while PNG compression and file I/O run off the render thread. Android32 and Android64 are built as native Geode targets and can be combined with the Windows build into one package.

Pending verification data is uploaded only after the paper-plane Submit action or main-menu Submit All action. The returned evidence ID is attached to the record request. A failed later step keeps the pending file and any already-returned ID for retry. After the record succeeds the local PNG is removed and re-clears are skipped while the server ClearEvidence sheet generation is unchanged. Recreating that sheet automatically starts a new generation. Submit All can also sync a new pending 100% capture to an already-existing 100% record without changing its frozen score. Older records with no pending capture remain submittable.
