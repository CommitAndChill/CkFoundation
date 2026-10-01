# PROGRESS — usf package root (F1) + component CPD request (F2)

## Current state

- As of: 2026-09-30, CkFoundation `dev` @ `e90e4fdbf` (clean), CkTests `dev` @ `feddf883` (clean).
- Baseline test counts + failing names: NOT YET CAPTURED (planner captures before the first build).
- Next action: executors run PHASE_1_F1 and PHASE_1_F2 in parallel; planner reviews, then builds.
- Blocked on: the Mars editor must be closed before the planner can build.

## Decision log

| Date | Decision | Why | Revisit when |
|---|---|---|---|
| 2026-09-30 | F1 as a per-definition field; F2 as a deferred request in CkUnrealComponent | See PROMPT.md locked decisions | A second host needs a different root policy |

## Blockers (executors write here instead of improvising)

| Date | Phase | What you expected | What you found (file:line) | Options you see |
|---|---|---|---|---|

## Entries (newest first; each: Ran / Confirmed / Inferred / Follow-ups)

### 2026-09-30 — review-fix executor: fixes A-G applied (not built)

**Ran.** Edits only; no build, editor, toolbox, test run or git state change. `tasklist | grep -i -E
"MarsEditor|UnrealEditor"` was empty before the `.as` edits, so the AngelScript tests were written.

- **A** `UCk_Utils_UnrealComponent_UE::Request_SetCustomPrimitiveData` takes the request BY VALUE (CkTimer
  `Request_Jump`/`Request_Consume` shape) in both the UFUNCTION and the definition; the delegate is set on that copy,
  so the caller's struct is never written.
- **B** Utils boundary: a handle carrying `ck::FTag_DestroyEntity_Initiate` completes `Failed_NotEnqueued` with a
  Verbose log and no ensure. `FProcessor_UnrealComponent_CancelPendingRequests` gains
  `RunAfter = TDepList<FProcessor_UnrealComponent_EndPlay>` (CkTimer has no EndPlay processor, so no edge to copy).
  Contract comments + `Source/CkUnrealComponent/Claude.md` table row added.
- **C** `UCk_Utils_Graphics_UE::Apply_CustomPrimitiveData(UPrimitiveComponent*, const FCk_CustomPrimitiveData&)`
  (BlueprintCallable, `[Ck] Apply Custom Primitive Data`, `Ck|Utils|Graphics`) holds the one switch; the
  UnrealComponent handler and the IsmProxy `ApplyCustomPrimitiveDataToComponent` (its own ensure kept verbatim) call
  it. `Source/CkGraphics/Claude.md` Key API + Used-by updated.
- **D** `DEFINE_TYPE_MAPPING(FVector4, "FVector4")` in `CkCore/Format/CkFormat_AngelScript.h`.
- **E** `Validate_GeneratedPackageRoot` rejects a root whose package file resolves under `FPaths::EngineDir()`; mount
  wording is now "mounted, non-read-only". CPD range check compares the index against `Last - (N - 1)` (no overflow);
  the message keeps the `past the last custom primitive data index` marker. `Source/CkUsf/Claude.md` updated.
- **F** `Get_CustomPrimitiveDataFloat` ensures (returns 0) on an index outside `[0, 36)`; contract comment updated.
- **G** C++: `DefaultRootIsUnchanged` asserts literals; `InvalidRootIsRejected` adds `/Engine/__CkUsfTest`,
  `/Game/Bad!Character`, `/Game//DoubleSlash`; the CPD range test is now
  `CkTests.UnitTests.CkUsf.LookValidator.CustomPrimitiveDataIndexOutOfRangeIsRejected` (same file). AS: `Applies`
  restores `FVector4(1,2,3,4)` at 8 and adds Vector2D@14, Vector@18, LinearColor@24, two writes to slot 30;
  `RejectsOutOfRange` restores `FVector4` at 33 and its baseline is now 28, 30 and the accepted Vector4 boundary at 32;
  new `..._RejectedWhileDestroying.as` (band 186000) and `..._SameRequestReusedCompletesOncePerCall.as` (band 187000),
  both on generated wrappers. VALIDATION.md expected-test list updated.

