#include "CkHexGrid_Processor.h"
#include "CkGrid/HexGridSystem/Persistence/CkHexGridReplication_Processor.h"

#include "CkGrid/HexGridSystem/Cell/CkHexCell_Fragment.h"
#include "CkGrid/HexGridSystem/Cell/CkHexCell_Utils.h"
#include "CkGrid/HexGridSystem/Grid/CkHexGrid_Utils.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Net/CkNet_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "CkGrid/HexGridSystem/Occupancy/CkHexGridOccupancy_Transaction.h"

CK_REGISTER_PROCESSOR(ck::FProcessor_HexGrid_BaselineDeadline);
CK_REGISTER_PROCESSOR(ck::FProcessor_HexGrid_Rules);
CK_REGISTER_PROCESSOR(ck::FProcessor_HexGrid_CancelRules);
CK_REGISTER_PROCESSOR(ck::FProcessor_HexGridBlocker_EndPlay);

CK_REGISTER_PROCESSOR(ck::FProcessor_HexGrid_EndPlay);

namespace ck::hex_rules
{
    auto IsLive(const FCk_Handle& InHandle) -> bool
    {
        return ck::IsValid(InHandle) && !UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InHandle,
            ECk_EntityLifetime_DestructionPhase::BeginDestroy);
    }

    auto WriteFailure(const FCk_Handle& InBoard) -> ECk_HexGridFailure
    {
        using Failure = ECk_HexGridFailure;
        if (!UCk_Utils_HexGrid_UE::Has(InBoard)) { return Failure::InvalidGrid; }
        if (!IsLive(InBoard)) { return Failure::Destroyed; }
        const auto Readiness = UCk_Utils_HexGrid_UE::Get_Readiness(UCk_Utils_HexGrid_UE::Cast(InBoard));
        if (Readiness.Get_State() != ECk_HexGridReadiness::Ready) { return Readiness.Get_Failure(); }
        if (!UCk_Utils_Net_UE::Get_HasAuthority(InBoard)) { return Failure::NotAuthority; }
        const auto Root = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InBoard);
        if (!IsLive(Root) || !Root.Has<TWeakObjectPtr<UWorld>>()) { return Failure::NotReady; }
        const auto* World = Root.Get<TWeakObjectPtr<UWorld>>().Get();
        const auto* Subsystem = World ? World->GetSubsystem<UCk_EcsWorld_Subsystem_UE>() : nullptr;
        // A fresh board in the destination world can accept requests. The retiring world cannot.
        return Subsystem && Subsystem->Get_LoadHold() != ECk_EcsWorld_LoadHold::Teardown ? Failure::None : Failure::NotReady;
    }

    auto CanWrite(const FCk_Handle& InBoard) -> bool
    { return WriteFailure(InBoard) == ECk_HexGridFailure::None; }

    template<typename T> constexpr bool IsPlacement = std::is_same_v<T, FCk_Request_HexGridPlacement_Place>
        || std::is_same_v<T, FCk_Request_HexGridPlacement_Move> || std::is_same_v<T, FCk_Request_HexGridPlacement_Remove>;

    template<typename T>
    auto Dispatch(T& Request, FCk_Handle Target, const FCk_HexGridPlacement_Result& Outcome,
        ECk_Request_OperationResult Generic) -> void
    {
        auto Delegate = Request.Get_OnResult();
        Request.Set_OnResult({});
        Delegate.ExecuteIfBound(Outcome);
        Request.TryFireCompletion(Target, Generic);
    }

    auto Finish(FHexRuleCommand& InCommand, ECk_Request_OperationResult InResult, ECk_HexGridFailure InFailure) -> void
    {
        std::visit([&](auto& Request)
        {
            using T = std::decay_t<decltype(Request)>;
            if constexpr (IsPlacement<T>)
            {
                FCk_HexGridPlacement_Result Rejected;
                Rejected.Set_Failure(InFailure);
                if constexpr (!std::is_same_v<T, FCk_Request_HexGridPlacement_Place>)
                { Rejected.Set_PlacementId(Request.Get_PlacementId()); }
                Dispatch(Request, InCommand._Target, Rejected, InResult);
            }
            else if constexpr (!std::is_same_v<T, FHexPlacementDeath>)
            { Request.TryFireCompletion(InCommand._Target, InResult); }
        }, InCommand._Rule);
    }

    auto Enqueue(FCk_Handle InBoard, FCk_Handle InTarget, FHexRule InRequest) -> void
    {
        auto Command = FHexRuleCommand{InTarget, MoveTemp(InRequest)};
        const auto Failure = WriteFailure(InBoard);
        if (Failure != ECk_HexGridFailure::None || !IsLive(InTarget))
        { Finish(Command, ECk_Request_OperationResult::Failed_NotEnqueued, Failure); return; }
        InBoard.AddOrGet<FFragment_HexGrid_RuleRequests>()._Commands.Add(MoveTemp(Command));
    }

    auto SetActive(FCk_Handle InBlocker, bool InActive) -> bool
    {
        auto& State = InBlocker.Get<FFragment_HexGridBlocker>();
        if (State._Active == InActive) { return true; }
        if (InActive)
        {
            for (const auto& Cell : State._Cells)
            { if (!IsLive(Cell)) { return false; } }
        }
        for (auto Cell : State._Cells)
        {
            if (!IsLive(Cell)) { continue; }
            if (InActive) { Cell.Add<FTag_HexCell_Disabled>(); }
            else { Cell.Remove<FTag_HexCell_Disabled>(); }
        }
        State._Active = InActive;
        hex_grid::ArmBoardReplication(State._Board);
        return true;
    }

    auto Apply(const FCk_Handle& InBoard, FHexRuleCommand& InCommand) -> void
    {
        using Result = ECk_Request_OperationResult;
        if (!IsLive(InBoard) || !IsLive(InCommand._Target))
        { Finish(InCommand, Result::Failed_Cancelled); return; }

        // Death notifications retire existing reservations even while public writes are held.
        if (const auto* Death = std::get_if<FHexPlacementDeath>(&InCommand._Rule))
        {
            auto Board = UCk_Utils_HexGrid_UE::Cast(InBoard);
            hex_grid::ApplyDeath(Board, *Death);
            return;
        }
        const auto Failure = WriteFailure(InBoard);
        if (Failure != ECk_HexGridFailure::None) { Finish(InCommand, Result::Failed, Failure); return; }
        std::visit([&](auto& Request)
        {
            using RequestType = std::decay_t<decltype(Request)>;
            auto Target = InCommand._Target;
            if constexpr (IsPlacement<RequestType>)
            {
                auto Board = UCk_Utils_HexGrid_UE::Cast(InBoard);
                const auto Outcome = hex_grid::ApplyPlacement(Board, Request);
                Dispatch(Request, Target, Outcome, Outcome.Get_Succeeded() ? Result::Succeeded : Result::Failed);
            }
            else if constexpr (std::is_same_v<RequestType, FHexPlacementDeath>) { return; }
            else if constexpr (std::is_same_v<RequestType, FCk_Request_HexGridBlocker_SetActive>)
            {
                if (!Target.Has<FFragment_HexGridBlocker>() || Target.Get<FFragment_HexGridBlocker>()._Board != InBoard)
                { Request.TryFireCompletion(Target, Result::Failed_Cancelled); return; }
                Request.TryFireCompletion(Target, SetActive(Target, Request.Get_Active()) ? Result::Succeeded : Result::Failed);
            }
            else
            {
                auto Grid = FCk_Handle_HexGrid{};
                if (!UCk_Utils_HexCell_UE::TryGet_Grid(UCk_Utils_HexCell_UE::Cast(Target), Grid) || Grid != InBoard)
                { Request.TryFireCompletion(Target, Result::Failed_Cancelled); return; }
                if constexpr (std::is_same_v<RequestType, FCk_Request_HexCell_SetBlocked>)
                {
                    if (Request.Get_Blocked() != Target.Has<FTag_HexCell_ManualBlock>())
                    {
                        if (Request.Get_Blocked())
                        { Target.Add<FTag_HexCell_ManualBlock>(); Target.Add<FTag_HexCell_Disabled>(); }
                        else
                        { Target.Remove<FTag_HexCell_ManualBlock>(); Target.Remove<FTag_HexCell_Disabled>(); }
                        hex_grid::ArmBoardReplication(InBoard);
                    }
                }
                else
                {
                    auto& Tags = Target.Get<FFragment_HexCell_Tags>().Get_Tags();
                    if constexpr (std::is_same_v<RequestType, FCk_Request_HexCell_AddTag>)
                    {
                        if (!Tags.HasTagExact(Request.Get_Tag()))
                        {
                            Tags.AddTag(Request.Get_Tag());
                            hex_grid::ArmBoardReplication(InBoard);
                        }
                    }
                    else if (Tags.RemoveTag(Request.Get_Tag())) { hex_grid::ArmBoardReplication(InBoard); }
                }
                Request.TryFireCompletion(Target, Result::Succeeded);
            }
        }, InCommand._Rule);
    }
}

