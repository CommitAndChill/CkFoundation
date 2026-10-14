#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Processor.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Control.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Utils.h"
#include "CkProceduralAnimation/CkProceduralAnimation_Log.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/TypeTraits/CkTypeTraits.h"

#include "CkEcs/Scheduler/CkProcessorRegistration.h"
#include "CkEcs/Request/CkRequest_Completion.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralLimbControl_Reconcile);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralLimbControl_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralLimbControl_CancelPendingRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralLimbControl_UpdateRecovery);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralLimbControl_ObserveRecovery);

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_ProceduralLimbControl_UpdateRecovery::
        ForEachEntity(TimeType InDeltaT, HandleType InHandle, FFragment_ProceduralLimbControl& InControl)
        -> void
    {
        FCk_ProceduralLimbControl_Control::UpdateRecovery(InHandle);
    }

    auto
        FProcessor_ProceduralLimbControl_ObserveRecovery::
        ForEachEntity(TimeType InDeltaT, HandleType InHandle, FFragment_ProceduralLimbControl& InControl)
        -> void
    {
        FCk_ProceduralLimbControl_Control::ObserveRecovery(InHandle);
    }

    auto
        FProcessor_ProceduralLimbControl_HandleRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralLimbControl& InControl,
            FFragment_ProceduralLimbControl_Requests& InRequests)
        -> void
    {
        auto Pending = MoveTemp(InRequests._Requests);
        InRequests._Requests.Reset();
        algo::ForEachRequest(Pending, ck::Visitor(ck::algo::Overload{
        [&](const FCk_Request_ProceduralArticulation_RecoverLimbs& InRequest) -> void
        {
            const auto Accepted = FCk_ProceduralLimbControl_Control::TryRecover(InHandle, InRequest.Get_LimbIds())
                == ECk_ProceduralLimbControl_AcquireResult::Accepted;
            InRequest.TryFireCompletion(InHandle, Accepted
                ? ECk_Request_OperationResult::Succeeded : ECk_Request_OperationResult::Failed);
        },
        [&](const FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs& InRequest) -> void
        {
            auto Result = ECk_Request_OperationResult::Failed;
            const auto Guard = MakeCompletionGuard(InRequest, InHandle, Result);
            const auto Unplanted = UCk_Utils_ProceduralArticulation_UE::Get_UnplantedLimbIds(InHandle);
            if (Unplanted.IsEmpty())
            {
                Result = ECk_Request_OperationResult::Succeeded;
                return;
            }
            const auto Acquired = FCk_ProceduralLimbControl_Control::TryRecover(InHandle, Unplanted);
            if (Acquired != ECk_ProceduralLimbControl_AcquireResult::Accepted)
            {
                procedural_animation::Log(TEXT("ProceduralArticulation RecoverUnplantedLimbs on [{}] did not admit limbs [{}]: ECk_ProceduralLimbControl_AcquireResult [{}]."),
                    InHandle, FString::JoinBy(Unplanted, TEXT(", "), [](FName InId) { return InId.ToString(); }), static_cast<int32>(Acquired));
                return;
            }
            Result = ECk_Request_OperationResult::Succeeded;
        }}), policy::DontResetContainer{});
        if (InRequests._Requests.IsEmpty())
        { InHandle.Remove<MarkedDirtyBy>(); }
    }

    auto
        FProcessor_ProceduralLimbControl_CancelPendingRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_ProceduralLimbControl_Requests& InRequests)
        -> void
    {
        request::FireCancelledForPending(InHandle, InRequests._Requests);
    }

    auto
        FProcessor_ProceduralLimbControl_Reconcile::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_ProceduralLimbControl& InControl)
        -> void
    {
        FCk_ProceduralLimbControl_Control::Service(InHandle, InDeltaT);
    }
}

// --------------------------------------------------------------------------------------------------------------------