**Inferred (needs the gate).** Everything compiles; the `Script/Generated/utils_unreal_component.as` wrapper
regenerates with the by-value request on the test boot; the FVector4 AS constructor of `FCk_CustomPrimitiveData_Value`
now registers; `FLinearColor` maps to a valid AS type name without a mapping row (the old tests already relied on it);
`IsUnderDirectory(..., EngineDir())` catches `/Engine/...` on this install; an invalid enum in the shared switch now
completes the UnrealComponent request `Succeeded` after the `CK_INVALID_ENUM` ensure (it was `Failed`).

**Follow-ups (not done, separate framework work).** Audit the other math types missing a `DEFINE_TYPE_MAPPING` row
(e.g. `FLinearColor`, `FVector3f`, `FVector4f`, `FColor`, `FIntVector4`); `CkAngelScript_TypeValidation` silently skips a
constructor registration whose type name it rejects. `UCk_Utils_IsmProxy_UE::Request_SetCustomPrimitiveData` still takes
its request by const ref and writes the delegate into the caller's struct (same defect as A).

### 2026-09-30 — F2 executor: step 5 done (AngelScript AutoTests written)

**Ran.** `tasklist | grep -i -E "MarsEditor|UnrealEditor"` returned nothing (exit 1) before writing and again
after. No build, editor, toolbox or git state change.

Files created in `Plugins/CkTests/Script/CkUnrealComponent/` (CRLF, ASCII, one test class per file; the two
rejection files also hold their hand-authored `ACk_AutoTest_..._Actor` wrapper):

| File (`CkAutoTest_UnrealComponent_SetCustomPrimitiveData_*.as`) | Y band (X=0) | Expected-error string on the wrapper |
|---|---|---|
| `Applies` | 181000 | — (generated wrapper) |
| `AppliesAfterSetup` | 182000 | — (generated wrapper) |
| `RejectsNonPrimitive` | 183000 | `is not a live PRIMITIVE component` (handler ensure) |
| `RejectsOutOfRange` | 184000 | `does not fit the engine's` (Utils boundary ensure) |
| `CancelledOnDestroy` | 185000 | — (generated wrapper) |

Shape: step sequencer (`Add_Step` / `Add_Step_WaitUntil` / `Run_Steps`); every "exactly once" / "unchanged"
assertion sits behind a 0.1s `Add_Step_WaitSeconds` window that follows a positive assertion (rule 1).
Components: `UStaticMeshComponent` archetype with `/Engine/BasicShapes/Cube`, Movable, `NoCollision`,
`StaticWorldBakePolicy::DoNotBake` (non-primitive test: `Make_Params(USceneComponent, ...)`). Owner entity:
`utils_entity_lifetime::Request_CreateEntity(InHandle)` + `utils_transform::Add`, child of the runner entity
so the harness teardown cleans up. Component handle: the return of `utils_unreal_component::Add`, readiness
via `ck::IsValid(utils_unreal_component::Get_Component(Handle))` (AddHappyPath precedent).

**Inferred (needs the gate run).**
- AS constructor forms: `FCk_Request_UnrealComponent_SetCustomPrimitiveData(FCk_CustomPrimitiveData(4,
  FCk_CustomPrimitiveData_Value(0.75f)))` relies on the `CK_DEFINE_CONSTRUCTORS`-emitted registrations;
  `0.75f` must resolve to the `float32` overload of `FCk_CustomPrimitiveData_Value` (a CK float binds as
  `float32`, `CkMacros_AngelScript.h:54`); `FVector4(1.0, 2.0, 3.0, 4.0)` is the engine bind
  (`Angelscript/.../Binds/Bind_FVector4.cpp:39`).
- `utils_unreal_component::Request_SetCustomPrimitiveData` / `Get_CustomPrimitiveDataFloat` exist only after
  the generator regenerates `Script/Generated/utils_unreal_component.as` on the gate boot; the getter is read as
  `float32` (generated float returns are `float32`, e.g. `utils_chain.as:143`).
