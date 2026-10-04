#pragma once

#include "CkOnline/CkOnline_Fragment_Data.h"

namespace ck
{
    struct FProcessorDescriptor;

    /** Typed features share this host's registry, clock, dispatch and teardown boundary. */
    class CKONLINE_API IOnlineServiceFeature
    {
    public:
        virtual ~IOnlineServiceFeature() = default;
        virtual auto TryAttach(FCk_Handle_OnlineService InService) -> bool = 0;
        virtual auto Get_LocalProcessors() const -> TArray<FProcessorDescriptor> = 0;
        virtual auto Dispatch() -> void = 0;
        virtual auto BeginShutdown() -> void = 0;
    };

    /** Adapter work is issued on the game thread; completion may arrive on any thread. */
    class CKONLINE_API IOnlineServiceAdapter
    {
    public:
        using FCompletion = TFunction<void(ECk_OnlineService_Result)>;
        virtual ~IOnlineServiceAdapter() = default;
        virtual auto Execute(FGuid InOperationId, FCompletion InCompletion) -> void = 0;
        virtual auto Cancel(FGuid InOperationId) -> void = 0;
        /** Release native work and reconcile remote effects even if an old callback arrives later. */
        virtual auto Shutdown() -> void = 0;
    };

    class FOnlineServiceInbox;
    class FEcsWorld;
    class FProcessorScheduler;
    class FProcessor_OnlineService_HandleRequests;
    class FProcessor_OnlineService_Update;
    struct FOnlineServiceDispatch;

    class CKONLINE_API FOnlineServiceHost : public TSharedFromThis<FOnlineServiceHost>
    {
    public:
        static auto TryCreate(TSharedRef<IOnlineServiceAdapter> InAdapter,
            FCk_Time InMonotonicTime) -> TSharedPtr<FOnlineServiceHost>;
        static auto TryCreateFeature(TSharedRef<IOnlineServiceFeature> InFeature,
            FCk_Time InMonotonicTime) -> TSharedPtr<FOnlineServiceHost>;
        explicit FOnlineServiceHost(TSharedRef<IOnlineServiceAdapter> InAdapter,
            FCk_Time InMonotonicTime);
        FOnlineServiceHost(TSharedPtr<IOnlineServiceAdapter> InAdapter,
            TSharedPtr<IOnlineServiceFeature> InFeature, FCk_Time InMonotonicTime);
        ~FOnlineServiceHost();

        auto Tick(FCk_Time InMonotonicTime) -> void;
        auto Request_Shutdown() -> void;
        auto Get_Handle() const -> FCk_Handle_OnlineService;
        auto Get_Diagnostics() const -> FCk_OnlineService_Diagnostics;
        auto Get_IsAcceptingRequests() const -> bool;
        auto Get_SupportsGenericOperations() const -> bool { return _Adapter.IsValid(); }
        auto Get_MonotonicTime() const -> FCk_Time { return _Now; }
        auto WithDispatchScope(TFunctionRef<void()> InNotify) -> void;

    private:
        friend class FProcessor_OnlineService_HandleRequests;
        friend class FProcessor_OnlineService_Update;

        auto DoInitialize() -> bool;
        auto DoHandleRequests() -> void;
        auto DoUpdate() -> void;
        auto DoDispatch() -> void;
        auto DoShutdown() -> void;
        auto DoComplete(FGuid InTicket, ECk_OnlineService_Result InResult) -> void;

        TSharedPtr<IOnlineServiceAdapter> _Adapter;
        TSharedPtr<IOnlineServiceFeature> _Feature;
        TSharedRef<FOnlineServiceInbox, ESPMode::ThreadSafe> _Inbox;
        TUniquePtr<FEcsWorld> _World;
        TUniquePtr<FProcessorScheduler> _Scheduler;
        TArray<FOnlineServiceDispatch> _Dispatch;
        FCk_Handle_OnlineService _Handle;
        FCk_OnlineService_Diagnostics _FinalDiagnostics;
        FCk_Time _Now;
        bool _Accepting = false;
        bool _Active = false;
        bool _ShuttingDown = false;
        bool _Destroying = false;
    };
}
