#include "CkRuntimeMesh/CkRuntimeMesh_Processor.h"

#include "CkRuntimeMesh/CkRuntimeMesh_Utils.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkResourceLoader/CkResourceLoader_Utils.h"

#include <Engine/StaticMesh.h>
#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_RuntimeMesh_Setup);
CK_REGISTER_PROCESSOR(ck::FProcessor_RuntimeMesh_Drain);
CK_REGISTER_PROCESSOR(ck::FProcessor_RuntimeMesh_SourceEndPlay);
CK_REGISTER_PROCESSOR(ck::FProcessor_RuntimeMesh_WorldEndPlay);

// --------------------------------------------------------------------------------------------------------------------

namespace ck::runtimemesh::ck_runtime_mesh_processor
{
    auto
        MapImportFailure(
            geometry::EImportFailure InFailure)
        -> ECk_RuntimeMesh_SetupFailure
    {
        switch (InFailure)
        {
            case geometry::EImportFailure::None: return ECk_RuntimeMesh_SetupFailure::None;
            case geometry::EImportFailure::InvalidOptions: return ECk_RuntimeMesh_SetupFailure::InvalidSpec;
            case geometry::EImportFailure::InvalidExecutionContext: return ECk_RuntimeMesh_SetupFailure::InvalidExecutionContext;
            case geometry::EImportFailure::MissingRenderData: return ECk_RuntimeMesh_SetupFailure::MissingRenderData;
            case geometry::EImportFailure::InvalidLOD: return ECk_RuntimeMesh_SetupFailure::InvalidLOD;
            case geometry::EImportFailure::CpuDataUnavailable: return ECk_RuntimeMesh_SetupFailure::CpuDataUnavailable;
            case geometry::EImportFailure::LimitExceeded: return ECk_RuntimeMesh_SetupFailure::LimitExceeded;
            case geometry::EImportFailure::InvalidIndexOrSection: return ECk_RuntimeMesh_SetupFailure::InvalidIndexOrSection;
            case geometry::EImportFailure::InvalidMaterial: return ECk_RuntimeMesh_SetupFailure::InvalidMaterial;
            case geometry::EImportFailure::InvalidAttribute: return ECk_RuntimeMesh_SetupFailure::InvalidAttribute;
            case geometry::EImportFailure::AmbiguousSeam: return ECk_RuntimeMesh_SetupFailure::AmbiguousSeam;
            case geometry::EImportFailure::InvalidTopology: return ECk_RuntimeMesh_SetupFailure::InvalidTopology;
            case geometry::EImportFailure::SelfIntersection: return ECk_RuntimeMesh_SetupFailure::SelfIntersection;
            case geometry::EImportFailure::NonPositiveVolume: return ECk_RuntimeMesh_SetupFailure::NonPositiveVolume;
            case geometry::EImportFailure::ConversionFailed: return ECk_RuntimeMesh_SetupFailure::ConversionFailed;
        }

        CK_TRIGGER_ENSURE(TEXT("Unhandled geometry::EImportFailure [{}]"), static_cast<int32>(InFailure));
        return ECk_RuntimeMesh_SetupFailure::ConversionFailed;
    }

    auto
        MakeTerminal(
            const FRuntimeMesh_QueuedSlice& InEntry,
            ECk_RuntimeMesh_SliceOutcome InOutcome)
        -> FCk_RuntimeMesh_SliceResult
    {
        return FCk_RuntimeMesh_SliceResult{InEntry.Get_Request().Get_OperationID(), InOutcome, {}, {}, {}, {}, 0, 0};
    }

    auto
        Get_CompletionResult(
            ECk_RuntimeMesh_SliceOutcome InOutcome)
        -> ECk_Request_OperationResult
    {
        switch (InOutcome)
        {
            case ECk_RuntimeMesh_SliceOutcome::Succeeded:
            case ECk_RuntimeMesh_SliceOutcome::NoIntersection:
            case ECk_RuntimeMesh_SliceOutcome::TouchingOnly:
            {
                return ECk_Request_OperationResult::Succeeded;
            }
            case ECk_RuntimeMesh_SliceOutcome::FailedCancelled:
            {
                return ECk_Request_OperationResult::Failed_Cancelled;
            }
            default:
            {
                return ECk_Request_OperationResult::Failed;
            }
        }
    }

    auto
        FireTerminal(
            const FRuntimeMesh_QueuedSlice& InEntry,
            const FCk_RuntimeMesh_SliceResult& InResult)
        -> void
    {
        // InEntry and InResult are owned copies, detached from the queue: either callback may destroy any entity the
        // payload names, so nothing here may read a fragment.
        InEntry.Get_Receiver().ExecuteIfBound(InResult);
        InEntry.Get_Request().TryFireCompletion(InEntry.Get_Source(), Get_CompletionResult(InResult.Get_Outcome()));
    }

    auto
        FireCancelled(
            const FRuntimeMesh_QueuedSlice& InEntry)
        -> void
    {
        FireTerminal(InEntry, MakeTerminal(InEntry, ECk_RuntimeMesh_SliceOutcome::FailedCancelled));
    }

