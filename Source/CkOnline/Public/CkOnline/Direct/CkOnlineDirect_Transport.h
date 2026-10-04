#pragma once

#include "CkOnline/Direct/CkOnlineDirect_Fragment_Data.h"

namespace ck::direct
{
    struct FTransportCompletion
    {
        FGuid Ticket;
        ECk_OnlineDirect_Result Result = ECk_OnlineDirect_Result::InvalidState;
        FString Detail;
    };

    /** Actorless, game-thread-owned, bounded reliable Direct control transport. */
    class CKONLINE_API FTransport
    {
    public:
        static auto ValidateSpec(const FCk_OnlineDirect_Spec& InSpec) -> bool;
        static auto ValidateHostAddress(const FString& InAddress) -> bool;
        static auto ValidateText(const FString& InText, int32 InMaxCharacters) -> bool;
        static auto ValidateDescriptor(const FString& InDescriptor) -> bool;
        explicit FTransport(const FCk_OnlineDirect_Spec& InSpec);
        ~FTransport();
        FTransport(const FTransport&) = delete;
        auto operator=(const FTransport&) -> FTransport& = delete;

        auto Execute(ECk_OnlineDirect_Action InAction, FGuid InTicket,
            const FString& InText, bool InReady, double InNow) -> void;
        auto Poll(double InNow) -> void;
        auto Close() -> void;
        auto DrainCompletions() -> TArray<FTransportCompletion>;
        auto Get_Snapshot() const -> FCk_OnlineDirect_Snapshot;
    private:
        struct FImpl;
        TUniquePtr<FImpl> _Impl;
    };
}
