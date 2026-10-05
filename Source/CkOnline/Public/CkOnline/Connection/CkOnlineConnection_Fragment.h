#pragma once

#include "CkOnline/Connection/CkOnlineConnection_Transport.h"
#include "CkEcs/Signal/CkSignal_Macros.h"

class UCk_Utils_OnlineConnection_UE;
namespace ck::connection
{
    class FFeature;
    struct FCommand
    {
        FCk_Request_OnlineConnection Request;
        FCk_OnlineConnection_Operation Operation;
        FEntryIntent Intent;
    };
    struct FFragment_OnlineConnection
    {
        CK_GENERATED_BODY(FFragment_OnlineConnection);
        friend class FFeature;
        friend class FProcessor_OnlineConnection_HandleRequests;
        friend class FProcessor_OnlineConnection_Update;
        friend class UCk_Utils_OnlineConnection_UE;

    private:
        TWeakPtr<FFeature> _Feature;
        FCk_OnlineConnection_Snapshot _Snapshot;
        FCk_OnlineConnection_Completion _LastCompletion;
        int32 _CompletedCount = 0;
        TArray<FCommand> _Requests;
        TOptional<FCommand> _Active;
        FEntryIntent _SessionIntent;
        double _Deadline = 0.0;
    };
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKONLINE_API, OnOnlineConnectionCompleted,
        FCk_Delegate_OnlineConnection_Completed, FCk_Handle_OnlineConnection, FCk_OnlineConnection_Completion);
    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKONLINE_API, OnOnlineConnectionChanged,
        FCk_Delegate_OnlineConnection_Changed, FCk_Handle_OnlineConnection, FCk_OnlineConnection_Snapshot);
}
