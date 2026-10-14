# Acceptance rubric and execution protocol

All rows are planned acceptance checks; none is a passing result until a log names it. Test paths use the feature namespaces in DESIGN §11.6 (`Ck.ProceduralAnimation.Action.{Builder,Progress,Mirror,Managed,Diagnostics}.*`, `Ck.ProceduralAnimation.ActionPlayer.Presets.*`, `Ck.ProceduralAnimation.Articulation.Query.*`, `Ck.ProceduralAnimation.Action.Editor.*`), never a campaign name. Preserve and rerun existing `ProceduralAnimation.Action` rows. Record each row as Verified/Failed/Blocked with the log path; all mandatory rows must pass for completion.

| ID | Observable acceptance | Phase |
|---|---|---|
| B1 | Body-only and multi-limb poses build/play through one compiler; no Slate dependency. | 1 |
| B2 | Duplicate/missing/nonfinite/invalid-space pose rejects with field context and no partial mutation. | 1 |
| B3 | Failed `Build_Into` leaves destination authored fields unchanged; a compiled clip survives source edits and GC. | 1, 3 |
| B4 | Native, transient AS, AS asset declaration and Blueprint authoring/play execute; a blocked route is recorded with the exact error, not papered over. | 1, 3, 5 |
| T1 | Old easing/spatial keys evaluate identically with default new fields, including phase boundaries; existing content hashes unchanged. | 1 |
| T2 | Custom progress is bounded/monotonic; plateaus work; reversal/duplicates/NaN/non-Linear easing reject; editor and runtime samples agree. | 1, 4 |
| T3 | A steep curve crossing multiple keys/windows keeps half-open elapsed-time event order with no duplicated events. | 1 |
| P1 | Presets produce validated existing Specs; count-only explicit; insufficient support rejects at admission; blocked return has a finite diagnosed outcome. | 2 |
| Q1 | Controlled/returning/unplanted queries match observations in topology order; empty recovery succeeds; drain-time changes resolve coherently without partial claims. | 2 |
| D1 | Known failures carry measured context (phase, limbs, elapsed/limit, observed/budget); `Get_ResultText` renders it and renders unknown reasons by identifier; no invented measurements. | 2 |
| R1 | XZ reflection uses the Y normal; positions, tangents, bends and orientations reflect correctly (`q' = (w, -S v)`); no negative scale. | 1 |
| R2 | Runtime mirror expands before claims, preserves source, rejects collisions/incompatible destinations; events remain singular; rest-normalized tracks convert through subject frames. | 5 |
| M1 | No callback before `Play`/`Play_Loaded` returns, including synchronous loaded rejection; a failed start reports Invalid with an invalid action. | 3 |
| M2 | Managed playback drains/releases with zero subscribers; retained counts return to baseline. | 3 |
| M3 | Record order includes windows and terminal; completion follows every preceding page and separates producer from delivery outcome; lost records surface as the producer outcome, never as a fabricated gap. | 3 |
| M4 | Public consume cannot act on a managed identity for as long as its stream is retained; public read stays non-consuming for every identity (the gameplay debugger depends on it); explicit identities keep existing cursor/gap behaviour. | 3 |
| M5 | A listener that cancels/releases/destroys/unbinds/plays inside a callback is safe; no fragment reference reused after callbacks; one completion per surviving recipient. | 3 |
| M6 | Scope loss suppresses dead recipients while recovery/drain continue; player loss fires `PlayerLost` once from EndPlay and destroys the lost entry's live action so nothing leaks under its scope. | 3 |
| M7 | Dispatch obeys `_ManagedRecordsPerTick` with fair progress across identities; zero `DeltaT`/empty page do not spin. | 3 |
| M8 | Late subscription sees only later pages; no retained replay, no exactly-once damage promise. | 3 |
| E1 | A fresh author chooses subject/recipe and creates pose-driven body/reach/sweep without manual arrays/slots/tracks or Advanced. | 4 |
| E2 | Whole-pose capture/update/move/delete/duplicate and grouping metadata undo/redo/save/reopen atomically, preserving IDs and unrelated keys/events. | 4 |
| E3 | Solved capture rejects stale/missing/partial evidence; valid conversion preserves declared space semantics. | 4 |
| E4 | Default Play shows production solved output; requested/recorded labels stay truthful; changed source hides stale recorded geometry. | 4 |
| E5 | Mounted picking/dragging, curve editing, focus, two tabs, PIE coexistence and close-during-load/seek work. | 4 |
| E6 | Cancelled creation leaves no assets; paired save failure identifies the failed asset; opening an old clip rewrites nothing. | 4 |
| E7 | Continuous body and sweep playback observed without capture-induced stalls; holds and sharp transitions remain authorable. | 4 |
| C1 | Crawler body motion uses the clip clock and accepted phase/window records; no parallel pose clock remains. | 6 |
| C2 | Existing crawler rows keep their baseline verdicts except the listed approved changes. | 6 |
| C3 | Dedicated and walking sweep demonstrators prove recovery and failure handling; no unimplemented enemy anatomy advertised. | 5 |
| C4 | The same pose/recipe via code and editor yields equivalent compiled data (hash equality). | 4 |
| F1 | Final focused gates, native AS-off Game build, independent review and measured cost report are complete. | 7 |

## Commands

From the Mars root, editor closed for any `--build`, per `CkAuto/.claude/skills/build-test/SKILL.md` (no `--parallel`, no `--config`, no `--generate`):

```powershell
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-Action.log --project="D:/Repositories/CkRepos/Orion"
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-All.log --project="D:/Repositories/CkRepos/Orion"
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --test-pattern CrawlerMelee --output=Saved/Logs/ProceduralErgonomics-Crawler.log --project="D:/Repositories/CkRepos/Orion"
```

Fresh discovery must show the intended rows; **zero selected is failure**, not a green gate. Execute runs sequentially; never two gates at once. Baseline for `ProceduralAnimation.Action`: R48 (132/131/1, inherited red `MountedToolkit.TwoTabsModesAndCapture`). Baseline for `CrawlerMelee`: captured at Phase 6 entry. A new unexpected failure blocks progression; attribute it per the skill's recipe before repair.

## Editor acceptance exercise

`[EDITOR-VERIFY]` means actual mounted tool observation by a human; compilation and mockups do not substitute. The executor writes the exact clicks into PROGRESS at the end of Phase 4.

1. Create Body Only subject plus Body Lean recipe. Capture Neutral, Pullback and Forward poses; change durations/progress curve and play solved preview. Record actual steps.
2. Reuse a configured dedicated limb; create Sweep, move endpoint and bend guide, capture an intermediate arc pose, mirror to an explicitly selected partner, then undo/redo and save/reopen.
3. Reuse a walking subject with explicit support policy. Borrow one eligible limb, observe trusted support and eventual return. Remove floor using the preview scenario and observe a readable failed return; restore floor and request recovery. Admission alone is not touchdown evidence.
4. Open two clips alongside PIE, edit one, verify the other remains independent, then close a loading/seeking preview. Check world/delegate/asset-root cleanup.
5. Connect the code-authored crawler clip; observe cancellation before strike, terminal accepted strike sample, miss/cooldown and target loss. Compare the same tuners before/after.

Archive source revisions, continuous recordings and logs. Screenshots establish layout only. Report outstanding environment checks as Blocked; never state completed authorship merely because APIs compile.
