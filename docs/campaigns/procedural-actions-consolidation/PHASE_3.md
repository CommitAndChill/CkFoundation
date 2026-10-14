# Phase 3 — Editor data leaves the runtime asset; poses are labelled keys

One executor session: runtime asset header, editor module, CkTests editor rows, host seeding script,
clip resave by the maintainer.

## Entry criteria

Phase 2 committed in all three repos. Baseline: `--test-pattern ProceduralAnimation.Action.Editor`
(expect 62 rows, all green except known reds) and `CrawlerMelee.ShippedClipsLoad` green.

## Design (decided)

- `UCk_ProceduralAction_Data` implements `IInterface_AssetUserData` with an **editor-only** array:

```cpp
#if WITH_EDITORONLY_DATA
    UPROPERTY(EditAnywhere, AdvancedDisplay, Instanced, Category = "Editor", meta = (AllowPrivateAccess = true))
    TArray<TObjectPtr<UAssetUserData>> _AssetUserDataEditorOnly;
#endif
```

  Interface implementation per engine precedent `UAnimationAsset` (`AnimationAsset.h:1185-1190`) and
  `USkeleton::AssetUserDataEditorOnly` (`Skeleton.h:1030`). Verify against the 5.7 fork that
  `GetAssetUserDataEditorOnly()` is the accessor (5.7 deprecates direct field access on
  `USkeletalMesh`, `SkeletalMesh.h:2396`).
- Editor module adds `UCk_ProceduralActionEditorData : public UAssetUserData` holding:

```cpp
UPROPERTY(EditAnywhere) FCk_ProceduralAction_EditorSubject _Subject;   // struct moves to the editor module
UPROPERTY() TArray<FCk_ProceduralAction_PoseLabel> _PoseLabels;        // {FName PhaseId; FCk_FloatRange_0to1 Time; FName Label}
UPROPERTY() FCk_ProceduralActionRecordedStore _Recorded;               // moves from runtime
```

  Access through `UCk_Utils_ProceduralActionEditor_UE::Get_EditorData(UCk_ProceduralAction_Data*)` →
  find-or-create in the editor-only array. All editor code and the host seeding script go through it.
- Runtime asset keeps: `BodyChannel`, `Slots`, `Phases`, `Markers`, `Windows`, `_CompatibilityVersion`
  (`VisibleAnywhere`, no setter; `PostLoad` gates on `< Current` and bumps to `3` after migrating).
  Migration in `PostLoad` (editor only): if `_EditorSubject`/`_EditorPoseGroups` (kept one version as
  `_DEPRECATED` fields, like `_Space_DEPRECATED`) hold data, create the sidecar, copy the subject,
  convert each pose group to one `PoseLabel{PhaseId, Time, Label}`, clear the deprecated fields.
- The `AllowedClasses` string naming the editor module (`Data.h:367-369`) goes away with the struct.
- A **pose** is now: a label at (phase, time). `RecordPose` writes keys for the ticked channels via
  `SetPoseKeys` and upserts one label. The timeline draws labels on the lane row of the first ticked
  channel at that time (merged when several labels share a time, per the current merge rule); the
  Poses row, `PoseGroups.{h,cpp}`, `UpsertGroup`, `Retime/ReadTangents/WriteTangents/RemovePoseKeys`
  helpers, the session staging API (`Session.cpp:297-372`) and `_IsBroken` are deleted. Selection kind
  `Pose` becomes a `Key` selection plus the label shown in Details (`_PoseName` stays, routed to
  `RenameLabel`).

## Step 1 — Red tests

In `CkTestsEditor/Private/CkProceduralAnimation/`:
- `Ck.ProceduralAnimation.Action.Editor.Asset.EditorDataLivesInSidecar`: create a clip, set a subject
  through `Get_EditorData`, save to a temp package, reload → subject present; the runtime asset has no
  `_EditorSubject` property (`FindPropertyByName` returns null). RED on entry.
- `...Editor.Asset.PostLoadMigratesLegacyEditorFields`: construct a clip object with the deprecated
  fields populated (use `FindPropertyByName` + `ImportText` on the `_DEPRECATED` names so the test
  compiles before and after), call `PostLoad` → sidecar populated, version 3, deprecated fields empty.
- `...Editor.Poses.RecordWritesKeysAndOneLabel`: record a pose → one label, keys on ticked channels only.
- Update the seven `PoseGroup` rows (list in PROMPT.md) to the label API; a row that only existed to
  cover group bookkeeping (`_IsBroken`, group GUID identity) is deleted, not rewritten.

## Step 2 — Runtime header + PostLoad; Step 3 — editor module; Step 4 — host

- Runtime: `Data.h` changes above; `_CompatibilityVersion` → `VisibleAnywhere` + `CK_PROPERTY_GET`.
- Editor: sidecar class + utils; `AuthoringSession` reads the subject through the sidecar
  (`DerivePreviewConfig` input); commands `SetEditorSubject` → `SetSubject` on the sidecar inside the
  same transaction (`Modify()` the sidecar object; it is `Instanced`, so the asset's transaction
  covers it — verify by undoing a subject change in the mounted test).
- Host `Script/Editor/Mars_CrawlerAttackClips.as:269,287`: `Clip.Set_EditorSubject(...)` →
  `utils_procedural_action_editor::Get_EditorData(Clip).Set_Subject(...)`. Editor scripts bind the
  UncookedOnly module in the editor; confirm the generated `utils_procedural_action_editor.as` appears.

## Step 5 — `[EDITOR-VERIFY]` clip resave (maintainer)

Open each of the four clips under `Content/Mars/Gameplay/Monsters/Crawler/Actions/`, confirm the
Details show the subject under the sidecar and the timeline shows labels on the key rows, save.
Then run `CrawlerMelee.ShippedClipsLoad`. Any BusterBlock clips get the same treatment when that
project bumps its pin (note in the PR body).

## Exit criteria

- `rg -n "WITH_EDITORONLY_DATA" Source/CkProceduralAnimation/Public/CkProceduralAnimation/Action/CkProceduralAction_Data.h`
  → the editor-only block holds only the user-data array and the two `_DEPRECATED` fields.
- `rg -n "CkProceduralAnimationEditor" Source/CkProceduralAnimation` → 0.
- `ls Source/CkProceduralAnimationEditor/Private/Authoring/CkProceduralActionPoseGroups.*` → none.
- `--test-pattern ProceduralAnimation.Action.Editor`: the three new rows green; row count = 62 − deleted
  + 3, recorded; `CrawlerMelee` 38/38 after the resave.
