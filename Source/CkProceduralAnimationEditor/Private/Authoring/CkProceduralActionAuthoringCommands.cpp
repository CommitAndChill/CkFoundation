#include "CkProceduralAnimationEditor/CkProceduralActionAuthoringCommands.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPoseGroups.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreviewSpace.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"
#include "CkProceduralAnimationEditor/CkProceduralActionSolvedCapture.h"

#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "CkProceduralActionAuthoringCommands"

namespace ck_procedural_action_authoring
{
    auto
        IsNormalized(
            double InTime)
        -> bool
    {
        return FMath::IsFinite(InTime) && InTime >= 0.0 && InTime <= 1.0;
    }

    auto
        HasPhase(
            const UCk_ProceduralAction_Data& InClip,
            FName InId)
        -> bool
    {
        return InClip.Get_Phases().ContainsByPredicate(
            [InId](const FCk_ProceduralAction_Phase& InPhase)
            { return InPhase.Get_Id() == InId; });
    }

    auto
        HasEventId(
            const UCk_ProceduralAction_Data& InClip,
            FName InId)
        -> bool
    {
        return InClip.Get_Markers().ContainsByPredicate(
                [InId](const FCk_ProceduralAction_Marker& InMarker)
                { return InMarker.Get_Id() == InId; })
            || InClip.Get_Windows().ContainsByPredicate(
                [InId](const FCk_ProceduralAction_Window& InWindow)
                { return InWindow.Get_Id() == InId; });
    }

    auto
        IsLater(
            const TArray<FCk_ProceduralAction_Phase>& InPhases,
            FName InFirstPhase,
            double InFirstTime,
            FName InLastPhase,
            double InLastTime)
        -> bool
    {
        if (NOT IsNormalized(InFirstTime) || NOT IsNormalized(InLastTime))
        { return false; }
        auto Start = TOptional<FCk_Time>{};
        auto End = TOptional<FCk_Time>{};
        auto Elapsed = FCk_Time{};
        for (const auto& Phase : InPhases)
        {
            const auto Duration = Phase.Get_Duration();
            if (NOT FMath::IsFinite(Duration.Get_Seconds()) || Duration <= FCk_Time{}
                || NOT FMath::IsFinite((Elapsed + Duration).Get_Seconds()))
            { return false; }
            if (Phase.Get_Id() == InFirstPhase)
            {
                if (Start.IsSet()) { return false; }
                Start = Elapsed + Duration * InFirstTime;
            }
            if (Phase.Get_Id() == InLastPhase)
            {
                if (End.IsSet()) { return false; }
                End = Elapsed + Duration * InLastTime;
            }
            Elapsed += Duration;
        }
        return Start.IsSet() && End.IsSet() && FMath::IsFinite(Start.GetValue().Get_Seconds())
            && FMath::IsFinite(End.GetValue().Get_Seconds()) && End.GetValue() > Start.GetValue();
    }

    auto
        IsAccepted(
            const FCk_ProceduralAction_ValidationReport& InReport)
        -> bool
    { return InReport.Get_Status() == ECk_ProceduralAction_ReportStatus::Valid; }

    auto
        IsError(
            const FCk_ProceduralAction_Diagnostic& InDiagnostic)
        -> bool
    { return InDiagnostic.Get_Severity() == ECk_ProceduralAction_DiagnosticSeverity::Error; }

    auto
        IsError(
            const ck::procedural_action::FDiagnostic& InDiagnostic)
        -> bool
    { return InDiagnostic.Get_Severity() == ck::procedural_action::EDiagnosticSeverity::Error; }

    template <typename TDiagnostic>
    auto
        DescribeErrors(
            const TArray<TDiagnostic>& InDiagnostics)
        -> FString
    {
        auto Text = FString{};
        for (const auto& Diagnostic : InDiagnostics)
        {
            if (NOT IsError(Diagnostic))
            { continue; }
            Text += FString::Printf(TEXT("%s%s [%s]: expected %s; got %s."), Text.IsEmpty() ? TEXT("") : TEXT(" "),
                *Diagnostic.Get_FieldPath(), *Diagnostic.Get_Code().ToString(), *Diagnostic.Get_Expected(),
                *Diagnostic.Get_Actual());
        }
        return Text.IsEmpty() ? FString{TEXT("The edit was rejected without a field diagnostic.")} : Text;
    }

    auto
        HasCode(
            const FCk_ProceduralAction_ValidationReport& InReport,
            const TCHAR* InCode)
        -> bool
    {
        return InReport.Get_Diagnostics().ContainsByPredicate([InCode](const FCk_ProceduralAction_Diagnostic& InDiagnostic)
            { return InDiagnostic.Get_Code() == FName{InCode}; });
    }

    auto
        JoinNames(
            const TArray<FName>& InNames)
        -> FString
    {
        return FString::JoinBy(InNames, TEXT(", "), [](FName InName) { return InName.ToString(); });
    }

    using FDraftEdit = TFunctionRef<bool(FCk_ProceduralAction_ClipDraft&, TArray<FCk_ProceduralAction_PoseGroup>&, FString&)>;

    // Draft -> builder edit -> Write_Into, keys and pose metadata in one transaction. The builder edit validates every input
    // before the transaction opens, so a refused edit neither dirties the package nor leaves an empty undo entry. The
    // result need not compile: an author may save a clip that is still missing keys, and Play stays gated on Compile.
    auto
        CommitDraft(
            UCk_ProceduralAction_Data& InClip,
            const FText& InDescription,
            FDraftEdit InEdit,
            FString& OutError)
        -> bool
    {
        auto Draft = FCk_ProceduralAction_ClipDraft{};
        const auto DraftReport = UCk_Utils_ProceduralActionBuilder_UE::Make_Draft_FromData(&InClip, Draft);
        if (NOT IsAccepted(DraftReport))
        {
            OutError = DescribeErrors(DraftReport.Get_Diagnostics());
            return false;
        }
        auto Groups = InClip.Get_EditorPoseGroups();
        if (NOT InEdit(Draft, Groups, OutError))
        { return false; }

        FScopedTransaction Transaction{InDescription};
        InClip.Modify();
        const auto WriteReport = UCk_Utils_ProceduralActionBuilder_UE::Write_Into(Draft, &InClip);
        const auto Written = IsAccepted(WriteReport);
        CK_ENSURE_IF_NOT(Written, TEXT("Write_Into refused the draft read from clip [{}]: {}"),
            InClip.GetPathName(), DescribeErrors(WriteReport.Get_Diagnostics()))
        {
            Transaction.Cancel();
            OutError = DescribeErrors(WriteReport.Get_Diagnostics());
            return false;
        }
        InClip.Set_EditorPoseGroups(Groups);
        InClip.PostEditChange();
        return true;
    }

    auto
        FindPhase(
            TArray<FCk_ProceduralAction_Phase>& InPhases,
            FName InPhaseId)
        -> FCk_ProceduralAction_Phase*
    {
        return InPhases.FindByPredicate([InPhaseId](const FCk_ProceduralAction_Phase& InPhase)
            { return InPhase.Get_Id() == InPhaseId; });
    }

