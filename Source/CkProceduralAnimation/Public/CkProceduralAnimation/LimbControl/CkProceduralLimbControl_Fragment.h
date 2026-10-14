#pragma once

#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control_Data.h"
#include "CkProceduralAnimation/LimbControl/CkProceduralLimbControl_Fragment_Data.h"
#include "CkProceduralAnimation/Gait/CkProceduralGaitParticipation_Control_Data.h"

#include <variant>

class FCk_ProceduralLimbControl_Control;

// --------------------------------------------------------------------------------------------------------------------

class UCk_Utils_ProceduralArticulation_UE;
class UCk_Utils_ProceduralAnimation_Debug_UE;

namespace ck
{
    class FProcessor_ProceduralLimbControl_HandleRequests;
    class FProcessor_ProceduralLimbControl_CancelPendingRequests;

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralLimbControl_Requests
    {
        CK_GENERATED_BODY(FFragment_ProceduralLimbControl_Requests);
        friend class ::UCk_Utils_ProceduralArticulation_UE;
        friend class FProcessor_ProceduralLimbControl_HandleRequests;
        friend class FProcessor_ProceduralLimbControl_CancelPendingRequests;

    public:
        using RequestType = std::variant<FCk_Request_ProceduralArticulation_RecoverLimbs,
            FCk_Request_ProceduralArticulation_RecoverUnplantedLimbs>;

    private:
        TArray<RequestType> _Requests;
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralLimbControl_Params
    {
        CK_GENERATED_BODY(FFragment_ProceduralLimbControl_Params);

    private:
        FCk_ProceduralLimbControl_Support _Support;
        FCk_ProceduralLimbControl_Return _Return;

    public:
        CK_PROPERTY_GET(_Support);
        CK_PROPERTY_GET(_Return);
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralLimbControl_Params, _Support, _Return);
    };

    struct CKPROCEDURALANIMATION_API FProceduralLimbControl_Controller
    {
        CK_GENERATED_BODY(FProceduralLimbControl_Controller);
        friend class ::FCk_ProceduralLimbControl_Control;
        friend class ::UCk_Utils_ProceduralAnimation_Debug_UE;

    private:
        FCk_ArticulationControlToken _Token;
        FCk_Handle _Owner;
        FCk_ArticulationChannelSet _Channels;
        FCk_ProceduralLimbControl_Return _ReturnPolicy;
        FCk_ProceduralLimbControl_Return _LocomotionReturnPolicy;
        TArray<FName> _LocomotorIds;
        FCk_ProceduralGaitParticipation_RecoveryCandidate _RecoveryCandidate;
        FCk_Time _NextProbeTime;
        int32 _ProbeAttempts = 0;
        bool _RecoveryStarted = false;
        bool _RecoveryNeedsRetry = false;
        bool _DedicatedReturned = false;
        bool _LocomotionReturned = false;
        bool _Autonomous = false;
        ECk_ProceduralLimbControl_ReturnState _ReturnState = ECk_ProceduralLimbControl_ReturnState::None;
        FCk_ProceduralArticulation_Pose _ReturnStart;
        FCk_ProceduralArticulation_Pose _ReturnActual;
        FCk_Time _ReturnStarted;
        FCk_Time _AcceptedReturnTime;
        FCk_Time _DedicatedReturnOriginTime;
        FCk_Time _PendingReturnTime;
        FCk_Time _AcceptedLocomotionTime;
        FCk_Time _PendingLocomotionTime;
        bool _PendingRecoveryPrepared = false;
        FCk_ArticulationStageKey _ReturnKey;
        bool _ReturnCaptured = false;
        bool _AwaitingSettlement = false;
        bool _ReattachmentNeeded = false;
        bool _PendingReattachment = false;
        uint64 _ReattachmentSettledRevision = 0;
        bool _ReleasePending = false;
        bool _ReturnAborted = false;
        FName _Reason;
        FCk_Time _MissedDeadline;
        FCk_Time _ElapsedAtMissedDeadline;

    public:
        CK_PROPERTY_GET(_Token);
        CK_PROPERTY_GET(_Owner);
        CK_PROPERTY_GET(_Channels);
        CK_PROPERTY_GET(_ReturnState);
        CK_PROPERTY_GET(_Reason);
        CK_PROPERTY_GET(_LocomotorIds);
        CK_PROPERTY_GET(_ReattachmentSettledRevision);
        CK_DEFINE_CONSTRUCTORS(FProceduralLimbControl_Controller, _Token, _Owner, _Channels, _ReturnPolicy);
    };

    // All recovery state belongs to the subject, never to the disposable action entity.
    struct CKPROCEDURALANIMATION_API FFragment_ProceduralLimbControl
    {
        CK_GENERATED_BODY(FFragment_ProceduralLimbControl);
        friend class ::FCk_ProceduralLimbControl_Control;
        friend class ::UCk_Utils_ProceduralAnimation_Debug_UE;

    private:
        TArray<FProceduralLimbControl_Controller> _Controllers;
        FCk_Time _ServiceTime;
        FCk_Time _LastDeltaTime;
        uint64 _NextRecoveryGeneration = 1;

    public:
        CK_PROPERTY_GET(_Controllers);
        CK_PROPERTY_GET(_ServiceTime);
    };
}

// --------------------------------------------------------------------------------------------------------------------
