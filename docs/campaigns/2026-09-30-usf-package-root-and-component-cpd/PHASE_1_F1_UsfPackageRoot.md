# PHASE 1 / F1 — per-look generated package root (CkUsf, CkUsfEditor, CkTests)

Read `PROMPT.md` in this folder first. You edit C++ only. You do not build, run the editor, commit or stage.
You own: `Source/CkUsf/**`, `Source/CkUsfEditor/**`, `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkUsf/**`.
Another executor is editing `Source/CkUnrealComponent/**` at the same time; do not touch it.

## Entry criteria

- `git -C D:/Repo/Mars/Plugins/CkFoundation status --short` shows nothing under `Source/CkUsf` or `Source/CkUsfEditor`.
- Anything else: STOP, record in `PROGRESS.md` Blockers.

## Pre-designed surface

`Source/CkUsf/Public/CkUsf/LookDefinition/CkUsf_LookDefinition.h`, directly after `_LookName`:

```cpp
    // Content root the generated master is saved under and loaded from, e.g. "/Game/MyGame/Materials/GeneratedLooks".
    // Empty keeps the framework root, which is where every look that ships with CkFoundation lives. A look that
    // belongs to a game sets this so its master is saved in the game's own content, not in the plugin's.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CkUsf")
    FString _GeneratedPackageRoot;

    UFUNCTION(BlueprintPure, Category = "CkUsf",
              DisplayName = "[Ck][Usf] Get Effective Generated Package Root")
    FString Get_EffectiveGeneratedPackageRoot() const;

    // InPackageRootOverride is the tests-only seam described in CkUsf_LookDefinition_Naming.h; when set it wins.
    auto
    Get_GeneratedMasterPackagePath(
        const FString& InPackageRootOverride = {}) const -> FString;

    auto
    Get_GeneratedMasterObjectPath(
        const FString& InPackageRootOverride = {}) const -> FString;
```

- `Get_EffectiveGeneratedPackageRoot` returns `_GeneratedPackageRoot` when non-empty, else `ck::usf::Get_GeneratedMasterPackageRoot()`.
- The two path members delegate to the existing `ck::usf::` naming functions, passing
  `InPackageRootOverride` when non-empty, else the effective root. Do not duplicate the `M_CkUsf_Look_%s` format string.
- The existing free functions in `CkUsf_LookDefinition_Naming.h` keep their signatures and behaviour.

## Steps

1. Add the field and the three members (definitions in `CkUsf_LookDefinition.cpp`, house function shape).
   → verify: `rg -n "_GeneratedPackageRoot" Source/CkUsf` shows the declaration and its uses only.
2. Generator: `CkUsf_Generator.cpp` ~449, replace the name-based path with `InDef->Get_GeneratedMasterPackagePath(InPackageRootOverride)`.
   Check the rest of the file for any other place that derives a path from a definition and convert those too.
3. Runtime lookup: `CkUsf_Utils.cpp:31` → `InLook->Get_GeneratedMasterObjectPath()`.
4. Sweep: `rg -n --no-ignore "Get_GeneratedMaster(PackagePath|ObjectPath)|Get_EffectiveLookName" Source ../CkTests/Source`.
   Convert a call site ONLY when it has a `UCkUsf_LookDefinition*` in hand and builds the path from that
   definition's name. Leave every name-literal caller alone (locked decision 3). List each site you
   converted and each you deliberately left in your report.
5. Validator (`CkUsf_LookValidator.cpp`, next to the existing asset-side rules):
   - non-empty `_GeneratedPackageRoot` must start with `/`, must not end with `/`, must be a valid long
     package name and must sit under a mounted content root. Otherwise a generation ERROR naming the look
     and the root. Generation must write no package for a rejected look.
   - a `_CustomPrimitiveData` param must fit the engine's custom primitive data:
     Scalar `index <= 35`, Vector `index + 3 <= 35`. Use the engine constant
     (`FCustomPrimitiveData::NumCustomPrimitiveDataFloats`, `SceneTypes.h`), not a literal. Otherwise an ERROR.
