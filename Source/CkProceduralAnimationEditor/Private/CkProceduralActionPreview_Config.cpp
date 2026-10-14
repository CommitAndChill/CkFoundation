#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"

#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace ck_procedural_action_preview_config
{
    const auto PreviewFloorHalfExtents = FVector{400.0, 400.0, 25.0};

    // The knee of a two-bone chain from InHip to InFoot, swung toward InPole; a foot past reach straightens the knee.
    auto
        SolveRestKnee(
            const FVector& InHip,
            const FVector& InFoot,
            const FVector& InPole,
            double InUpper,
            double InLower)
        -> FVector
    {
        const auto ToFoot = InFoot - InHip;
        const auto Distance = FMath::Clamp(ToFoot.Size(), FMath::Abs(InUpper - InLower) + UE_KINDA_SMALL_NUMBER, InUpper + InLower);
        const auto Along = ToFoot.GetSafeNormal();
        const auto Forward = (InUpper * InUpper - InLower * InLower + Distance * Distance) / (2.0 * Distance);
        const auto Rise = FMath::Sqrt(FMath::Max(InUpper * InUpper - Forward * Forward, 0.0));
        const auto ToPole = InPole - InHip;
        const auto Bend = (ToPole - Along * FVector::DotProduct(ToPole, Along)).GetSafeNormal();
        return InHip + Along * Forward + Bend * Rise;
    }

    auto
        MakeRestChainBinding(
            FName InLimbId,
            const FVector& InHip,
            const FVector& InFoot,
            const FVector& InPole,
            const TArray<float>& InLengths)
        -> FCk_ProceduralActionPreview_LimbBinding
    {
        auto Joints = TArray<FVector>{InHip};
        if (InLengths.Num() == 2)
        { Joints.Add(SolveRestKnee(InHip, InFoot, InPole, InLengths[0], InLengths[1])); }
        else
        {
            for (auto Index = 1; Index < InLengths.Num(); ++Index)
            { Joints.Add(FMath::Lerp(InHip, InFoot, static_cast<double>(Index) / InLengths.Num())); }
        }
        Joints.Add(InFoot);
        auto Frames = TArray<FTransform>{};
        for (auto Index = 0; Index + 1 < Joints.Num(); ++Index)
        { Frames.Add(FTransform{FRotationMatrix::MakeFromX(Joints[Index + 1] - Joints[Index]).ToQuat(), Joints[Index]}); }
        auto Binding = FCk_ProceduralActionPreview_LimbBinding{};
        Binding.Set_LimbId(InLimbId);
        Binding.Set_SegmentFrames(Frames);
        Binding.Set_TerminalFrame(FTransform{InFoot});
        return Binding;
    }

    // A rig or gait that lives inside a legacy preview asset goes away with it, so the clip keeps its own copy.
    template <typename TData>
    auto
        OwnedByClip(
            TData* InData,
            UCk_ProceduralAction_Data& InClip,
            const TCHAR* InName)
        -> TSoftObjectPtr<TData>
    {
        if (InData == nullptr)
        { return {}; }
        if (InData->IsAsset())
        { return TSoftObjectPtr<TData>{InData}; }
        return TSoftObjectPtr<TData>{DuplicateObject<TData>(InData, &InClip, FName{InName})};
    }

    // The quoted strings of a config line, in order: a TMap<FString, FString> is written as (("Key","Value"),...).
    auto
        ReadQuoted(
            const FString& InLine)
        -> TArray<FString>
    {
        auto Quoted = TArray<FString>{};
        auto Start = InLine.Find(TEXT("\""));
        while (Start != INDEX_NONE)
        {
            const auto End = InLine.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start + 1);
            if (End == INDEX_NONE)
            { break; }
            Quoted.Add(InLine.Mid(Start + 1, End - Start - 1));
            Start = InLine.Find(TEXT("\""), ESearchCase::CaseSensitive, ESearchDir::FromStart, End + 1);
        }
        return Quoted;
    }
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

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_action_preview_config
{
    // A preview steps a whole lunge in one frame when asked to, so its sampling allows any limb to cross its whole reach
    // and turn half a revolution per step; a scenario that cares about the budget authors its own tuning.
    auto
        Get_DerivedPreviewTuning(
            float InLongestReachCm)
        -> FCk_ProceduralAction_Tuning
    {
        auto Tuning = FCk_ProceduralAction_Tuning{};
        auto Sampling = Tuning.Get_Sampling();
        Sampling.Set_MaxEndpointTravelCm(FMath::Max(Sampling.Get_MaxEndpointTravelCm(), 2.0f * InLongestReachCm));
        Sampling.Set_MaxSegmentAngularStepDegrees(180.0f);
        Tuning.Set_Sampling(Sampling);
        return Tuning;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    auto
        MakeRestLimbBinding(
            const FCk_ProceduralLeg_Spec& InLeg,
            const FTransform& InRootFrame)
        -> FCk_ProceduralActionPreview_LimbBinding
    {
        return ck_procedural_action_preview_config::MakeRestChainBinding(InLeg.Get_Id(),
            InRootFrame.TransformPosition(InLeg.Get_Placement().Get_HipLocal()),
            InRootFrame.TransformPosition(InLeg.Get_Placement().Get_RestFootLocal()),
            InRootFrame.TransformPosition(InLeg.Get_Chain().Get_PoleLocal()),
            InLeg.Get_Chain().Get_SegmentLengths());
    }

    auto
        DerivePreviewConfig(
            const UCk_ProceduralAction_Data& InClip)
        -> UCk_ProceduralActionPreview_Config*
    {
        const auto& Subject = InClip.Get_EditorSubject();
        if (auto* Scenario = Cast<UCk_ProceduralActionPreview_Config>(Subject.Get_Scenario().LoadSynchronous()))
        {
            auto* Copy = DuplicateObject<UCk_ProceduralActionPreview_Config>(Scenario, GetTransientPackage());
            Copy->ClearFlags(RF_Public | RF_Standalone | RF_Transactional);
            Copy->SetFlags(RF_Transient);
            if (NOT Subject.Get_SlotBindings().IsEmpty())
            { Copy->Set_SlotBindings(Subject.Get_SlotBindings()); }
            return Copy;
        }

        auto* Config = NewObject<UCk_ProceduralActionPreview_Config>(GetTransientPackage(), NAME_None, RF_Transient);
        Config->Set_SlotBindings(Subject.Get_SlotBindings());
        auto* Rig = Subject.Get_WalkingRig().LoadSynchronous();
        if (Rig == nullptr)
        {
            Config->Set_SubjectMode(ECk_ProceduralActionPreview_SubjectMode::Standalone);
            Config->Set_Layout(Subject.Get_Layout());
            auto Bindings = TArray<FCk_ProceduralActionPreview_LimbBinding>{};
            auto LongestReach = 0.0f;
            for (const auto& Limb : Subject.Get_Layout().Get_Limbs())
            {
                auto Reach = 0.0f;
                for (const auto Length : Limb.Get_SegmentLengths())
                { Reach += Length; }
                LongestReach = FMath::Max(LongestReach, Reach);
                const auto Hip = Limb.Get_AttachmentLocal().GetTranslation();
                const auto Foot = (Limb.Get_RestEndpointLocal() * Limb.Get_AttachmentLocal()).GetTranslation();
                const auto Pole = Hip + Limb.Get_AttachmentLocal().TransformVectorNoScale(Limb.Get_BendLocal());
                Bindings.Add(ck_procedural_action_preview_config::MakeRestChainBinding(
                    Limb.Get_Id(), Hip, Foot, Pole, Limb.Get_SegmentLengths()));
            }
            Config->Set_LimbBindings(Bindings);
            Config->Set_ActionTuning(ck_procedural_action_preview_config::Get_DerivedPreviewTuning(LongestReach));
            return Config;
        }

        const auto& Legs = Rig->Get_Legs();
        auto Clearance = 0.0;
        for (const auto& Leg : Legs)
        { Clearance = FMath::Max(Clearance, -Leg.Get_Placement().Get_RestFootLocal().Z); }
        const auto RootFrame = FTransform{FVector{0.0, 0.0, Clearance}};
        const auto FloorHalfExtents = ck_procedural_action_preview_config::PreviewFloorHalfExtents;
        Config->Set_SubjectMode(ECk_ProceduralActionPreview_SubjectMode::Walking);
        Config->Set_WalkingRig(Rig);
        Config->Set_WalkingGait(Subject.Get_WalkingGait().LoadSynchronous());
        Config->Set_RootFrame(RootFrame);
        Config->Set_PresentationFrame(RootFrame);
        Config->Set_HasStaticFloor(true);
        Config->Set_StaticFloorFrame(FTransform{FVector{0.0, 0.0, -FloorHalfExtents.Z}});
        Config->Set_StaticFloorHalfExtents(FloorHalfExtents);
        auto Bindings = TArray<FCk_ProceduralActionPreview_LimbBinding>{};
        auto LongestReach = 0.0f;
        for (const auto& Leg : Legs)
        {
            Bindings.Add(MakeRestLimbBinding(Leg, RootFrame));
            auto Reach = 0.0f;
            for (const auto Length : Leg.Get_Chain().Get_SegmentLengths())
            { Reach += Length; }
            LongestReach = FMath::Max(LongestReach, Reach);
        }
        Config->Set_LimbBindings(Bindings);
        Config->Set_ActionTuning(ck_procedural_action_preview_config::Get_DerivedPreviewTuning(LongestReach));
        auto Locomotion = FCk_ProceduralLimbControl_Locomotion{};
        Locomotion.Get_Support().Set_Mode(ECk_ProceduralLimbControl_SupportMode::CountOnly);
        Locomotion.Get_Support().Set_MinimumTrustedContacts(FMath::Max(1, Legs.Num() - 2));
        Config->Set_WalkingLocomotion(Locomotion);
        return Config;
    }

    auto
        MakeSubjectFromLegacyPreview(
            const UCk_ProceduralActionPreview_Config& InPreview,
            UCk_ProceduralAction_Data& InClip)
        -> FCk_ProceduralAction_EditorSubject
    {
        auto Subject = FCk_ProceduralAction_EditorSubject{};
        Subject.Set_SlotBindings(InPreview.Get_SlotBindings());
        if (InPreview.Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
        {
            Subject.Set_WalkingRig(ck_procedural_action_preview_config::OwnedByClip(
                InPreview.Get_WalkingRig().Get(), InClip, TEXT("SubjectRig")));
            Subject.Set_WalkingGait(ck_procedural_action_preview_config::OwnedByClip(
                InPreview.Get_WalkingGait().Get(), InClip, TEXT("SubjectGait")));
            return Subject;
        }
        Subject.Set_Layout(InPreview.Get_Layout());
        Subject.Set_Scenario(TSoftObjectPtr<UObject>{const_cast<UCk_ProceduralActionPreview_Config*>(&InPreview)});
        return Subject;
    }

    auto
        Get_PreviewSiblingPackageName(
            const FString& InClipPackageName)
        -> FString
    {
        auto Name = FPackageName::GetLongPackageAssetName(InClipPackageName);
        if (Name.EndsWith(TEXT("_DA")))
        { Name.LeftChopInline(3); }
        return FPackageName::GetLongPackagePath(InClipPackageName) + TEXT("/") + Name + TEXT("_Preview_DA");
    }

    auto
        FindLegacyPreview(
            const UCk_ProceduralAction_Data& InClip)
        -> UCk_ProceduralActionPreview_Config*
    {
        auto Pairs = FString{};
        if (GConfig != nullptr && GConfig->GetString(TEXT("/Script/CkProceduralAnimationEditor.Ck_ProceduralActionEditor_UserSettings"),
            TEXT("_PreviewPathsByClip"), Pairs, GEditorPerProjectIni))
        {
            const auto Quoted = ck_procedural_action_preview_config::ReadQuoted(Pairs);
            for (auto Index = 0; Index + 1 < Quoted.Num(); Index += 2)
            {
                if (Quoted[Index] != InClip.GetPathName())
                { continue; }
                if (auto* Paired = LoadObject<UCk_ProceduralActionPreview_Config>(nullptr, *Quoted[Index + 1]))
                { return Paired; }
            }
        }

        const auto SiblingPackage = Get_PreviewSiblingPackageName(InClip.GetOutermost()->GetName());
        if (NOT FPackageName::IsValidLongPackageName(SiblingPackage) || NOT FPackageName::DoesPackageExist(SiblingPackage))
        { return nullptr; }
        return LoadObject<UCk_ProceduralActionPreview_Config>(nullptr,
            *(SiblingPackage + TEXT(".") + FPackageName::GetLongPackageAssetName(SiblingPackage)));
    }
}

// --------------------------------------------------------------------------------------------------------------------
