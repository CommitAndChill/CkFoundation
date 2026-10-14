# Phase 2 — presets, subject queries, recovery convenience and failure context

**Entry.** Phase 1 gate recorded in PROGRESS on the current bytes. Read DESIGN §8, §11.4 (`FCk_ProceduralAction_FailureContext` only), §11.5, §11.6. Read `ActionPlayer/CkProceduralActionPlayer_Utils.cpp`, `LimbControl/CkProceduralLimbControl_Fragment_Data.h`, `LimbControl/CkProceduralLimbControl_Processor.cpp`, `Private/CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h` (`TryRecover`, `Get_Observation`), `Articulation/CkProceduralArticulation_Utils.cpp:338-370`, and every `CommitState(... Cancelling)` / `Finish(` site in `ActionPlayer/CkProceduralActionPlayer_Service.cpp` and `_Observe.cpp` (grep them; the audit lists line numbers).

## Steps

1. **Tests first (red).**
   - `Ck.ProceduralAnimation.ActionPlayer.Presets.DedicatedSpecAddsAndMatchesTuning`, `.BorrowingSpecCarriesSupportAndReturn`, `.CountOnlyBelowOneEnsures` (use the CkTests ensure-expectation helper the existing admission tests use; find it with `rg -n 'ExpectEnsure|AddExpectedError' Plugins/CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation` and mimic), `.BorrowingWithInsufficientContactsRejectsAtAdmission` (compose a walker fixture as `Test_ProceduralLimbControl_Deadline.cpp` does; CountOnly with minimum 99 → the manual action on a walking limb finishes `Rejected`).
   - `Ck.ProceduralAnimation.Articulation.Query.ControlledAndReturningMatchObservations` (controlled true while an action holds a limb, returning true during return, both false after), `.UnplantedIdsInTopologyOrder` (no-floor failed return leaves two limbs unplanted; IDs equal `Get_Limbs` order filtered), `.RecoverUnplantedEmptySetSucceeds`, `.RecoverUnplantedUsesDrainTimeSet` (enqueue, then make a third limb unplanted before the drain; all three recover or the request fails coherently with no partial claim; assert through `Get_LimbControl`).
   - `Ck.ProceduralAnimation.Action.Diagnostics.ReturnTimeoutCarriesElapsedAndLimit`, `.SamplingBudgetCarriesObservedAndBudget`, `.UnknownReasonRendersIdentifier`, `.ContextRidesTerminalRecord`.
   - AS: extend `CkAutoTest_ProceduralAnimation_ArticulationPublicSurface.as` with the three queries and `Request_RecoverUnplantedLimbs`.
2. **Presets** (§11.5): `FCk_ProceduralActionPlayer_BorrowingSpec` in `ActionPlayer/CkProceduralActionPlayer_Fragment_Data.h`; the three `Make_*` on the player Utils. Verify where support mode validity is enforced (`rg -n 'MinimumTrustedContacts' Plugins/CkFoundation/Source/CkProceduralAnimation`); if `Attach` does not reject `CountOnly` with `0`, add that rejection to the existing admission path (atomic, ensure, no partial state) — it is a validation boundary the preset relies on.
3. **Queries and recovery** (§11.5): the three getters over `FCk_ProceduralLimbControl_Control::Get_Observation` per limb in `Get_Limbs` order; `FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs` added to the `FFragment_ProceduralLimbControl_Requests` variant; `Request_RecoverUnplantedLimbs` follows the Utils boundary contract (`CK_ENSURE_IF_NOT` single shape, `Failed_NotEnqueued` inside the body); the drain gathers unplanted IDs then calls `TryRecover`; empty set → `Succeeded`.
4. **Failure context** (§11.4): the struct; `_Context` on `FFragment_ProceduralAction`, `FCk_ProceduralAction_Terminal`, `FCk_ProceduralAction_Record`; `FReceiptStore::Finish` takes the context (new overload with the struct; the old signature forwards with `{}`); populate at: `AcquisitionTimeout`, `LoadTimeout`, `InputTimeout`, `PublicationTimeout`, `SamplingBudgetExceeded` (travel vs limit, degrees vs limit), `ReceiptOverflow` (records vs `RecordsPerInstance`), `RecoveryFailed` (elapsed vs `_Return._Timeout` where the limb control exposes it; otherwise leave zeros and say so in the doc). `Get_ResultText` formats `Outcome`, `Recovery`, `Reason` and only the captured context fields that are non-zero.
5. Comment audit, then:
   ```powershell
   Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-P2.log --project="D:/Repositories/CkRepos/Orion"
   ```
   (`ProceduralAnimation` is wider than Phase 1 because `LimbControl`/`Articulation` rows are touched; expect the R48 Action verdicts plus the `ProceduralAnimation.*` rows listed red in `Plugins/CkTests/AutomationGate.json` and Mars `AutomationGate.json`.)

## Decision gates

- After step 1: red as in Phase 1.
- After step 5: every new row passes; `=== Known reds ===` shows no `NEW FAILURES` other than `MountedToolkit.TwoTabsModesAndCapture`. A new red in `Gait`/`SurfaceMotion` rows that this phase did not touch → run alone; green alone → load-sensitive, record; red alone → STOP.
- `CountOnlyBelowOneEnsures` fails because no ensure fires → the preset lacks its ensure; fix the preset, not the test.
- Anything else → STOP, PROGRESS blockers.

## Exit

New rows pass; wider pattern shows no new failures; `Request_RecoverUnplantedLimbs` appears in `Script/Generated/utils_procedural_articulation.as` after the test boot (grep); PROGRESS updated.

## Fences

- Do not modify `Request_RecoverLimbs` beyond what the new variant forces (flagged adjacent smell stays its owner's).
- Do not store the context as `FString`; FNames, `FCk_Time` and integers only.
- No new processor in this phase.
