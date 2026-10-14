#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"

namespace ck_procedural_action_preview_config
{
    auto
        IsFiniteRigidBox(
            const FTransform& InFrame,
            const FVector& InHalfExtents)
        -> bool
    {
        return NOT InFrame.ContainsNaN() && InFrame.GetRotation().IsNormalized()
            && InFrame.GetScale3D().Equals(FVector::OneVector, 1.0e-6)
            && FMath::IsFinite(InHalfExtents.X) && FMath::IsFinite(InHalfExtents.Y)
            && FMath::IsFinite(InHalfExtents.Z)
            && InHalfExtents.X > 0.0 && InHalfExtents.Y > 0.0 && InHalfExtents.Z > 0.0;
    }

    auto
        IsFiniteNonzeroScale(
            const FVector& InScale)
        -> bool
    {
        return NOT InScale.ContainsNaN()
            && FMath::IsFinite(InScale.X) && FMath::IsFinite(InScale.Y) && FMath::IsFinite(InScale.Z)
            && NOT FMath::IsNearlyZero(InScale.X)
            && NOT FMath::IsNearlyZero(InScale.Y)
            && NOT FMath::IsNearlyZero(InScale.Z);
    }
}

auto
    UCk_ProceduralActionPreview_Config::
    ValidateSurface(
        FString& OutError) const
    -> bool
{
    OutError.Reset();
    if (_HasStaticFloor && NOT ck_procedural_action_preview_config::IsFiniteRigidBox(
        _StaticFloorFrame, _StaticFloorHalfExtents))
    { OutError = TEXT("Preview floor requires a finite rigid frame and positive half extents."); return false; }
    if (_StaticObstacles.Num() > 16)
    { OutError = TEXT("Preview supports at most 16 static obstacles."); return false; }
    for (auto Index = 0; Index < _StaticObstacles.Num(); ++Index)
    {
        const auto& Obstacle = _StaticObstacles[Index];
        if (NOT ck_procedural_action_preview_config::IsFiniteRigidBox(
            Obstacle.Get_Frame(), Obstacle.Get_HalfExtents()))
        {
            OutError = FString::Printf(TEXT("Preview obstacle %d requires a finite rigid frame and positive half extents."), Index);
            return false;
        }
    }
    return true;
}

auto
    UCk_ProceduralActionPreview_Config::
    ValidateAppearance(
        FString& OutError) const
    -> bool
{
    OutError.Reset();
    if (NOT ck_procedural_action_preview_config::IsFiniteNonzeroScale(_BodyPreviewMeshScale))
    { OutError = TEXT("Body preview mesh scale must have three finite, nonzero components."); return false; }
    for (const auto& Binding : _LimbBindings)
    {
        if (NOT ck_procedural_action_preview_config::IsFiniteNonzeroScale(Binding.Get_PreviewMeshScale()))
        {
            OutError = FString::Printf(TEXT("Preview mesh scale for limb %s must have three finite, nonzero components."),
                *Binding.Get_LimbId().ToString());
            return false;
        }
    }
    return true;
}

auto
    UCk_ProceduralActionPreview_Config::
    ValidateScenario(
        FString& OutError) const
    -> bool
{
    OutError.Reset();
    constexpr auto MaximumSteps = int64{3600};
    constexpr auto MaximumEvents = 64;
    if (_ObservationHorizonStep < 0 || _ObservationHorizonStep > MaximumSteps)
    {
        OutError = TEXT("Scenario ObservationHorizonStep must be between 0 and 3600 preview steps.");
        return false;
    }
    if (_ScenarioEvents.Num() > MaximumEvents || (_ObservationHorizonStep == 0 && NOT _ScenarioEvents.IsEmpty()))
    {
        OutError = TEXT("Scenario needs a positive observation horizon and at most 64 events.");
        return false;
    }
    const auto Walking = _SubjectMode == ECk_ProceduralActionPreview_SubjectMode::Walking;
    const auto* Rig = _WalkingRig.Get();
    auto FloorPresent = _HasStaticFloor;
    auto PreviousStep = int64{0};
    auto DetachedIds = TSet<FName>{};
    for (auto Index = 0; Index < _ScenarioEvents.Num(); ++Index)
    {
        const auto& Event = _ScenarioEvents[Index];
        const auto Step = Event.Get_Step();
        if (Step <= PreviousStep || Step >= _ObservationHorizonStep)
        {
            OutError = FString::Printf(TEXT("Scenario event %d must have a unique ascending step before ObservationHorizonStep."), Index);
            return false;
        }
        PreviousStep = Step;
        const auto& Ids = Event.Get_LimbIds();
        switch (Event.Get_Kind())
        {
        case ECk_ProceduralActionPreview_ScenarioEventKind::RemoveFloor:
        case ECk_ProceduralActionPreview_ScenarioEventKind::RestoreFloor:
        {
            const auto Removing = Event.Get_Kind() == ECk_ProceduralActionPreview_ScenarioEventKind::RemoveFloor;
            if (NOT _HasStaticFloor || NOT Ids.IsEmpty() || FloorPresent != Removing)
            {
                OutError = FString::Printf(TEXT("Scenario floor event %d requires a configured floor, no limb IDs, and the opposite current floor state."), Index);
                return false;
            }
            FloorPresent = NOT Removing;
            break;
        }
        case ECk_ProceduralActionPreview_ScenarioEventKind::RecoverLimbs:
        case ECk_ProceduralActionPreview_ScenarioEventKind::DetachLimb:
        {
            const auto Detach = Event.Get_Kind() == ECk_ProceduralActionPreview_ScenarioEventKind::DetachLimb;
            if (NOT Walking || Rig == nullptr || Ids.IsEmpty() || Ids.Num() > 64
                || (Detach && Ids.Num() != 1))
            {
                OutError = FString::Printf(TEXT("Scenario limb event %d requires a walking rig and %s named limb IDs."),
                    Index, Detach ? TEXT("one") : TEXT("1 to 64"));
                return false;
            }
            auto SeenIds = TSet<FName>{};
            for (const auto Id : Ids)
            {
                if (Id.IsNone() || SeenIds.Contains(Id)
                    || NOT Rig->Get_Legs().ContainsByPredicate([Id](const auto& InLeg)
                        { return InLeg.Get_Id() == Id; })
                    || DetachedIds.Contains(Id))
                {
                    OutError = FString::Printf(TEXT("Scenario event %d has an unknown, duplicate, or already detached limb ID %s."),
                        Index, *Id.ToString());
                    return false;
                }
                SeenIds.Add(Id);
                if (Detach)
                { DetachedIds.Add(Id); }
            }
            if (NOT Detach && NOT FloorPresent)
            {
                OutError = FString::Printf(TEXT("Scenario recovery event %d needs a restored floor."), Index);
                return false;
            }
            break;
        }
        default:
            OutError = FString::Printf(TEXT("Scenario event %d has an unknown kind."), Index);
            return false;
        }
    }
    return true;
}
