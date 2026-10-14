#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"
#include "Math/Axis.h"

class FCk_ProceduralActionSolvedCaptureTarget;
class FCk_ProceduralActionSolvedSnapshot;
class UCk_ProceduralActionPreview_Config;

// Edits the authored asset only. Each successful command is one editor transaction;
// Compile remains the authority on whether the resulting draft may play.
// Pose, mirror and phase-timing commands copy the clip into a draft, apply the builder operations and write it back with
// Write_Into, keys and editor pose metadata in the same transaction. Every builder operation validates its inputs, but the
// result may still not compile (a phase without its 0 and 1 keys): such a clip stays saveable and Play reports why.
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionAuthoringCommands
{
public:
    explicit FCk_ProceduralActionAuthoringCommands(
        UCk_ProceduralAction_Data* InClip);

    auto
        AddPhase(
            FName InId,
            FCk_Time InDuration,
            FString& OutError)
        -> bool;
    auto
        ReorderPhase(
            FName InId,
            int32 InNewIndex,
            FString& OutError)
        -> bool;
    auto
        RemovePhase(
            FName InId,
            FString& OutError)
        -> bool;
    auto
        AddSlot(
            FName InId,
            FString& OutError)
        -> bool;
    auto
        RemoveSlot(
            FName InId,
            FString& OutError)
        -> bool;
    auto
        RenameSlot(
            UCk_ProceduralActionPreview_Config* InConfig,
            FName InOldId,
            FName InNewId,
            FString& OutError)
        -> bool;
    auto
        AddLimbTrack(
            FName InPhaseId,
            FName InSlotId,
            FString& OutError)
        -> bool;
    auto
        AddBodyKey(
            FName InPhaseId,
            double InNormalizedTime,
            const FTransform& InValue,
            FString& OutError)
        -> bool;
    auto
        AddPositionKey(
            FName InPhaseId,
            FName InSlotId,
            double InNormalizedTime,
            const FVector& InValue,
            FString& OutError)
        -> bool;
    auto
        SetPositionKey(
            FName InPhaseId,
            FName InSlotId,
            double InNormalizedTime,
            const FVector& InValue,
            FString& OutError)
        -> bool;
    auto
        SetBendKey(
            FName InPhaseId,
            FName InSlotId,
            double InNormalizedTime,
            const FVector& InValue,
            FString& OutError)
        -> bool;
    auto
        AddMarker(
            FName InId,
            FName InPhaseId,
            double InNormalizedTime,
            FString& OutError)
        -> bool;
    auto
        AddWindow(
            FName InId,
            FName InStartPhaseId,
            double InStartTime,
            FName InEndPhaseId,
            double InEndTime,
            FString& OutError)
        -> bool;
    auto
        MoveMarker(
            FName InId,
            FName InPhaseId,
            double InNormalizedTime,
            FString& OutError)
        -> bool;
    auto
        MoveWindowEdge(
            FName InId,
            bool InStartEdge,
            FName InPhaseId,
            double InNormalizedTime,
            FString& OutError)
        -> bool;
    auto
        SetTrackSpace(
            FName InPhaseId,
            FName InSlotId,
            ECk_ProceduralAction_TrackSpace InSpace,
            FString& OutError)
        -> bool;
    auto
        MirrorSlot(
            FName InSourceSlot,
            FName InDestinationSlot,
            EAxis::Type InPlaneNormal,
            FString& OutError)
        -> bool;
    // InParticipants are the channels the pose must cover (slots, or FCk_ProceduralAction_PoseGroup::Get_BodyChannelName);
    // a participant the pose omits, or a pose channel outside them, is refused. The time must be unkeyed on every channel.
    auto
        CapturePose(
            FName InPhaseId,
            const FCk_ProceduralAction_PoseKey& InPose,
            const TArray<FName>& InParticipants,
            FName InLabel,
            FGuid& OutPoseId,
            FString& OutError)
        -> bool;
    // Replaces the values of an existing pose at its own phase and time; the pose must list exactly the group's channels.
    auto
        UpdatePose(
            const FGuid& InPoseId,
            const FCk_ProceduralAction_PoseKey& InPose,
            FString& OutError)
        -> bool;
    auto
        DuplicatePose(
            const FGuid& InPoseId,
            FName InPhaseId,
            double InNormalizedTime,
            FGuid& OutPoseId,
            FString& OutError)
        -> bool;
    auto
        MovePose(
            const FGuid& InPoseId,
            FName InPhaseId,
            double InNormalizedTime,
            FString& OutError)
        -> bool;
    auto
        DeletePose(
            const FGuid& InPoseId,
            FString& OutError)
        -> bool;
    // Requires a ready, settled snapshot recorded for InTarget's session revision; converts every requested channel into
    // its slot's declared track space and writes nothing unless the whole conversion succeeds.
    auto
        CaptureSolvedPose(
            const FCk_ProceduralActionSolvedSnapshot& InSnapshot,
            const FCk_ProceduralActionSolvedCaptureTarget& InTarget,
            FGuid& OutPoseId,
            FString& OutError)
        -> bool;
    // Validated like Add_Phase; the phase's keys keep their normalized times.
    auto
        SetPhaseDuration(
            FName InPhaseId,
            FCk_Time InDuration,
            FString& OutError)
        -> bool;
    // Sets the phase's preset easing and progress curve together, because a custom curve requires Linear easing.
    auto
        SetPhaseTiming(
            FName InPhaseId,
            ECk_ProceduralAction_Easing InEasing,
            const FCk_ProceduralAction_ProgressCurve& InProgress,
            FString& OutError)
        -> bool;
    // Fills an empty clip from a recipe and names each recipe key time as a pose; never overwrites authored content.
    auto
        CreateFromRecipe(
            const FCk_ProceduralAction_RecipeSpec& InRecipe,
            FString& OutError)
        -> bool;
    auto
        RetargetSlot(
            UCk_ProceduralActionPreview_Config* InConfig,
            FName InSlotId,
            FName InLimbId,
            FString& OutError)
        -> bool;

private:
    auto
        GetClip(
            FString& OutError) const
        -> UCk_ProceduralAction_Data*;

    TWeakObjectPtr<UCk_ProceduralAction_Data> _Clip;
};
