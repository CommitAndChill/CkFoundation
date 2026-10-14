#include "CkProceduralAnimation/ActionPlayer/CkProceduralActionPlayer_Processor.h"

#include "CkProceduralAnimation/Action/CkProceduralAction_Utils.h"
#include "CkProceduralAnimation/Core/CkProceduralActionReceiptStore.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_ManagedDispatch);
CK_REGISTER_PROCESSOR(ck::FProcessor_ProceduralActionPlayer_ManagedEndPlay);

namespace ck_procedural_action_player_managed
{
    auto
    Get_IsReadable(
        ECk_ProceduralAction_ReadResult InResult)
        -> bool
    {
        return InResult == ECk_ProceduralAction_ReadResult::Succeeded
            || InResult == ECk_ProceduralAction_ReadResult::Pending;
    }

    // The action may already be gone (scope lost) or released by a listener; only a live action is acted on.
    auto
    Get_IsLive(
        const FCk_Handle_ProceduralAction& InAction)
        -> bool
    {
        return ck::IsValid(InAction) && UCk_Utils_ProceduralAction_UE::Has(InAction)
            && NOT InAction.Has<ck::FTag_DestroyEntity_Initiate>();
    }

    auto
    ReleaseIfLive(
        FCk_Handle_ProceduralAction InAction)
        -> void
    {
        if (NOT Get_IsLive(InAction))
        { return; }

        UCk_Utils_ProceduralAction_UE::Request_Release(InAction, FCk_Request_ProceduralAction_Release{}, {});
    }
}

