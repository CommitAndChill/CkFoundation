#pragma once

#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"
#include "CkProceduralAnimation/Core/CkProceduralActionEvaluation.h"
#include "CkProceduralAnimationEditor/CkProceduralActionPreview_Config.h"

namespace ck::procedural_action_editor
{
    CKPROCEDURALANIMATIONEDITOR_API auto ResolveBodyWorld(
        const UCk_ProceduralActionPreview_Config& InConfig,
        const procedural_action::FCompiledClip& InClip,
        const procedural_action::FEvaluatedPose& InPose,
        FTransform& OutBodyWorld) -> bool;

    CKPROCEDURALANIMATIONEDITOR_API auto ResolveSlotFrame(
        const UCk_ProceduralActionPreview_Config& InConfig,
        const procedural_action::FCompiledSlot& InSlot,
        const FTransform& InBodyWorld,
        FTransform& OutFrame,
        float& OutPositionScale) -> bool;

    // The bound limb's rest endpoint in the slot's track space.
    CKPROCEDURALANIMATIONEDITOR_API auto ResolveRestInTrackSpace(
        const UCk_ProceduralActionPreview_Config& InConfig,
        const procedural_action::FCompiledSlot& InSlot,
        FTransform& OutRestInTrackSpace) -> bool;

    CKPROCEDURALANIMATIONEDITOR_API auto ToWorldPosition(
        const FTransform& InFrame, float InPositionScale, const FVector& InTrackPosition) -> FVector;

    CKPROCEDURALANIMATIONEDITOR_API auto ToTrackPosition(
        const FTransform& InFrame, float InPositionScale, const FVector& InWorldPosition) -> FVector;

    // The hip of an observed limb: observation segments are centered on the rigid solver bone and their scale is visual
    // only, so the hip is half the first segment's length back along it.
    CKPROCEDURALANIMATIONEDITOR_API auto TryGetObservedHip(
        const FCk_ProceduralArticulation_LimbPose& InLimb, double InFirstSegmentLengthCm, FVector& OutHip) -> bool;

    // The reach (sum of segment lengths) and first segment length of a subject limb; false when the subject has no such limb.
    CKPROCEDURALANIMATIONEDITOR_API auto Get_LimbReach(
        const UCk_ProceduralActionPreview_Config& InConfig, FName InLimbId, double& OutReachCm, double& OutFirstLengthCm) -> bool;
}
