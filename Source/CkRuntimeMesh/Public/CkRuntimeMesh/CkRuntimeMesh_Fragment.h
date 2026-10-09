#pragma once

#include "CkRuntimeMesh/CkRuntimeMesh_Fragment_Data.h"
#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Geometry.h"
#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Slice.h"

#include "CkEcs/Snapshot/CkSnapshot_Posture.h"
#include "CkResourceLoader/CkResourceLoader_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

class UWorld;
class UCk_Utils_RuntimeMesh_UE;

namespace ck
{
    struct FRuntimeMesh_QueuedSlice;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck::runtimemesh
{
    // Provisional ceilings on in-flight slices per world and on slices executed per drain; tuning inputs to the
    // benchmark, not measured budgets.
    inline constexpr auto QueueCapacity = int32{16};
    inline constexpr auto DrainBudgetPerTick = int32{2};

    namespace ck_runtime_mesh_processor
    {
        auto
        Latch(
            ck::FRuntimeMesh_QueuedSlice& InEntry) -> void;

        auto
        Process(
            ck::FRuntimeMesh_QueuedSlice& InEntry) -> FCk_RuntimeMesh_SliceResult;
    }

    CKRUNTIMEMESH_API auto
    CancelWorld(
        UWorld* InWorld) -> void;

    CKRUNTIMEMESH_API auto
    MakeCutOptions(
        const FCk_Request_RuntimeMesh_Slice& InRequest) -> slice::FCutOptions;

    CKRUNTIMEMESH_API auto
    MakeImportOptions(
        const FCk_RuntimeMesh_ImportLimits& InLimits) -> geometry::FImportOptions;

    CKRUNTIMEMESH_API auto
    MakeMetrics(
        const geometry::FValidatedGeometry& InGeometry) -> FCk_RuntimeMesh_Metrics;

    CKRUNTIMEMESH_API auto
    MapSliceOutcome(
        slice::ECutOutcome InOutcome) -> ECk_RuntimeMesh_SliceOutcome;

    CKRUNTIMEMESH_API auto
    IsIndependentOwner(
        const FCk_Handle_RuntimeMesh& InSource,
        const FCk_Handle& InOwner) -> bool;
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    struct CKRUNTIMEMESH_API FFragment_RuntimeMesh : FCk_Snapshot_Session
    {
    public:
        CK_GENERATED_BODY(FFragment_RuntimeMesh);

    public:
        friend class FProcessor_RuntimeMesh_Setup;

    private:
        ECk_RuntimeMesh_SetupState _SetupState = ECk_RuntimeMesh_SetupState::Pending;
        ECk_RuntimeMesh_SetupFailure _SetupFailure = ECk_RuntimeMesh_SetupFailure::None;
        runtimemesh::geometry::FValidatedGeometryPtr _Geometry;

    public:
        CK_PROPERTY_GET(_SetupState);
        CK_PROPERTY_GET(_SetupFailure);
        CK_PROPERTY_GET(_Geometry);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_RuntimeMesh, _SetupState, _SetupFailure, _Geometry);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Exists only while the soft source resolves; removing it at Ready/Failed releases the batch root, so a
    // terminal mesh retains neither its Spec nor its source asset.
    struct CKRUNTIMEMESH_API FFragment_RuntimeMesh_PendingImport : FCk_Snapshot_Session
    {
    public:
        CK_GENERATED_BODY(FFragment_RuntimeMesh_PendingImport);

    public:
        friend class FProcessor_RuntimeMesh_Setup;

    private:
        FSoftObjectPath _SourcePath;
        runtimemesh::geometry::FImportOptions _Options;
        FCk_ResourceLoader_RootedAssetBatch _Batch;

    public:
        CK_PROPERTY_GET(_SourcePath);
        CK_PROPERTY_GET(_Options);
        CK_PROPERTY_GET(_Batch);

    public:
        CK_DEFINE_CONSTRUCTORS(FFragment_RuntimeMesh_PendingImport, _SourcePath, _Options);
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct FRuntimeMesh_OperationKey
    {
    public:
        CK_GENERATED_BODY(FRuntimeMesh_OperationKey);

    private:
        FCk_Handle_RuntimeMesh _Source;
        FGuid _OperationID;

    public:
        CK_PROPERTY_GET(_Source);
        CK_PROPERTY_GET(_OperationID);

    public:
        CK_DEFINE_CONSTRUCTORS(FRuntimeMesh_OperationKey, _Source, _OperationID);

    public:
        auto
        operator==(
            const FRuntimeMesh_OperationKey& InOther) const -> bool
        {
            return _Source == InOther._Source && _OperationID == InOther._OperationID;
        }
    };

    // --------------------------------------------------------------------------------------------------------------------

    struct FRuntimeMesh_QueuedSlice
    {
    public:
        CK_GENERATED_BODY(FRuntimeMesh_QueuedSlice);

    private:
        FCk_Handle_RuntimeMesh _Source;
        FCk_Handle _QueueEntity;
        FCk_Request_RuntimeMesh_Slice _Request;
        FCk_Delegate_RuntimeMesh_OnSliceResolved _Receiver;

    public:
        CK_PROPERTY_GET(_Source);
        CK_PROPERTY_GET(_QueueEntity);
        CK_PROPERTY_GET(_Request);
        CK_PROPERTY_GET(_Receiver);

    public:
        CK_DEFINE_CONSTRUCTORS(FRuntimeMesh_QueuedSlice, _Source, _QueueEntity, _Request, _Receiver);
    };

    // --------------------------------------------------------------------------------------------------------------------

    // Lives on the world's transient entity. _InFlight keeps a drained request's key until its terminal result is
    // latched, so a callback cannot reuse an operation ID that is still executing.
    struct CKRUNTIMEMESH_API FFragment_RuntimeMesh_Queue : FCk_Snapshot_Session
    {
    public:
        CK_GENERATED_BODY(FFragment_RuntimeMesh_Queue);

    public:
        friend class FProcessor_RuntimeMesh_Drain;
        friend class FProcessor_RuntimeMesh_SourceEndPlay;
        friend class FProcessor_RuntimeMesh_WorldEndPlay;
        friend class ::UCk_Utils_RuntimeMesh_UE;
        friend CKRUNTIMEMESH_API auto runtimemesh::CancelWorld(UWorld*) -> void;
        friend auto runtimemesh::ck_runtime_mesh_processor::Latch(FRuntimeMesh_QueuedSlice&) -> void;

    private:
        bool _Closing = false;
        TArray<FRuntimeMesh_QueuedSlice> _Pending;
        TArray<FRuntimeMesh_OperationKey> _InFlight;

    public:
        CK_PROPERTY_GET(_Closing);
        CK_PROPERTY_GET(_Pending);
        CK_PROPERTY_GET(_InFlight);
    };
}

// --------------------------------------------------------------------------------------------------------------------