namespace ck
{
    auto FProcessor_HexGrid_Rules::ForEachEntity(TimeType, HandleType InBoard, FFragment_HexGrid_RuleRequests& InRequests) const -> void
    {
        _Batches.Add(FBatch{InBoard, MoveTemp(InRequests._Commands)});
        InBoard.Remove<FFragment_HexGrid_RuleRequests>();
    }

    auto FProcessor_HexGrid_Rules::DoTick(TimeType InTime) -> void
    {
        TRACE_CPUPROFILER_EVENT_SCOPE(CkHexGrid_Rules_DoTick);
        TProcessor::DoTick(InTime);
        // No callback runs until all eligible board queues have been detached from the registry.
        for (auto& Batch : _Batches)
        {
            for (auto& Command : Batch._Commands)
            { hex_rules::Apply(Batch._Board, Command); }
        }
        _Batches.Reset();
    }

    auto FProcessor_HexGrid_CancelRules::ForEachEntity(TimeType, HandleType InBoard, FFragment_HexGrid_RuleRequests& InRequests) -> void
    {
        auto Commands = MoveTemp(InRequests._Commands);
        InBoard.Remove<FFragment_HexGrid_RuleRequests>();
        for (auto& Command : Commands)
        { hex_rules::Finish(Command, ECk_Request_OperationResult::Failed_Cancelled); }
    }

