#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Data.h"
#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"

#include "Engine/AssetUserData.h"
#include "Kismet/BlueprintFunctionLibrary.h"

#include "CkProceduralActionEditorData.generated.h"

class UCk_ProceduralActionPreview_Config;

// --------------------------------------------------------------------------------------------------------------------

/** The creature the clip editor previews a clip on. The runtime binds a clip to whatever subject plays it, so this lives
 *  only in the clip's editor data. A walking subject names a rig and a gait; otherwise the layout's limbs are dedicated,
 *  and an empty layout previews the body alone. The editor derives the preview (limb frames at rest, a floor,
 *  locomotion) from it; _Scenario, when set, is an authored preview config whose fields replace the derived ones. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionEditor_Subject
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralActionEditor_Subject);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walking", meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UCk_ProceduralRig_Data> _WalkingRig;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Walking", meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UCk_ProceduralGait_Data> _WalkingGait;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dedicated", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_Layout _Layout;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_SlotBinding> _SlotBindings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UCk_ProceduralActionPreview_Config> _Scenario;
public:
    auto
        Get_IsEmpty() const
        -> bool
    { return _WalkingRig.IsNull() && _Layout.Get_Limbs().IsEmpty() && _Scenario.IsNull(); }

    CK_PROPERTY(_WalkingRig);
    CK_PROPERTY(_WalkingGait);
    CK_PROPERTY(_Layout);
    CK_PROPERTY(_SlotBindings);
    CK_PROPERTY(_Scenario);
};

// --------------------------------------------------------------------------------------------------------------------

/** A name for the keys at one phase and key time. It holds no transforms and no channel list: the keys in the phases stay
 *  the only motion, and the channels a label covers are whichever are keyed at its time. */
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionEditor_PoseLabel
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralActionEditor_PoseLabel);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = true))
    FName _PhaseId;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = true))
    FCk_FloatRange_0to1 _Time;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Pose", meta = (AllowPrivateAccess = true))
    FName _Label;
public:
    CK_PROPERTY(_PhaseId);
    CK_PROPERTY(_Time);
    CK_PROPERTY(_Label);
    CK_DEFINE_CONSTRUCTORS(FCk_ProceduralActionEditor_PoseLabel, _PhaseId, _Time, _Label);
};

// --------------------------------------------------------------------------------------------------------------------

/** The clip editor's data for one clip: its preview subject and its pose labels. Stored in the clip's editor-only
 *  asset user data, so it saves with the clip and never reaches a cook. Reach it through
 *  UCk_Utils_ProceduralActionEditor_UE::Get_EditorData. */
UCLASS(BlueprintType, DefaultToInstanced, EditInlineNew)
class CKPROCEDURALANIMATIONEDITOR_API UCk_ProceduralActionEditorData : public UAssetUserData
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_ProceduralActionEditorData);
private:
    UPROPERTY(EditAnywhere, Category = "Preview", meta = (AllowPrivateAccess = true, ShowOnlyInnerProperties))
    FCk_ProceduralActionEditor_Subject _Subject;
    UPROPERTY()
    TArray<FCk_ProceduralActionEditor_PoseLabel> _PoseLabels;
public:
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction|Editor")
    FCk_ProceduralActionEditor_Subject
    Get_Subject() const;

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction|Editor")
    void
    Set_Subject(
        const FCk_ProceduralActionEditor_Subject& InSubject);

    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction|Editor")
    TArray<FCk_ProceduralActionEditor_PoseLabel>
    Get_PoseLabels() const;

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction|Editor")
    void
    Set_PoseLabels(
        const TArray<FCk_ProceduralActionEditor_PoseLabel>& InLabels);
};

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable)
class CKPROCEDURALANIMATIONEDITOR_API UCk_Utils_ProceduralActionEditor_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_Utils_ProceduralActionEditor_UE);

public:
    /** The clip's editor data, created on first use. Creating it changes the clip: call it inside the transaction of
     *  the edit that needs it, or before saving a clip built in code. Null only for a null clip. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralAction|Editor",
              DisplayName = "[Ck][ProceduralAction] Get Editor Data")
    static UCk_ProceduralActionEditorData*
    Get_EditorData(
        UCk_ProceduralAction_Data* InClip);

    /** The clip's editor data when it has any; never creates it. */
    UFUNCTION(BlueprintPure, Category = "Ck|Utils|ProceduralAction|Editor",
              DisplayName = "[Ck][ProceduralAction] Try Get Editor Data")
    static UCk_ProceduralActionEditorData*
    TryGet_EditorData(
        const UCk_ProceduralAction_Data* InClip);

    /** Moves a clip's legacy editor fields (subject and pose groups saved on the clip itself) into its editor data:
     *  the subject is copied and each pose group becomes one label at its phase and time. Clears the legacy fields. */
    static auto
        MigrateLegacyEditorData(
            UCk_ProceduralAction_Data& InClip)
        -> void;
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    // The clip's preview subject; empty when the clip has no editor data yet. Never creates it.
    CKPROCEDURALANIMATIONEDITOR_API auto
        Get_Subject(
            const UCk_ProceduralAction_Data& InClip)
        -> FCk_ProceduralActionEditor_Subject;

    // The clip's pose labels; empty when the clip has no editor data yet. Never creates it.
    CKPROCEDURALANIMATIONEDITOR_API auto
        Get_PoseLabels(
            const UCk_ProceduralAction_Data& InClip)
        -> TArray<FCk_ProceduralActionEditor_PoseLabel>;

    // The label at a phase and key time, if any.
    CKPROCEDURALANIMATIONEDITOR_API auto
        FindPoseLabel(
            const TArray<FCk_ProceduralActionEditor_PoseLabel>& InLabels,
            FName InPhaseId,
            double InTime)
        -> const FCk_ProceduralActionEditor_PoseLabel*;

    // The channel name the editor uses for the body; limb channels are named by their slot.
    CKPROCEDURALANIMATIONEDITOR_API auto
        Get_BodyChannelName()
        -> FName;

    // Two key times name one pose within this tolerance, the precision a gizmo or a curve drag writes at.
    CKPROCEDURALANIMATIONEDITOR_API auto
        IsSamePoseTime(
            double InA,
            double InB)
        -> bool;

    // The channels a pose sets: the body when its body mode is Set, then every limb it targets.
    CKPROCEDURALANIMATIONEDITOR_API auto
        Get_PoseChannels(
            const FCk_ProceduralAction_PoseKey& InPose)
        -> TArray<FName>;

    // Every channel with a key at InTime in the phase, the body first, then the limb tracks in their order. A limb
    // channel counts its position, orientation and bend keys.
    CKPROCEDURALANIMATIONEDITOR_API auto
        Get_ChannelsKeyedAt(
            const FCk_ProceduralAction_Phase& InPhase,
            double InTime)
        -> TArray<FName>;
}

// --------------------------------------------------------------------------------------------------------------------
