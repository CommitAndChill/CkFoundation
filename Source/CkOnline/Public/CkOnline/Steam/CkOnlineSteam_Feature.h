#pragma once

#include "CkOnline/CkOnline_Host.h"
#include "CkOnline/Steam/CkOnlineSteam_Fragment.h"
#include "CkOnline/Steam/CkOnlineSteam_Transport.h"

namespace ck::steam
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
        auto CanDispatchWork() const -> bool;
        auto CaptureSnapshot() -> void;
        struct FDispatch
        {
            FCk_Request_OnlineSteam Request;
            FCk_OnlineSteam_Completion Completion;
        };
        auto Complete(FGuid InTicket, ECk_OnlineSteam_Result InResult, const FString& InDetail) -> void;
        auto SetCompletion(const FCk_Request_OnlineSteam& InRequest,
            ECk_OnlineSteam_Result InResult, const FString& InDetail) -> void;
        TSharedRef<ITransport> _Transport;
        FCk_Handle_OnlineSteam _Handle;
        TMap<FGuid, FCk_Request_OnlineSteam> _Pending;
        TMap<FGuid, double> _Deadlines;
        TArray<FDispatch> _Dispatch;
        bool _Changed = false;
        bool _Closed = false;
    };
}
