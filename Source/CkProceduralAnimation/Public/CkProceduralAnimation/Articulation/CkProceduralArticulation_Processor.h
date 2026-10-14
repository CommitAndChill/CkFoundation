#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Fragment.h"
#include "CkProceduralAnimation/Rig/CkProceduralRig_Processor.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

namespace ck
{
    class FProcessor_ProceduralActionPlayer_Service;

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralArticulation_Update : public ck_exp::TProcessor<
        FProcessor_ProceduralArticulation_Update,
        FCk_Handle_ProceduralArticulation,
        TReadOnly<FFragment_ProceduralArticulation_Params>,
        TReadOnly<FFragment_ProceduralArticulation_Bindings>,
        TReadWrite<FFragment_ProceduralArticulation_Pose>,
        TReadWrite<FFragment_ProceduralArticulation_Health>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using RunAfter = TDepList<FProcessor_ProceduralRig_Update, FProcessor_ProceduralActionPlayer_Service>;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralArticulation_Params& InParams,
            const FFragment_ProceduralArticulation_Bindings& InBindings,
            FFragment_ProceduralArticulation_Pose& InPose,
            FFragment_ProceduralArticulation_Health& InHealth)
            -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralArticulation_Observe : public ck_exp::TProcessor<
        FProcessor_ProceduralArticulation_Observe,
        FCk_Handle_ProceduralArticulation,
        TReadOnly<FFragment_ProceduralArticulation_Params>,
        TReadWrite<FFragment_ProceduralArticulation_Pose>,
        TReadWrite<FFragment_ProceduralArticulation_Health>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        // The derived-request settle precedes Finalize; Gameplay_Camera reads the settled transform pass before component push.
        using Group = FGroup_Gameplay_Camera;
        using RunAfter = TDepList<FProcessor_ProceduralArticulation_Update>;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralArticulation_Params& InParams,
            FFragment_ProceduralArticulation_Pose& InPose,
            FFragment_ProceduralArticulation_Health& InHealth)
            -> void;
    };
}
