# Phase 6 — Mars crawler body-attack migration

**Entry.** Phases 1–3 and 5 recorded. This phase edits Mars `Script/ECS/Crawler/*` and `Script/Tests/CrawlerMelee/*` only. Read in full: `Mars_Crawler_Attack.as`, `Mars_Crawler_ActionAdapter.as`, `Mars_Crawler_Processor_AttackTick.as`, `Mars_Crawler_Processor_AttackResolve.as`, `Mars_Crawler_Feature.as` (attack fragments and `FMars_Crawler_AttackTiming`/`FMars_Crawler_AttackPose`), `Mars_Crawler_Utils.as`, and the Mars project `CLAUDE.md` gameplay conventions (request doctrine, kernel/control/assembly, `Validate()` mixins). Load `ck-game-angelscript-gameplay`, `ck-game-testing-discipline`.

**Baseline.** Before any edit run the crawler lane and record it (this pattern is not covered by R48):
```powershell
Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --test-pattern CrawlerMelee --output=Saved/Logs/ProceduralErgonomics-P6-Baseline.log --project="D:/Repositories/CkRepos/Orion"
```
Expected 34 selected; record pass/fail names. Then run the same pattern after the migration.

## Approved behaviour changes (DESIGN §9, AUDIT X9)

| Old | New |
|---|---|
| `CancelSeconds` eases `From → identity` with `RecoverEasing` | Cancel is `Request_Cancel` on the clip action; the body returns through the action's dedicated return; `CancelSeconds` maps to `Tuning.Transition.DedicatedReturn` (override) |
| `PoseTimeoutSeconds` (manual input watchdog) | Not applicable to a clip; removed from the attack timing spec (keep the field only if another consumer reads it; `rg` first) |
| Offensive phases decided from the game clock | Offensive window = clip window `Offense` over `[Windup 0, Recover 1)`; `AttackResolve` reads the receipt interval's `Windows`/`PhaseId` |
| Easings `ECk_TweenEasing` (full set) | `ECk_ProceduralAction_Easing` (Linear/Quad/Cubic); `Validate()` on `FMars_Crawler_AttackPose` rejects any other value with a message naming the field |

## Steps

1. **Tests first (red)**: add `Mars_AutoTest_CrawlerMelee_ClipDrivesPhases.as` (phase changes observed only from accepted receipts' `PhaseId`; no `PhaseElapsed` advance when no receipt arrived), `..._CancelReturnsThroughAction.as` (cancel before commit → action `Cancelled`, body offset returns to identity through the return, no game-side ease), `..._OffenseWindowFromReceipt.as` (a hit outside the `Offense` window is ignored). Keep every existing crawler test; update only assertions that encode the approved changes above, each with a one-line reason in the test.
2. **Clip generation**: `utils_crawler::Make_AttackClip(const FMars_Crawler_AttackSpec&) -> UCk_ProceduralAction_Data` using `utils_procedural_action_builder` (body channel IdentityOffset; phases Windup/Strike/Recover with the easings; keys identity → Windup → Strike → identity; window `Offense`). The clip object is owned by the crawler entity script (a UPROPERTY on the script, not a fragment), rebuilt only when the spec revision changes.
3. **Adapter**: `Create_AttackAction` uses `Create_FromLoadedClip` with a tuning override (`DedicatedReturn = CancelSeconds`); `Submit_AttackOffset` and the phase clock in `AttackTick` are deleted; `AttackTick` only starts, cancels and observes state (`Get_State`), and `Enter(...)` is driven by `AttackResolve` from the accepted `PhaseId`; cooldown stays game-owned.
4. **Resolve**: unchanged receipt drain; derive phase changes and the offensive window from the interval; damage dedup and ledger unchanged.
5. Comment audit, then the crawler lane (test-only; `.as` changes):
   ```powershell
   Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --test --test-pattern CrawlerMelee --output=Saved/Logs/ProceduralErgonomics-P6.log --project="D:/Repositories/CkRepos/Orion"
   ```

## Decision gates

- Baseline captured with names before any edit; otherwise STOP.
- After step 5: every baseline-green test still green except those whose assertion was updated for an approved change (list them by name with the reason); new rows green. A newly red test not in that list → run alone; red alone → it is this migration's; fix or STOP after two attempts.
- A phase transition that cannot be expressed from accepted phase/window records → STOP with the missing observation named; do not reintroduce a game clock.

## Exit

Crawler lane green against the baseline modulo the listed approved changes; `rg -n 'PhaseElapsed' Script/ECS/Crawler` returns 0 hits; PROGRESS updated with before/after names.

## Fences

- No framework edits in this phase.
- No gameplay policy (damage, cooldown, targeting) moves into the clip or into CkFoundation.
- `Script/Generated/DynamicHandleTypes.json` is not touched (no new typed handle).
