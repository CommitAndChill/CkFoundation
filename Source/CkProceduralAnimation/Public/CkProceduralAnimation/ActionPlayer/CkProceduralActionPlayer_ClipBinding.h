#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Observation.h"
#include "CkProceduralAnimation/Core/CkProceduralActionEvaluation.h"
#include "CkProceduralAnimation/Core/CkProceduralActionValidation.h"

namespace ck_procedural_action_player_service
{
    // Shared admission check for asynchronously loaded and already-loaded clips.
    auto
    BindCompiledClip(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::FProceduralActionClipParams& InParams,
        const ck::procedural_action::FCompiledClip& InClip,
        TArray<FName>& OutLimbIds)
        -> bool;

    // Resolves a compiled clip's initial values against a copied accepted entry and current topology.
    CKPROCEDURALANIMATION_API auto
    CaptureClipInitial(
        const FCk_Handle_ProceduralArticulation& InSubject,
        const ck::FFragment_ProceduralAction_Compiled& InCompiled,
        const FCk_ProceduralArticulation_Pose& InEntry,
        const FCk_ProceduralArticulation_Pose& InDefault,
        ck::procedural_action::FInitialPose& OutInitial)
        -> bool;
}