- Completion delegate: `FCk_Delegate_Request_OnCompleted(this, n"OnX")` with
  `UFUNCTION() void OnX(FCk_Handle InOwner, ECk_Request_OperationResult InResult)` and `InOwner == TypedHandle`
  comparison — precedent `CkChain/CkAutoTest_Chain_AttachInvalidLinkNotEnqueued.as`.

### 2026-09-30 — F2 executor: steps 1-4 and 6 done; step 5 PENDING (editor open)

**Ran.** Read-only research plus edits; no build, no editor, no git state change. Entry criteria held:
`git status --short` showed nothing under `Source/CkUnrealComponent` at start. Before step 5,
`tasklist | grep -i -E "MarsEditor|UnrealEditor"` listed `MarsEditor.exe` (PID 42936), so no `.as` file was
written.

Files touched:
- `Source/CkUnrealComponent/CkUnrealComponent.Build.cs` — `CkGraphics` added to the public deps.
- `Source/CkUnrealComponent/Public/CkUnrealComponent/CkUnrealComponent_Fragment_Data.h` — `FCk_Request_UnrealComponent_SetCustomPrimitiveData` (pre-designed shape, verbatim) after the Spec; includes `CkEcs/Request/CkRequest_Data.h`, `CkGraphics/CkGraphics_Common.h`.
- `.../CkUnrealComponent_Fragment.h` — `ck::FFragment_UnrealComponent_Requests` (`std::variant<...>` list, CkTimer shape; friends HandleRequests + Utils).
- `.../CkUnrealComponent_Processor.h/.cpp` — `FProcessor_UnrealComponent_HandleRequests` (`FGroup_PostTransform`, `RunAfter` PushTransform, `MarkedDirtyBy` = the requests fragment, view excludes `NeedsSetup`, `FTag_DestroyEntity_Initiate`, `CK_IGNORE_PENDING_KILL`) and `FProcessor_UnrealComponent_CancelPendingRequests` (`FGroup_EndPlay`, `CK_IF_END_PLAY`, `request::FireCancelledForPending`); both registered.
- `.../CkUnrealComponent_Utils.h/.cpp` — `Request_SetCustomPrimitiveData`, `Get_CustomPrimitiveDataFloat`.
- `Source/CkUnrealComponent/Claude.md` — new module doc.
- `Source/CLAUDE.md` — decision-tree row added, `(no doc yet)` removed from both mentions, tier row gains Graphics.
- Not touched: `CkGraphics_Common.h` (gate 3 needs no change, see below).

**Confirmed (from code).**
- Gate 1 (does a `MarkedDirtyBy` processor re-run once an excluded tag is removed later): YES. The main pass
  ticks every node every frame (`CkEcs/.../Scheduler/CkProcessorScheduler.cpp:238-307`); the only main-pass skip
  is the empty-view skip, which looks at the view's REQUIRED include types only, never at excludes (`:250-283`);
  `TProcessorBase::Tick` has no dirty gate (`CkEcs/.../Processor/CkProcessor.h:382-399`). `MarkedDirtyBy` only
  feeds the pump phase (`CkProcessorScheduler.cpp:517-622`). The requests fragment stays on the entity until a
  drain empties it, so the queued request is drained by the first main pass after Setup removes `NeedsSetup`.
  This is the CkTimer/IsmProxy shape and what `CkEcs/Claude.md` § "What a persistence handler may wait for"
  relies on. Storage shape chosen accordingly: CkTimer's (copy, `Reset()`, drain the copy, remove the
  fragment only if still empty) — not IsmProxy's `CopyAndRemove`.
- Gate 2: `ck::FTag_DestroyEntity_Initiate` at `CkEcs/.../Handle/CkHandle.h:34`, added synchronously and
  recursively to lifetime dependents by `Request_DestroyEntity` (`CkEcs/.../EntityLifetime/CkEntityLifetime_Utils.cpp:134-141`);
  `ck::request::FireCancelledForPending` at `CkEcs/.../Request/CkRequest_Completion.h:101-123` (handles the variant list).
