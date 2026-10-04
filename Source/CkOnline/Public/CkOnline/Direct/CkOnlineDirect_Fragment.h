#pragma once

#include "CkOnline/Direct/CkOnlineDirect_Fragment_Data.h"
#include "CkEcs/Signal/CkSignal_Macros.h"

class UCk_Utils_OnlineDirect_UE;
namespace ck::direct
{
    class FFeature;
    struct FFragment_OnlineDirect
    {
        CK_GENERATED_BODY(FFragment_OnlineDirect);
        friend class FFeature;
        friend class UCk_Utils_OnlineDirect_UE;
        TWeakPtr<FFeature> Feature;
        FCk_OnlineDirect_Snapshot Snapshot;
        FCk_OnlineDirect_Completion LastCompletion;
        int32 CompletedCount = 0;
        TArray<FCk_Request_OnlineDirect> Requests;
        TSet<FGuid> AdmittedIds;
    };
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKONLINE_API, OnOnlineDirectCompleted,
        FCk_Delegate_OnlineDirect_Completed, FCk_Handle_OnlineDirect, FCk_OnlineDirect_Completion);
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKONLINE_API, OnOnlineDirectChanged,
        FCk_Delegate_OnlineDirect_Changed, FCk_Handle_OnlineDirect, FCk_OnlineDirect_Snapshot);
}
