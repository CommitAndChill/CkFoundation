#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorData.h"
#include "Math/Axis.h"

class FCk_ProceduralActionSolvedCaptureTarget;
class FCk_ProceduralActionSolvedSnapshot;
class UCk_ProceduralActionPreview_Config;

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    // A compiler or builder diagnostic as one line: "<field> [<code>]: expected <expected>; got <actual>".
    template <typename TDiagnostic>
    auto
        Describe(
            const TDiagnostic& InDiagnostic)
        -> FString
    {
        return FString::Printf(TEXT("%s [%s]: expected %s; got %s"), *InDiagnostic.Get_FieldPath(),
            *InDiagnostic.Get_Code().ToString(), *InDiagnostic.Get_Expected(), *InDiagnostic.Get_Actual());
    }
}

// --------------------------------------------------------------------------------------------------------------------

// A key that moved in time on one channel, so the pose label at its time can follow.
class CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionRetimedKey
{
    CK_GENERATED_BODY(FCk_ProceduralActionRetimedKey);

private:
    double _From = 0.0;
    double _To = 0.0;

public:
    CK_PROPERTY_GET(_From);
    CK_PROPERTY_GET(_To);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralActionRetimedKey, _From, _To);
};

// --------------------------------------------------------------------------------------------------------------------

// Edits the authored asset only. Every command copies the clip into a builder draft, applies its edit and writes it back
// with Write_Into in one editor transaction, keys and editor data (pose labels, subject) together; an edit that changes
// nothing opens no transaction. Every command validates its inputs before the transaction opens, but the result may
// still not compile (a phase without its key at 1): such a clip stays saveable and Play reports why. A label names the
// keys at its phase and time: it follows a moved key when no other channel keeps a key at its old time, and it goes when
// no channel is keyed at its time any more.
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
    // Renames the phase and every marker, window edge and pose label that names it.
    auto
        RenamePhase(
            FName InOldId,
            FName InNewId,
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
    // Renames the slot, its tracks and its subject binding together; with a scenario, its binding and initial pose too.
    auto
        RenameSlot(
            UCk_ProceduralActionPreview_Config* InScenario,
            FName InOldId,
            FName InNewId,
            FString& OutError)
        -> bool;
    // Replaces a phase's body keys and their interpolation; InRetimed names the keys that moved. Refused when the compiler
    // rejects the phase's key times.
    auto
        SetBodyKeys(
            FName InPhaseId,
            const TArray<FCk_ProceduralAction_TransformKey>& InKeys,
            ECk_ProceduralAction_Interpolation InInterpolation,
            const TArray<FCk_ProceduralActionRetimedKey>& InRetimed,
            FString& OutError)
        -> bool;
    // Replaces a slot track's position keys and its interpolation; InRetimed names the keys that moved. Refused when the
    // compiler rejects the phase's key times.
    auto
        SetPositionKeys(
            FName InPhaseId,
            FName InSlotId,
            const TArray<FCk_ProceduralAction_VectorKey>& InKeys,
            ECk_ProceduralAction_Interpolation InInterpolation,
            const TArray<FCk_ProceduralActionRetimedKey>& InRetimed,
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
    // Moves both edges at once, so a window keeps its length when dragged.
    auto
        MoveWindow(
            FName InId,
            const FCk_ProceduralAction_TimeRef& InStart,
            const FCk_ProceduralAction_TimeRef& InEnd,
            FString& OutError)
        -> bool;
    // Removes the marker or window named InId.
    auto
        RemoveEvent(
            FName InId,
            FString& OutError)
        -> bool;
    // The slot's frame for the whole clip; refused once any of its tracks has keys, which the change would reinterpret.
    auto
        SetSlotSpace(
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
    // Requires a ready, settled snapshot recorded for InTarget's session revision; converts every requested channel into
    // its slot's declared track space and writes nothing unless the whole conversion succeeds. Records it like RecordPose.
    auto
        CaptureSolvedPose(
            const FCk_ProceduralActionSolvedSnapshot& InSnapshot,
            const FCk_ProceduralActionSolvedCaptureTarget& InTarget,
            FString& OutError)
        -> bool;
    // Validated like Add_Phase; the phase's keys keep their normalized times.
    auto
        SetPhaseDuration(
            FName InPhaseId,
            FCk_Time InDuration,
            FString& OutError)
        -> bool;
    auto
        SetPhaseEasing(
            FName InPhaseId,
            ECk_ProceduralAction_Easing InEasing,
            FString& OutError)
        -> bool;
    // Fills an empty clip from a recipe and labels each recipe key time; never overwrites authored content.
    auto
        CreateFromRecipe(
            const FCk_ProceduralAction_RecipeSpec& InRecipe,
            FString& OutError)
        -> bool;
    // Binds the clip slot to a limb of the preview subject InConfig describes; the binding is kept on the clip's subject.
    auto
        RetargetSlot(
            const UCk_ProceduralActionPreview_Config* InConfig,
            FName InSlotId,
            FName InLimbId,
            FString& OutError)
        -> bool;
    auto
        SetSubject(
            const FCk_ProceduralActionEditor_Subject& InSubject,
            FString& OutError)
        -> bool;
    // Writes a pose's keys at its time, replacing the kinds of key it sets (a track the phase lacks is added); labels stay
    // as they are.
    auto
        SetPoseKeys(
            FName InPhaseId,
            const FCk_ProceduralAction_PoseKey& InPose,
            FString& OutError)
        -> bool;
    // SetPoseKeys, then labels the time: a label already there keeps its name unless InLabel names it anew; a new label is
    // InLabel, or "Pose N" when InLabel is None.
    auto
        RecordPose(
            FName InPhaseId,
            const FCk_ProceduralAction_PoseKey& InPose,
            FName InLabel,
            FString& OutError)
        -> bool;
    // Moves every key a channel has at InFrom (position, orientation and bend ride together) to InTo in the same phase.
    auto
        MoveKey(
            FName InPhaseId,
            FName InChannel,
            double InFrom,
            double InTo,
            FString& OutError)
        -> bool;
    auto
        DeleteKey(
            FName InPhaseId,
            FName InChannel,
            double InTime,
            FString& OutError)
        -> bool;
    auto
        SetReachPolicy(
            FName InPhaseId,
            FName InSlotId,
            ECk_ProceduralAction_ReachPolicy InPolicy,
            FString& OutError)
        -> bool;
    // Renames the label at a phase and key time; refused when no label is there.
    auto
        RenameLabel(
            FName InPhaseId,
            double InTime,
            FName InLabel,
            FString& OutError)
        -> bool;

private:
    auto
        GetClip(
            FString& OutError) const
        -> UCk_ProceduralAction_Data*;

    TWeakObjectPtr<UCk_ProceduralAction_Data> _Clip;
};

// --------------------------------------------------------------------------------------------------------------------
