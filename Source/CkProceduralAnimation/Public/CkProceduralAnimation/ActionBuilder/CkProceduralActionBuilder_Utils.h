#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include "CkProceduralAnimation/ActionBuilder/CkProceduralActionBuilder_Data.h"

#include <Kismet/BlueprintFunctionLibrary.h>

#include "CkProceduralActionBuilder_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

/** Value operations on an FCk_ProceduralAction_ClipDraft, shared by code, script, Blueprint and the editor commands.
 *  Every mutation validates all of its inputs first and leaves the draft unchanged when it reports Invalid; diagnostics
 *  use the compiler's field paths. Times inside a phase are normalized; durations are FCk_Time. Inputs are never clamped,
 *  sorted or invented: a key is inserted at its place in an already strictly increasing key array, nothing more.
 *  Build_Into compiles the draft with the one clip compiler before it touches the destination; it is not Play and not an
 *  editor transaction. The destination is caller-owned: keep it referenced (a UPROPERTY, a rooted owner) for as long as
 *  it is built and played, because an Outer alone does not keep it alive. */
UCLASS(NotBlueprintable)
class CKPROCEDURALANIMATION_API UCk_Utils_ProceduralActionBuilder_UE : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_ProceduralActionBuilder_UE);

public:
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Set Body Channel")
    static FCk_ProceduralAction_ValidationReport
    Set_BodyChannel(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_BodyChannel& InChannel);

    /** Sets the frame every track of the slot is keyed in, for the whole clip. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Set Slot Frame")
    static FCk_ProceduralAction_ValidationReport
    Set_SlotFrame(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        FName InSlot,
        ECk_ProceduralAction_TrackSpace InSpace);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Add Phase")
    static FCk_ProceduralAction_ValidationReport
    Add_Phase(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Phase& InPhase);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Add Slot")
    static FCk_ProceduralAction_ValidationReport
    Add_Slot(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Slot& InSlot);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Add Body Key")
    static FCk_ProceduralAction_ValidationReport
    Add_BodyKey(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        const FCk_ProceduralAction_TransformKey& InKey);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Add Limb Key")
    static FCk_ProceduralAction_ValidationReport
    Add_LimbKey(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        FCk_FloatRange_0to1 InTime,
        const FCk_ProceduralAction_LimbPoseTarget& InTarget);

    /** Atomic over the body and every listed limb: one rejected channel writes nothing. Writes zero tangents. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Add Pose Key")
    static FCk_ProceduralAction_ValidationReport
    Add_PoseKey(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        const FCk_ProceduralAction_PoseKey& InPose);

    /** Requires an existing key at the pose time on every channel it writes and replaces only the values. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Replace Pose Key")
    static FCk_ProceduralAction_ValidationReport
    Replace_PoseKey(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        FName InPhaseId,
        const FCk_ProceduralAction_PoseKey& InPose);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Add Marker")
    static FCk_ProceduralAction_ValidationReport
    Add_Marker(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Marker& InMarker);

    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Add Window")
    static FCk_ProceduralAction_ValidationReport
    Add_Window(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_Window& InWindow);

    /** Writes a reflected copy of every source track into the destination slot and gives the destination the source
     *  slot's frame. Rejects rest-normalized sources (their frame needs subject topology), an existing
     *  destination track in any written phase, and a destination that already has tracks in another frame. Body motion and
     *  events are never mirrored. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Mirror Slot")
    static FCk_ProceduralAction_ValidationReport
    Mirror_Slot(
        UPARAM(ref) FCk_ProceduralAction_ClipDraft& InDraft,
        const FCk_ProceduralAction_MirrorSpec& InMirror);

    /** OutDraft is reset when the report is Invalid. A Valid report may carry warnings. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Make From Recipe")
    static FCk_ProceduralAction_ValidationReport
    Make_FromRecipe(
        const FCk_ProceduralAction_RecipeSpec& InRecipe,
        FCk_ProceduralAction_ClipDraft& OutDraft);

    /** Copies the clip's authored fields; OutDraft is reset when the report is Invalid. The copy is not compiled. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Make Draft From Data")
    static FCk_ProceduralAction_ValidationReport
    Make_Draft_FromData(
        UCk_ProceduralAction_Data* InSource,
        FCk_ProceduralAction_ClipDraft& OutDraft);

    /** Replaces the destination's five authored fields only when the draft compiles; otherwise leaves them untouched. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Build Into")
    static FCk_ProceduralAction_ValidationReport
    Build_Into(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        UCk_ProceduralAction_Data* InDestination);

    /** The incremental authoring path: replaces the destination's five authored fields without compiling the draft, so
     *  an author can save a clip that is still missing keys or references. It rejects only a null destination or an
     *  unknown CompatibilityVersion. Nothing about playback changes: Create_FromClip and Build_Into still compile and
     *  refuse such a clip with the compiler's diagnostics. */
    UFUNCTION(BlueprintCallable, Category = "Ck|Utils|ProceduralActionBuilder",
        DisplayName = "[Ck][ProceduralActionBuilder] Write Into")
    static FCk_ProceduralAction_ValidationReport
    Write_Into(
        const FCk_ProceduralAction_ClipDraft& InDraft,
        UCk_ProceduralAction_Data* InDestination);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action
{
    CKPROCEDURALANIMATION_API auto
        ReflectVector(
            const FVector& InVector,
            ECk_ProceduralAction_MirrorPlane InPlane)
        -> FVector;

    // q' = (w, -S v) for the plane's reflection S: the proper rotation S R S, never a negative-scale transform.
    CKPROCEDURALANIMATION_API auto
        ReflectRotation(
            const FQuat& InRotation,
            ECk_ProceduralAction_MirrorPlane InPlane)
        -> FQuat;

    // Interior keys get the finite-difference (Catmull-Rom) tangent of their neighbours, arrive equal to leave; the first
    // and last keys get zero tangents, so consecutive phases meet at rest.
    CKPROCEDURALANIMATION_API auto
        ComputeCatmullRomTangents(
            TArray<FCk_ProceduralAction_VectorKey>& InOutKeys)
        -> void;
}

// --------------------------------------------------------------------------------------------------------------------
