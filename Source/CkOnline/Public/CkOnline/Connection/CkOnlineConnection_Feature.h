#pragma once

#include "CkOnline/CkOnline_Host.h"
#include "CkOnline/Connection/CkOnlineConnection_Fragment.h"

namespace ck::connection
{
    class CKONLINE_API FFeature : public IOnlineServiceFeature, public TSharedFromThis<FFeature>
    {
    public:
        static auto TryCreate(TSharedRef<ITransport> InTransport) -> TSharedPtr<FFeature>;
        explicit FFeature(TSharedRef<ITransport> InTransport);
        auto TryAttach(FCk_Handle_OnlineService InService) -> bool override;
        auto Get_LocalProcessors() const -> TArray<FProcessorDescriptor> override;
        auto Dispatch() -> void override;
        auto BeginShutdown() -> void override;
        auto HandleRequests(double InNow) -> void;
        auto Update(double InNow) -> void;
    private:
        friend class UCk_Utils_OnlineConnection_UE;
        auto TryQueue(const FCk_Request_OnlineConnection& InRequest, FCk_OnlineConnection_Operation& OutOperation) -> bool;
        auto IsIdle() const -> bool;
        auto CanDispatchWork() const -> bool;
        auto MatchesControl(const FEntryIntent& InIntent) const -> bool;
        auto TryPrepare(const FCk_Request_OnlineConnection& InRequest, FEntryIntent& OutIntent) const -> bool;
        auto Complete(FCommand InCommand, ECk_OnlineConnection_Result InResult, const FString& InDetail) -> void;
        auto CompleteActive(ECk_OnlineConnection_Result InResult, const FString& InDetail) -> void;
        auto StartReturn(FCommand InCommand, double InNow) -> void;
        auto Recover(ECk_OnlineConnection_Result InResult, const FString& InDetail, double InNow) -> void;
        auto SetState(ECk_OnlineConnection_State InState, const FString& InDetail = {}) -> void;
        TSharedRef<ITransport> _Transport;
        FCk_Handle_OnlineConnection _Handle;
        struct FDispatch
        {
            FCk_Request_OnlineConnection Request;
            FCk_OnlineConnection_Completion Completion;
        };
        TArray<FDispatch> _Dispatch;
        bool _Changed = false;
        bool _Closed = false;
    };
}