- Gate 3: `FCk_CustomPrimitiveData` uses `CK_DEFINE_CONSTRUCTORS(..., _CustomDataIndex, _Value)`
  (`CkGraphics_Common.h:143`), and `CK_DEFINE_CONSTRUCTOR_2` emits `CK_ANGELSCRIPT_CTOR_REGISTRATION` itself
  (`CkCore/.../Macros/CkMacros.h:162-165`, reached through `CK_DEFINE_CONSTRUCTORS` at `:208`); `FCk_CustomPrimitiveData_Value(float)` is registered explicitly
  (`CkGraphics_Common.h:112`). No edit needed.
- Engine: `UPrimitiveComponent::SetCustomPrimitiveDataFloat/Vector2/Vector3/Vector4` (`Engine/Classes/Components/PrimitiveComponent.h:1138,1142,1149,1156`),
  `GetCustomPrimitiveData()` (`:1172`), `FCustomPrimitiveData::NumCustomPrimitiveDataFloats` = 9*4 = 36 (`Engine/Public/SceneTypes.h:38-39`, reached via `PrimitiveComponent.h:22`).

**Inferred (needs the build / test run).**
- Everything compiles (not built). In particular: `CK_ANGELSCRIPT_CTOR_REGISTRATION` accepting
  `(int32, FCk_CustomPrimitiveData_Value)` for `FCk_CustomPrimitiveData` (no AS call site uses that ctor today);
  the generated `Script/Generated/utils_unreal_component.as` gaining the two new wrappers on the test boot.
- A request queued before setup lands in the frame Setup runs if Setup precedes HandleRequests in that frame's
  main pass (it does via `RunAfter` PushTransform → Setup), otherwise the next frame.

**Follow-ups.**
- Step 5 (five AngelScript AutoTests in `Plugins/CkTests/Script/CkUnrealComponent/`) is PENDING: resume the F2
  executor once the editor is closed. Planned shape: one class per file, isolated Y bands 181000-185000 at
  X=0 (unused today), archetype `UStaticMeshComponent` with `/Engine/BasicShapes/Cube`, `NoCollision` and
  `StaticWorldBakePolicy::DoNotBake`; hand-written `A..._Actor` wrappers with `Get_ExpectedLogErrors` only for
  `RejectsNonPrimitive` ("Cannot set custom primitive data on UnrealComponent") and `RejectsOutOfRange`
  ("does not fit the engine's"). Call sites through `utils_unreal_component::` as VALIDATION.md states — they
  depend on the generated wrapper being regenerated before the test boot's AS compile.
- Staging: `Source/CkUnrealComponent/Claude.md` is NEW and matches CkFoundation's `.gitignore:49` (`*.md`), so
  `git status` lists it only under `--ignored`; it needs `git add -f` (other modules' `Claude.md` are tracked
  the same way).
- Deviation (minor): the requests fragment friends only HandleRequests + Utils, not the EndPlay/cancel
  processor. The cancel processor reads through the public `Get_Requests()` getter exactly as CkTimer's does,
  so a friend would be unused.

### 2026-09-30 — PHASE_1_F1 executor (per-look generated package root)

**Ran.** Edits only; no build, no editor, no git state change. Entry criteria held (`git status` clean under
`Source/CkUsf`, `Source/CkUsfEditor`). All eight steps done. Files touched:

