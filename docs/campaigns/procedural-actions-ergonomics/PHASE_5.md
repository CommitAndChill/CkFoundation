# Phase 5 — runtime mirroring, examples and documentation

**Entry.** Phases 1–3 recorded (Phase 4 may be in flight on editor files; this phase touches runtime, scripts and docs only). Read DESIGN §8 (mirror, runtime expansion), §9 (examples), §11.2 helpers, `ActionPlayer/CkProceduralActionPlayer_ClipBinding.h`, `_Service.cpp:45-135` (`BindCompiledClip`), `Script/CkProceduralAnimation/Examples/CkProceduralAction_Noncombat.as`, `docs/campaigns/procedural-actions/{API,AUTHORING,GETTING_STARTED}.md`, `Source/CkProceduralAnimation/PROCEDURAL_ACTIONS.md`.

## Steps

1. **Tests first (red).**
   - `Ck.ProceduralAnimation.Action.Mirror.Runtime.ExpandsBeforeClaimAndKeepsSourceUnchanged` (loaded clip with one slot + `FCk_ProceduralAction_PlayOptions::_Mirrors`: the compiled clip bound to the action has two slots; the asset's `_Phases` are byte-identical before/after; windows appear once in receipts), `.Runtime.RejectsOccupiedDestinationMissingSourceIncompatibleBinding`, `.Runtime.RestNormalizedConvertsThroughSubjectFrames` (two mirrored limbs in a known layout; accepted endpoints are mirror images to 0.1 cm).
   - AS: `CkAutoTest_ProceduralAnimation_NoncombatManaged.as` (managed body clip through the example helper, completion observed), `..._BuilderSweep.as` (transient pose-builder sweep on a dedicated limb), `..._BorrowingSweep.as` (walking limb borrowed with `Make_BorrowingPlayerSpec`, returns; then floor removed → failed return diagnosed through `Get_ResultText` and `Get_UnplantedLimbIds`, then `Request_RecoverUnplantedLimbs` after the floor returns). Use the gym fixture composition helpers in `CkTests/Script/CkProceduralAnimation/CkProceduralAnimationGym_Fixture.as`.
2. **Runtime mirror**: `FCk_ProceduralAction_PlayOptions::_Mirrors : TArray<FCk_ProceduralAction_MirrorSpec>` (CK_PROPERTY, on PlayOptions so both soft and loaded specs carry it); expansion applied in `BindCompiledClip`'s caller before binding: compile source → expand into a copy of the compiled clip (new `ck::procedural_action::ExpandMirrors(const FCompiledClip&, const TArray<FCk_ProceduralAction_MirrorSpec>&, const FCk_Handle_ProceduralArticulation&, FCompiledClip&, TArray<FDiagnostic>&)`), using the subject's limb rest frames for `LimbRestNormalized` tracks; reject occupied destination, missing source, missing/incompatible destination binding; events untouched. Both `Create_FromClip` (service load path) and the loaded path go through it.
3. **Examples** (`Script/CkProceduralAnimation/Examples/`): extend `CkProceduralAction_Noncombat.as` with `PlayManagedGesture` (uses `Play_Loaded` + completion; no receipt loop) and add `CkProceduralAction_Builder.as` (recipe → pose key → `Build_Into` → `Play_Loaded`), `CkProceduralAction_Sweep.as` (dedicated sweep and the borrowing variant via `Make_BorrowingPlayerSpec`, with rejection and cancellation paths). C++ and Blueprint parity: the native tests from Phases 1–3 are the C++ route; add one BP VM row for `Mirror` to `Test_ProceduralAction_BlueprintParity.cpp`.
4. **Docs**: GETTING_STARTED leads with composition once, then `Play_Loaded` with a completion, then the pose builder, then explicit receipts for damage. API.md gains the builder, managed, preset, query and diagnostic sections with reason → likely cause → corrective action for every `ECk_ProceduralAction_Outcome`. AUTHORING.md marks the old one-by-one workflow as superseded by the pose workspace (Phase 4) and keeps the Advanced path. `PROCEDURAL_ACTIONS.md` and `Claude.md` reference the new namespaces (`utils_procedural_action_builder`). No campaign names in shipped docs.
5. Comment audit, then:
   ```powershell
   Set-Location "D:/Repositories/CkRepos/Orion"; ./CkAuto/UnrealToolbox.exe --build --target=Editor --test --test-pattern ProceduralAnimation --output=Saved/Logs/ProceduralErgonomics-P5.log --project="D:/Repositories/CkRepos/Orion"
   ```

## Decision gates

- Red first.
- After step 5: new rows pass; no new failures beyond the listed reds and the inherited mounted red. A `Gait`/`SurfaceMotion` red → run alone; green alone → record load-sensitive.
- `RestNormalizedConvertsThroughSubjectFrames` off by a constant → the rest frame used for conversion differs from the one `CaptureClipInitial` uses; read `_Service.cpp:328-405` and use the same source; do not tune the tolerance.
- Anything else → STOP.

## Exit

Rows pass; `rg -n 'Mars|Crawler|whip' Plugins/CkFoundation/Script Plugins/CkFoundation/docs/campaigns/procedural-actions Plugins/CkFoundation/Source/CkProceduralAnimation` returns 0 hits; PROGRESS updated.

## Fences

- Expansion never mutates the source asset or the draft; it produces a new compiled clip.
- No implicit body mirroring; no duplicated windows/markers.
- Examples use only public Utils and generated namespaces; no `Private/` includes, no Mars names.