namespace ck
{
    auto
    FProcessor_ProceduralActionPlayer_ManagedDispatch::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        FFragment_ProceduralActionPlayer& InPlayer,
        FFragment_ProceduralActionPlayer_Managed& InManaged)
        -> void
    {
        if (InManaged._Entries.IsEmpty())
        { return; }

        // Listeners may start new plays (map insertions) or destroy the player, so the pass walks a sorted copy of the
        // identities and re-reads every fragment after each callback instead of holding a map reference across one.
        auto Identities = TArray<int64>{};
        for (const auto& Entry : InManaged._Entries)
        {
            if (NOT Entry.Value._Completed)
            { Identities.Add(Entry.Key); }
        }
        if (Identities.IsEmpty())
        { return; }

        Identities.Sort();
        const auto Count = Identities.Num();
        const auto Start = FMath::Max(InManaged._NextDispatchIndex, 0) % Count;
        const auto& Capacity = InHandle.Get<FFragment_ProceduralActionPlayer_Params>().Get_Capacity();
        const auto MaxPageRecords = Capacity.Get_RecordsPerInstance() + 2;
        auto Budget = Capacity.Get_ManagedRecordsPerTick();
        auto LastServiced = int32{INDEX_NONE};
        auto CompletedPositions = TArray<int32>{};

        for (auto Offset = 0; Offset < Count && Budget > 0; ++Offset)
        {
            if (NOT Get_IsDispatchable(InHandle))
            { return; }

            const auto Position = (Start + Offset) % Count;
            const auto Dispatched = DoDispatch(InHandle, FCk_ProceduralAction_Identity{Identities[Position]},
                FMath::Min(Budget, MaxPageRecords));
            Budget -= Dispatched.Delivered;
            if (Dispatched.Serviced)
            { LastServiced = Position; }

            if (Dispatched.Completed)
            { CompletedPositions.Add(Position); }
        }

        if (LastServiced == INDEX_NONE || NOT Get_IsDispatchable(InHandle))
        { return; }

        // The next pass starts after the last identity serviced, counted in the next pass's sorted order, which no
        // longer contains the identities completed in this one.
        const auto CompletedAtOrBefore = CompletedPositions.FilterByPredicate([LastServiced](int32 InPosition)
        { return InPosition <= LastServiced; }).Num();
        InHandle.Get<FFragment_ProceduralActionPlayer_Managed>()._NextDispatchIndex
            = LastServiced + 1 - CompletedAtOrBefore;
    }

    auto
    FProcessor_ProceduralActionPlayer_ManagedDispatch::
    DoDispatch(
        HandleType InHandle,
        FCk_ProceduralAction_Identity InIdentity,
        int32 InMaxRecords)
        -> FDispatchResult
    {
        auto Result = FDispatchResult{};
        const auto* Entry = InHandle.Get<FFragment_ProceduralActionPlayer_Managed>()._Entries.Find(
            InIdentity.Get_Value());
        const auto EntryExists = Entry != nullptr;
        CK_ENSURE_IF_NOT(EntryExists,
            TEXT("Managed dispatch lost the entry of ProceduralAction identity [{}] within one pass."),
            InIdentity.Get_Value())
        { return Result; }

        const auto& Store = InHandle.Get<FFragment_ProceduralActionPlayer>()._Receipts;
        auto Acknowledged = int64{0};
        const auto Page = Store.Get_AcknowledgedThrough(InIdentity, Acknowledged)
            ? Store.Read(InIdentity, Acknowledged, InMaxRecords)
            : FCk_ProceduralAction_ReadPage{ECk_ProceduralAction_ReadResult::StreamExpired, {}, 0, true, false};

        if (NOT ck_procedural_action_player_managed::Get_IsReadable(Page.Get_Result()))
        {
            Result.Serviced = true;
            Result.Completed = true;
            const auto Expiry = FCk_ProceduralAction_ManagedCompletion{InIdentity, {},
                ECk_ProceduralAction_DeliveryStatus::Expired, {}};
            const auto StreamExpired = Page.Get_Result() == ECk_ProceduralAction_ReadResult::StreamExpired;
            CK_ENSURE_IF_NOT(StreamExpired,
                TEXT("Managed dispatch could not read ProceduralAction identity [{}] at its acknowledged position: [{}]."),
                InIdentity.Get_Value(), Page.Get_Result())
            {
                DoComplete(InHandle, Expiry);
                return Result;
            }

            DoComplete(InHandle, Expiry);
            return Result;
        }

        const auto ReachedTerminal = Page.Get_Terminal() && NOT Page.Get_HasMore();
        if (Page.Get_Records().IsEmpty() && NOT ReachedTerminal)
        { return Result; }

        // Copied before the acknowledgement below, which removes a released terminal stream from the store.
        auto Terminal = FCk_ProceduralAction_Result{};
        auto Context = FCk_ProceduralAction_FailureContext{};
        for (const auto& Record : Page.Get_Records())
        {
            if (Record.Get_Kind() != ECk_ProceduralAction_RecordKind::Terminal)
            { continue; }

            Terminal = Record.Get_Result();
            Context = Record.Get_Context();
        }
        const auto ManagedPage = FCk_ProceduralAction_ManagedPage{InIdentity, Page.Get_Records(), ReachedTerminal};
        const auto RecordListeners = Entry->Get_RecordListeners();
        const auto PoseListeners = Entry->Get_PoseListeners();
        Result.Delivered = Page.Get_Records().Num();
        Result.Serviced = true;

        if (NOT ManagedPage.Get_Records().IsEmpty())
        {
            for (const auto& Listener : RecordListeners)
            { Listener.ExecuteIfBound(InHandle, ManagedPage); }

            for (const auto& Record : ManagedPage.Get_Records())
            {
                if (Record.Get_Kind() != ECk_ProceduralAction_RecordKind::AcceptedPose)
                { continue; }

                for (const auto& Listener : PoseListeners)
                { Listener.ExecuteIfBound(InHandle, Record.Get_Receipt()); }
            }
        }

        // A destroyed player keeps its records unacknowledged; its end of play completes the entry as PlayerLost.
        if (NOT Get_IsDispatchable(InHandle))
        { return Result; }

        if (NOT ManagedPage.Get_Records().IsEmpty())
        {
            InHandle.Get<FFragment_ProceduralActionPlayer>()._Receipts.Consume(InIdentity,
                Page.Get_LastStreamSequence());
        }

        if (NOT ReachedTerminal)
        { return Result; }

        Result.Completed = true;
        DoComplete(InHandle, FCk_ProceduralAction_ManagedCompletion{InIdentity, Terminal,
            ECk_ProceduralAction_DeliveryStatus::Complete, Context});
        return Result;
    }

    auto
    FProcessor_ProceduralActionPlayer_ManagedDispatch::
    DoComplete(
        HandleType InHandle,
        const FCk_ProceduralAction_ManagedCompletion& InCompletion)
        -> void
    {
        const auto IdentityValue = InCompletion.Get_Identity().Get_Value();
        auto* Entry = InHandle.Get<FFragment_ProceduralActionPlayer_Managed>()._Entries.Find(IdentityValue);
        if (Entry == nullptr)
        { return; }

        // Marked before the callback: a recipient that destroys the player must not also receive PlayerLost.
        Entry->_Completed = true;
        const auto Completion = Entry->_Completion;
        const auto Action = Entry->_Action;
        Completion.ExecuteIfBound(InHandle, InCompletion);
        ck_procedural_action_player_managed::ReleaseIfLive(Action);

        if (NOT Get_IsDispatchable(InHandle))
        { return; }

        InHandle.Get<FFragment_ProceduralActionPlayer_Managed>()._Entries.Remove(IdentityValue);
    }

    auto
    FProcessor_ProceduralActionPlayer_ManagedDispatch::
    Get_IsDispatchable(
        const HandleType& InHandle)
        -> bool
    {
        return ck::IsValid(InHandle) && NOT InHandle.Has<FTag_DestroyEntity_Initiate>()
            && InHandle.Has<FFragment_ProceduralActionPlayer>()
            && InHandle.Has<FFragment_ProceduralActionPlayer_Managed>();
    }

    auto
    FProcessor_ProceduralActionPlayer_ManagedEndPlay::
    ForEachEntity(
        TimeType InDeltaT,
        HandleType InHandle,
        FFragment_ProceduralActionPlayer_Managed& InManaged)
        -> void
    {
        auto Entries = MoveTemp(InManaged._Entries);
        InManaged._Entries.Reset();

        Entries.KeySort([](int64 InA, int64 InB) { return InA < InB; });

        // Without its player the action can never be released, so it would outlive its delivery under its scope.
        for (const auto& Entry : Entries)
        {
            auto Action = Entry.Value._Action;
            if (ck_procedural_action_player_managed::Get_IsLive(Action))
            { UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Action); }
        }

        for (const auto& Entry : Entries)
        {
            if (Entry.Value._Completed)
            { continue; }

            Entry.Value._Completion.ExecuteIfBound(InHandle, FCk_ProceduralAction_ManagedCompletion{
                FCk_ProceduralAction_Identity{Entry.Key}, {}, ECk_ProceduralAction_DeliveryStatus::PlayerLost, {}});
        }
    }
}
