#pragma once

#include "CkOnline/Steam/CkOnlineSteam_Transport.h"

namespace ck::steam
{
    /** Value-only OSS boundary. SDK/session objects remain inside the private provider. */
    struct FSessionIdentity
    {
        bool Available = false;
        bool LoggedIn = false;
        FString AccountId;
        FString DisplayName;
        FString Detail;
    };

    struct FSessionCompletion
    {
        FGuid Ticket;
        ECk_OnlineSteam_Action Action = ECk_OnlineSteam_Action::Invalid;
        ECk_OnlineSteam_Result Result = ECk_OnlineSteam_Result::Failed;
        FString Detail;
        TArray<FCk_OnlineSteam_Lobby> SearchResults;
    };

    class CKONLINE_API ISessionProvider
    {
    public:
        virtual ~ISessionProvider() = default;
        virtual auto ObserveIdentity() const -> FSessionIdentity = 0;
        virtual auto Execute(const FCk_Request_OnlineSteam& InRequest, FGuid InTicket,
            const FCk_OnlineSteam_Spec& InSpec) -> void = 0;
        virtual auto DrainCompletions() -> TArray<FSessionCompletion> = 0;
        virtual auto HasSession() const -> bool = 0;
        virtual auto Get_LobbyId() const -> FString = 0;
        virtual auto InvalidateSearch() -> void = 0;
    };

    /** Exercises the same late-success reconciliation for OSS and injected test providers. */
    CKONLINE_API auto IsValidSpec(const FCk_OnlineSteam_Spec& InSpec) -> bool;
    CKONLINE_API auto TryCreateSessionTransport(const FCk_OnlineSteam_Spec& InSpec,
        TSharedRef<ISessionProvider> InProvider) -> TSharedPtr<ITransport>;
}