    // Removes the keys a pose wrote at its phase and time; tracks stay, because they carry the slot frame.
    auto
        RemovePoseKeys(
            TArray<FCk_ProceduralAction_Phase>& InOutPhases,
            const FCk_ProceduralAction_PoseGroup& InGroup)
        -> void
    {
        auto* Phase = FindPhase(InOutPhases, InGroup.Get_PhaseId());
        if (Phase == nullptr)
        { return; }
        const auto Time = InGroup.Get_Time().Get_Value();
        const auto AtTime = [Time](const auto& InKey) { return InKey.Get_Time().Get_Value() == Time; };
        for (const auto Channel : InGroup.Get_Channels())
        {
            if (Channel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
            {
                Phase->Get_BodyKeys().RemoveAll(AtTime);
                continue;
            }
            for (auto& Track : Phase->Get_LimbTracks())
            {
                if (Track.Get_Slot() != Channel)
                { continue; }
                Track.Get_PositionKeys().RemoveAll(AtTime);
                Track.Get_OrientationKeys().RemoveAll(AtTime);
                Track.Get_BendKeys().RemoveAll(AtTime);
            }
        }
    }

    struct FKeyTangents
    {
        FVector Arrive = FVector::ZeroVector;
        FVector Leave = FVector::ZeroVector;
    };

    struct FPoseTangents
    {
        TOptional<FKeyTangents> Body;
        TMap<FName, FKeyTangents> Positions;
        TMap<FName, FKeyTangents> Bends;
    };

    // Add_PoseKey writes zero tangents; a moved or duplicated pose keeps the tangents its keys had.
    auto
        ReadTangents(
            const TArray<FCk_ProceduralAction_Phase>& InPhases,
            const FCk_ProceduralAction_PoseGroup& InGroup)
        -> FPoseTangents
    {
        auto Result = FPoseTangents{};
        const auto* Phase = InPhases.FindByPredicate([&InGroup](const FCk_ProceduralAction_Phase& InPhase)
            { return InPhase.Get_Id() == InGroup.Get_PhaseId(); });
        if (Phase == nullptr)
        { return Result; }
        const auto Time = InGroup.Get_Time().Get_Value();
        const auto AtTime = [Time](const auto& InKey) { return InKey.Get_Time().Get_Value() == Time; };
        if (const auto* Key = Phase->Get_BodyKeys().FindByPredicate(AtTime))
        { Result.Body = FKeyTangents{Key->Get_ArriveTangent(), Key->Get_LeaveTangent()}; }
        for (const auto& Track : Phase->Get_LimbTracks())
        {
            if (const auto* Key = Track.Get_PositionKeys().FindByPredicate(AtTime))
            { Result.Positions.Add(Track.Get_Slot(), FKeyTangents{Key->Get_ArriveTangent(), Key->Get_LeaveTangent()}); }
            if (const auto* Key = Track.Get_BendKeys().FindByPredicate(AtTime))
            { Result.Bends.Add(Track.Get_Slot(), FKeyTangents{Key->Get_ArriveTangent(), Key->Get_LeaveTangent()}); }
        }
        return Result;
    }

    auto
        WriteTangents(
            TArray<FCk_ProceduralAction_Phase>& InOutPhases,
            FName InPhaseId,
            double InTime,
            const TArray<FName>& InChannels,
            const FPoseTangents& InTangents)
        -> void
    {
        auto* Phase = FindPhase(InOutPhases, InPhaseId);
        if (Phase == nullptr)
        { return; }
        const auto AtTime = [InTime](const auto& InKey) { return InKey.Get_Time().Get_Value() == InTime; };
        if (InTangents.Body.IsSet() && InChannels.Contains(FCk_ProceduralAction_PoseGroup::Get_BodyChannelName()))
        {
            if (auto* Key = Phase->Get_BodyKeys().FindByPredicate(AtTime))
            { Key->Set_ArriveTangent(InTangents.Body->Arrive).Set_LeaveTangent(InTangents.Body->Leave); }
        }
        for (auto& Track : Phase->Get_LimbTracks())
        {
            if (NOT InChannels.Contains(Track.Get_Slot()))
            { continue; }
            if (const auto* Tangents = InTangents.Positions.Find(Track.Get_Slot()))
            {
                if (auto* Key = Track.Get_PositionKeys().FindByPredicate(AtTime))
                { Key->Set_ArriveTangent(Tangents->Arrive).Set_LeaveTangent(Tangents->Leave); }
            }
            if (const auto* Tangents = InTangents.Bends.Find(Track.Get_Slot()))
            {
                if (auto* Key = Track.Get_BendKeys().FindByPredicate(AtTime))
                { Key->Set_ArriveTangent(Tangents->Arrive).Set_LeaveTangent(Tangents->Leave); }
            }
        }
    }

    auto
        FindEditablePose(
            const UCk_ProceduralAction_Data& InClip,
            const FGuid& InPoseId,
            FCk_ProceduralActionPoseGroupView& OutView,
            FString& OutError)
        -> bool
    {
        if (NOT ck::procedural_action_editor::FindPoseGroup(InClip, InPoseId, OutView))
        {
            OutError = FString::Printf(TEXT("Pose %s does not exist in this clip."), *InPoseId.ToString());
            return false;
        }
        if (OutView.Get_IsBroken())
        {
            OutError = FString::Printf(TEXT("Pose %s is broken: %s Repair its keys or delete it first."),
                *OutView.Get_Group().Get_Label().ToString(), *OutView.Get_BrokenReason());
            return false;
        }
        return true;
    }

    auto
        UpsertGroup(
            TArray<FCk_ProceduralAction_PoseGroup>& InOutGroups,
            const FCk_ProceduralAction_PoseGroup& InGroup)
        -> void
    {
        if (auto* Existing = InOutGroups.FindByPredicate([&InGroup](const FCk_ProceduralAction_PoseGroup& InCandidate)
            { return InCandidate.Get_Id() == InGroup.Get_Id(); }))
        {
            *Existing = InGroup;
            return;
        }
        InOutGroups.Add(InGroup);
    }

    auto
        HasSameChannels(
            const TArray<FName>& InExpected,
            const TArray<FName>& InActual,
            FString& OutError)
        -> bool
    {
        const auto Missing = InExpected.FilterByPredicate([&InActual](FName InName) { return NOT InActual.Contains(InName); });
        const auto Extra = InActual.FilterByPredicate([&InExpected](FName InName) { return NOT InExpected.Contains(InName); });
        if (NOT Missing.IsEmpty())
        {
            OutError = FString::Printf(TEXT("The pose is missing required participant(s): %s. A whole pose covers every participant."),
                *JoinNames(Missing));
            return false;
        }
        if (NOT Extra.IsEmpty())
        {
            OutError = FString::Printf(TEXT("The pose writes channel(s) that are not participants: %s."), *JoinNames(Extra));
            return false;
        }
        return true;
    }

    auto
        CapturePoseKey(
            UCk_ProceduralAction_Data& InClip,
            FName InPhaseId,
            const FCk_ProceduralAction_PoseKey& InPose,
            FName InLabel,
            ECk_ProceduralAction_PoseProvenance InProvenance,
            FGuid& OutPoseId,
            FString& OutError)
        -> bool
    {
        const auto Channels = ck::procedural_action_editor::Get_PoseChannels(InPose);
        const auto NewId = FGuid::NewGuid();
        const auto Committed = CommitDraft(InClip, LOCTEXT("CapturePose", "Capture Action Pose"),
            [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>& InOutGroups,
                FString& OutEditError)
        {
            const auto Report = UCk_Utils_ProceduralActionBuilder_UE::Add_PoseKey(InOutDraft, InPhaseId, InPose);
            if (NOT IsAccepted(Report))
            {
                OutEditError = (HasCode(Report, TEXT("KeyTime"))
                    ? FString{TEXT("A key already exists at this time; use Update Pose to change it or Duplicate to another time. ")}
                    : FString{}) + DescribeErrors(Report.Get_Diagnostics());
                return false;
            }
            auto Group = FCk_ProceduralAction_PoseGroup{NewId, InPhaseId, InPose.Get_Time(), Channels};
            Group.Set_Label(InLabel.IsNone() ? FName{*FString::Printf(TEXT("Pose %d"), InOutGroups.Num() + 1)} : InLabel);
            Group.Set_Provenance(InProvenance);
            InOutGroups.Add(Group);
            return true;
        }, OutError);
        if (Committed)
        { OutPoseId = NewId; }
        return Committed;
    }

    // Converts one solved endpoint into its slot's declared track space; the frames match the runtime's resolution
    // (BodyLocal = the current root, RootAtStart = the preview root the action started from).
    auto
        ConvertSolvedLimb(
            const UCk_ProceduralActionPreview_Config& InConfig,
            const TArray<FCk_ProceduralAction_Phase>& InPhases,
            const FCk_ProceduralAction_Slot& InSlot,
            FName InPhaseId,
            const FTransform& InRootWorld,
            const FTransform& InBodyWorld,
            const FTransform& InEndpointWorld,
            FCk_ProceduralAction_LimbPoseTarget& OutTarget,
            FString& OutError)
        -> bool
    {
        const auto TrackOf = [&InSlot](const FCk_ProceduralAction_Phase& InPhase)
        {
            return InPhase.Get_LimbTracks().FindByPredicate([&InSlot](const FCk_ProceduralAction_LimbTrack& InTrack)
                { return InTrack.Get_Slot() == InSlot.Get_Id(); });
        };
        const auto* TargetPhase = InPhases.FindByPredicate([InPhaseId](const FCk_ProceduralAction_Phase& InPhase)
            { return InPhase.Get_Id() == InPhaseId; });
        const auto* Track = TargetPhase == nullptr ? nullptr : TrackOf(*TargetPhase);
        for (auto Index = 0; Track == nullptr && Index < InPhases.Num(); ++Index)
        { Track = TrackOf(InPhases[Index]); }
        const auto Space = Track == nullptr ? ECk_ProceduralAction_TrackSpace::BodyLocal : Track->Get_Space();
        const auto Anchor = Track == nullptr ? FName{} : Track->Get_AnchorId();

        auto Frame = FTransform::Identity;
        auto Scale = 1.0f;
        switch (Space)
        {
            case ECk_ProceduralAction_TrackSpace::BodyLocal:
                Frame = InRootWorld;
                break;
            case ECk_ProceduralAction_TrackSpace::RootAtStart:
                Frame = InConfig.Get_RootFrame();
                break;
            case ECk_ProceduralAction_TrackSpace::AnchorAtStart:
            case ECk_ProceduralAction_TrackSpace::LimbRestNormalized:
            {
                const auto Compiled = ck::procedural_action::FCompiledSlot{InSlot.Get_Id(),
                    ECk_ProceduralAction_InitialSource::CapturedAcceptedPose, false, false, FTransform::Identity, Space, Anchor};
                if (NOT ck::procedural_action_editor::ResolveSlotFrame(InConfig, Compiled, InBodyWorld, Frame, Scale))
                {
                    OutError = FString::Printf(TEXT("%s: the preview subject cannot resolve its %s frame."),
                        *InSlot.Get_Id().ToString(), *UEnum::GetValueAsString(Space));
                    return false;
                }
                break;
            }
            default:
                OutError = FString::Printf(TEXT("%s: unsupported track space."), *InSlot.Get_Id().ToString());
                return false;
        }

        const auto Position = ck::procedural_action_editor::ToTrackPosition(Frame, Scale, InEndpointWorld.GetTranslation());
        if (Position.ContainsNaN() || NOT FMath::IsFinite(Position.SizeSquared()))
        {
            OutError = FString::Printf(TEXT("%s: the converted position is malformed."), *InSlot.Get_Id().ToString());
            return false;
        }
        OutTarget = FCk_ProceduralAction_LimbPoseTarget{InSlot.Get_Id(), Position};
        const auto KeysOrientation = InSlot.Get_RequiresTerminalOrientation()
            || (Track != nullptr && NOT Track->Get_OrientationKeys().IsEmpty());
        if (KeysOrientation)
        {
            OutTarget.Set_OrientationMode(ECk_ProceduralAction_ValueMode::Set);
            OutTarget.Set_Orientation((Frame.GetRotation().Inverse() * InEndpointWorld.GetRotation()).GetNormalized());
        }
        return true;
    }

    auto
        IsRigidFinite(
            const FTransform& InTransform)
        -> bool
    {
        return NOT InTransform.ContainsNaN() && InTransform.GetRotation().IsNormalized()
            && InTransform.GetScale3D().Equals(FVector::OneVector, 1.0e-4);
    }
}

FCk_ProceduralActionAuthoringCommands::FCk_ProceduralActionAuthoringCommands(
    UCk_ProceduralAction_Data* InClip)
    : _Clip(InClip)
{}

auto
    FCk_ProceduralActionAuthoringCommands::
    GetClip(
        FString& OutError) const
    -> UCk_ProceduralAction_Data*
{
    OutError.Reset();
    auto* Clip = _Clip.Get();
    if (Clip == nullptr)
    { OutError = TEXT("The clip is no longer available."); }
    return Clip;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddPhase(
        FName InId,
        FCk_Time InDuration,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (InId.IsNone() || ck_procedural_action_authoring::HasPhase(*Clip, InId))
    { OutError = TEXT("Phase ID must be nonempty and unique."); return false; }
    if (NOT FMath::IsFinite(InDuration.Get_Seconds()) || InDuration.Get_Seconds() <= 0.0)
    { OutError = TEXT("Phase duration must be finite and positive."); return false; }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "AddPhase", "Add Action Phase")};
    Clip->Modify();
    auto Phases = Clip->Get_Phases();
    Phases.Add(FCk_ProceduralAction_Phase{InId, InDuration});
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    MoveMarker(
        FName InId,
        FName InPhaseId,
        double InNormalizedTime,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT ck_procedural_action_authoring::HasPhase(*Clip, InPhaseId)
        || NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime))
    { OutError = TEXT("Marker needs an existing phase and time in [0,1]."); return false; }
    auto Markers = Clip->Get_Markers();
    auto* Marker = Markers.FindByPredicate([InId](const auto& InMarker)
        { return InMarker.Get_Id() == InId; });
    if (Marker == nullptr)
    { OutError = TEXT("Marker ID does not exist."); return false; }
    if (Marker->Get_At().Get_PhaseId() == InPhaseId
        && FMath::IsNearlyEqual(Marker->Get_At().Get_Time().Get_Value(), InNormalizedTime))
    { return true; }
    Marker->Set_At(FCk_ProceduralAction_TimeRef{InPhaseId, FCk_FloatRange_0to1{InNormalizedTime}});
    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "MoveMarker", "Move Action Marker")};
    Clip->Modify();
    Clip->Set_Markers(Markers);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    MoveWindowEdge(
        FName InId,
        bool InStartEdge,
        FName InPhaseId,
        double InNormalizedTime,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT ck_procedural_action_authoring::HasPhase(*Clip, InPhaseId)
        || NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime))
    { OutError = TEXT("Window edge needs an existing phase and time in [0,1]."); return false; }
    auto Windows = Clip->Get_Windows();
    auto* Window = Windows.FindByPredicate([InId](const auto& InWindow)
        { return InWindow.Get_Id() == InId; });
    if (Window == nullptr)
    { OutError = TEXT("Window ID does not exist."); return false; }
    const auto Ref = FCk_ProceduralAction_TimeRef{InPhaseId, FCk_FloatRange_0to1{InNormalizedTime}};
    const auto Start = InStartEdge ? Ref : Window->Get_Start();
    const auto End = InStartEdge ? Window->Get_End() : Ref;
    if (NOT ck_procedural_action_authoring::IsLater(Clip->Get_Phases(),
        Start.Get_PhaseId(), Start.Get_Time().Get_Value(), End.Get_PhaseId(), End.Get_Time().Get_Value()))
    { OutError = TEXT("Window end must remain strictly after its start; the interval is half-open."); return false; }
    if (InStartEdge) { Window->Set_Start(Ref); }
    else { Window->Set_End(Ref); }
    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "MoveWindowEdge", "Move Action Window Edge")};
    Clip->Modify();
    Clip->Set_Windows(Windows);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    ReorderPhase(
        FName InId,
        int32 InNewIndex,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    auto Phases = Clip->Get_Phases();
    const auto OldIndex = Phases.IndexOfByPredicate([InId](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == InId; });
    if (OldIndex == INDEX_NONE || NOT Phases.IsValidIndex(InNewIndex))
    { OutError = TEXT("Phase ID or destination index is invalid."); return false; }
    if (OldIndex == InNewIndex) { return true; }

    const auto Moved = Phases[OldIndex];
    Phases.RemoveAt(OldIndex);
    Phases.Insert(Moved, InNewIndex);
    for (const auto& Window : Clip->Get_Windows())
    {
        if (NOT ck_procedural_action_authoring::IsLater(Phases,
            Window.Get_Start().Get_PhaseId(), Window.Get_Start().Get_Time().Get_Value(),
            Window.Get_End().Get_PhaseId(), Window.Get_End().Get_Time().Get_Value()))
        {
            OutError = FString::Printf(TEXT("Reordering would invert window %s."), *Window.Get_Id().ToString());
            return false;
        }
    }
    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "ReorderPhase", "Reorder Action Phase")};
    Clip->Modify();
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RemovePhase(
        FName InId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT ck_procedural_action_authoring::HasPhase(*Clip, InId))
    { OutError = TEXT("Phase ID does not exist."); return false; }
    for (const auto& Marker : Clip->Get_Markers())
    {
        if (Marker.Get_At().Get_PhaseId() == InId)
        { OutError = FString::Printf(TEXT("Phase is referenced by marker %s."), *Marker.Get_Id().ToString()); return false; }
    }
    for (const auto& Window : Clip->Get_Windows())
    {
        if (Window.Get_Start().Get_PhaseId() == InId || Window.Get_End().Get_PhaseId() == InId)
        { OutError = FString::Printf(TEXT("Phase is referenced by window %s."), *Window.Get_Id().ToString()); return false; }
    }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "RemovePhase", "Remove Action Phase")};
    Clip->Modify();
    auto Phases = Clip->Get_Phases();
    Phases.RemoveAll([InId](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == InId; });
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddSlot(
        FName InId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (InId.IsNone() || Clip->Get_Slots().ContainsByPredicate(
        [InId](const FCk_ProceduralAction_Slot& InSlot) { return InSlot.Get_Id() == InId; }))
    { OutError = TEXT("Slot ID must be nonempty and unique."); return false; }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "AddSlot", "Add Action Slot")};
    Clip->Modify();
    auto Slots = Clip->Get_Slots();
    Slots.Add(FCk_ProceduralAction_Slot{InId});
    Clip->Set_Slots(Slots);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RemoveSlot(
        FName InId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT Clip->Get_Slots().ContainsByPredicate(
        [InId](const FCk_ProceduralAction_Slot& InSlot) { return InSlot.Get_Id() == InId; }))
    { OutError = TEXT("Slot ID does not exist."); return false; }
    for (const auto& Phase : Clip->Get_Phases())
    {
        if (Phase.Get_LimbTracks().ContainsByPredicate(
            [InId](const FCk_ProceduralAction_LimbTrack& InTrack) { return InTrack.Get_Slot() == InId; }))
        { OutError = FString::Printf(TEXT("Slot is referenced by phase %s."), *Phase.Get_Id().ToString()); return false; }
    }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "RemoveSlot", "Remove Action Slot")};
    Clip->Modify();
    auto Slots = Clip->Get_Slots();
    Slots.RemoveAll([InId](const FCk_ProceduralAction_Slot& InSlot) { return InSlot.Get_Id() == InId; });
    Clip->Set_Slots(Slots);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RenameSlot(
        UCk_ProceduralActionPreview_Config* InConfig,
        FName InOldId,
        FName InNewId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT IsValid(InConfig))
    { OutError = TEXT("Select a separate preview config before renaming a slot."); return false; }
    if (InOldId.IsNone() || InNewId.IsNone() || InOldId == InNewId)
    { OutError = TEXT("Slot rename needs distinct nonempty old and new IDs."); return false; }
    const auto OldCount = Clip->Get_Slots().FilterByPredicate([InOldId](const auto& InSlot)
        { return InSlot.Get_Id() == InOldId; }).Num();
    if (OldCount != 1 || Clip->Get_Slots().ContainsByPredicate([InNewId](const auto& InSlot)
        { return InSlot.Get_Id() == InNewId; }))
    { OutError = TEXT("Old slot must exist exactly once and new slot ID must be unused."); return false; }
    const auto OldBindingCount = InConfig->Get_SlotBindings().FilterByPredicate([InOldId](const auto& InBinding)
        { return InBinding.Get_Slot() == InOldId; }).Num();
    if (OldBindingCount > 1 || InConfig->Get_SlotBindings().ContainsByPredicate([InNewId](const auto& InBinding)
        { return InBinding.Get_Slot() == InNewId; }))
    { OutError = TEXT("Preview slot bindings are ambiguous for the old or new ID."); return false; }
    if (InConfig->Get_InitialLimbEndpoints().Contains(InNewId)
        || InConfig->Get_InitialBendDirections().Contains(InNewId))
    { OutError = TEXT("Preview initial endpoint or bend already uses the new slot ID."); return false; }

    auto Slots = Clip->Get_Slots();
    auto Phases = Clip->Get_Phases();
    auto Bindings = InConfig->Get_SlotBindings();
    auto InitialEndpoints = InConfig->Get_InitialLimbEndpoints();
    auto InitialBends = InConfig->Get_InitialBendDirections();
    for (auto& Slot : Slots)
    {
        if (Slot.Get_Id() != InOldId)
        { continue; }
        auto Renamed = FCk_ProceduralAction_Slot{InNewId};
        Renamed.Set_InitialSource(Slot.Get_InitialSource());
        Renamed.Set_RequiresTerminalOrientation(Slot.Get_RequiresTerminalOrientation());
        Renamed.Set_HasExplicitRestTarget(Slot.Get_HasExplicitRestTarget());
        Renamed.Set_ExplicitRestTarget(Slot.Get_ExplicitRestTarget());
        Slot = MoveTemp(Renamed);
    }
    for (auto& Phase : Phases)
    {
        auto Tracks = Phase.Get_LimbTracks();
        auto Changed = false;
        for (auto& Track : Tracks)
        {
            if (Track.Get_Slot() != InOldId)
            { continue; }
            auto Renamed = FCk_ProceduralAction_LimbTrack{InNewId};
            Renamed.Set_Space(Track.Get_Space());
            Renamed.Set_AnchorId(Track.Get_AnchorId());
            Renamed.Set_Interpolation(Track.Get_Interpolation());
            Renamed.Set_ReachPolicy(Track.Get_ReachPolicy());
            Renamed.Set_ClearanceResponse(Track.Get_ClearanceResponse());
            Renamed.Set_PositionKeys(Track.Get_PositionKeys());
            Renamed.Set_OrientationKeys(Track.Get_OrientationKeys());
            Renamed.Set_BendKeys(Track.Get_BendKeys());
            Track = MoveTemp(Renamed);
            Changed = true;
        }
        if (Changed)
        { Phase.Set_LimbTracks(Tracks); }
    }
    for (auto& Binding : Bindings)
    {
        if (Binding.Get_Slot() == InOldId)
        { Binding = FCk_ProceduralAction_SlotBinding{InNewId, Binding.Get_LimbId()}; }
    }
    if (const auto* Endpoint = InitialEndpoints.Find(InOldId))
    {
        const auto Value = *Endpoint;
        InitialEndpoints.Remove(InOldId);
        InitialEndpoints.Add(InNewId, Value);
    }
    if (const auto* Bend = InitialBends.Find(InOldId))
    {
        const auto Value = *Bend;
        InitialBends.Remove(InOldId);
        InitialBends.Add(InNewId, Value);
    }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "RenameSlot", "Rename Action Slot")};
    Clip->Modify();
    InConfig->Modify();
    Clip->Set_Slots(Slots);
    Clip->Set_Phases(Phases);
    InConfig->Set_SlotBindings(Bindings);
    InConfig->Set_InitialLimbEndpoints(InitialEndpoints);
    InConfig->Set_InitialBendDirections(InitialBends);
    Clip->PostEditChange();
    InConfig->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddLimbTrack(
        FName InPhaseId,
        FName InSlotId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT Clip->Get_Slots().ContainsByPredicate(
        [InSlotId](const FCk_ProceduralAction_Slot& InSlot) { return InSlot.Get_Id() == InSlotId; }))
    { OutError = TEXT("The track slot is not declared."); return false; }
    auto Phases = Clip->Get_Phases();
    auto* Phase = Phases.FindByPredicate([InPhaseId](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (Phase == nullptr)
    { OutError = TEXT("Phase ID does not exist."); return false; }
    if (Phase->Get_LimbTracks().ContainsByPredicate(
        [InSlotId](const FCk_ProceduralAction_LimbTrack& InTrack) { return InTrack.Get_Slot() == InSlotId; }))
    { OutError = TEXT("This phase already has a track for the slot."); return false; }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "AddTrack", "Add Action Limb Track")};
    Clip->Modify();
    auto Tracks = Phase->Get_LimbTracks();
    Tracks.Add(FCk_ProceduralAction_LimbTrack{InSlotId});
    Phase->Set_LimbTracks(Tracks);
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddBodyKey(
        FName InPhaseId,
        double InNormalizedTime,
        const FTransform& InValue,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT Clip->Get_BodyChannel().Get_Enabled())
    { OutError = TEXT("Enable the body channel before adding body keys."); return false; }
    if (NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime))
    { OutError = TEXT("Key time must be in [0,1]."); return false; }
    if (InValue.ContainsNaN() || NOT InValue.GetScale3D().Equals(FVector::OneVector, 1.0e-4))
    { OutError = TEXT("Body key must be a finite rigid transform."); return false; }
    auto Phases = Clip->Get_Phases();
    auto* Phase = Phases.FindByPredicate([InPhaseId](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (Phase == nullptr)
    { OutError = TEXT("Phase ID does not exist."); return false; }
    auto Keys = Phase->Get_BodyKeys();
    if (Keys.ContainsByPredicate([InNormalizedTime](const FCk_ProceduralAction_TransformKey& InKey)
        { return FMath::IsNearlyEqual(InKey.Get_Time().Get_Value(), InNormalizedTime, 1.0e-6); }))
    { OutError = TEXT("A body key already exists at this time."); return false; }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "AddBodyKey", "Add Action Body Key")};
    Clip->Modify();
    Keys.Add(FCk_ProceduralAction_TransformKey{FCk_FloatRange_0to1{InNormalizedTime},
        InValue.GetTranslation(), InValue.GetRotation()});
    Keys.Sort([](const auto& InLeft, const auto& InRight)
        { return InLeft.Get_Time().Get_Value() < InRight.Get_Time().Get_Value(); });
    Phase->Set_BodyKeys(Keys);
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddPositionKey(
        FName InPhaseId,
        FName InSlotId,
        double InNormalizedTime,
        const FVector& InValue,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime) || InValue.ContainsNaN())
    { OutError = TEXT("Position key needs finite coordinates and time in [0,1]."); return false; }
    auto Phases = Clip->Get_Phases();
    auto* Phase = Phases.FindByPredicate([InPhaseId](const FCk_ProceduralAction_Phase& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (Phase == nullptr)
    { OutError = TEXT("Phase ID does not exist."); return false; }
    auto Tracks = Phase->Get_LimbTracks();
    auto* Track = Tracks.FindByPredicate([InSlotId](const FCk_ProceduralAction_LimbTrack& InTrack)
        { return InTrack.Get_Slot() == InSlotId; });
    if (Track == nullptr)
    { OutError = TEXT("This phase has no track for the slot."); return false; }
    auto Keys = Track->Get_PositionKeys();
    if (Keys.ContainsByPredicate([InNormalizedTime](const FCk_ProceduralAction_VectorKey& InKey)
        { return FMath::IsNearlyEqual(InKey.Get_Time().Get_Value(), InNormalizedTime, 1.0e-6); }))
    { OutError = TEXT("A position key already exists at this time."); return false; }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "AddPositionKey", "Add Action Position Key")};
    Clip->Modify();
    Keys.Add(FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{InNormalizedTime}, InValue});
    Keys.Sort([](const auto& InLeft, const auto& InRight)
        { return InLeft.Get_Time().Get_Value() < InRight.Get_Time().Get_Value(); });
    Track->Set_PositionKeys(Keys);
    Phase->Set_LimbTracks(Tracks);
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetPositionKey(
        FName InPhaseId,
        FName InSlotId,
        double InNormalizedTime,
        const FVector& InValue,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime) || InValue.ContainsNaN())
    { OutError = TEXT("Endpoint gizmo needs finite coordinates and time in [0,1]."); return false; }
    auto Phases = Clip->Get_Phases();
    auto* Phase = Phases.FindByPredicate([InPhaseId](const auto& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (Phase == nullptr)
    { OutError = TEXT("Selected phase no longer exists."); return false; }
    auto Tracks = Phase->Get_LimbTracks();
    auto* Track = Tracks.FindByPredicate([InSlotId](const auto& InTrack)
        { return InTrack.Get_Slot() == InSlotId; });
    if (Track == nullptr)
    { OutError = TEXT("Selected limb track no longer exists."); return false; }
    auto Keys = Track->Get_PositionKeys();
    auto* Existing = Keys.FindByPredicate([InNormalizedTime](const auto& InKey)
        { return FMath::IsNearlyEqual(InKey.Get_Time().Get_Value(), InNormalizedTime, 1.0e-6); });
    if (Existing != nullptr)
    { Existing->Set_Value(InValue); }
    else
    { Keys.Add(FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{InNormalizedTime}, InValue}); }
    Keys.Sort([](const auto& InLeft, const auto& InRight)
        { return InLeft.Get_Time().Get_Value() < InRight.Get_Time().Get_Value(); });
    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "SetEndpoint", "Move Action Endpoint")};
    Clip->Modify();
    Track->Set_PositionKeys(Keys);
    Phase->Set_LimbTracks(Tracks);
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetBendKey(
        FName InPhaseId,
        FName InSlotId,
        double InNormalizedTime,
        const FVector& InValue,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime)
        || InValue.ContainsNaN() || InValue.SizeSquared() <= 1.0e-8)
    { OutError = TEXT("Bend gizmo needs a finite nonzero direction and time in [0,1]."); return false; }
    auto Phases = Clip->Get_Phases();
    auto* Phase = Phases.FindByPredicate([InPhaseId](const auto& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (Phase == nullptr)
    { OutError = TEXT("Selected phase no longer exists."); return false; }
    auto Tracks = Phase->Get_LimbTracks();
    auto* Track = Tracks.FindByPredicate([InSlotId](const auto& InTrack)
        { return InTrack.Get_Slot() == InSlotId; });
    if (Track == nullptr)
    { OutError = TEXT("Selected limb track no longer exists."); return false; }
    auto Keys = Track->Get_BendKeys();
    auto* Existing = Keys.FindByPredicate([InNormalizedTime](const auto& InKey)
        { return FMath::IsNearlyEqual(InKey.Get_Time().Get_Value(), InNormalizedTime, 1.0e-6); });
    if (Existing != nullptr)
    { Existing->Set_Value(InValue.GetSafeNormal()); }
    else
    { Keys.Add(FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{InNormalizedTime}, InValue.GetSafeNormal()}); }
    Keys.Sort([](const auto& InLeft, const auto& InRight)
        { return InLeft.Get_Time().Get_Value() < InRight.Get_Time().Get_Value(); });
    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "SetBend", "Move Action Bend Guide")};
    Clip->Modify();
    Track->Set_BendKeys(Keys);
    Phase->Set_LimbTracks(Tracks);
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddMarker(
        FName InId,
        FName InPhaseId,
        double InNormalizedTime,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (InId.IsNone() || ck_procedural_action_authoring::HasEventId(*Clip, InId))
    { OutError = TEXT("Event ID must be nonempty and unique."); return false; }
    if (NOT ck_procedural_action_authoring::HasPhase(*Clip, InPhaseId)
        || NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime))
    { OutError = TEXT("Marker needs an existing phase and time in [0,1]."); return false; }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "AddMarker", "Add Action Marker")};
    Clip->Modify();
    auto Markers = Clip->Get_Markers();
    Markers.Add(FCk_ProceduralAction_Marker{InId,
        FCk_ProceduralAction_TimeRef{InPhaseId, FCk_FloatRange_0to1{InNormalizedTime}}});
    Clip->Set_Markers(Markers);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddWindow(
        FName InId,
        FName InStartPhaseId,
        double InStartTime,
        FName InEndPhaseId,
        double InEndTime,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (InId.IsNone() || ck_procedural_action_authoring::HasEventId(*Clip, InId))
    { OutError = TEXT("Event ID must be nonempty and unique."); return false; }
    if (NOT ck_procedural_action_authoring::IsNormalized(InStartTime)
        || NOT ck_procedural_action_authoring::IsNormalized(InEndTime)
        || NOT ck_procedural_action_authoring::IsLater(Clip->Get_Phases(), InStartPhaseId, InStartTime,
            InEndPhaseId, InEndTime))
    { OutError = TEXT("Window end must follow its start across existing phases."); return false; }

    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "AddWindow", "Add Action Window")};
    Clip->Modify();
    auto Windows = Clip->Get_Windows();
    Windows.Add(FCk_ProceduralAction_Window{InId,
        FCk_ProceduralAction_TimeRef{InStartPhaseId, FCk_FloatRange_0to1{InStartTime}},
        FCk_ProceduralAction_TimeRef{InEndPhaseId, FCk_FloatRange_0to1{InEndTime}}});
    Clip->Set_Windows(Windows);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetTrackSpace(
        FName InPhaseId,
        FName InSlotId,
        ECk_ProceduralAction_TrackSpace InSpace,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    auto Phases = Clip->Get_Phases();
    auto* Phase = Phases.FindByPredicate([InPhaseId](const auto& InPhase)
        { return InPhase.Get_Id() == InPhaseId; });
    if (Phase == nullptr)
    { OutError = TEXT("Phase ID does not exist."); return false; }
    if (NOT Phase->Get_LimbTracks().ContainsByPredicate([InSlotId](const auto& InTrack)
        { return InTrack.Get_Slot() == InSlotId; }))
    { OutError = TEXT("Track does not exist for this slot and phase."); return false; }
    if (InSpace != ECk_ProceduralAction_TrackSpace::BodyLocal
        && InSpace != ECk_ProceduralAction_TrackSpace::RootAtStart
        && InSpace != ECk_ProceduralAction_TrackSpace::LimbRestNormalized)
    { OutError = TEXT("Choose BodyLocal, RootAtStart, or LimbRestNormalized here; named anchors need an explicit ID in Details."); return false; }
    auto Changed = false;
    for (auto& CandidatePhase : Phases)
    {
        auto Tracks = CandidatePhase.Get_LimbTracks();
        auto* Track = Tracks.FindByPredicate([InSlotId](const auto& InTrack)
            { return InTrack.Get_Slot() == InSlotId; });
        if (Track == nullptr)
        { continue; }
        if (NOT Track->Get_PositionKeys().IsEmpty() || NOT Track->Get_OrientationKeys().IsEmpty()
            || NOT Track->Get_BendKeys().IsEmpty())
        { OutError = TEXT("Set slot space before keying any phase; changing it now would reinterpret existing coordinates."); return false; }
        Changed |= Track->Get_Space() != InSpace || NOT Track->Get_AnchorId().IsNone();
        Track->Set_Space(InSpace);
        Track->Set_AnchorId(NAME_None);
        CandidatePhase.Set_LimbTracks(Tracks);
    }
    if (NOT Changed)
    { return true; }
    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "SetTrackSpace", "Set Action Track Space")};
    Clip->Modify();
    Clip->Set_Phases(Phases);
    Clip->PostEditChange();
    return true;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    MirrorSlot(
        FName InSourceSlot,
        FName InDestinationSlot,
        EAxis::Type InPlaneNormal,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    auto Plane = ECk_ProceduralAction_MirrorPlane::XZ;
    switch (InPlaneNormal)
    {
        case EAxis::X: Plane = ECk_ProceduralAction_MirrorPlane::YZ; break;
        case EAxis::Y: Plane = ECk_ProceduralAction_MirrorPlane::XZ; break;
        case EAxis::Z: Plane = ECk_ProceduralAction_MirrorPlane::XY; break;
        default:
            OutError = TEXT("Mirror needs an X, Y, or Z reflection plane normal.");
            return false;
    }
    return ck_procedural_action_authoring::CommitDraft(*Clip, LOCTEXT("MirrorSlot", "Mirror Action Slot"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>&, FString& OutEditError)
    {
        const auto Report = UCk_Utils_ProceduralActionBuilder_UE::Mirror_Slot(InOutDraft,
            FCk_ProceduralAction_MirrorSpec{InSourceSlot, InDestinationSlot, Plane});
        if (NOT ck_procedural_action_authoring::IsAccepted(Report))
        {
            OutEditError = ck_procedural_action_authoring::DescribeErrors(Report.Get_Diagnostics());
            return false;
        }
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    CapturePose(
        FName InPhaseId,
        const FCk_ProceduralAction_PoseKey& InPose,
        const TArray<FName>& InParticipants,
        FName InLabel,
        FGuid& OutPoseId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    const auto Channels = ck::procedural_action_editor::Get_PoseChannels(InPose);
    if (InParticipants.IsEmpty())
    {
        OutError = TEXT("Select at least one participating channel before capturing a pose.");
        return false;
    }
    if (NOT ck_procedural_action_authoring::HasSameChannels(InParticipants, Channels, OutError))
    { return false; }
    return ck_procedural_action_authoring::CapturePoseKey(*Clip, InPhaseId, InPose, InLabel,
        ECk_ProceduralAction_PoseProvenance::Requested, OutPoseId, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    UpdatePose(
        const FGuid& InPoseId,
        const FCk_ProceduralAction_PoseKey& InPose,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    auto View = FCk_ProceduralActionPoseGroupView{};
    if (NOT ck_procedural_action_authoring::FindEditablePose(*Clip, InPoseId, View, OutError))
    { return false; }
    auto Group = View.Get_Group();
    if (NOT ck_procedural_action_authoring::HasSameChannels(Group.Get_Channels(),
        ck::procedural_action_editor::Get_PoseChannels(InPose), OutError))
    { return false; }
    auto Pose = InPose;
    Pose.Set_Time(Group.Get_Time());
    Group.Set_Provenance(ECk_ProceduralAction_PoseProvenance::Requested);
    return ck_procedural_action_authoring::CommitDraft(*Clip, LOCTEXT("UpdatePose", "Update Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>& InOutGroups,
            FString& OutEditError)
    {
        const auto Report = UCk_Utils_ProceduralActionBuilder_UE::Replace_PoseKey(InOutDraft, Group.Get_PhaseId(), Pose);
        if (NOT ck_procedural_action_authoring::IsAccepted(Report))
        {
            OutEditError = ck_procedural_action_authoring::DescribeErrors(Report.Get_Diagnostics());
            return false;
        }
        ck_procedural_action_authoring::UpsertGroup(InOutGroups, Group);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    DuplicatePose(
        const FGuid& InPoseId,
        FName InPhaseId,
        double InNormalizedTime,
        FGuid& OutPoseId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime))
    {
        OutError = TEXT("Duplicate needs a time in [0,1].");
        return false;
    }
    auto View = FCk_ProceduralActionPoseGroupView{};
    if (NOT ck_procedural_action_authoring::FindEditablePose(*Clip, InPoseId, View, OutError))
    { return false; }
    const auto& Source = View.Get_Group();
    auto Pose = FCk_ProceduralAction_PoseKey{};
    if (NOT ck::procedural_action_editor::ReadPoseKey(Clip->Get_Phases(), Source, Pose, OutError))
    { return false; }
    const auto Tangents = ck_procedural_action_authoring::ReadTangents(Clip->Get_Phases(), Source);
    Pose.Set_Time(FCk_FloatRange_0to1{InNormalizedTime});
    const auto NewId = FGuid::NewGuid();
    const auto Committed = ck_procedural_action_authoring::CommitDraft(*Clip, LOCTEXT("DuplicatePose", "Duplicate Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>& InOutGroups,
            FString& OutEditError)
    {
        const auto Report = UCk_Utils_ProceduralActionBuilder_UE::Add_PoseKey(InOutDraft, InPhaseId, Pose);
        if (NOT ck_procedural_action_authoring::IsAccepted(Report))
        {
            OutEditError = ck_procedural_action_authoring::DescribeErrors(Report.Get_Diagnostics());
            return false;
        }
        ck_procedural_action_authoring::WriteTangents(InOutDraft.Get_Phases(), InPhaseId, InNormalizedTime,
            Source.Get_Channels(), Tangents);
        auto Copy = FCk_ProceduralAction_PoseGroup{NewId, InPhaseId, FCk_FloatRange_0to1{InNormalizedTime}, Source.Get_Channels()};
        Copy.Set_Label(Source.Get_Label());
        Copy.Set_Provenance(Source.Get_Provenance());
        InOutGroups.Add(Copy);
        return true;
    }, OutError);
    if (Committed)
    { OutPoseId = NewId; }
    return Committed;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    MovePose(
        const FGuid& InPoseId,
        FName InPhaseId,
        double InNormalizedTime,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT ck_procedural_action_authoring::IsNormalized(InNormalizedTime))
    {
        OutError = TEXT("Move needs a time in [0,1].");
        return false;
    }
    auto View = FCk_ProceduralActionPoseGroupView{};
    if (NOT ck_procedural_action_authoring::FindEditablePose(*Clip, InPoseId, View, OutError))
    { return false; }
    const auto Source = View.Get_Group();
    if (Source.Get_PhaseId() == InPhaseId && Source.Get_Time().Get_Value() == InNormalizedTime)
    { return true; }
    auto Pose = FCk_ProceduralAction_PoseKey{};
    if (NOT ck::procedural_action_editor::ReadPoseKey(Clip->Get_Phases(), Source, Pose, OutError))
    { return false; }
    const auto Tangents = ck_procedural_action_authoring::ReadTangents(Clip->Get_Phases(), Source);
    Pose.Set_Time(FCk_FloatRange_0to1{InNormalizedTime});
    return ck_procedural_action_authoring::CommitDraft(*Clip, LOCTEXT("MovePose", "Move Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>& InOutGroups,
            FString& OutEditError)
    {
        ck_procedural_action_authoring::RemovePoseKeys(InOutDraft.Get_Phases(), Source);
        const auto Report = UCk_Utils_ProceduralActionBuilder_UE::Add_PoseKey(InOutDraft, InPhaseId, Pose);
        if (NOT ck_procedural_action_authoring::IsAccepted(Report))
        {
            OutEditError = ck_procedural_action_authoring::DescribeErrors(Report.Get_Diagnostics());
            return false;
        }
        ck_procedural_action_authoring::WriteTangents(InOutDraft.Get_Phases(), InPhaseId, InNormalizedTime,
            Source.Get_Channels(), Tangents);
        auto Moved = Source;
        Moved.Set_PhaseId(InPhaseId);
        Moved.Set_Time(FCk_FloatRange_0to1{InNormalizedTime});
        ck_procedural_action_authoring::UpsertGroup(InOutGroups, Moved);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    DeletePose(
        const FGuid& InPoseId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    auto View = FCk_ProceduralActionPoseGroupView{};
    if (NOT ck::procedural_action_editor::FindPoseGroup(*Clip, InPoseId, View))
    {
        OutError = FString::Printf(TEXT("Pose %s does not exist in this clip."), *InPoseId.ToString());
        return false;
    }
    const auto Group = View.Get_Group();
    return ck_procedural_action_authoring::CommitDraft(*Clip, LOCTEXT("DeletePose", "Delete Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>& InOutGroups, FString&)
    {
        ck_procedural_action_authoring::RemovePoseKeys(InOutDraft.Get_Phases(), Group);
        InOutGroups.RemoveAll([&Group](const FCk_ProceduralAction_PoseGroup& InCandidate)
            { return InCandidate.Get_Id() == Group.Get_Id(); });
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    CaptureSolvedPose(
        const FCk_ProceduralActionSolvedSnapshot& InSnapshot,
        const FCk_ProceduralActionSolvedCaptureTarget& InTarget,
        FGuid& OutPoseId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    const auto* Config = InTarget.Get_Config();
    if (Config == nullptr)
    {
        OutError = TEXT("Select the preview subject the Solver ran before capturing a solved pose.");
        return false;
    }
    if (NOT InSnapshot.Get_Ready())
    {
        OutError = TEXT("The Solver preview is not ready: pause it at a step with no seek or preparation pending.");
        return false;
    }
    if (NOT InSnapshot.Get_Settled())
    {
        OutError = TEXT("The Solver pose is not settled: its last accepted pose is not the subject's latest settled revision.");
        return false;
    }
    if (InSnapshot.Get_PreviewRevision() != InTarget.Get_SessionRevision())
    {
        OutError = FString::Printf(TEXT("The Solver recording belongs to revision %llu but the clip and preview are at revision %llu. Restart Solver first."),
            InSnapshot.Get_PreviewRevision(), InTarget.Get_SessionRevision());
        return false;
    }
    if (InTarget.Get_Channels().IsEmpty())
    {
        OutError = TEXT("Select at least one channel to capture from the Solver.");
        return false;
    }

    const auto& Solved = InSnapshot.Get_Pose();
    const auto RootWorld = Solved.Get_RootFrame();
    if (NOT ck_procedural_action_authoring::IsRigidFinite(RootWorld))
    {
        OutError = TEXT("The solved snapshot is malformed: its root frame is not a finite rigid transform.");
        return false;
    }
    const auto PresentationLocal = Config->Get_PresentationFrame().GetRelativeTransform(Config->Get_RootFrame());
    const auto BodyWorld = Solved.Get_HasBodyPresentation() ? Solved.Get_BodyPresentation() : PresentationLocal * RootWorld;

    auto Pose = FCk_ProceduralAction_PoseKey{InTarget.Get_Time()};
    auto Limbs = TArray<FCk_ProceduralAction_LimbPoseTarget>{};
    auto Missing = TArray<FName>{};
    auto Malformed = TArray<FName>{};
    for (const auto Channel : InTarget.Get_Channels())
    {
        if (Channel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
        {
            if (NOT Solved.Get_HasBodyPresentation())
            {
                Missing.Add(Channel);
                continue;
            }
            if (Config->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking)
            {
                OutError = TEXT("Solved body capture is not available for a walking preview subject.");
                return false;
            }
            const auto ComposedLocal = Solved.Get_BodyPresentation().GetRelativeTransform(RootWorld);
            const auto Offset = FTransform{(PresentationLocal.GetRotation().Inverse() * ComposedLocal.GetRotation()).GetNormalized(),
                ComposedLocal.GetTranslation() - PresentationLocal.GetTranslation()};
            if (NOT ck_procedural_action_authoring::IsRigidFinite(Solved.Get_BodyPresentation())
                || NOT ck_procedural_action_authoring::IsRigidFinite(Offset))
            {
                Malformed.Add(Channel);
                continue;
            }
            Pose.Set_BodyMode(ECk_ProceduralAction_ValueMode::Set);
            Pose.Set_Body(Offset);
            continue;
        }
        const auto* Slot = Clip->Get_Slots().FindByPredicate([Channel](const FCk_ProceduralAction_Slot& InSlot)
            { return InSlot.Get_Id() == Channel; });
        const auto* Binding = Config->Get_SlotBindings().FindByPredicate([Channel](const FCk_ProceduralAction_SlotBinding& InBinding)
            { return InBinding.Get_Slot() == Channel; });
        const auto* Limb = Binding == nullptr ? nullptr : Solved.Get_Limbs().FindByPredicate(
            [Binding](const FCk_ProceduralArticulation_LimbPose& InLimb) { return InLimb.Get_Id() == Binding->Get_LimbId(); });
        if (Slot == nullptr || Limb == nullptr)
        {
            Missing.Add(Channel);
            continue;
        }
        if (NOT ck_procedural_action_authoring::IsRigidFinite(Limb->Get_AcceptedEndpoint()))
        {
            Malformed.Add(Channel);
            continue;
        }
        auto Target = FCk_ProceduralAction_LimbPoseTarget{};
        auto ConversionError = FString{};
        if (NOT ck_procedural_action_authoring::ConvertSolvedLimb(*Config, Clip->Get_Phases(), *Slot, InTarget.Get_PhaseId(),
            RootWorld, BodyWorld, Limb->Get_AcceptedEndpoint(), Target, ConversionError))
        {
            OutError = FString::Printf(TEXT("The solved snapshot cannot be converted: %s Nothing was written."), *ConversionError);
            return false;
        }
        Limbs.Add(MoveTemp(Target));
    }
    if (NOT Missing.IsEmpty())
    {
        OutError = FString::Printf(TEXT("The solved snapshot has no complete evidence for: %s. Nothing was written."),
            *ck_procedural_action_authoring::JoinNames(Missing));
        return false;
    }
    if (NOT Malformed.IsEmpty())
    {
        OutError = FString::Printf(TEXT("The solved snapshot is malformed for: %s (non-finite or non-rigid geometry). Nothing was written."),
            *ck_procedural_action_authoring::JoinNames(Malformed));
        return false;
    }
    Pose.Set_Limbs(Limbs);
    return ck_procedural_action_authoring::CapturePoseKey(*Clip, InTarget.Get_PhaseId(), Pose, InTarget.Get_Label(),
        ECk_ProceduralAction_PoseProvenance::Solved, OutPoseId, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    CreateFromRecipe(
        const FCk_ProceduralAction_RecipeSpec& InRecipe,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT Clip->Get_Phases().IsEmpty() || NOT Clip->Get_Slots().IsEmpty() || Clip->Get_BodyChannel().Get_Enabled()
        || NOT Clip->Get_Markers().IsEmpty() || NOT Clip->Get_Windows().IsEmpty())
    {
        OutError = TEXT("A recipe creates a new motion and never overwrites authored content; start from an empty clip.");
        return false;
    }
    auto Recipe = FCk_ProceduralAction_ClipDraft{};
    const auto RecipeReport = UCk_Utils_ProceduralActionBuilder_UE::Make_FromRecipe(InRecipe, Recipe);
    if (NOT ck_procedural_action_authoring::IsAccepted(RecipeReport))
    {
        OutError = ck_procedural_action_authoring::DescribeErrors(RecipeReport.Get_Diagnostics());
        return false;
    }
    auto Derived = TArray<FCk_ProceduralActionPoseGroupView>{};
    ck::procedural_action_editor::DerivePoseGroups(Recipe.Get_Phases(), {}, Derived);
    return ck_procedural_action_authoring::CommitDraft(*Clip, LOCTEXT("CreateFromRecipe", "Create Action From Recipe"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>& InOutGroups, FString&)
    {
        InOutDraft = Recipe;
        InOutGroups.Reset();
        for (const auto& View : Derived)
        {
            const auto& Source = View.Get_Group();
            auto Group = FCk_ProceduralAction_PoseGroup{FGuid::NewGuid(), Source.Get_PhaseId(), Source.Get_Time(),
                Source.Get_Channels()};
            Group.Set_Label(Source.Get_Label());
            InOutGroups.Add(Group);
        }
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetPhaseDuration(
        FName InPhaseId,
        FCk_Time InDuration,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT FMath::IsFinite(InDuration.Get_Seconds()) || InDuration <= FCk_Time{})
    {
        OutError = TEXT("A phase duration must be a finite time greater than zero.");
        return false;
    }
    return ck_procedural_action_authoring::CommitDraft(*Clip, LOCTEXT("SetPhaseDuration", "Set Action Phase Duration"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>&, FString& OutEditError)
    {
        auto* Phase = ck_procedural_action_authoring::FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        {
            OutEditError = FString::Printf(TEXT("Phase %s does not exist in this clip."), *InPhaseId.ToString());
            return false;
        }
        Phase->Set_Duration(InDuration);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetPhaseTiming(
        FName InPhaseId,
        ECk_ProceduralAction_Easing InEasing,
        const FCk_ProceduralAction_ProgressCurve& InProgress,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    return ck_procedural_action_authoring::CommitDraft(*Clip, LOCTEXT("SetPhaseTiming", "Set Action Phase Timing"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, TArray<FCk_ProceduralAction_PoseGroup>&, FString& OutEditError)
    {
        auto* Phase = ck_procedural_action_authoring::FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        {
            OutEditError = FString::Printf(TEXT("Phase %s does not exist in this clip."), *InPhaseId.ToString());
            return false;
        }
        Phase->Set_Easing(InEasing);
        const auto Report = UCk_Utils_ProceduralActionBuilder_UE::Set_PhaseProgress(InOutDraft, InPhaseId, InProgress);
        if (NOT ck_procedural_action_authoring::IsAccepted(Report))
        {
            OutEditError = ck_procedural_action_authoring::DescribeErrors(Report.Get_Diagnostics());
            return false;
        }
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RetargetSlot(
        UCk_ProceduralActionPreview_Config* InConfig,
        FName InSlotId,
        FName InLimbId,
        FString& OutError)
    -> bool
{
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (InConfig == nullptr)
    { OutError = TEXT("Select a separate preview subject config before retargeting."); return false; }
    if (InSlotId.IsNone() || InLimbId.IsNone()
        || NOT Clip->Get_Slots().ContainsByPredicate([InSlotId](const auto& InSlot)
            { return InSlot.Get_Id() == InSlotId; }))
    { OutError = TEXT("Retargeting needs a declared clip slot and named limb ID."); return false; }
    const auto Walking = InConfig->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking;
    const auto Exists = Walking
        ? InConfig->Get_WalkingRig() != nullptr
            && InConfig->Get_WalkingRig()->Get_Legs().ContainsByPredicate([InLimbId](const auto& InLeg)
                { return InLeg.Get_Id() == InLimbId; })
        : InConfig->Get_Layout().Get_Limbs().ContainsByPredicate([InLimbId](const auto& InLimb)
            { return InLimb.Get_Id() == InLimbId; });
    if (NOT Exists || NOT InConfig->Get_LimbBindings().ContainsByPredicate([InLimbId](const auto& InBinding)
        { return InBinding.Get_LimbId() == InLimbId; }))
    { OutError = TEXT("Target limb must exist in the preview topology and have an explicit visual/control binding."); return false; }
    auto Bindings = InConfig->Get_SlotBindings();
    if (Bindings.ContainsByPredicate([InSlotId, InLimbId](const auto& InBinding)
        { return InBinding.Get_Slot() != InSlotId && InBinding.Get_LimbId() == InLimbId; }))
    { OutError = TEXT("Target limb is already bound to a different slot."); return false; }
    const auto Index = Bindings.IndexOfByPredicate([InSlotId](const auto& InBinding)
        { return InBinding.Get_Slot() == InSlotId; });
    if (Index != INDEX_NONE && Bindings[Index].Get_LimbId() == InLimbId)
    { return true; }
    const FScopedTransaction Transaction{NSLOCTEXT("CkProceduralActionEditor", "RetargetSlot", "Retarget Action Preview Slot")};
    InConfig->Modify();
    if (Index == INDEX_NONE)
    { Bindings.Add(FCk_ProceduralAction_SlotBinding{InSlotId, InLimbId}); }
    else
    { Bindings[Index] = FCk_ProceduralAction_SlotBinding{InSlotId, InLimbId}; }
    InConfig->Set_SlotBindings(Bindings);
    InConfig->PostEditChange();
    return true;
}

#undef LOCTEXT_NAMESPACE