- `D:\Repo\Mars\Plugins\CkFoundation\Source\CkUsf\Public\CkUsf\LookDefinition\CkUsf_LookDefinition.h` — `_GeneratedPackageRoot`, `Get_EffectiveGeneratedPackageRoot` (UFUNCTION), `Get_GeneratedMasterPackagePath` / `Get_GeneratedMasterObjectPath` (C++ only), exactly the pre-designed surface, after `_LookName`.
- `D:\Repo\Mars\Plugins\CkFoundation\Source\CkUsf\Public\CkUsf\LookDefinition\CkUsf_LookDefinition.cpp` — definitions; both path members delegate to the `ck::usf::` free functions (format string not duplicated) through one helper `ck_usf_look_definition::Get_PackageRoot` (override if non-empty, else effective root).
- `D:\Repo\Mars\Plugins\CkFoundation\Source\CkUsf\Public\CkUsf\Apply\CkUsf_Utils.cpp` — runtime lookup via `InLook->Get_GeneratedMasterObjectPath()`; now-unused Naming include dropped.
- `D:\Repo\Mars\Plugins\CkFoundation\Source\CkUsfEditor\Public\CkUsfEditor\Generator\CkUsf_Generator.cpp` — package path via `InDef->Get_GeneratedMasterPackagePath(InPackageRootOverride)`; now-unused Naming include dropped.
- `D:\Repo\Mars\Plugins\CkFoundation\Source\CkUsfEditor\Public\CkUsfEditor\Generator\CkUsf_Generator.h` — override comment: "empty" now means the look's own root.
- `D:\Repo\Mars\Plugins\CkFoundation\Source\CkUsfEditor\Public\CkUsfEditor\Generator\CkUsf_LookValidator.cpp` — `Validate_GeneratedPackageRoot` (leading `/`, no trailing `/`, `FPackageName::IsValidTextForLongPackageName` on the would-be package path, then `FPackageName::IsValidLongPackageName(.., IncludeReadOnlyRoots=false, ..)` for the mounted+writable root); CPD range rule against `FCustomPrimitiveData::NumCustomPrimitiveDataFloats` (Scalar reads 1, Vector reads 4). Both in the cheap pre-IO section, so a rejected look returns before `DoGenerate_LookMaterial` creates a package.
- `D:\Repo\Mars\Plugins\CkFoundation\Source\CkUsf\Claude.md` — purpose line, Key API, authoring step 1/2, asset-side rules, CPD range rule.
- `D:\Repo\Mars\Plugins\CkTests\Source\CkTests\Private\UnitTests\CkUsf\Test_Usf_GeneratesUsableMasters.cpp` — generated-this-run path via `Def->Get_GeneratedMasterObjectPath(TestPackageRoot)`; shipped arm keeps `Get_LookMasterMaterial` (which now resolves through the definition) and names `Def->Get_GeneratedMasterObjectPath()` in its assertion.
- `D:\Repo\Mars\Plugins\CkTests\Source\CkTests\Private\UnitTests\CkUsf\Test_Usf_GeneratedPackageRoot.cpp` — NEW, six tests `CkTests.UnitTests.CkUsf.GeneratedPackageRoot.{DefaultRootIsUnchanged, CustomRootIsUsed, TestOverrideWins, InvalidRootIsRejected, CustomPrimitiveDataIndexOutOfRangeIsRejected, CustomRootGeneratesAndResolves}`.

Call sites converted: `CkUsf_Utils.cpp:30`, `CkUsf_Generator.cpp:448`, `Test_Usf_GeneratesUsableMasters.cpp:85`.
Left alone (name literals, locked decision 3): `CkUsf_CelShadeSubsystem.cpp:350`, `CkUsf_CrossHatchSubsystem.cpp:207`,
`CkUsf_ScreenDitherSubsystem.cpp:218`, `CkUsf_HandDrawnSubsystem.cpp:220`, `CkVat_Subsystem.cpp:39`,
`CkPixelArt_Subsystem.cpp:627`, `CkParticles_ScriptDefinition_Naming.h:1369` (comment only), and in CkTests
`Test_Usf_StylizeSettings.cpp:242,377`, `Test_Usf_ScreenDither.cpp:378,402`, `Test_Usf_HandDrawn.cpp:404`,
`Test_Usf_CrossHatch.cpp:384`, `Test_Usf_CelShade.cpp:502`, `CkUsf_TestLookMasters.h:42` (lane-root helper).
`Get_EffectiveLookName` hits that only log/compare names (`CkUsfEditor_Module.cpp:81`, `CkUsf_EditorConsoleCommands.cpp:26`,
`CkUsf_Generator.cpp:890`, NiagaraSpriteContract/StylizeContract/MultiPass tests) build no path. NiagaraSpriteContract's
`Get_MasterMaterial` goes through `Get_LookMasterMaterial`, so it follows the definition automatically.

