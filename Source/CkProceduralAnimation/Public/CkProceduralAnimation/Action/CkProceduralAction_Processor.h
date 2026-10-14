#pragma once

#include "CkProceduralAnimation/Action/CkProceduralAction_Fragment.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Fragment.h"
#include "CkEcs/Processor/CkProcessor.h"
#include "CkEcs/Scheduler/CkProcessorGroups.h"

namespace ck
{
    class CKPROCEDURALANIMATION_API FProcessor_ProceduralAction_HandleRequests : public ck_exp::TProcessor<
        FProcessor_ProceduralAction_HandleRequests,
        FCk_Handle_ProceduralAction,
        TReadOnly<FFragment_ProceduralAction_Params>,
        TReadWrite<FFragment_ProceduralAction>,
        TReadWrite<FFragment_ProceduralAction_Requests>,
        TExclude<FTag_DestroyEntity_Initiate>,
        CK_IGNORE_PENDING_KILL>
    {
    public:
        using Group = FGroup_Transform_Derived;
        using MarkedDirtyBy = FFragment_ProceduralAction_Requests;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralAction_Params& InParams,
            FFragment_ProceduralAction& InAction,
            FFragment_ProceduralAction_Requests& InRequests)
            -> void;
    };

    class CKPROCEDURALANIMATION_API FProcessor_ProceduralAction_EndPlay : public ck_exp::TProcessor<
        FProcessor_ProceduralAction_EndPlay,
        FCk_Handle_ProceduralAction,
        TReadOnly<FFragment_ProceduralAction_Params>,
        TReadOnly<FFragment_ProceduralAction>,
        CK_IF_END_PLAY>
    {
    public:
        using Group = FGroup_EndPlay;
        using TProcessor::TProcessor;

        static auto
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralAction_Params& InParams,
            const FFragment_ProceduralAction& InAction)
            -> void;
    };
}
