#pragma once

#include <CoreMinimal.h>
#include <Containers/Queue.h>

namespace ck::online_achievements
{
    enum class EOperation : uint8 { None, Store, Progress, Reset };

    struct FIdentity
    {
        uint64 Owner = 0;
        uint64 Account = 0;
        uint64 Provider = 0;
        uint32 App = 0;

        auto operator==(const FIdentity&) const -> bool = default;
    };

    struct FNativeResult
    {
        int64 OperationId = 0;
        uint64 GameId = 0;
        int32 Result = 0;
    };

    // Only this bounded process-lifetime ingress is referenced by native callback threads.
    struct FCallbackIngress
    {
        TAtomic<int64> OperationId{0};
        TAtomic<int32> QueuedCount{0};
        TAtomic<bool> Overflowed{false};
        TQueue<FNativeResult, EQueueMode::Mpsc> Results;

        auto Receive(uint64 InGameId, int32 InResult) -> void
        {
            if (QueuedCount.IncrementExchange() >= 64)
            {
                QueuedCount.DecrementExchange();
                Overflowed.Store(true);
                return;
            }
            Results.Enqueue({OperationId.Load(), InGameId, InResult});
        }

        auto Dequeue(FNativeResult& OutResult) -> bool
        {
            if (!Results.Dequeue(OutResult))
            {
                return false;
            }
            QueuedCount.DecrementExchange();
            return true;
        }
    };

    struct FCompletion
    {
        EOperation Operation = EOperation::None;
        int64 Id = 0;
        uint64 GameId = 0;
        int32 Result = 0;
    };

    /** Production admission and dispatch, with no SDK or UObject dependency. */
    class FOperationOwner
    {
    public:
        explicit FOperationOwner(FCallbackIngress& InIngress) : Ingress(InIngress) {}

        auto Acquire(FIdentity InIdentity) -> bool
        {
            if (InIdentity.Owner == 0 || InIdentity.Account == 0 || InIdentity.Provider == 0 || InIdentity.App == 0)
            {
                return false;
            }
            if (Attached)
            {
                if (Identity.Owner == InIdentity.Owner && Identity != InIdentity)
                {
                    IdentityChanged = true;
                }
                return Identity == InIdentity && !IdentityChanged;
            }
            if (Pending || Dispatching)
            {
                return false;
            }
            Identity = InIdentity;
            Attached = true;
            IdentityChanged = false;
            return true;
        }

        auto Detach() -> void { Attached = false; }

        template <typename NativeOperation>
        auto TryBegin(EOperation InOperation, FIdentity InIdentity, double InNow, NativeOperation&& InExecute) -> bool
        {
            if (InOperation == EOperation::None || Pending || Dispatching || !Acquire(InIdentity))
            {
                return false;
            }
            // Retired callback payloads are discarded before publishing a fresh ingress token.
            FNativeResult Retired;
            while (Ingress.Dequeue(Retired)) {}
            Ingress.Overflowed.Store(false);
            Operation = InOperation;
            Id += 1;
            Pending = true;
            TimedOut = false;
            Deadline = InNow + 30.0;
            Ingress.OperationId.Store(Id);
            if (!InExecute())
            {
                Pending = false;
                Ingress.OperationId.Store(0);
                return false;
            }
            return true;
        }

        auto Pump(FIdentity InCurrentIdentity, double InNow, TFunctionRef<void(FCompletion)> InNotify) -> void
        {
            TGuardValue<bool> DispatchGuard(Dispatching, true);
            // Freeze this dispatch batch. Notification reentry can never start another native op.
            TArray<FNativeResult> Batch;
            FNativeResult Result;
            while (Ingress.Dequeue(Result))
            {
                Batch.Add(Result);
            }
            if (Attached && Identity != InCurrentIdentity)
            {
                IdentityChanged = true;
            }
            if (Ingress.Overflowed.Exchange(false) && Pending && !TimedOut)
            {
                TimedOut = true;
                if (Attached)
                {
                    InNotify({Operation, Id, Identity.App, -4});
                }
            }
            for (const auto& Entry : Batch)
            {
                if (!Pending || Entry.OperationId != Id || Entry.GameId != Identity.App)
                {
                    continue;
                }
                Pending = false;
                TimedOut = false;
                Ingress.OperationId.Store(0);
                if (Attached)
                {
                    InNotify({Operation, Id, Entry.GameId, IdentityChanged ? -3 : Entry.Result});
                }
            }
            if (Pending && !TimedOut && InNow >= Deadline)
            {
                TimedOut = true;
                if (Attached)
                {
                    InNotify({Operation, Id, Identity.App, IdentityChanged ? -3 : -1});
                }
            }
        }

        auto IsPending() const -> bool { return Pending; }
        auto IsUnconfirmed() const -> bool { return Pending && TimedOut; }
        auto GetId() const -> int64 { return Id; }
        auto GetIdentity() const -> FIdentity { return Identity; }
        auto CanMutate(FIdentity InIdentity) -> bool { return !Pending && !Dispatching && Acquire(InIdentity); }

    private:
        FCallbackIngress& Ingress;
        FIdentity Identity;
        EOperation Operation = EOperation::None;
        int64 Id = 0;
        bool Attached = false;
        bool IdentityChanged = false;
        bool Pending = false;
        bool TimedOut = false;
        bool Dispatching = false;
        double Deadline = 0.0;
    };
}