    auto hex_rules::EnqueueDeath(FCk_Handle_HexGrid Board, FGuid PlacementId, FCk_Handle Dying) -> void
    {
        if (!hex_rules::IsLive(Board) || !UCk_Utils_Net_UE::Get_HasAuthority(Board)) { return; }
        auto& Queue = Board.AddOrGet<FFragment_HexGrid_RuleRequests>()._Commands;
        if (!Queue.ContainsByPredicate([&](const FHexRuleCommand& Command)
        {
            const auto* Prior = std::get_if<FHexPlacementDeath>(&Command._Rule);
            return Prior && Prior->_PlacementId == PlacementId && Prior->_ExpectedHandle == Dying;
        }))
        { Queue.Add(FHexRuleCommand{Board, FHexPlacementDeath{PlacementId, Dying}}); }
    }

    auto FProcessor_HexGrid_BaselineDeadline::ForEachEntity(TimeType, HandleType InBoard,
        const FFragment_HexGridBaselineDeadline& InDeadline) -> void
    {
        if (FPlatformTime::Seconds() < InDeadline._ExpiresAt) { return; }
        auto Board = UCk_Utils_HexGrid_UE::Cast(InBoard);
        hex_grid::OnNetBaselineRejected(Board, ECk_HexGridFailure::ReplicationTimedOut);
    }

    auto FProcessor_HexGrid_EndPlay::ForEachEntity(TimeType, HandleType InBoard, FFragment_HexGridReadiness& InState) -> void
    {
        InState._Current = {ECk_HexGridReadiness::Destroying, ECk_HexGridFailure::Destroyed, {}};
        hex_grid::ClearPlacements(InBoard);
        auto Grid = UCk_Utils_HexGrid_UE::Cast(InBoard);
        UUtils_Signal_HexGrid_ReadinessChanged::Broadcast(Grid, ck::MakePayload(Grid, InState._Current));
    }

    auto FProcessor_HexGridBlocker_EndPlay::ForEachEntity(TimeType, HandleType InBlocker, FFragment_HexGridBlocker& InState) -> void
    {
        auto Board = InState._Board;
        if (!hex_rules::IsLive(Board))
        { return; }
        hex_rules::SetActive(InBlocker, false);
        hex_grid::ArmBoardReplication(Board);
        if (!Board.Has<FFragment_HexGrid_RuleRequests>())
        { return; }

        // Detach cancellations before invoking user code. A completion can append a command
        // or destroy the board. Commands for other targets retain their submission order.
        auto& Queue = Board.Get<FFragment_HexGrid_RuleRequests>()._Commands;
        TArray<FHexRuleCommand> Cancelled;
        for (auto& Command : Queue)
        {
            if (Command._Target == InBlocker)
            { Cancelled.Add(FHexRuleCommand{Command._Target, MoveTemp(Command._Rule)}); }
        }
        Queue.RemoveAll([&](const auto& Command) { return Command._Target == InBlocker; });
        if (Queue.IsEmpty())
        { Board.Remove<FFragment_HexGrid_RuleRequests>(); }
        for (auto& Command : Cancelled)
        { hex_rules::Finish(Command, ECk_Request_OperationResult::Failed_Cancelled); }
    }
}
