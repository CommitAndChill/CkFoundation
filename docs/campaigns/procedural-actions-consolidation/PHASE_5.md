# Phase 5 — Service split and pose fragment split (behaviour-preserving)

One executor session, runtime module only. No behaviour change: the spec is the existing suite at
its Phase-4 baseline.

## Entry criteria

Phase 4 committed. Baseline: `--test-pattern ProceduralAnimation` (full pattern, ~446 rows incl.
Mars-side ones; record `Passed/Failed` and names) and `CrawlerMelee` 38/38.

## Step 1 — Service split

`ActionPlayer/CkProceduralActionPlayer_Service.cpp` `ForEachEntity` (:391-1416) becomes a
`ck::Technique` (`CkCore/Technique/CkTechnique.h`; house pattern in `Source/CLAUDE.md` "Technique
pipeline") over one context:

```cpp
struct FContext_ProceduralActionPlayer_Service
{
    FCk_Handle_ProceduralActionPlayer Player;
    FFragment_ProceduralActionPlayer* PlayerFragment = nullptr;
    FCk_Handle_ProceduralArticulation Subject;
    TArray<FCk_Handle_ProceduralAction> ActionSnapshot;
    // …per-pass scratch that today lives as locals across the 1,000-line body
};

struct FTechnique_ProceduralActionPlayer_Service
    : ck::Technique<FTechnique_ProceduralActionPlayer_Service, FContext_ProceduralActionPlayer_Service&>
{
    FTechnique_ProceduralActionPlayer_Service()
    {
        AddStep(&ThisType::DrainRecoveries);       // :402-425
        AddStep(&ThisType::SnapshotActions);       // :426-440
        AddStep(&ThisType::AdmitAndLoad);          // admission + BindCompiledClip
        AddStep(&ThisType::Interrupt);             // :542-638 (watchdogs, LimbUnavailable, terminal transitions)
        AddStep(&ThisType::PlanCandidates);        // :600-882 (return prefix + clip candidates)
        AddStep(&ThisType::Stage);                 // :883-1390
        AddStep(&ThisType::DispatchKeys);          // :1391-1416
    }
    // static auto StepName(FTechnique…& InSelf, FContext…& InCtx) -> void; per step
};
```

Each step is a `static auto` on the technique, bodies moved verbatim (then simplified only where a
local was only alive across one step). Add `FFragment_ProceduralAction::Get_Token()` to replace the
ten hand-built `FCk_ArticulationControlToken{Identity, Generation}` sites. Every silent `return` in
the staging pipeline (`:984, :987, :1036, :1078, :1082, :1385, :1389`) records a stall reason into
`FFragment_ProceduralAction::_Context` (the `FailureContext`) so the publication watchdog names it.

Split the file: `…_Service.cpp` (processor + technique registration), `…_Service_Admission.cpp`,
`…_Service_Planning.cpp`, `…_Service_Staging.cpp`; one private header for the technique (public
folder, `Internal` naming as in Phase 2).

## Step 2 — Pose fragment split

`FFragment_ProceduralArticulation_Pose` (22 members) → `_Pose` (issued/settled/expected handles and
poses, revisions), `_LegacyCandidate` (`_LegacyHandles/_LegacyPoses/_LegacyPresentation/_HasLegacyPresentation/_LegacyWalker`),
`_ControlCandidate` (`_PredictedSubjectPose`, pending stage data). Friends updated; views that only
read one part take only that fragment.

`IsRigid` has three definitions with different tolerances (`Control.cpp:33`, `Service.cpp:100`,
`Utils.cpp:54`); one `ck::procedural_animation::IsRigid(const FTransform&, double InTolerance)` in
`Core/`.

## Exit criteria

- `--test-pattern ProceduralAnimation`: identical `Passed/Failed` names to the Phase-4 baseline.
- `CrawlerMelee` 38/38.
- `wc -l …_Service*.cpp` each ≤ 450; `rg -n "FCk_ArticulationControlToken\{" Source/CkProceduralAnimation` → ≤2 (the helper and the fragment).
- `rg -n "IsRigid\(" Source/CkProceduralAnimation --type cpp | rg "auto\s+IsRigid|IsRigid\(const"` → 1 definition.
