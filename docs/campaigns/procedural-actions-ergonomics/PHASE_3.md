# Phase 3 — managed playback

**Entry.** Phases 1–2 gates recorded on the current bytes. Read DESIGN §7, §11.4, AUDIT § Adversarial review (all six items are requirements here). Read `Action/CkProceduralAction_Utils.cpp` in full, `ActionPlayer/CkProceduralActionPlayer_Processor.h/.cpp`, `_Observe.cpp`, `_Service.cpp:533-700`, `Core/CkProceduralActionReceiptStore.cpp:395-680`, `Source/CkEcs/Public/CkEcs/Scheduler/CkProcessorGroups.h`, and `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkProceduralAnimation/Action/Test_ProceduralAction_Manual.cpp` + `CkTest_ProceduralActionReentry.h` (the reentry delegate-target pattern to mimic).

## Steps

1. **Tests first (red)** in `Test_ProceduralAction_Managed.cpp` (world fixture as `Test_ProceduralAction_Clip.cpp`), one row per VALIDATION M1–M8:
   - M1 `Ck.ProceduralAnimation.Action.Managed.NoCallbackBeforePlayReturns`: `Play_Loaded` with a clip whose bindings are invalid → `Start._Action` valid, state `Finished`, completion NOT fired inside the call (assert a counter is 0 after return), fired exactly once on a later tick with `Outcome Rejected`, `Delivery Complete`.
   - M1b `.InvalidScopeReturnsFailedReportWithoutCallback`.
   - M2 `.DrainsWithoutListenersAndReturnsStorageToBaseline`: play BodyPulse to terminal with only the completion bound; after completion + 2 ticks `Get_RetainedCount()` equals the pre-play count and `Get_RetainedReceiptIdentities` no longer lists the identity.
   - M3 `.RecordsArriveInOrderWithWindowsAndTerminal`: a records listener accumulates every page; sequences strictly increase with no gaps; the last record is Terminal; completion fires after the last page (ordering counter).
   - M4 `.PublicConsumeAndReadAreSealed`: `Read_Receipts` → `ManagedDelivery`, zero records; `Request_ConsumeReceipts` → `Failed_NotEnqueued`; an explicit `Create_FromLoadedClip` identity on the same player keeps `Pending/Succeeded` reads.
   - M5 `.ListenerReentrancy`: from inside a records callback, in separate sub-cases: `Request_Cancel`, `Request_Release`, destroy the scope, destroy the player, `UnbindFrom_OnManagedRecords`, and `Play_Loaded` another clip; each sub-case ends with exactly one completion per surviving recipient and no ensure.
   - M6 `.ScopeLostSuppressesDeadRecipientWhileDrainContinues`: bind completion on a UObject that is destroyed with the scope; the stream still terminates and the store returns to baseline; a second, surviving recipient receives `OwnerLost` with `Complete` delivery.
   - M6b `.PlayerLostFiresOnceFromEndPlay`.
   - M7 `.BudgetFairnessAndZeroDeltaT`: `_ManagedRecordsPerTick = 2`, three concurrent identities; each advances every pass; a zero-`DeltaT` tick does not spin or duplicate pages.
   - M8 `.LateSubscriptionHasNoReplay`: bind a records listener after N pages were delivered; it sees only later pages.
   - AS: `CkAutoTest_ProceduralAnimation_ActionManagedPublicSurface.as` (Play_Loaded the LimbNod example, bind records, assert completion `Completed` and `Complete`). BP VM: a `Play_Loaded` + completion row in `Test_ProceduralAction_BlueprintParity.cpp` (mimic its existing loaded-clip VM row).
2. **Data and store** (§11.4): enums, structs, delegates, `_ManagedRecordsPerTick` + `ValidateCapacity`, `FReceiptStore::Get_AcknowledgedThrough`.
3. **Fragment and Utils**: `FFragment_ProceduralActionPlayer_Managed` (added in `Add`); `DoCreate(..., EProceduralActionDelivery, const FCk_Delegate_ProceduralAction_OnManagedCompleted&)`; `DoCreate_Loaded` extracted from `Create_FromLoadedClip`; `Play`, `Play_Loaded`; bind/unbind/`Get_IsManagedDelivery`; sealing in `Read_Receipts` and `Request_ConsumeReceipts`. Registration happens right after a successful `Reserve*` and before `Request_CreateEntity`; if entity creation fails, `AbortUnpublished` AND remove the entry (no orphan).
4. **Processors**: `FProcessor_ProceduralActionPlayer_ManagedDispatch` (FGroup_Gameplay_Camera, `RunAfter = TDepList<FProcessor_ProceduralActionPlayer_Observe>`, excludes `FTag_DestroyEntity_Initiate`): snapshot sorted identities; rotate start by `_NextDispatchIndex`; per identity: `Get_AcknowledgedThrough` → `Read(Identity, Ack, min(remaining budget, RecordsPerInstance + 2))`; `StreamExpired` → completion `Expired` with `Result{}`; copy page; copy listener arrays; call listeners; revalidate player handle + `Has<Managed>` + `Has<Player>`; `Consume(Identity, Page.LastStreamSequence)`; if page terminal and `NOT HasMore`: completion (`Complete`, result + context from the terminal record copied before consume), mark completed, `Request_Release` on the action handle only when `ck::IsValid` and no `FTag_DestroyEntity_Initiate`, remove the entry. `FProcessor_ProceduralActionPlayer_ManagedEndPlay` (FGroup_EndPlay, `CK_IF_END_PLAY`): `PlayerLost` for every uncompleted entry, clear. Register both with `CK_REGISTER_PROCESSOR`.
5. `Get_ResultText` consumers: none yet; doc the managed route in `PROCEDURAL_ACTIONS.md` in Phase 5, not here.
6. Comment audit, then:
   ```powershell
   Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation.Action --output=Saved/Logs/ProceduralErgonomics-P3.log --project="D:/Repositories/CkRepos/Orion"
   ```

## Decision gates

- After step 1: red.
- After step 6: all M rows pass; explicit rows (`Test_ProceduralAction_Clip.cpp`, `_Manual.cpp`, `_RecordedGap.cpp`, `ReceiptStore`) keep their verdicts; the only failure is the inherited mounted red.
  - M1 fires the callback inside `Play_Loaded` → registration happened after `Finish`, or dispatch is being invoked synchronously; fix the order, never defer `Finish`.
  - M2 leaves one retained identity → the stream was never `MarkEntityRemoved` (release not requested) or acknowledgement stopped short of the terminal sequence; inspect `Get_AcknowledgedThrough` vs `LastSequence` in the test before changing code.
  - An ensure in M5's destroy-player sub-case → storage was touched after the callback without revalidation.
  - Anything else → STOP, PROGRESS blockers with the concrete sequence.

## Exit

M1–M8 + AS + BP rows pass; explicit cursor/gap rows unchanged; `rg -n 'Get_IsManagedDelivery' Plugins/CkFoundation/Script/Generated` hits after the boot; PROGRESS updated.

## Fences

- Managed records are NOT a `CK_DEFINE_SIGNAL` (AUDIT X10).
- No mirror of the cursor in the managed entry; read the store's acknowledged position.
- Never call `Create_FromLoadedClip` from `Play_Loaded`; both call `DoCreate_Loaded`.
- Never append to the managed `TMap` while iterating it; snapshot keys.
- Do not emit `OnFinished` from the dispatcher or change `CommitState`.
