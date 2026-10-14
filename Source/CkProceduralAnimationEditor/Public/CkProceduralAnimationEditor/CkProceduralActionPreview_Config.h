#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment_Data.h"
#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment_Data.h"
#include "CkProceduralAnimation/Gait/CkProceduralGait_Fragment_Data.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Fragment_Data.h"

#include "CkCore/Types/DataAsset/CkDataAsset.h"
#include "CkProceduralAnimationEditor/CkProceduralActionEditorData.h"

#include "CkProceduralActionPreview_Config.generated.h"

class UStaticMesh;
class UCk_ProceduralGait_Data;
class UCk_ProceduralRig_Data;

UENUM(BlueprintType)
enum class ECk_ProceduralActionPreview_SubjectMode : uint8
{
    Standalone,
    Walking
};

UENUM(BlueprintType)
enum class ECk_ProceduralActionPreview_ScenarioEventKind : uint8
{
    RemoveFloor,
    RestoreFloor,
    RecoverLimbs,
    DetachLimb
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_ProceduralActionPreview_ScenarioEventKind);

// Preview environment requests are replayed at an absolute 60 Hz preview step.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionPreview_ScenarioEvent
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralActionPreview_ScenarioEvent);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (AllowPrivateAccess = true))
    int64 _Step = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (AllowPrivateAccess = true))
    ECk_ProceduralActionPreview_ScenarioEventKind _Kind = ECk_ProceduralActionPreview_ScenarioEventKind::RemoveFloor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (AllowPrivateAccess = true))
    TArray<FName> _LimbIds;
public:
    CK_PROPERTY(_Step);
    CK_PROPERTY(_Kind);
    CK_PROPERTY(_LimbIds);
};

// Preview-only rig declaration. Segment transforms are instantiated as transient entities by Solver later;
// no registry handle is persisted in this asset.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionPreview_LimbBinding
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralActionPreview_LimbBinding);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject", meta = (AllowPrivateAccess = true))
    FName _LimbId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject", meta = (AllowPrivateAccess = true))
    TArray<FTransform> _SegmentFrames;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject", meta = (AllowPrivateAccess = true))
    FTransform _TerminalFrame = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UStaticMesh> _PreviewMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (AllowPrivateAccess = true))
    FVector _PreviewMeshScale = FVector::OneVector;
public:
    CK_PROPERTY(_LimbId);
    CK_PROPERTY(_SegmentFrames);
    CK_PROPERTY(_TerminalFrame);
    CK_PROPERTY(_PreviewMesh);
    CK_PROPERTY(_PreviewMeshScale);
};

// Preview-only static Jolt box. Kept separate from the walking support floor.
USTRUCT(BlueprintType)
struct CKPROCEDURALANIMATIONEDITOR_API FCk_ProceduralActionPreview_Obstacle
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(FCk_ProceduralActionPreview_Obstacle);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (AllowPrivateAccess = true))
    FTransform _Frame = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (AllowPrivateAccess = true, Units = "cm"))
    FVector _HalfExtents = FVector{10.0, 50.0, 50.0};
public:
    CK_PROPERTY(_Frame);
    CK_PROPERTY(_HalfExtents);
};

