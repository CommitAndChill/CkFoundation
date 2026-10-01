# VALIDATION — F1 + F2 (run by the planner; all commands from `D:\Repo\Mars`, editor closed)

## Gate (class 2, `ck-change-control`)

1. Baseline BEFORE the first build, on the untouched tree:
   `./CkAuto/UnrealToolbox.exe --test --no-live --project-prefix Ck --test-pattern "Usf UnrealComponent" --output=Saved/Logs/Test-Editor.log --project="D:\Repo\Mars"`
   → record pass/fail counts and failing names in PROGRESS.md.
2. Build + targeted tests after both phases are reviewed:
   `./CkAuto/UnrealToolbox.exe --build --generate --target=Editor --test --project-prefix Ck --discover-fresh --test-pattern "Usf UnrealComponent" --output=Saved/Logs/BuildTest.log --project="D:\Repo\Mars"`
   → verdict is the `=== Test summary ===` block. Exit 76 = AngelScript failed to compile in the test boot
   (read the log for `Angelscript: Error`); 77 = an editor is open; 78 = contaminated run, re-run.
3. Real-RHI generation verdict:
   `./CkAuto/UnrealToolbox.exe --test --no-nullrhi --project-prefix Ck --test-pattern "CkUsf" --output=Saved/Logs/Test-Editor.log --project="D:\Repo\Mars"`
4. Report as a delta: "baseline N failing {names} → after N failing {names}".

## Expected new tests (all green)

- `CkTests.UnitTests.CkUsf.GeneratedPackageRoot.{DefaultRootIsUnchanged, CustomRootIsUsed, TestOverrideWins, InvalidRootIsRejected, CustomRootGeneratesAndResolves}`
- `CkTests.UnitTests.CkUsf.LookValidator.CustomPrimitiveDataIndexOutOfRangeIsRejected`
- `UnrealComponent_SetCustomPrimitiveData_{Applies, AppliesAfterSetup, RejectsNonPrimitive, RejectsOutOfRange, CancelledOnDestroy, RejectedWhileDestroying, SameRequestReusedCompletesOncePerCall}`
- No CkTests test exercises `Request_SetCustomPrimitiveData` on an `IsmProxy` (checked 2026-09-30), so its move onto `UCk_Utils_Graphics_UE::Apply_CustomPrimitiveData` is covered only by compilation plus an `IsmProxy`/`IsmRenderer` pattern run staying at its baseline verdict — add `Ism` to the `--test-pattern`.
- `Test_Usf_GeneratesUsableMasters` unchanged verdict.

## Three environments

- **C++:** compiles in the editor target; exercised by the tests above.
- **AngelScript:** the test boot compiles clean (no exit 76, no `Angelscript: Error` in the fresh log); the AutoTests call `utils_unreal_component::Request_SetCustomPrimitiveData` and `Get_CustomPrimitiveDataFloat`.
- **Blueprint `[EDITOR-VERIFY]`** (human): filled in by the F2 executor below.

## Blueprint

`[EDITOR-VERIFY]` — human-only, after the gate build, in the Mars editor. Report each line pass/fail.

1. Launch the Mars editor on the freshly built binary. Toolbar → Blueprints → Open Level Blueprint.
2. Right-click the graph, type `[Ck][UnrealComponent] Request Set Custom` (untick "Context Sensitive" if it
   does not show). Node title is exactly `[Ck][UnrealComponent] Request Set Custom Primitive Data`, under
   category `Ck > Utils > UnrealComponent`.
3. Place it. Pins: exec in/out; `In Unreal Component` (typed `Ck Handle Unreal Component`, by-ref diamond);
   `In Request` (`Ck Request Unreal Component Set Custom Primitive Data` struct); `In Delegate` (event
   delegate pin — leaving it unconnected compiles, because of `AutoCreateRefTerm`); return value of type
   `Ck Handle Unreal Component`.
4. Drag off `In Request` → `Make Ck Request Unreal Component Set Custom Primitive Data`: the Make node exists
   and shows a `Data` pin of type `Ck Custom Primitive Data`. Split or Make that pin: it shows
   `Custom Data Index` (int, cannot be dragged below 0 in a Details panel) and `Value`
   (`Ck Custom Primitive Data Value`).
5. Add a Blueprint variable of type `Ck Request Unreal Component Set Custom Primitive Data`, compile, and
   expand its default value in Details: `Data → Value → Type` is a dropdown listing Float, Vector2, Vector3,
   Vector4, LinearColor; switching it shows only the matching value field (`EditConditionHides`).
6. Drag a plain `Ck Handle` wire into `In Unreal Component`: the `<AsUnrealComponent>` autocast node is
   inserted.
7. Right-click → type `[Ck][UnrealComponent] Get Custom` → `[Ck][UnrealComponent] Get Custom Primitive Data
   Float` appears as a PURE node (no exec pins) with inputs `In Unreal Component`, `In Index` (int) and a
   float return.
8. Wire both nodes off BeginPlay (an `Add` of a `StaticMeshComponent` on a fresh entity with a Transform,
   then Request Set Custom Primitive Data with index 4 / Float 0.75, then a delay and Get Custom Primitive Data
   Float at index 4 into a Print String). Compile: no errors. PIE: prints `0.75`.
9. Optional negative: set the index to 36 and re-run PIE: an ensure names the index and count, the bound
   completion event (if any) reports `Failed (Not Enqueued)`, and the Print String shows `0.0`.

## Definition of done

Every gate above ran AFTER the last edit; module `Claude.md` files updated; comment audit done; PROGRESS.md open items resolved or explicitly handed to the human.
