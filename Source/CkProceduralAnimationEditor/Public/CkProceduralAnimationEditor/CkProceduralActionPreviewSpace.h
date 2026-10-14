#pragma once

#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"
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

    CKPROCEDURALANIMATIONEDITOR_API auto ResolveAuthoredRest(
        const UCk_ProceduralActionPreview_Config& InConfig,
        const procedural_action::FCompiledSlot& InSlot,
        FTransform& OutRestInTrackSpace) -> bool;

    CKPROCEDURALANIMATIONEDITOR_API auto ToWorldPosition(
        const FTransform& InFrame, float InPositionScale, const FVector& InTrackPosition) -> FVector;

    CKPROCEDURALANIMATIONEDITOR_API auto ToTrackPosition(
        const FTransform& InFrame, float InPositionScale, const FVector& InWorldPosition) -> FVector;
}