UCLASS(BlueprintType)
class CKPROCEDURALANIMATIONEDITOR_API UCk_ProceduralActionPreview_Config : public UCk_DataAsset_PDA
{
    GENERATED_BODY()
public:
    CK_GENERATED_BODY(UCk_ProceduralActionPreview_Config);
private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject", meta = (AllowPrivateAccess = true))
    ECk_ProceduralActionPreview_SubjectMode _SubjectMode = ECk_ProceduralActionPreview_SubjectMode::Standalone;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Placement", meta = (AllowPrivateAccess = true))
    FTransform _RootFrame = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Topology", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_Layout _Layout;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Clearance", meta = (AllowPrivateAccess = true))
    FCk_ProceduralArticulation_ClearanceSettings _StandaloneClearanceSettings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralActionPreview_LimbBinding> _LimbBindings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Bindings", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralAction_SlotBinding> _SlotBindings;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Walking", meta = (AllowPrivateAccess = true))
    TObjectPtr<UCk_ProceduralRig_Data> _WalkingRig;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Walking", meta = (AllowPrivateAccess = true))
    TObjectPtr<UCk_ProceduralGait_Data> _WalkingGait;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Walking", meta = (AllowPrivateAccess = true))
    FCk_ProceduralLimbControl_Locomotion _WalkingLocomotion;
    // Preview assembly only. Runtime clips keep motion/events; the production Player pins these tuners.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Playback|Action Tuning", meta = (AllowPrivateAccess = true))
    FCk_ProceduralAction_Tuning _ActionTuning;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Subject|Bindings", meta = (AllowPrivateAccess = true))
    FTransform _PresentationFrame = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (AllowPrivateAccess = true))
    bool _HasStaticFloor = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (AllowPrivateAccess = true))
    FTransform _StaticFloorFrame = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface", meta = (AllowPrivateAccess = true, Units = "cm"))
    FVector _StaticFloorHalfExtents = FVector{250.0, 250.0, 10.0};
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface|Obstacles", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralActionPreview_Obstacle> _StaticObstacles;
    // Zero preserves the usual stop at action terminal. A positive horizon keeps the private world
    // observing recovery after the action has closed, then stops at this bounded step.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (AllowPrivateAccess = true))
    int64 _ObservationHorizonStep = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Scenario", meta = (AllowPrivateAccess = true))
    TArray<FCk_ProceduralActionPreview_ScenarioEvent> _ScenarioEvents;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Authored|Initial Pose", meta = (AllowPrivateAccess = true))
    FTransform _InitialBody = FTransform::Identity;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Authored|Initial Pose", meta = (AllowPrivateAccess = true))
    TMap<FName, FTransform> _InitialLimbEndpoints;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Authored|Initial Pose", meta = (AllowPrivateAccess = true))
    TMap<FName, FVector> _InitialBendDirections;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UStaticMesh> _BodyPreviewMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Appearance", meta = (AllowPrivateAccess = true))
    FVector _BodyPreviewMeshScale = FVector::OneVector;
public:
    auto
        ValidateAppearance(
            FString& OutError) const
        -> bool;
    auto
        ValidateSurface(
            FString& OutError) const
        -> bool;
    auto
        ValidateScenario(
            FString& OutError) const
        -> bool;

    CK_PROPERTY(_SubjectMode);
    CK_PROPERTY(_RootFrame);
    CK_PROPERTY(_Layout);
    CK_PROPERTY(_StandaloneClearanceSettings);
    CK_PROPERTY(_LimbBindings);
    CK_PROPERTY(_SlotBindings);
    CK_PROPERTY(_WalkingRig);
    CK_PROPERTY(_WalkingGait);
    CK_PROPERTY(_WalkingLocomotion);
    CK_PROPERTY(_ActionTuning);
    CK_PROPERTY(_PresentationFrame);
    CK_PROPERTY(_HasStaticFloor);
    CK_PROPERTY(_StaticFloorFrame);
    CK_PROPERTY(_StaticFloorHalfExtents);
    CK_PROPERTY(_StaticObstacles);
    CK_PROPERTY(_ObservationHorizonStep);
    CK_PROPERTY(_ScenarioEvents);
    CK_PROPERTY(_InitialBody);
    CK_PROPERTY(_InitialLimbEndpoints);
    CK_PROPERTY(_InitialBendDirections);
    CK_PROPERTY(_BodyPreviewMesh);
    CK_PROPERTY(_BodyPreviewMeshScale);
};

// --------------------------------------------------------------------------------------------------------------------

namespace ck::procedural_action_editor
{
    /** The preview a clip's editor subject describes, as a new transient object in the transient package; the caller roots
     *  it. With a scenario the scenario is copied whole and the subject's slot bindings, when it has any, replace the
     *  scenario's. Otherwise a walking subject stands its rig at rest on a static floor whose top is at Z 0, with the root
     *  at the rig's clearance, a limb binding per leg and count-only support of max(1, legs - 2); a dedicated subject holds
     *  its layout at the origin with a limb binding per limb; an empty subject previews the body alone. Without a scenario
     *  the sampling budget lets a limb travel twice the longest chain reach and turn 180 degrees per step. */
    CKPROCEDURALANIMATIONEDITOR_API auto
        DerivePreviewConfig(
            const UCk_ProceduralAction_Data& InClip)
        -> UCk_ProceduralActionPreview_Config*;

    // A rig leg at its rest pose under InRootFrame: a frame at the start of each segment, +X along it, the terminal at the
    // rest foot; a two-segment knee bends toward the leg's pole.
    CKPROCEDURALANIMATIONEDITOR_API auto
        MakeRestLimbBinding(
            const FCk_ProceduralLeg_Spec& InLeg,
            const FTransform& InRootFrame)
        -> FCk_ProceduralActionPreview_LimbBinding;
}

// --------------------------------------------------------------------------------------------------------------------