    auto
        Latch(
            FRuntimeMesh_QueuedSlice& InEntry)
        -> void
    {
        auto WorldEntity = InEntry.Get_QueueEntity();

        if (ck::Is_NOT_Valid(WorldEntity, ck::IsValid_Policy_IncludePendingKill{})
            || NOT WorldEntity.Has<FFragment_RuntimeMesh_Queue>())
        { return; }

        auto& Queue = WorldEntity.Get<FFragment_RuntimeMesh_Queue, ck::IsValid_Policy_IncludePendingKill>();
        Queue._InFlight.RemoveSingle(FRuntimeMesh_OperationKey{InEntry.Get_Source(), InEntry.Get_Request().Get_OperationID()});
    }

    auto
        Process(
            FRuntimeMesh_QueuedSlice& InEntry)
        -> FCk_RuntimeMesh_SliceResult
    {
        const auto& Source = InEntry.Get_Source();
        const auto& ResultOwner = InEntry.Get_Request().Get_ResultOwner();

        if (ck::Is_NOT_Valid(Source)
            || ck::Is_NOT_Valid(ResultOwner)
            || NOT InEntry.Get_Receiver().IsBound()
            || UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(Source, ECk_EntityLifetime_DestructionPhase::BeginDestroy)
            || UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(ResultOwner, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
        { return MakeTerminal(InEntry, ECk_RuntimeMesh_SliceOutcome::FailedCancelled); }

        const auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Source);

        if (ck::Is_NOT_Valid(World)
            || World->IsBeingCleanedUp()
            || NOT IsIndependentOwner(Source, ResultOwner)
            || NOT UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(ResultOwner))
        { return MakeTerminal(InEntry, ECk_RuntimeMesh_SliceOutcome::FailedCancelled); }

        // Geometry is immutable once Ready and the request was admitted only for a Ready source.
        const auto& Mesh = Source.Get<FFragment_RuntimeMesh>();
        const auto IsSourceReady = Mesh.Get_SetupState() == ECk_RuntimeMesh_SetupState::Ready && Mesh.Get_Geometry().IsValid();
        CK_ENSURE_IF_NOT(IsSourceReady,
            TEXT("RuntimeMesh source [{}] lost its Ready geometry while slice [{}] was queued"),
            Source, InEntry.Get_Request().Get_OperationID().ToString())
        { return MakeTerminal(InEntry, ECk_RuntimeMesh_SliceOutcome::InternalFailure); }

        const auto SourceGeometry = Mesh.Get_Geometry();
        const auto Cut = slice::Cut(*SourceGeometry, MakeCutOptions(InEntry.Get_Request()));
        const auto Outcome = MapSliceOutcome(Cut.Get_Outcome());

        if (Outcome != ECk_RuntimeMesh_SliceOutcome::Succeeded)
        { return MakeTerminal(InEntry, Outcome); }

        auto Positive = UCk_Utils_RuntimeMesh_UE::Adopt_Validated(ResultOwner, Cut.Get_Positive());

        if (ck::Is_NOT_Valid(Positive))
        { return MakeTerminal(InEntry, ECk_RuntimeMesh_SliceOutcome::InternalFailure); }

        const auto Negative = UCk_Utils_RuntimeMesh_UE::Adopt_Validated(ResultOwner, Cut.Get_Negative());

        if (ck::Is_NOT_Valid(Negative))
        {
            UCk_Utils_EntityLifetime_UE::Request_DestroyEntity(Positive);
            return MakeTerminal(InEntry, ECk_RuntimeMesh_SliceOutcome::InternalFailure);
        }

        return FCk_RuntimeMesh_SliceResult{
            InEntry.Get_Request().Get_OperationID(),
            Outcome,
            Positive,
            Negative,
            MakeMetrics(*Cut.Get_Positive()),
            MakeMetrics(*Cut.Get_Negative()),
            Cut.Get_PositiveCapTriangles(),
            Cut.Get_NegativeCapTriangles()};
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_RuntimeMesh_Setup::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMesh& InMesh,
            FFragment_RuntimeMesh_PendingImport& InPendingImport)
        -> void
    {
        // Removing the pending fragment releases its batch root, in every terminal state.
        const auto Terminate = [&](ECk_RuntimeMesh_SetupFailure InFailure)
        {
            InMesh._SetupState = ECk_RuntimeMesh_SetupState::Failed;
            InMesh._SetupFailure = InFailure;
            InHandle.Try_Remove<FFragment_RuntimeMesh_PendingImport>();
        };

        if (InMesh._SetupState != ECk_RuntimeMesh_SetupState::Pending)
        {
            InHandle.Try_Remove<FFragment_RuntimeMesh_PendingImport>();
            return;
        }

        if (UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InHandle, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
        {
            Terminate(ECk_RuntimeMesh_SetupFailure::InvalidExecutionContext);
            return;
        }

        if (NOT InPendingImport._Batch.Get_IsRequested())
        {
            InPendingImport._Batch = UCk_Utils_ResourceLoader_UE::RequestLoad_RootedBatch(
                TEXT("RuntimeMesh.Setup"), {InPendingImport._SourcePath});
        }

        if (NOT InPendingImport._Batch.Get_IsReady() && NOT InPendingImport._Batch.Get_HasFailed())
        { return; }

        const auto* Asset = InPendingImport._Batch.Get_HasFailed()
            ? nullptr
            : Cast<UStaticMesh>(InPendingImport._Batch.Get_ResolvedObject(InPendingImport._SourcePath));

        if (ck::Is_NOT_Valid(Asset))
        {
            Terminate(ECk_RuntimeMesh_SetupFailure::LoadFailed);
            return;
        }

        const auto Imported = runtimemesh::geometry::Import(*Asset, InPendingImport._Options);

        if (NOT Imported.Get_IsReady())
        {
            Terminate(runtimemesh::ck_runtime_mesh_processor::MapImportFailure(Imported.Get_Failure()));
            return;
        }

        InMesh._Geometry = Imported.Get_Geometry();
        InMesh._SetupState = ECk_RuntimeMesh_SetupState::Ready;
        InHandle.Try_Remove<FFragment_RuntimeMesh_PendingImport>();
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_RuntimeMesh_Drain::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMesh_Queue& InQueue)
        -> void
    {
        if (InQueue._Closing)
        { return; }

        // Detach before executing: a callback may enqueue, and that request must wait for a later drain.
        // Nothing below touches InQueue once the batch is detached.
        const auto Count = FMath::Min(runtimemesh::DrainBudgetPerTick, InQueue._Pending.Num());
        auto Batch = TArray<FRuntimeMesh_QueuedSlice>{};
        Batch.Reserve(Count);

        for (auto Index = 0; Index < Count; ++Index)
        { Batch.Add(MoveTemp(InQueue._Pending[Index])); }

        InQueue._Pending.RemoveAt(0, Count, EAllowShrinking::No);

        for (auto& Entry : Batch)
        {
            const auto Result = runtimemesh::ck_runtime_mesh_processor::Process(Entry);
            runtimemesh::ck_runtime_mesh_processor::Latch(Entry);
            runtimemesh::ck_runtime_mesh_processor::FireTerminal(Entry, Result);
        }
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_RuntimeMesh_SourceEndPlay::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_RuntimeMesh& InMesh)
        -> void
    {
        auto WorldEntity = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InHandle.Get_RegistryView());

        if (ck::Is_NOT_Valid(WorldEntity, ck::IsValid_Policy_IncludePendingKill{})
            || NOT WorldEntity.Has<FFragment_RuntimeMesh_Queue>())
        { return; }

        auto& Queue = WorldEntity.Get<FFragment_RuntimeMesh_Queue, ck::IsValid_Policy_IncludePendingKill>();
        auto Retained = TArray<FRuntimeMesh_QueuedSlice>{};
        auto Cancelled = TArray<FRuntimeMesh_QueuedSlice>{};

        for (auto& Entry : Queue._Pending)
        {
            if (Entry.Get_Source() == InHandle)
            {
                Queue._InFlight.RemoveSingle(FRuntimeMesh_OperationKey{Entry.Get_Source(), Entry.Get_Request().Get_OperationID()});
                Cancelled.Add(MoveTemp(Entry));
            }
            else
            { Retained.Add(MoveTemp(Entry)); }
        }

        Queue._Pending = MoveTemp(Retained);

        for (const auto& Entry : Cancelled)
        { runtimemesh::ck_runtime_mesh_processor::FireCancelled(Entry); }
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_RuntimeMesh_WorldEndPlay::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMesh_Queue& InQueue)
        -> void
    {
        InQueue._Closing = true;

        const auto Cancelled = MoveTemp(InQueue._Pending);
        InQueue._Pending.Reset();
        InQueue._InFlight.Reset();

        for (const auto& Entry : Cancelled)
        { runtimemesh::ck_runtime_mesh_processor::FireCancelled(Entry); }
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::runtimemesh
{
    auto
        CancelWorld(
            UWorld* InWorld)
        -> void
    {
        if (ck::Is_NOT_Valid(InWorld))
        { return; }

        auto WorldEntity = UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*InWorld);

        if (ck::Is_NOT_Valid(WorldEntity, ck::IsValid_Policy_IncludePendingKill{})
            || NOT WorldEntity.Has<FFragment_RuntimeMesh_Queue>())
        { return; }

        auto& Queue = WorldEntity.Get<FFragment_RuntimeMesh_Queue, ck::IsValid_Policy_IncludePendingKill>();
        Queue._Closing = true;

        const auto Cancelled = MoveTemp(Queue._Pending);
        Queue._Pending.Reset();
        Queue._InFlight.Reset();

        for (const auto& Entry : Cancelled)
        { ck_runtime_mesh_processor::FireCancelled(Entry); }
    }
}

// --------------------------------------------------------------------------------------------------------------------
