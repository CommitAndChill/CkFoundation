#pragma once

#include "CkOnline/CkOnline_Host.h"
#include "CkEcs/Signal/CkSignal_Macros.h"

class UCk_Utils_OnlineService_UE;

namespace ck
{
    struct FOnlineServicePending
    {
        CK_GENERATED_BODY(FOnlineServicePending);

    private:
        FCk_Request_OnlineService_Execute _Request;
        FCk_Time _Deadline;

    public:
        CK_PROPERTY_GET(_Request);
        CK_PROPERTY_GET(_Deadline);
        CK_DEFINE_CONSTRUCTORS(FOnlineServicePending, _Request, _Deadline);
    };

    struct FFragment_OnlineService
    {
        CK_GENERATED_BODY(FFragment_OnlineService);
        friend class FOnlineServiceHost;
        friend class UCk_Utils_OnlineService_UE;

    private:
        TWeakPtr<FOnlineServiceHost> _Host;
        TMap<FGuid, FOnlineServicePending> _Pending;
        FCk_OnlineService_Diagnostics _Diagnostics;

    public:
        CK_PROPERTY_GET(_Host);
        CK_PROPERTY_GET(_Pending);
        CK_PROPERTY_GET(_Diagnostics);
        CK_DEFINE_CONSTRUCTORS(FFragment_OnlineService, _Host, _Diagnostics);
    };

    struct FFragment_OnlineService_Requests
    {
        CK_GENERATED_BODY(FFragment_OnlineService_Requests);
        friend class FOnlineServiceHost;
        friend class UCk_Utils_OnlineService_UE;

    public:
        using RequestType = std::variant<FCk_Request_OnlineService_Execute, FCk_Request_OnlineService_Cancel>;

    private:
        TArray<RequestType> _Requests;

    public:
        CK_PROPERTY_GET(_Requests);
    };

    CK_DEFINE_SIGNAL_AND_UTILS_WITH_DELEGATE(CKONLINE_API, OnOnlineServiceCompleted,
        FCk_Delegate_OnlineService_Completed, FCk_Handle_OnlineService, FCk_OnlineService_Completion);
}
