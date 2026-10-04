#pragma once

#include "CkOnline/CkOnline_Host.h"
#include "CkOnline/Direct/CkOnlineDirect_Fragment.h"
#include "CkOnline/Direct/CkOnlineDirect_Transport.h"

namespace ck::direct
{
    class CKONLINE_API FFeature : public IOnlineServiceFeature, public TSharedFromThis<FFeature>
    {
    public:
        static auto TryCreate(const FCk_OnlineDirect_Spec& InSpec) -> TSharedPtr<FFeature>;
        explicit FFeature(const FCk_OnlineDirect_Spec& InSpec);
        ~FFeature();
        auto TryAttach(FCk_Handle_OnlineService InService) -> bool override;
        auto Get_LocalProcessors() const -> TArray<FProcessorDescriptor> override;
        auto Dispatch() -> void override;
        auto BeginShutdown() -> void override;
        auto HandleRequests(double InNow) -> void;
        auto Update(double InNow) -> void;
    private:
        auto Complete(FGuid InTicket, ECk_OnlineDirect_Result InResult, const FString& InDetail) -> void;
        auto CaptureSnapshot() -> void;
        struct FDispatch
        {
            FCk_Request_OnlineDirect Request;
            FCk_OnlineDirect_Completion Completion;
        };
        FTransport _Transport;
        FCk_Handle_OnlineDirect _Handle;
        TMap<FGuid, FCk_Request_OnlineDirect> _Pending;
        TArray<FDispatch> _Dispatch;
        bool _Changed = false;
        bool _Closed = false;
    };
}