6. `Test_Usf_GeneratesUsableMasters.cpp`: where it asserts the SHIPPED master resolves, resolve through the
   definition (`Def->Get_GeneratedMasterObjectPath()`), so a game-rooted look is checked at its own root.
7. New tests, `Plugins/CkTests/Source/CkTests/Private/UnitTests/CkUsf/Test_Usf_GeneratedPackageRoot.cpp`,
   pretty names `CkTests.UnitTests.CkUsf.GeneratedPackageRoot.<Case>`, flags and helpers as the neighbouring
   CkUsf tests use (`kCkUnitTestFlags`; the generation cases carry the same NonNullRHI treatment and
   skip-as-environmental behaviour as `Test_Usf_GeneratesUsableMasters`):
   - `DefaultRootIsUnchanged`: a transient definition named `X` with an empty root →
     `Get_GeneratedMasterObjectPath()` equals `ck::usf::Get_GeneratedMasterObjectPath(FName("X"))`, and
     `Get_EffectiveGeneratedPackageRoot()` equals `ck::usf::Get_GeneratedMasterPackageRoot()`.
   - `CustomRootIsUsed`: root `/Game/__CkUsfTest` → object path is `/Game/__CkUsfTest/M_CkUsf_Look_X.M_CkUsf_Look_X`.
   - `TestOverrideWins`: definition root set AND override passed → path is under the override.
   - `InvalidRootIsRejected`: `"NoLeadingSlash"`, `"/Game/Trailing/"`, `"/NotAMountedRoot/X"` each produce a
     validator error and no package exists at the would-be path afterwards.
   - `CustomPrimitiveDataIndexOutOfRangeIsRejected`: Scalar at 36 and Vector at 33 are errors; Scalar at 35
     and Vector at 32 are accepted.
   - `CustomRootGeneratesAndResolves` (real RHI): generate a small existing-shader look copy with a custom
     root under the test package root, assert the package exists there and `Get_LookMasterMaterial` returns
     it, then delete it exactly as the neighbouring test cleans up.
8. Docs: `Source/CkUsf/Claude.md` — Purpose line ("saves it under the look's generated package root,
   `/CkFoundation/CkUsf/GeneratedLooks/` by default"), "Authoring a new look" step 1/2 (a game look sets
   `_GeneratedPackageRoot`), the parameter-contract paragraph (CPD range rule). No campaign wording.

## Decision gates

| After | Expect | Otherwise |
|---|---|---|
| Step 4 sweep | Only the generator, `CkUsf_Utils.cpp` and the usable-masters test needed conversion | If a runtime module builds a path from a definition somewhere else, convert it and report it. If you are unsure whether a site has a definition or a name literal: STOP, Blocker. |
| Step 5 | The validator already has one place where asset-side rules accumulate errors | If the validator has no access to mount-point queries without a new module dependency: STOP, Blocker (do not add a dependency on your own). |
| Step 7 | An existing helper creates a transient look definition for tests | If none exists, build one with `NewObject<UCkUsf_LookDefinition>(GetTransientPackage())`. |

## Exit criteria

- All eight steps done; `rg -n "InPackageRootOverride" Source/CkUsf Source/CkUsfEditor` still shows the test seam intact.
- `PROGRESS.md` updated: files touched (full paths), sites converted/left, anything you could not verify
  marked INFERRED. You do not claim "compiles" — the planner builds.
- Comment audit done (root `CLAUDE.md`).

## Fences

- Do NOT change `Get_GeneratedMasterPackageRoot()`'s return value or the free functions' signatures.
- Do NOT move or regenerate any existing master.
- Do NOT touch the stylize subsystems, CkVat, CkPixelArt or CkParticles.
- Do NOT make `_GeneratedPackageRoot` a `FDirectoryPath`/soft path type; it is a long-package-name string like the root it replaces.
