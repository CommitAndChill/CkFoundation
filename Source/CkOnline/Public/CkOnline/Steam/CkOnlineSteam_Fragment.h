#pragma once

#include "CkOnline/Steam/CkOnlineSteam_Fragment_Data.h"
#include "CkEcs/Signal/CkSignal_Macros.h"

class UCk_Utils_OnlineSteam_UE;
namespace ck::steam
{
    class FFeature;
    struct FFragment_OnlineSteam
    {
        CK_GENERATED_BODY(FFragment_OnlineSteam);
        friend class FFeature;
        friend class UCk_Utils_OnlineSteam_UE;
        TWeakPtr<FFeature> Feature;
        FCk_OnlineSteam_Snapshot Snapshot;
        FCk_OnlineSteam_Completion LastCompletion;
        int32 CompletedCount = 0;
        TArray<FCk_Request_OnlineSteam> Requests;
        TSet<FGuid> AdmittedIds;
    };
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKONLINE_API, OnOnlineSteamCompleted,
        FCk_Delegate_OnlineSteam_Completed, FCk_Handle_OnlineSteam, FCk_OnlineSteam_Completion);
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKONLINE_API, OnOnlineSteamChanged,
        FCk_Delegate_OnlineSteam_Changed, FCk_Handle_OnlineSteam, FCk_OnlineSteam_Snapshot);
}
