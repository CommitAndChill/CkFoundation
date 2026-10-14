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
    // The editor-only metadata a command may change beside the draft, in the same transaction.
    struct FEditorMetadata
    {
        TArray<FCk_ProceduralAction_PoseGroup> Groups;
        FCk_ProceduralAction_EditorSubject Subject;
    };

    using FDraftEdit = TFunctionRef<bool(FCk_ProceduralAction_ClipDraft&, FEditorMetadata&, FString&)>;

    auto
        IsNormalized(
            double InTime)
        -> bool
    {
        return FMath::IsFinite(InTime) && InTime >= 0.0 && InTime <= 1.0;
    }

    auto
        IsAccepted(
            const FCk_ProceduralAction_ValidationReport& InReport)
        -> bool
    { return InReport.Get_Status() == ECk_ProceduralAction_ReportStatus::Valid; }

    auto
        DescribeErrors(
            const TArray<FCk_ProceduralAction_Diagnostic>& InDiagnostics)
        -> FString
    {
        auto Text = FString{};
        for (const auto& Diagnostic : InDiagnostics)
        {
            if (Diagnostic.Get_Severity() != ECk_ProceduralAction_DiagnosticSeverity::Error)
            { continue; }
            Text += FString::Printf(TEXT("%s%s."), Text.IsEmpty() ? TEXT("") : TEXT(" "),
                *ck::procedural_action_editor::Describe(Diagnostic));
        }
        return Text.IsEmpty() ? FString{TEXT("The edit was rejected without a field diagnostic.")} : Text;
    }

    auto
        Reject(
            const FCk_ProceduralAction_ValidationReport& InReport,
            FString& OutError)
        -> bool
    {
        if (IsAccepted(InReport))
        { return true; }
        OutError = DescribeErrors(InReport.Get_Diagnostics());
        return false;
    }

    auto
        Refuse(
            const TCHAR* InReason,
            FString& OutError)
        -> bool
    {
        OutError = InReason;
        return false;
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

    template <typename TStruct>
    auto
        IsSame(
            const TStruct& InA,
            const TStruct& InB)
        -> bool
    { return TStruct::StaticStruct()->CompareScriptStruct(&InA, &InB, PPF_None); }

    auto
        IsSame(
            const TArray<FCk_ProceduralAction_PoseGroup>& InA,
            const TArray<FCk_ProceduralAction_PoseGroup>& InB)
        -> bool
    {
        if (InA.Num() != InB.Num())
        { return false; }
        for (auto Index = 0; Index < InA.Num(); ++Index)
        {
            if (NOT IsSame(InA[Index], InB[Index]))
            { return false; }
        }
        return true;
    }

    // Draft -> edit -> Write_Into, keys and editor metadata in one transaction. The edit validates every input before the
    // transaction opens, so a refused edit neither dirties the package nor leaves an empty undo entry, and an edit that
    // changes nothing opens no transaction. InAlsoWrite changes other objects (a scenario config) inside the same
    // transaction. The result need not compile: Play stays gated on Compile.
    auto
        CommitDraft(
            UCk_ProceduralAction_Data& InClip,
            const FText& InDescription,
            FDraftEdit InEdit,
            FString& OutError,
            TFunction<void()> InAlsoWrite = {})
        -> bool
    {
        auto Draft = FCk_ProceduralAction_ClipDraft{};
        if (NOT Reject(UCk_Utils_ProceduralActionBuilder_UE::Make_Draft_FromData(&InClip, Draft), OutError))
        { return false; }
        const auto Original = Draft;
        auto Metadata = FEditorMetadata{InClip.Get_EditorPoseGroups(), InClip.Get_EditorSubject()};
        if (NOT InEdit(Draft, Metadata, OutError))
        { return false; }
        const auto Unchanged = IsSame(Draft, Original) && IsSame(Metadata.Groups, InClip.Get_EditorPoseGroups())
            && IsSame(Metadata.Subject, InClip.Get_EditorSubject()) && NOT InAlsoWrite;
        if (Unchanged)
        { return true; }

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
        InClip.Set_EditorPoseGroups(Metadata.Groups);
        InClip.Set_EditorSubject(Metadata.Subject);
        if (InAlsoWrite)
        { InAlsoWrite(); }
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

    auto
        FindTrack(
            FCk_ProceduralAction_Phase& InPhase,
            FName InSlotId)
        -> FCk_ProceduralAction_LimbTrack*
    {
        return InPhase.Get_LimbTracks().FindByPredicate([InSlotId](const FCk_ProceduralAction_LimbTrack& InTrack)
            { return InTrack.Get_Slot() == InSlotId; });
    }

    auto
        HasSlot(
            const FCk_ProceduralAction_ClipDraft& InDraft,
            FName InId)
        -> bool
    {
        return InDraft.Get_Slots().ContainsByPredicate([InId](const FCk_ProceduralAction_Slot& InSlot)
            { return InSlot.Get_Id() == InId; });
    }

    auto
        HasEventId(
            const FCk_ProceduralAction_ClipDraft& InDraft,
            FName InId)
        -> bool
    {
        return InDraft.Get_Markers().ContainsByPredicate([InId](const FCk_ProceduralAction_Marker& InMarker)
                { return InMarker.Get_Id() == InId; })
            || InDraft.Get_Windows().ContainsByPredicate([InId](const FCk_ProceduralAction_Window& InWindow)
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
        AtTime(
            double InTime)
    {
        return [InTime](const auto& InKey)
        { return ck::procedural_action_editor::IsSamePoseTime(InKey.Get_Time().Get_Value(), InTime); };
    }

    template <typename TKey>
    auto
        SortByTime(
            TArray<TKey>& InOutKeys)
        -> void
    {
        InOutKeys.Sort([](const TKey& InLeft, const TKey& InRight)
            { return InLeft.Get_Time().Get_Value() < InRight.Get_Time().Get_Value(); });
    }

    // A pose follows a moved key while the key is its only channel; a pose over several channels lets the moved channel go,
    // because its other channels stay where they were. A pose left with no channel is gone.
    auto
        RetimeGroups(
            TArray<FCk_ProceduralAction_PoseGroup>& InOutGroups,
            FName InPhaseId,
            FName InChannel,
            const TArray<double>& InKeyedTimes,
            const TArray<FCk_ProceduralActionRetimedKey>& InRetimed)
        -> void
    {
        for (auto& Group : InOutGroups)
        {
            if (Group.Get_PhaseId() != InPhaseId || NOT Group.Get_Channels().Contains(InChannel))
            { continue; }
            const auto Time = Group.Get_Time().Get_Value();
            const auto StillKeyed = InKeyedTimes.ContainsByPredicate([Time](double InKeyed)
                { return ck::procedural_action_editor::IsSamePoseTime(InKeyed, Time); });
            const auto* Moved = InRetimed.FindByPredicate([Time](const FCk_ProceduralActionRetimedKey& InRetime)
                { return ck::procedural_action_editor::IsSamePoseTime(InRetime.Get_From(), Time); });
            if (Moved != nullptr && Group.Get_Channels().Num() == 1)
            {
                Group.Set_Time(FCk_FloatRange_0to1{Moved->Get_To()});
                continue;
            }
            if (Moved != nullptr || NOT StillKeyed)
            { Group.Get_Channels().Remove(InChannel); }
        }
        InOutGroups.RemoveAll([](const FCk_ProceduralAction_PoseGroup& InGroup) { return InGroup.Get_Channels().IsEmpty(); });
    }

    // Removes the keys a pose wrote at its phase and time; tracks stay.
    auto
        RemovePoseKeys(
            TArray<FCk_ProceduralAction_Phase>& InOutPhases,
            const FCk_ProceduralAction_PoseGroup& InGroup)
        -> void
    {
        auto* Phase = FindPhase(InOutPhases, InGroup.Get_PhaseId());
        if (Phase == nullptr)
        { return; }
        const auto KeyedAtGroup = AtTime(InGroup.Get_Time().Get_Value());
        for (const auto Channel : InGroup.Get_Channels())
        {
            if (Channel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
            {
                Phase->Get_BodyKeys().RemoveAll(KeyedAtGroup);
                continue;
            }
            if (auto* Track = FindTrack(*Phase, Channel))
            {
                Track->Get_PositionKeys().RemoveAll(KeyedAtGroup);
                Track->Get_OrientationKeys().RemoveAll(KeyedAtGroup);
                Track->Get_BendKeys().RemoveAll(KeyedAtGroup);
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
            TArray<FCk_ProceduralAction_Phase> InPhases,
            const FCk_ProceduralAction_PoseGroup& InGroup)
        -> FPoseTangents
    {
        auto Result = FPoseTangents{};
        auto* Phase = FindPhase(InPhases, InGroup.Get_PhaseId());
        if (Phase == nullptr)
        { return Result; }
        const auto KeyedAtGroup = AtTime(InGroup.Get_Time().Get_Value());
        if (const auto* Key = Phase->Get_BodyKeys().FindByPredicate(KeyedAtGroup))
        { Result.Body = FKeyTangents{Key->Get_ArriveTangent(), Key->Get_LeaveTangent()}; }
        for (const auto& Track : Phase->Get_LimbTracks())
        {
            if (const auto* Key = Track.Get_PositionKeys().FindByPredicate(KeyedAtGroup))
            { Result.Positions.Add(Track.Get_Slot(), FKeyTangents{Key->Get_ArriveTangent(), Key->Get_LeaveTangent()}); }
            if (const auto* Key = Track.Get_BendKeys().FindByPredicate(KeyedAtGroup))
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
        const auto KeyedAtTime = AtTime(InTime);
        if (InTangents.Body.IsSet() && InChannels.Contains(FCk_ProceduralAction_PoseGroup::Get_BodyChannelName()))
        {
            if (auto* Key = Phase->Get_BodyKeys().FindByPredicate(KeyedAtTime))
            { Key->Set_ArriveTangent(InTangents.Body->Arrive).Set_LeaveTangent(InTangents.Body->Leave); }
        }
        for (auto& Track : Phase->Get_LimbTracks())
        {
            if (NOT InChannels.Contains(Track.Get_Slot()))
            { continue; }
            if (const auto* Tangents = InTangents.Positions.Find(Track.Get_Slot()))
            {
                if (auto* Key = Track.Get_PositionKeys().FindByPredicate(KeyedAtTime))
                { Key->Set_ArriveTangent(Tangents->Arrive).Set_LeaveTangent(Tangents->Leave); }
            }
            if (const auto* Tangents = InTangents.Bends.Find(Track.Get_Slot()))
            {
                if (auto* Key = Track.Get_BendKeys().FindByPredicate(KeyedAtTime))
                { Key->Set_ArriveTangent(Tangents->Arrive).Set_LeaveTangent(Tangents->Leave); }
            }
        }
    }

    // A phase's first bend key also writes its direction at 0: the compiler needs a bend track's start key, because a
    // limb may enter the phase with no bend direction to move from. Sections otherwise start from the carried pose.
    auto
        EnsureBendStart(
            FCk_ProceduralAction_LimbTrack& InOutTrack,
            const FVector& InDirection)
        -> void
    {
        auto& Bends = InOutTrack.Get_BendKeys();
        if (Bends.ContainsByPredicate(AtTime(0.0)))
        { return; }
        if (Bends.Num() != 1)
        { return; }
        Bends.Insert(FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{0.0}, InDirection}, 0);
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
            [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
        {
            const auto Report = UCk_Utils_ProceduralActionBuilder_UE::Add_PoseKey(InOutDraft, InPhaseId, InPose);
            if (NOT IsAccepted(Report))
            {
                OutEditError = (HasCode(Report, TEXT("KeyTime"))
                    ? FString{TEXT("A key already exists at this time; use Update Pose to change it or Duplicate to another time. ")}
                    : FString{}) + DescribeErrors(Report.Get_Diagnostics());
                return false;
            }
            if (auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId))
            {
                for (const auto& Limb : InPose.Get_Limbs())
                {
                    auto* Track = FindTrack(*Phase, Limb.Get_Slot());
                    if (Track != nullptr && Limb.Get_BendMode() == ECk_ProceduralAction_ValueMode::Set)
                    { EnsureBendStart(*Track, Limb.Get_Bend()); }
                }
            }
            auto Group = FCk_ProceduralAction_PoseGroup{NewId, InPhaseId, InPose.Get_Time(), Channels};
            Group.Set_Label(InLabel.IsNone() ? FName{*FString::Printf(TEXT("Pose %d"), InOutMetadata.Groups.Num() + 1)} : InLabel);
            Group.Set_Provenance(InProvenance);
            InOutMetadata.Groups.Add(Group);
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
            const FTransform& InRootWorld,
            const FTransform& InBodyWorld,
            const FTransform& InEndpointWorld,
            FCk_ProceduralAction_LimbPoseTarget& OutTarget,
            FString& OutError)
        -> bool
    {
        const auto Space = InSlot.Get_Space();
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
            case ECk_ProceduralAction_TrackSpace::LimbRestNormalized:
            {
                const auto Compiled = ck::procedural_action::FCompiledSlot{InSlot.Get_Id(),
                    ECk_ProceduralAction_InitialSource::CapturedAcceptedPose, Space};
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
        const auto KeysOrientation = InPhases.ContainsByPredicate([&InSlot](const FCk_ProceduralAction_Phase& InPhase)
            {
                return InPhase.Get_LimbTracks().ContainsByPredicate([&InSlot](const FCk_ProceduralAction_LimbTrack& InTrack)
                    { return InTrack.Get_Slot() == InSlot.Get_Id() && NOT InTrack.Get_OrientationKeys().IsEmpty(); });
            });
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

    auto
        RenameBindings(
            TArray<FCk_ProceduralAction_SlotBinding>& InOutBindings,
            FName InOldId,
            FName InNewId)
        -> void
    {
        for (auto& Binding : InOutBindings)
        {
            if (Binding.Get_Slot() == InOldId)
            { Binding = FCk_ProceduralAction_SlotBinding{InNewId, Binding.Get_LimbId()}; }
        }
    }

    auto
        IsUnambiguousRename(
            const TArray<FCk_ProceduralAction_SlotBinding>& InBindings,
            FName InOldId,
            FName InNewId)
        -> bool
    {
        return InBindings.FilterByPredicate([InOldId](const FCk_ProceduralAction_SlotBinding& InBinding)
                { return InBinding.Get_Slot() == InOldId; }).Num() <= 1
            && NOT InBindings.ContainsByPredicate([InNewId](const FCk_ProceduralAction_SlotBinding& InBinding)
                { return InBinding.Get_Slot() == InNewId; });
    }

    template <typename TValue>
    auto
        RenameKey(
            TMap<FName, TValue>& InOutMap,
            FName InOldId,
            FName InNewId)
        -> void
    {
        if (const auto* Value = InOutMap.Find(InOldId))
        {
            const auto Copy = *Value;
            InOutMap.Remove(InOldId);
            InOutMap.Add(InNewId, Copy);
        }
    }

    // Overwrites the pose's channels at its time: the kinds of key the pose sets are replaced and keep their tangents,
    // the kinds it keeps stay as they are.
    auto
        WritePoseKeys(
            FCk_ProceduralAction_ClipDraft& InOutDraft,
            FName InPhaseId,
            const FCk_ProceduralAction_PoseKey& InPose,
            FString& OutError)
        -> bool
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Phase ID does not exist."), OutError); }
        const auto Time = InPose.Get_Time().Get_Value();
        const auto Channels = ck::procedural_action_editor::Get_PoseChannels(InPose);
        const auto Tangents = ReadTangents(InOutDraft.Get_Phases(),
            FCk_ProceduralAction_PoseGroup{FGuid{}, InPhaseId, InPose.Get_Time(), Channels});
        const auto KeyedAtTime = AtTime(Time);
        if (InPose.Get_BodyMode() == ECk_ProceduralAction_ValueMode::Set)
        { Phase->Get_BodyKeys().RemoveAll(KeyedAtTime); }
        for (const auto& Limb : InPose.Get_Limbs())
        {
            auto* Track = FindTrack(*Phase, Limb.Get_Slot());
            if (Track == nullptr)
            { continue; }
            Track->Get_PositionKeys().RemoveAll(KeyedAtTime);
            if (Limb.Get_OrientationMode() == ECk_ProceduralAction_ValueMode::Set)
            { Track->Get_OrientationKeys().RemoveAll(KeyedAtTime); }
            if (Limb.Get_BendMode() == ECk_ProceduralAction_ValueMode::Set)
            { Track->Get_BendKeys().RemoveAll(KeyedAtTime); }
        }
        if (NOT Reject(UCk_Utils_ProceduralActionBuilder_UE::Add_PoseKey(InOutDraft, InPhaseId, InPose), OutError))
        { return false; }
        WriteTangents(InOutDraft.Get_Phases(), InPhaseId, Time, Channels, Tangents);
        if (auto* Written = FindPhase(InOutDraft.Get_Phases(), InPhaseId))
        {
            for (const auto& Limb : InPose.Get_Limbs())
            {
                auto* Track = FindTrack(*Written, Limb.Get_Slot());
                if (Track != nullptr && Limb.Get_BendMode() == ECk_ProceduralAction_ValueMode::Set)
                { EnsureBendStart(*Track, Limb.Get_Bend()); }
            }
        }
        return true;
    }

    // Every time a channel is still keyed at in a phase, for the pose groups that name it.
    auto
        Get_KeyedTimes(
            const FCk_ProceduralAction_Phase& InPhase,
            FName InChannel)
        -> TArray<double>
    {
        auto Times = TArray<double>{};
        const auto AddTime = [&Times](const auto& InKey) { Times.AddUnique(InKey.Get_Time().Get_Value()); };
        if (InChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
        {
            for (const auto& Key : InPhase.Get_BodyKeys())
            { AddTime(Key); }
            return Times;
        }
        if (const auto* Track = InPhase.Get_LimbTracks().FindByPredicate([InChannel](const FCk_ProceduralAction_LimbTrack& InTrack)
            { return InTrack.Get_Slot() == InChannel; }))
        {
            for (const auto& Key : Track->Get_PositionKeys())
            { AddTime(Key); }
            for (const auto& Key : Track->Get_OrientationKeys())
            { AddTime(Key); }
            for (const auto& Key : Track->Get_BendKeys())
            { AddTime(Key); }
        }
        return Times;
    }

    template <typename TKey>
    auto
        RetimeKeys(
            TArray<TKey>& InOutKeys,
            double InFrom,
            double InTo)
        -> void
    {
        if (auto* Key = InOutKeys.FindByPredicate(AtTime(InFrom)))
        { Key->Set_Time(FCk_FloatRange_0to1{InTo}); }
        SortByTime(InOutKeys);
    }

    auto
        HasTime(
            const TArray<double>& InTimes,
            double InTime)
        -> bool
    {
        return InTimes.ContainsByPredicate([InTime](double InCandidate)
            { return ck::procedural_action_editor::IsSamePoseTime(InCandidate, InTime); });
    }
}

// --------------------------------------------------------------------------------------------------------------------

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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("AddPhase", "Add Action Phase"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        return Reject(UCk_Utils_ProceduralActionBuilder_UE::Add_Phase(InOutDraft, FCk_ProceduralAction_Phase{InId, InDuration}),
            OutEditError);
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    ReorderPhase(
        FName InId,
        int32 InNewIndex,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("ReorderPhase", "Reorder Action Phase"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        auto& Phases = InOutDraft.Get_Phases();
        const auto OldIndex = Phases.IndexOfByPredicate([InId](const FCk_ProceduralAction_Phase& InPhase)
            { return InPhase.Get_Id() == InId; });
        if (OldIndex == INDEX_NONE || NOT Phases.IsValidIndex(InNewIndex))
        { return Refuse(TEXT("Phase ID or destination index is invalid."), OutEditError); }
        const auto Moved = Phases[OldIndex];
        Phases.RemoveAt(OldIndex);
        Phases.Insert(Moved, InNewIndex);
        for (const auto& Window : InOutDraft.Get_Windows())
        {
            if (NOT IsLater(Phases, Window.Get_Start().Get_PhaseId(), Window.Get_Start().Get_Time().Get_Value(),
                Window.Get_End().Get_PhaseId(), Window.Get_End().Get_Time().Get_Value()))
            {
                OutEditError = FString::Printf(TEXT("Reordering would invert window %s."), *Window.Get_Id().ToString());
                return false;
            }
        }
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RemovePhase(
        FName InId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("RemovePhase", "Remove Action Phase"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        if (FindPhase(InOutDraft.Get_Phases(), InId) == nullptr)
        { return Refuse(TEXT("Phase ID does not exist."), OutEditError); }
        for (const auto& Marker : InOutDraft.Get_Markers())
        {
            if (Marker.Get_At().Get_PhaseId() == InId)
            {
                OutEditError = FString::Printf(TEXT("Marker %s sits in this section; move or delete it first."), *Marker.Get_Id().ToString());
                return false;
            }
        }
        for (const auto& Window : InOutDraft.Get_Windows())
        {
            if (Window.Get_Start().Get_PhaseId() == InId || Window.Get_End().Get_PhaseId() == InId)
            {
                OutEditError = FString::Printf(TEXT("Window %s starts or ends in this section; move or delete it first."), *Window.Get_Id().ToString());
                return false;
            }
        }
        InOutDraft.Get_Phases().RemoveAll([InId](const FCk_ProceduralAction_Phase& InPhase) { return InPhase.Get_Id() == InId; });
        InOutMetadata.Groups.RemoveAll([InId](const FCk_ProceduralAction_PoseGroup& InGroup) { return InGroup.Get_PhaseId() == InId; });
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RenamePhase(
        FName InOldId,
        FName InNewId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (InNewId.IsNone())
    { return Refuse(TEXT("A section needs a name."), OutError); }
    if (InOldId == InNewId)
    { return Refuse(*FString::Printf(TEXT("The section is already named %s."), *InNewId.ToString()), OutError); }
    return CommitDraft(*Clip, LOCTEXT("RenamePhase", "Rename Action Phase"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InOldId);
        if (Phase == nullptr)
        {
            OutEditError = FString::Printf(TEXT("Phase %s does not exist in this clip."), *InOldId.ToString());
            return false;
        }
        if (FindPhase(InOutDraft.Get_Phases(), InNewId) != nullptr)
        {
            OutEditError = FString::Printf(TEXT("Another section is already named %s."), *InNewId.ToString());
            return false;
        }
        auto Renamed = FCk_ProceduralAction_Phase{InNewId, Phase->Get_Duration()};
        Renamed.Set_Easing(Phase->Get_Easing());
        Renamed.Set_BodyInterpolation(Phase->Get_BodyInterpolation());
        Renamed.Set_BodyKeys(Phase->Get_BodyKeys());
        Renamed.Set_LimbTracks(Phase->Get_LimbTracks());
        *Phase = MoveTemp(Renamed);
        const auto RenameRef = [InOldId, InNewId](const FCk_ProceduralAction_TimeRef& InRef) -> FCk_ProceduralAction_TimeRef
        {
            return InRef.Get_PhaseId() == InOldId
                ? FCk_ProceduralAction_TimeRef{InNewId, InRef.Get_Time()}
                : InRef;
        };
        for (auto& Marker : InOutDraft.Get_Markers())
        { Marker.Set_At(RenameRef(Marker.Get_At())); }
        for (auto& Window : InOutDraft.Get_Windows())
        {
            Window.Set_Start(RenameRef(Window.Get_Start()));
            Window.Set_End(RenameRef(Window.Get_End()));
        }
        for (auto& Group : InOutMetadata.Groups)
        {
            if (Group.Get_PhaseId() == InOldId)
            { Group.Set_PhaseId(InNewId); }
        }
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddSlot(
        FName InId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("AddSlot", "Add Action Slot"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    { return Reject(UCk_Utils_ProceduralActionBuilder_UE::Add_Slot(InOutDraft, FCk_ProceduralAction_Slot{InId}), OutEditError); },
        OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RemoveSlot(
        FName InId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("RemoveSlot", "Remove Action Slot"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        if (NOT HasSlot(InOutDraft, InId))
        { return Refuse(TEXT("Slot ID does not exist."), OutEditError); }
        for (auto& Phase : InOutDraft.Get_Phases())
        {
            if (FindTrack(Phase, InId) != nullptr)
            {
                OutEditError = FString::Printf(TEXT("Slot is referenced by phase %s."), *Phase.Get_Id().ToString());
                return false;
            }
        }
        InOutDraft.Get_Slots().RemoveAll([InId](const FCk_ProceduralAction_Slot& InSlot) { return InSlot.Get_Id() == InId; });
        InOutMetadata.Subject.Get_SlotBindings().RemoveAll([InId](const FCk_ProceduralAction_SlotBinding& InBinding)
            { return InBinding.Get_Slot() == InId; });
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RenameSlot(
        UCk_ProceduralActionPreview_Config* InScenario,
        FName InOldId,
        FName InNewId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (InOldId.IsNone() || InNewId.IsNone() || InOldId == InNewId)
    { return Refuse(TEXT("Slot rename needs distinct nonempty old and new IDs."), OutError); }
    if (InScenario != nullptr)
    {
        if (NOT IsUnambiguousRename(InScenario->Get_SlotBindings(), InOldId, InNewId))
        { return Refuse(TEXT("Scenario slot bindings are ambiguous for the old or new ID."), OutError); }
        if (InScenario->Get_InitialLimbEndpoints().Contains(InNewId) || InScenario->Get_InitialBendDirections().Contains(InNewId))
        { return Refuse(TEXT("Scenario initial endpoint or bend already uses the new slot ID."), OutError); }
    }
    return CommitDraft(*Clip, LOCTEXT("RenameSlot", "Rename Action Slot"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        auto& Slots = InOutDraft.Get_Slots();
        const auto OldCount = Slots.FilterByPredicate([InOldId](const FCk_ProceduralAction_Slot& InSlot)
            { return InSlot.Get_Id() == InOldId; }).Num();
        if (OldCount != 1 || HasSlot(InOutDraft, InNewId))
        { return Refuse(TEXT("Old slot must exist exactly once and new slot ID must be unused."), OutEditError); }
        if (NOT IsUnambiguousRename(InOutMetadata.Subject.Get_SlotBindings(), InOldId, InNewId))
        { return Refuse(TEXT("Subject slot bindings are ambiguous for the old or new ID."), OutEditError); }
        for (auto& Slot : Slots)
        {
            if (Slot.Get_Id() != InOldId)
            { continue; }
            auto Renamed = FCk_ProceduralAction_Slot{InNewId};
            Renamed.Set_InitialSource(Slot.Get_InitialSource());
            Renamed.Set_Space(Slot.Get_Space());
            Slot = MoveTemp(Renamed);
        }
        for (auto& Phase : InOutDraft.Get_Phases())
        {
            for (auto& Track : Phase.Get_LimbTracks())
            {
                if (Track.Get_Slot() != InOldId)
                { continue; }
                auto Renamed = FCk_ProceduralAction_LimbTrack{InNewId};
                Renamed.Set_Interpolation(Track.Get_Interpolation());
                Renamed.Set_ReachPolicy(Track.Get_ReachPolicy());
                Renamed.Set_PositionKeys(Track.Get_PositionKeys());
                Renamed.Set_OrientationKeys(Track.Get_OrientationKeys());
                Renamed.Set_BendKeys(Track.Get_BendKeys());
                Track = MoveTemp(Renamed);
            }
        }
        for (auto& Group : InOutMetadata.Groups)
        {
            for (auto& Channel : Group.Get_Channels())
            {
                if (Channel == InOldId)
                { Channel = InNewId; }
            }
        }
        RenameBindings(InOutMetadata.Subject.Get_SlotBindings(), InOldId, InNewId);
        return true;
    }, OutError, InScenario == nullptr ? TFunction<void()>{} : TFunction<void()>{[InScenario, InOldId, InNewId]()
    {
        InScenario->Modify();
        auto Bindings = InScenario->Get_SlotBindings();
        RenameBindings(Bindings, InOldId, InNewId);
        InScenario->Set_SlotBindings(Bindings);
        auto Endpoints = InScenario->Get_InitialLimbEndpoints();
        RenameKey(Endpoints, InOldId, InNewId);
        InScenario->Set_InitialLimbEndpoints(Endpoints);
        auto Bends = InScenario->Get_InitialBendDirections();
        RenameKey(Bends, InOldId, InNewId);
        InScenario->Set_InitialBendDirections(Bends);
        InScenario->PostEditChange();
    }});
}

auto
    FCk_ProceduralActionAuthoringCommands::
    AddLimbTrack(
        FName InPhaseId,
        FName InSlotId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("AddTrack", "Add Action Limb Track"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        if (NOT HasSlot(InOutDraft, InSlotId))
        { return Refuse(TEXT("The track slot is not declared."), OutEditError); }
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Phase ID does not exist."), OutEditError); }
        if (FindTrack(*Phase, InSlotId) != nullptr)
        { return Refuse(TEXT("This phase already has a track for the slot."), OutEditError); }
        Phase->Get_LimbTracks().Add(FCk_ProceduralAction_LimbTrack{InSlotId});
        return true;
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT IsNormalized(InNormalizedTime))
    { return Refuse(TEXT("Key time must be in [0,1]."), OutError); }
    if (InValue.ContainsNaN() || NOT InValue.GetScale3D().Equals(FVector::OneVector, 1.0e-4))
    { return Refuse(TEXT("Body key must be a finite rigid transform."), OutError); }
    return CommitDraft(*Clip, LOCTEXT("AddBodyKey", "Add Action Body Key"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        if (NOT InOutDraft.Get_BodyChannel().Get_Enabled())
        { return Refuse(TEXT("Enable the body channel before adding body keys."), OutEditError); }
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Phase ID does not exist."), OutEditError); }
        auto& Keys = Phase->Get_BodyKeys();
        if (Keys.ContainsByPredicate(AtTime(InNormalizedTime)))
        { return Refuse(TEXT("A body key already exists at this time."), OutEditError); }
        Keys.Add(FCk_ProceduralAction_TransformKey{FCk_FloatRange_0to1{InNormalizedTime}, InValue.GetTranslation(), InValue.GetRotation()});
        SortByTime(Keys);
        return true;
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT IsNormalized(InNormalizedTime) || InValue.ContainsNaN())
    { return Refuse(TEXT("Position key needs finite coordinates and time in [0,1]."), OutError); }
    return CommitDraft(*Clip, LOCTEXT("AddPositionKey", "Add Action Position Key"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Phase ID does not exist."), OutEditError); }
        auto* Track = FindTrack(*Phase, InSlotId);
        if (Track == nullptr)
        { return Refuse(TEXT("This phase has no track for the slot."), OutEditError); }
        auto& Keys = Track->Get_PositionKeys();
        if (Keys.ContainsByPredicate(AtTime(InNormalizedTime)))
        { return Refuse(TEXT("A position key already exists at this time."), OutEditError); }
        Keys.Add(FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{InNormalizedTime}, InValue});
        SortByTime(Keys);
        return true;
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT IsNormalized(InNormalizedTime) || InValue.ContainsNaN())
    { return Refuse(TEXT("Endpoint gizmo needs finite coordinates and time in [0,1]."), OutError); }
    return CommitDraft(*Clip, LOCTEXT("SetEndpoint", "Move Action Endpoint"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Selected phase no longer exists."), OutEditError); }
        auto* Track = FindTrack(*Phase, InSlotId);
        if (Track == nullptr)
        { return Refuse(TEXT("Selected limb track no longer exists."), OutEditError); }
        auto& Keys = Track->Get_PositionKeys();
        if (auto* Existing = Keys.FindByPredicate(AtTime(InNormalizedTime)))
        { Existing->Set_Value(InValue); }
        else
        { Keys.Add(FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{InNormalizedTime}, InValue}); }
        SortByTime(Keys);
        return true;
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (NOT IsNormalized(InNormalizedTime) || InValue.ContainsNaN() || InValue.SizeSquared() <= 1.0e-8)
    { return Refuse(TEXT("Bend gizmo needs a finite nonzero direction and time in [0,1]."), OutError); }
    const auto Direction = InValue.GetSafeNormal();
    return CommitDraft(*Clip, LOCTEXT("SetBend", "Move Action Bend Guide"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Selected phase no longer exists."), OutEditError); }
        auto* Track = FindTrack(*Phase, InSlotId);
        if (Track == nullptr)
        { return Refuse(TEXT("Selected limb track no longer exists."), OutEditError); }
        auto& Keys = Track->Get_BendKeys();
        if (auto* Existing = Keys.FindByPredicate(AtTime(InNormalizedTime)))
        { Existing->Set_Value(Direction); }
        else
        { Keys.Add(FCk_ProceduralAction_VectorKey{FCk_FloatRange_0to1{InNormalizedTime}, Direction}); }
        SortByTime(Keys);
        EnsureBendStart(*Track, Direction);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetBodyKeys(
        FName InPhaseId,
        const TArray<FCk_ProceduralAction_TransformKey>& InKeys,
        ECk_ProceduralAction_Interpolation InInterpolation,
        const TArray<FCk_ProceduralActionRetimedKey>& InRetimed,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("SetBodyKeys", "Edit Action Body Curve"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Phase ID does not exist."), OutEditError); }
        Phase->Set_BodyKeys(InKeys);
        Phase->Set_BodyInterpolation(InInterpolation);
        auto Times = TArray<double>{};
        for (const auto& Key : InKeys)
        { Times.Add(Key.Get_Time().Get_Value()); }
        RetimeGroups(InOutMetadata.Groups, InPhaseId, FCk_ProceduralAction_PoseGroup::Get_BodyChannelName(), Times, InRetimed);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetPositionKeys(
        FName InPhaseId,
        FName InSlotId,
        const TArray<FCk_ProceduralAction_VectorKey>& InKeys,
        ECk_ProceduralAction_Interpolation InInterpolation,
        const TArray<FCk_ProceduralActionRetimedKey>& InRetimed,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("SetPositionKeys", "Edit Action Endpoint Curve"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        auto* Track = Phase == nullptr ? nullptr : FindTrack(*Phase, InSlotId);
        if (Track == nullptr)
        { return Refuse(TEXT("This phase has no track for the slot."), OutEditError); }
        Track->Set_PositionKeys(InKeys);
        Track->Set_Interpolation(InInterpolation);
        auto Times = TArray<double>{};
        for (const auto& Key : InKeys)
        { Times.Add(Key.Get_Time().Get_Value()); }
        RetimeGroups(InOutMetadata.Groups, InPhaseId, InSlotId, Times, InRetimed);
        return true;
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("AddMarker", "Add Action Marker"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        if (InId.IsNone() || HasEventId(InOutDraft, InId))
        { return Refuse(TEXT("Event ID must be nonempty and unique."), OutEditError); }
        if (FindPhase(InOutDraft.Get_Phases(), InPhaseId) == nullptr || NOT IsNormalized(InNormalizedTime))
        { return Refuse(TEXT("Marker needs an existing phase and time in [0,1]."), OutEditError); }
        InOutDraft.Get_Markers().Add(FCk_ProceduralAction_Marker{InId,
            FCk_ProceduralAction_TimeRef{InPhaseId, FCk_FloatRange_0to1{InNormalizedTime}}});
        return true;
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("AddWindow", "Add Action Window"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        if (InId.IsNone() || HasEventId(InOutDraft, InId))
        { return Refuse(TEXT("Event ID must be nonempty and unique."), OutEditError); }
        if (NOT IsLater(InOutDraft.Get_Phases(), InStartPhaseId, InStartTime, InEndPhaseId, InEndTime))
        { return Refuse(TEXT("Window end must follow its start across existing phases."), OutEditError); }
        InOutDraft.Get_Windows().Add(FCk_ProceduralAction_Window{InId,
            FCk_ProceduralAction_TimeRef{InStartPhaseId, FCk_FloatRange_0to1{InStartTime}},
            FCk_ProceduralAction_TimeRef{InEndPhaseId, FCk_FloatRange_0to1{InEndTime}}});
        return true;
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("MoveMarker", "Move Action Marker"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        if (FindPhase(InOutDraft.Get_Phases(), InPhaseId) == nullptr || NOT IsNormalized(InNormalizedTime))
        { return Refuse(TEXT("Marker needs an existing phase and time in [0,1]."), OutEditError); }
        auto* Marker = InOutDraft.Get_Markers().FindByPredicate([InId](const FCk_ProceduralAction_Marker& InMarker)
            { return InMarker.Get_Id() == InId; });
        if (Marker == nullptr)
        { return Refuse(TEXT("Marker ID does not exist."), OutEditError); }
        Marker->Set_At(FCk_ProceduralAction_TimeRef{InPhaseId, FCk_FloatRange_0to1{InNormalizedTime}});
        return true;
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("MoveWindowEdge", "Move Action Window Edge"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        if (FindPhase(InOutDraft.Get_Phases(), InPhaseId) == nullptr || NOT IsNormalized(InNormalizedTime))
        { return Refuse(TEXT("Window edge needs an existing phase and time in [0,1]."), OutEditError); }
        auto* Window = InOutDraft.Get_Windows().FindByPredicate([InId](const FCk_ProceduralAction_Window& InWindow)
            { return InWindow.Get_Id() == InId; });
        if (Window == nullptr)
        { return Refuse(TEXT("Window ID does not exist."), OutEditError); }
        const auto Ref = FCk_ProceduralAction_TimeRef{InPhaseId, FCk_FloatRange_0to1{InNormalizedTime}};
        const auto Start = InStartEdge ? Ref : Window->Get_Start();
        const auto End = InStartEdge ? Window->Get_End() : Ref;
        if (NOT IsLater(InOutDraft.Get_Phases(), Start.Get_PhaseId(), Start.Get_Time().Get_Value(),
            End.Get_PhaseId(), End.Get_Time().Get_Value()))
        { return Refuse(TEXT("Window end must remain strictly after its start; the interval is half-open."), OutEditError); }
        Window->Set_Start(Start);
        Window->Set_End(End);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    MoveWindow(
        FName InId,
        const FCk_ProceduralAction_TimeRef& InStart,
        const FCk_ProceduralAction_TimeRef& InEnd,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    return CommitDraft(*Clip, LOCTEXT("MoveWindow", "Move Action Window"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        if (FindPhase(InOutDraft.Get_Phases(), InStart.Get_PhaseId()) == nullptr
            || FindPhase(InOutDraft.Get_Phases(), InEnd.Get_PhaseId()) == nullptr)
        { return Refuse(TEXT("Window edges need existing phases."), OutEditError); }
        auto* Window = InOutDraft.Get_Windows().FindByPredicate([InId](const FCk_ProceduralAction_Window& InWindow)
            { return InWindow.Get_Id() == InId; });
        if (Window == nullptr)
        { return Refuse(TEXT("Window ID does not exist."), OutEditError); }
        if (NOT IsLater(InOutDraft.Get_Phases(), InStart.Get_PhaseId(), InStart.Get_Time().Get_Value(),
            InEnd.Get_PhaseId(), InEnd.Get_Time().Get_Value()))
        { return Refuse(TEXT("Window end must remain strictly after its start; the interval is half-open."), OutEditError); }
        Window->Set_Start(InStart);
        Window->Set_End(InEnd);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RemoveEvent(
        FName InId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    return CommitDraft(*Clip, LOCTEXT("RemoveEvent", "Remove Action Event"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        const auto Removed = InOutDraft.Get_Markers().RemoveAll([InId](const FCk_ProceduralAction_Marker& InMarker) { return InMarker.Get_Id() == InId; })
            + InOutDraft.Get_Windows().RemoveAll([InId](const FCk_ProceduralAction_Window& InWindow) { return InWindow.Get_Id() == InId; });
        if (Removed == 0)
        { return Refuse(TEXT("No marker or window has that name."), OutEditError); }
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetSlotSpace(
        FName InSlotId,
        ECk_ProceduralAction_TrackSpace InSpace,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("SetSlotSpace", "Set Action Slot Space"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        auto* Slot = InOutDraft.Get_Slots().FindByPredicate([InSlotId](const FCk_ProceduralAction_Slot& InSlot)
            { return InSlot.Get_Id() == InSlotId; });
        if (Slot == nullptr)
        { return Refuse(TEXT("Slot ID does not exist."), OutEditError); }
        if (Slot->Get_Space() == InSpace)
        { return true; }
        for (auto& Phase : InOutDraft.Get_Phases())
        {
            const auto* Track = FindTrack(Phase, InSlotId);
            if (Track != nullptr && (NOT Track->Get_PositionKeys().IsEmpty() || NOT Track->Get_OrientationKeys().IsEmpty()
                || NOT Track->Get_BendKeys().IsEmpty()))
            { return Refuse(TEXT("Set slot space before keying any phase; changing it now would reinterpret existing coordinates."), OutEditError); }
        }
        return Reject(UCk_Utils_ProceduralActionBuilder_UE::Set_SlotFrame(InOutDraft, InSlotId, InSpace), OutEditError);
    }, OutError);
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
    using namespace ck_procedural_action_authoring;
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
            return Refuse(TEXT("Mirror needs an X, Y, or Z reflection plane normal."), OutError);
    }
    return CommitDraft(*Clip, LOCTEXT("MirrorSlot", "Mirror Action Slot"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        return Reject(UCk_Utils_ProceduralActionBuilder_UE::Mirror_Slot(InOutDraft,
            FCk_ProceduralAction_MirrorSpec{InSourceSlot, InDestinationSlot, Plane}), OutEditError);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (InParticipants.IsEmpty())
    { return Refuse(TEXT("Select at least one participating channel before capturing a pose."), OutError); }
    if (NOT HasSameChannels(InParticipants, ck::procedural_action_editor::Get_PoseChannels(InPose), OutError))
    { return false; }
    return CapturePoseKey(*Clip, InPhaseId, InPose, InLabel, ECk_ProceduralAction_PoseProvenance::Requested, OutPoseId, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    UpdatePose(
        const FGuid& InPoseId,
        const FCk_ProceduralAction_PoseKey& InPose,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    auto View = FCk_ProceduralActionPoseGroupView{};
    if (NOT FindEditablePose(*Clip, InPoseId, View, OutError))
    { return false; }
    auto Group = View.Get_Group();
    if (NOT HasSameChannels(Group.Get_Channels(), ck::procedural_action_editor::Get_PoseChannels(InPose), OutError))
    { return false; }
    auto Pose = InPose;
    Pose.Set_Time(Group.Get_Time());
    Group.Set_Provenance(ECk_ProceduralAction_PoseProvenance::Requested);
    return CommitDraft(*Clip, LOCTEXT("UpdatePose", "Update Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        if (NOT Reject(UCk_Utils_ProceduralActionBuilder_UE::Replace_PoseKey(InOutDraft, Group.Get_PhaseId(), Pose), OutEditError))
        { return false; }
        UpsertGroup(InOutMetadata.Groups, Group);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT IsNormalized(InNormalizedTime))
    { return Refuse(TEXT("Duplicate needs a time in [0,1]."), OutError); }
    auto View = FCk_ProceduralActionPoseGroupView{};
    if (NOT FindEditablePose(*Clip, InPoseId, View, OutError))
    { return false; }
    const auto& Source = View.Get_Group();
    auto Pose = FCk_ProceduralAction_PoseKey{};
    if (NOT ck::procedural_action_editor::ReadPoseKey(Clip->Get_Phases(), Source, Pose, OutError))
    { return false; }
    const auto Tangents = ReadTangents(Clip->Get_Phases(), Source);
    Pose.Set_Time(FCk_FloatRange_0to1{InNormalizedTime});
    const auto NewId = FGuid::NewGuid();
    const auto Committed = CommitDraft(*Clip, LOCTEXT("DuplicatePose", "Duplicate Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        if (NOT Reject(UCk_Utils_ProceduralActionBuilder_UE::Add_PoseKey(InOutDraft, InPhaseId, Pose), OutEditError))
        { return false; }
        WriteTangents(InOutDraft.Get_Phases(), InPhaseId, InNormalizedTime, Source.Get_Channels(), Tangents);
        auto Copy = FCk_ProceduralAction_PoseGroup{NewId, InPhaseId, FCk_FloatRange_0to1{InNormalizedTime}, Source.Get_Channels()};
        Copy.Set_Label(Source.Get_Label());
        Copy.Set_Provenance(Source.Get_Provenance());
        InOutMetadata.Groups.Add(Copy);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT IsNormalized(InNormalizedTime))
    { return Refuse(TEXT("Move needs a time in [0,1]."), OutError); }
    auto View = FCk_ProceduralActionPoseGroupView{};
    if (NOT FindEditablePose(*Clip, InPoseId, View, OutError))
    { return false; }
    const auto Source = View.Get_Group();
    if (Source.Get_PhaseId() == InPhaseId && ck::procedural_action_editor::IsSamePoseTime(Source.Get_Time().Get_Value(), InNormalizedTime))
    { return true; }
    auto Pose = FCk_ProceduralAction_PoseKey{};
    if (NOT ck::procedural_action_editor::ReadPoseKey(Clip->Get_Phases(), Source, Pose, OutError))
    { return false; }
    const auto Tangents = ReadTangents(Clip->Get_Phases(), Source);
    Pose.Set_Time(FCk_FloatRange_0to1{InNormalizedTime});
    return CommitDraft(*Clip, LOCTEXT("MovePose", "Move Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        RemovePoseKeys(InOutDraft.Get_Phases(), Source);
        if (NOT Reject(UCk_Utils_ProceduralActionBuilder_UE::Add_PoseKey(InOutDraft, InPhaseId, Pose), OutEditError))
        { return false; }
        WriteTangents(InOutDraft.Get_Phases(), InPhaseId, InNormalizedTime, Source.Get_Channels(), Tangents);
        auto Moved = Source;
        Moved.Set_PhaseId(InPhaseId);
        Moved.Set_Time(FCk_FloatRange_0to1{InNormalizedTime});
        UpsertGroup(InOutMetadata.Groups, Moved);
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
    using namespace ck_procedural_action_authoring;
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
    return CommitDraft(*Clip, LOCTEXT("DeletePose", "Delete Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString&)
    {
        RemovePoseKeys(InOutDraft.Get_Phases(), Group);
        InOutMetadata.Groups.RemoveAll([&Group](const FCk_ProceduralAction_PoseGroup& InCandidate)
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    const auto* Config = InTarget.Get_Config();
    if (Config == nullptr)
    { return Refuse(TEXT("Select the preview subject the Solver ran before capturing a solved pose."), OutError); }
    if (NOT InSnapshot.Get_Ready())
    { return Refuse(TEXT("The Solver preview is not ready: pause it at a step with no seek or preparation pending."), OutError); }
    if (NOT InSnapshot.Get_Settled())
    { return Refuse(TEXT("The Solver pose is not settled: its last accepted pose is not the subject's latest settled revision."), OutError); }
    if (InSnapshot.Get_PreviewRevision() != InTarget.Get_SessionRevision())
    {
        OutError = FString::Printf(TEXT("The Solver recording belongs to revision %llu but the clip and preview are at revision %llu. Restart Solver first."),
            InSnapshot.Get_PreviewRevision(), InTarget.Get_SessionRevision());
        return false;
    }
    if (InTarget.Get_Channels().IsEmpty())
    { return Refuse(TEXT("Select at least one channel to capture from the Solver."), OutError); }

    const auto& Solved = InSnapshot.Get_Pose();
    const auto RootWorld = Solved.Get_RootFrame();
    if (NOT IsRigidFinite(RootWorld))
    { return Refuse(TEXT("The solved snapshot is malformed: its root frame is not a finite rigid transform."), OutError); }
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
            { return Refuse(TEXT("Solved body capture is not available for a walking preview subject."), OutError); }
            const auto ComposedLocal = Solved.Get_BodyPresentation().GetRelativeTransform(RootWorld);
            const auto Offset = FTransform{(PresentationLocal.GetRotation().Inverse() * ComposedLocal.GetRotation()).GetNormalized(),
                ComposedLocal.GetTranslation() - PresentationLocal.GetTranslation()};
            if (NOT IsRigidFinite(Solved.Get_BodyPresentation()) || NOT IsRigidFinite(Offset))
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
        if (NOT IsRigidFinite(Limb->Get_AcceptedEndpoint()))
        {
            Malformed.Add(Channel);
            continue;
        }
        auto Target = FCk_ProceduralAction_LimbPoseTarget{};
        auto ConversionError = FString{};
        if (NOT ConvertSolvedLimb(*Config, Clip->Get_Phases(), *Slot, RootWorld, BodyWorld, Limb->Get_AcceptedEndpoint(),
            Target, ConversionError))
        {
            OutError = FString::Printf(TEXT("The solved snapshot cannot be converted: %s Nothing was written."), *ConversionError);
            return false;
        }
        Limbs.Add(MoveTemp(Target));
    }
    if (NOT Missing.IsEmpty())
    {
        OutError = FString::Printf(TEXT("The solved snapshot has no complete evidence for: %s. Nothing was written."), *JoinNames(Missing));
        return false;
    }
    if (NOT Malformed.IsEmpty())
    {
        OutError = FString::Printf(TEXT("The solved snapshot is malformed for: %s (non-finite or non-rigid geometry). Nothing was written."),
            *JoinNames(Malformed));
        return false;
    }
    Pose.Set_Limbs(Limbs);
    return CapturePoseKey(*Clip, InTarget.Get_PhaseId(), Pose, InTarget.Get_Label(), ECk_ProceduralAction_PoseProvenance::Solved,
        OutPoseId, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    CreateFromRecipe(
        const FCk_ProceduralAction_RecipeSpec& InRecipe,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    auto Recipe = FCk_ProceduralAction_ClipDraft{};
    if (NOT Reject(UCk_Utils_ProceduralActionBuilder_UE::Make_FromRecipe(InRecipe, Recipe), OutError))
    { return false; }
    auto Derived = TArray<FCk_ProceduralActionPoseGroupView>{};
    ck::procedural_action_editor::DerivePoseGroups(Recipe.Get_Phases(), {}, Derived);
    return CommitDraft(*Clip, LOCTEXT("CreateFromRecipe", "Create Action From Recipe"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        if (NOT InOutDraft.Get_Phases().IsEmpty() || NOT InOutDraft.Get_Slots().IsEmpty() || InOutDraft.Get_BodyChannel().Get_Enabled()
            || NOT InOutDraft.Get_Markers().IsEmpty() || NOT InOutDraft.Get_Windows().IsEmpty())
        { return Refuse(TEXT("A recipe creates a new motion and never overwrites authored content; start from an empty clip."), OutEditError); }
        InOutDraft = Recipe;
        InOutMetadata.Groups.Reset();
        for (const auto& View : Derived)
        {
            const auto& Source = View.Get_Group();
            auto Group = FCk_ProceduralAction_PoseGroup{FGuid::NewGuid(), Source.Get_PhaseId(), Source.Get_Time(), Source.Get_Channels()};
            Group.Set_Label(Source.Get_Label());
            InOutMetadata.Groups.Add(Group);
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
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT FMath::IsFinite(InDuration.Get_Seconds()) || InDuration <= FCk_Time{})
    { return Refuse(TEXT("A phase duration must be a finite time greater than zero."), OutError); }
    return CommitDraft(*Clip, LOCTEXT("SetPhaseDuration", "Set Action Phase Duration"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
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
    SetPhaseEasing(
        FName InPhaseId,
        ECk_ProceduralAction_Easing InEasing,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    return CommitDraft(*Clip, LOCTEXT("SetPhaseEasing", "Set Action Phase Easing"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        {
            OutEditError = FString::Printf(TEXT("Phase %s does not exist in this clip."), *InPhaseId.ToString());
            return false;
        }
        Phase->Set_Easing(InEasing);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RetargetSlot(
        const UCk_ProceduralActionPreview_Config* InConfig,
        FName InSlotId,
        FName InLimbId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    if (InConfig == nullptr)
    { return Refuse(TEXT("The clip has no preview subject to bind the slot to."), OutError); }
    const auto Walking = InConfig->Get_SubjectMode() == ECk_ProceduralActionPreview_SubjectMode::Walking;
    const auto Exists = Walking
        ? InConfig->Get_WalkingRig() != nullptr
            && InConfig->Get_WalkingRig()->Get_Legs().ContainsByPredicate([InLimbId](const FCk_ProceduralLeg_Spec& InLeg)
                { return InLeg.Get_Id() == InLimbId; })
        : InConfig->Get_Layout().Get_Limbs().ContainsByPredicate([InLimbId](const FCk_ProceduralLimb_Layout& InLimb)
            { return InLimb.Get_Id() == InLimbId; });
    if (InLimbId.IsNone() || NOT Exists || NOT InConfig->Get_LimbBindings().ContainsByPredicate(
        [InLimbId](const FCk_ProceduralActionPreview_LimbBinding& InBinding) { return InBinding.Get_LimbId() == InLimbId; }))
    { return Refuse(TEXT("Target limb must exist in the preview topology and have an explicit visual/control binding."), OutError); }
    return CommitDraft(*Clip, LOCTEXT("RetargetSlot", "Bind Action Slot"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        if (InSlotId.IsNone() || NOT HasSlot(InOutDraft, InSlotId))
        { return Refuse(TEXT("Retargeting needs a declared clip slot and named limb ID."), OutEditError); }
        auto& Bindings = InOutMetadata.Subject.Get_SlotBindings();
        if (Bindings.ContainsByPredicate([InSlotId, InLimbId](const FCk_ProceduralAction_SlotBinding& InBinding)
            { return InBinding.Get_Slot() != InSlotId && InBinding.Get_LimbId() == InLimbId; }))
        { return Refuse(TEXT("Target limb is already bound to a different slot."), OutEditError); }
        Bindings.RemoveAll([InSlotId](const FCk_ProceduralAction_SlotBinding& InBinding) { return InBinding.Get_Slot() == InSlotId; });
        Bindings.Add(FCk_ProceduralAction_SlotBinding{InSlotId, InLimbId});
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetEditorSubject(
        const FCk_ProceduralAction_EditorSubject& InSubject,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr) { return false; }
    return CommitDraft(*Clip, LOCTEXT("SetEditorSubject", "Set Action Preview Subject"),
        [&](FCk_ProceduralAction_ClipDraft&, FEditorMetadata& InOutMetadata, FString&)
    {
        InOutMetadata.Subject = InSubject;
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetPoseKeys(
        FName InPhaseId,
        const FCk_ProceduralAction_PoseKey& InPose,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT IsNormalized(InPose.Get_Time().Get_Value()))
    { return Refuse(TEXT("Key time must be in [0,1]."), OutError); }
    return CommitDraft(*Clip, LOCTEXT("SetPoseKeys", "Edit Action Key"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    { return WritePoseKeys(InOutDraft, InPhaseId, InPose, OutEditError); }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RecordPose(
        FName InPhaseId,
        const FCk_ProceduralAction_PoseKey& InPose,
        FName InLabel,
        FGuid& OutPoseId,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    const auto Channels = ck::procedural_action_editor::Get_PoseChannels(InPose);
    if (Channels.IsEmpty())
    { return Refuse(TEXT("Tick at least one channel for Record to cover."), OutError); }
    const auto Time = InPose.Get_Time().Get_Value();
    if (NOT IsNormalized(Time))
    { return Refuse(TEXT("Key time must be in [0,1]."), OutError); }
    auto PoseId = FGuid::NewGuid();
    const auto Committed = CommitDraft(*Clip, LOCTEXT("RecordPose", "Record Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        if (NOT WritePoseKeys(InOutDraft, InPhaseId, InPose, OutEditError))
        { return false; }
        auto* Existing = InOutMetadata.Groups.FindByPredicate([InPhaseId, Time](const FCk_ProceduralAction_PoseGroup& InGroup)
        {
            return InGroup.Get_PhaseId() == InPhaseId
                && ck::procedural_action_editor::IsSamePoseTime(InGroup.Get_Time().Get_Value(), Time);
        });
        if (Existing != nullptr)
        {
            PoseId = Existing->Get_Id();
            for (const auto Channel : Channels)
            { Existing->Get_Channels().AddUnique(Channel); }
            Existing->Set_Provenance(ECk_ProceduralAction_PoseProvenance::Requested);
            if (NOT InLabel.IsNone())
            { Existing->Set_Label(InLabel); }
            return true;
        }
        auto Group = FCk_ProceduralAction_PoseGroup{PoseId, InPhaseId, InPose.Get_Time(), Channels};
        Group.Set_Label(InLabel.IsNone() ? FName{*FString::Printf(TEXT("Pose %d"), InOutMetadata.Groups.Num() + 1)} : InLabel);
        InOutMetadata.Groups.Add(Group);
        return true;
    }, OutError);
    if (Committed)
    { OutPoseId = PoseId; }
    return Committed;
}

auto
    FCk_ProceduralActionAuthoringCommands::
    MoveKey(
        FName InPhaseId,
        FName InChannel,
        double InFrom,
        double InTo,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (NOT IsNormalized(InFrom) || NOT IsNormalized(InTo))
    { return Refuse(TEXT("Key times must be in [0,1]."), OutError); }
    return CommitDraft(*Clip, LOCTEXT("MoveKey", "Move Action Key"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Phase ID does not exist."), OutEditError); }
        const auto Before = Get_KeyedTimes(*Phase, InChannel);
        if (NOT HasTime(Before, InFrom))
        { return Refuse(TEXT("The channel has no key at that time."), OutEditError); }
        if (HasTime(Before, InTo))
        { return Refuse(TEXT("The channel already has a key at the new time."), OutEditError); }
        if (InChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
        { RetimeKeys(Phase->Get_BodyKeys(), InFrom, InTo); }
        else if (auto* Track = FindTrack(*Phase, InChannel))
        {
            RetimeKeys(Track->Get_PositionKeys(), InFrom, InTo);
            RetimeKeys(Track->Get_OrientationKeys(), InFrom, InTo);
            RetimeKeys(Track->Get_BendKeys(), InFrom, InTo);
        }
        RetimeGroups(InOutMetadata.Groups, InPhaseId, InChannel, Get_KeyedTimes(*Phase, InChannel),
            {FCk_ProceduralActionRetimedKey{InFrom, InTo}});
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    DeleteKey(
        FName InPhaseId,
        FName InChannel,
        double InTime,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    return CommitDraft(*Clip, LOCTEXT("DeleteKey", "Delete Action Key"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        if (Phase == nullptr)
        { return Refuse(TEXT("Phase ID does not exist."), OutEditError); }
        const auto KeyedAtTime = AtTime(InTime);
        if (InChannel == FCk_ProceduralAction_PoseGroup::Get_BodyChannelName())
        { Phase->Get_BodyKeys().RemoveAll(KeyedAtTime); }
        else if (auto* Track = FindTrack(*Phase, InChannel))
        {
            Track->Get_PositionKeys().RemoveAll(KeyedAtTime);
            Track->Get_OrientationKeys().RemoveAll(KeyedAtTime);
            Track->Get_BendKeys().RemoveAll(KeyedAtTime);
        }
        RetimeGroups(InOutMetadata.Groups, InPhaseId, InChannel, Get_KeyedTimes(*Phase, InChannel), {});
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    SetReachPolicy(
        FName InPhaseId,
        FName InSlotId,
        ECk_ProceduralAction_ReachPolicy InPolicy,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    return CommitDraft(*Clip, LOCTEXT("SetReachPolicy", "Set Action Reach Policy"),
        [&](FCk_ProceduralAction_ClipDraft& InOutDraft, FEditorMetadata&, FString& OutEditError)
    {
        auto* Phase = FindPhase(InOutDraft.Get_Phases(), InPhaseId);
        auto* Track = Phase == nullptr ? nullptr : FindTrack(*Phase, InSlotId);
        if (Track == nullptr)
        { return Refuse(TEXT("This phase has no track for the slot."), OutEditError); }
        Track->Set_ReachPolicy(InPolicy);
        return true;
    }, OutError);
}

auto
    FCk_ProceduralActionAuthoringCommands::
    RenamePose(
        const FGuid& InPoseId,
        FName InLabel,
        FString& OutError)
    -> bool
{
    using namespace ck_procedural_action_authoring;
    auto* Clip = GetClip(OutError);
    if (Clip == nullptr)
    { return false; }
    if (InLabel.IsNone())
    { return Refuse(TEXT("A pose needs a name."), OutError); }
    return CommitDraft(*Clip, LOCTEXT("RenamePose", "Rename Action Pose"),
        [&](FCk_ProceduralAction_ClipDraft&, FEditorMetadata& InOutMetadata, FString& OutEditError)
    {
        auto* Group = InOutMetadata.Groups.FindByPredicate([&InPoseId](const FCk_ProceduralAction_PoseGroup& InGroup)
            { return InGroup.Get_Id() == InPoseId; });
        if (Group == nullptr)
        { return Refuse(TEXT("The pose does not exist in this clip."), OutEditError); }
        Group->Set_Label(InLabel);
        return true;
    }, OutError);
}

#undef LOCTEXT_NAMESPACE