**Confirmed (read, not compiled).** Engine signatures: `FPackageName::IsValidTextForLongPackageName(FStringView, FText*)`
(`CoreUObject/Public/Misc/PackageName.h:241`), `FPackageName::IsValidLongPackageName(FStringView, bool, FText*)` (`:231`;
impl `Private/Misc/PackageName.cpp:1284` = text check + closest mount point, read-only roots excluded),
`FPackageName::DoesPackageExist(const FString&, FString*, bool)` (`:382`; logs a Warning on an unmountable name,
`PackageName.cpp:1908`, hence the invalid-root test checks `FindPackage` instead), `FCustomPrimitiveData::NumCustomPrimitiveDataFloats`
(`Engine/Public/SceneTypes.h:39`, = 36), vector CPD reads a float4 (`Engine/Private/Materials/MaterialExpressions.cpp:7994`,
`MCT_Float4`), `FindPackage(UObject*, const TCHAR*)` (`CoreUObject/Public/UObject/UObjectGlobals.h:1182`; `ResolveName2`
creates nothing for a dot-free name, `UObjectGlobals.cpp:1155`), `AddExpectedErrorPlain` (`Core/Public/Misc/AutomationTest.h:1885`,
Occurrences < 0 = ignore), `TStrongObjectPtr::operator->/Get() const` return non-const (`Core/Public/UObject/StrongObjectPtrTemplates.h:101,117`),
`FString::StartsWith/EndsWith(const TCHAR*, ...)` (`Core/Public/Containers/UnrealString.h.inl:1552,1608`).
Only shipped CPD look: `VisualLodNearFade` index 0 (`Script/CkUsf/CkUsf_VisualLodFadeLooks_Assets.as:83`) — inside the new range rule.
Every existing look has an empty root, so generation and runtime lookup resolve byte-identical paths.

**Inferred.** (a) The whole change compiles (UHT accepts the non-UFUNCTION trailing-return members inside the UCLASS —
precedent `CkUsf_OutlineSubsystem.h:110`; FString→FStringView converts implicitly at the two `FPackageName` calls).
(b) `CustomPrimitiveDataIndexOutOfRangeIsRejected` accepted cases read the probe .ush; the assertion keys on the
range-rule wording only, so an IO or signature error there cannot flip it. (c) `CustomRootGeneratesAndResolves` may log
the `Get_LookMasterMaterial` "still compiling" Warning if the non-forced `Validate_LookShaderCompile` leaves the shader
map unapplied — a warning, not a failure. (d) All six tests green — not run.

**Follow-ups.** Deviations from the doc, for the planner: `CustomRootGeneratesAndResolves` sets the definition root to
the lane root itself (`ck_test_usf::Get_TestPackageRoot()`, no override passed) rather than a sub-folder of it, so the
neighbouring cleanup helper `Delete_TestGeneratedMaster` applies unchanged. New helpers are non-`static` in their named
namespaces (root CLAUDE.md), unlike the older `static` helpers beside them. Dropped the now-unused
`CkUsf_LookDefinition_Naming.h` include from `CkUsf_Utils.cpp`, `CkUsf_Generator.cpp` and
`Test_Usf_GeneratesUsableMasters.cpp`. `[EDITOR-VERIFY]`: the `[Ck][Usf] Get Effective Generated Package Root` BP node
and `_GeneratedPackageRoot` in an AS `asset ... of UCkUsf_LookDefinition` block.

## Open items

| Item | Owner | Status |
|---|---|---|
| Blueprint `[EDITOR-VERIFY]` for the new nodes | human | open |
| Baseline capture | planner | open |
| F2 step 5: AngelScript AutoTests (editor was open at step 5) | F2 executor, resumed by planner | done 2026-09-30 (written, not run — the gate run is the planner's) |

No completion claim may be written in this file while any row above is unresolved.
