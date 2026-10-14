#pragma once

#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Fragment_Data.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"
#include "CkProceduralAnimation/Articulation/CkProceduralArticulation_Control_Data.h"

#include <variant>

class UCk_Utils_ProceduralActionPlayer_UE;
class UCk_Utils_ProceduralAction_UE;
struct FCk_Handle_ProceduralAction;

namespace ck
{
    class FProcessor_ProceduralActionPlayer_HandleRequests;
    class FProcessor_ProceduralActionPlayer_CancelPendingRequests;
    class FProcessor_ProceduralActionPlayer_Service;
    class FProcessor_ProceduralActionPlayer_Observe;
    class FProcessor_ProceduralActionPlayer_ReconcileOwnerLoss;
    class FProcessor_ProceduralActionPlayer_EndPlay;
    struct FTechnique_ProceduralActionPlayer_Service;
    class FProcessor_ProceduralAction_EndPlay;
    class FProcessor_ProceduralAction_HandleRequests;

    struct CKPROCEDURALANIMATION_API FProceduralActionPendingRecovery
    {
        CK_GENERATED_BODY(FProceduralActionPendingRecovery);

    private:
        FCk_ProceduralAction_Identity _Identity;
        FCk_ArticulationControlToken _Token;
        ECk_ProceduralAction_Outcome _Outcome = ECk_ProceduralAction_Outcome::OwnerLost;
        FName _Reason = NAME_None;

    public:
        CK_PROPERTY_GET(_Identity);
        CK_PROPERTY_GET(_Token);
        CK_PROPERTY_GET(_Outcome);
        CK_PROPERTY_GET(_Reason);
        CK_DEFINE_CONSTRUCTORS(FProceduralActionPendingRecovery, _Identity, _Token, _Outcome, _Reason);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralActionPlayer_Params
    {
        CK_GENERATED_BODY(FFragment_ProceduralActionPlayer_Params);

    private:
        FCk_ProceduralAction_Capacity _Capacity;
        FCk_ProceduralAction_Tuning _Tuning;

    public:
        CK_PROPERTY_GET(_Capacity);
        CK_PROPERTY_GET(_Tuning);
        CK_DEFINE_CONSTRUCTORS(FFragment_ProceduralActionPlayer_Params, _Capacity, _Tuning);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralActionPlayer
    {
        CK_GENERATED_BODY(FFragment_ProceduralActionPlayer);
        friend class ::UCk_Utils_ProceduralActionPlayer_UE;
        friend class ::UCk_Utils_ProceduralAction_UE;
        friend class FProcessor_ProceduralActionPlayer_HandleRequests;
        friend class FProcessor_ProceduralActionPlayer_Service;
        friend struct FTechnique_ProceduralActionPlayer_Service;
        friend class FProcessor_ProceduralActionPlayer_Observe;
        friend class FProcessor_ProceduralActionPlayer_ReconcileOwnerLoss;
        friend class FProcessor_ProceduralActionPlayer_EndPlay;
        friend class FProcessor_ProceduralAction_EndPlay;
        friend class FProcessor_ProceduralAction_HandleRequests;

    private:
        procedural_action::FReceiptStore _Receipts;
        TArray<FCk_Handle_ProceduralAction> _Actions;
        TArray<FProceduralActionPendingRecovery> _PendingRecoveries;

    public:
        CK_PROPERTY_GET(_Receipts);
    };

    struct CKPROCEDURALANIMATION_API FFragment_ProceduralActionPlayer_Requests
    {
        CK_GENERATED_BODY(FFragment_ProceduralActionPlayer_Requests);
        friend class ::UCk_Utils_ProceduralActionPlayer_UE;
        friend class FProcessor_ProceduralActionPlayer_HandleRequests;
        friend class FProcessor_ProceduralActionPlayer_CancelPendingRequests;

    public:
        using RequestType = std::variant<FCk_Request_ProceduralActionPlayer_ConsumeReceipts>;

    private:
        TArray<RequestType> _Requests;

    public:
        CK_PROPERTY_GET(_Requests);
    };
}
