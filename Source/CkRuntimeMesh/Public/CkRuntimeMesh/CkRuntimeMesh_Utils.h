#pragma once

#include "CkRuntimeMesh/CkRuntimeMesh_Fragment.h"

#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcsExt/CkEcsExt_Utils.h"

#include "CkRuntimeMesh_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_RuntimeMesh"))
class CKRUNTIMEMESH_API UCk_Utils_RuntimeMesh_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_RuntimeMesh_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_RuntimeMesh);

public:
    friend class UCk_Utils_Ecs_Base_UE;
    friend auto ck::runtimemesh::ck_runtime_mesh_processor::Process(
        ck::FRuntimeMesh_QueuedSlice&) -> FCk_RuntimeMesh_SliceResult;

public:
    /** Composes the geometry feature on InHandle and starts loading the source; setup resolves asynchronously to
     *  Ready or a typed Failed reason (Get_SetupState / Get_SetupFailure). */
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck][RuntimeMesh] Add")
    static FCk_Handle_RuntimeMesh
    Add(
        UPARAM(ref) FCk_Handle& InHandle,
        const FCk_RuntimeMesh_Spec& InSpec);

    // Not providing a Remove by design: derived results are standalone entities, so their lifetime is the
    // entity's - use UCk_Utils_EntityLifetime_UE::Request_DestroyEntity.

public:
    static auto
    Has(
        const FCk_Handle& InHandle) -> bool;

private:
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck][RuntimeMesh] Cast",
              meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_RuntimeMesh
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck][RuntimeMesh] Handle -> RuntimeMesh Handle",
              meta = (CompactNodeTitle = "<AsRuntimeMesh>", BlueprintAutocast))
    static FCk_Handle_RuntimeMesh
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck] Get Invalid RuntimeMesh Handle",
              meta = (CompactNodeTitle = "INVALID_RuntimeMeshHandle", Keywords = "make"))
    static FCk_Handle_RuntimeMesh
    Get_InvalidHandle() { return {}; }

public:
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck][RuntimeMesh] Get Setup State")
    static ECk_RuntimeMesh_SetupState
    Get_SetupState(
        const FCk_Handle_RuntimeMesh& InMesh);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck][RuntimeMesh] Get Setup Failure")
    static ECk_RuntimeMesh_SetupFailure
    Get_SetupFailure(
        const FCk_Handle_RuntimeMesh& InMesh);

    /** Ready geometry only; check Get_SetupState first. */
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck][RuntimeMesh] Get Metrics")
    static FCk_RuntimeMesh_Metrics
    Get_Metrics(
        const FCk_Handle_RuntimeMesh& InMesh);

    /** Ready geometry only. A caller-owned snapshot of the unique mesh-local vertex positions (cm), e.g. as
     *  convex-body input. */
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck][RuntimeMesh] Copy Local Vertices")
    static TArray<FVector>
    Copy_LocalVerticesCm(
        const FCk_Handle_RuntimeMesh& InMesh);

public:
    /** Enqueues a plane slice of a Ready source. InReceiver gets exactly one typed result per accepted call, then
     *  InDelegate the generic completion: NoIntersection and TouchingOnly complete Succeeded with no result
     *  entities; a source that is not Ready, a full queue, or a malformed request is rejected on this call stack
     *  with Failed_NotEnqueued. */
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|RuntimeMesh",
              DisplayName = "[Ck][RuntimeMesh] Request Slice",
              meta = (AutoCreateRefTerm = "InDelegate"))
    static FCk_Handle_RuntimeMesh
    Request_Slice(
        UPARAM(ref) FCk_Handle_RuntimeMesh& InSource,
        const FCk_Request_RuntimeMesh_Slice& InRequest,
        const FCk_Delegate_RuntimeMesh_OnSliceResolved& InReceiver,
        const FCk_Delegate_Request_OnCompleted& InDelegate);

private:
    // The drain's only way to publish geometry it has already validated; not a consumer API.
    static auto
    Adopt_Validated(
        const FCk_Handle& InResultOwner,
        ck::runtimemesh::geometry::FValidatedGeometryPtr InGeometry) -> FCk_Handle_RuntimeMesh;
};

// --------------------------------------------------------------------------------------------------------------------
