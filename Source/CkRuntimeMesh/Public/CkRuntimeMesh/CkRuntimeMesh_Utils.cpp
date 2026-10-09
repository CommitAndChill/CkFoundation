#include "CkRuntimeMesh/CkRuntimeMesh_Utils.h"

#include "CkRuntimeMesh/Internal/CkRuntimeMesh_Slice.h"

#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include <Engine/World.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck::runtimemesh
{
    auto
        MakeImportOptions(
            const FCk_RuntimeMesh_ImportLimits& InLimits)
        -> geometry::FImportOptions
    {
        auto Options = geometry::FImportOptions{};
        Options.Set_LODIndex(InLimits.Get_LODIndex());
        Options.Set_SeamToleranceCm(InLimits.Get_SeamToleranceCm());
        Options.Set_IntersectionToleranceCm(InLimits.Get_IntersectionToleranceCm());
        Options.Set_MinimumTriangleAreaCm2(InLimits.Get_MinimumTriangleAreaCm2());
        Options.Set_MinimumVolumeCm3(InLimits.Get_MinimumVolumeCm3());
        Options.Set_MaximumVertices(InLimits.Get_MaximumVertices());
        Options.Set_MaximumTriangles(InLimits.Get_MaximumTriangles());
        return Options;
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        MakeCutOptions(
            const FCk_Request_RuntimeMesh_Slice& InRequest)
        -> slice::FCutOptions
    {
        const auto& ReflectedPlane = InRequest.Get_Plane();
        auto Plane = slice::FPlaneFrameLocal{};
        Plane.Set_PositionCm(ReflectedPlane.Get_PositionCm());
        Plane.Set_Normal(ReflectedPlane.Get_Normal());
        Plane.Set_Tangent(ReflectedPlane.Get_Tangent());

        const auto& ReflectedCap = InRequest.Get_Cap();
        const auto& Color = ReflectedCap.Get_Color();
        auto Cap = slice::FCapOptions{};
        Cap.Set_MaterialID(ReflectedCap.Get_MaterialID());
        Cap.Set_Color(FVector4f{Color.R, Color.G, Color.B, Color.A});
        Cap.Set_CmPerUVUnit(ReflectedCap.Get_CmPerUVUnit());
        Cap.Set_UVOffset(FVector2f{
            static_cast<float>(ReflectedCap.Get_UVOffset().X),
            static_cast<float>(ReflectedCap.Get_UVOffset().Y)});

        const auto& ReflectedLimits = InRequest.Get_Limits();
        auto Limits = slice::FCutLimits{};
        Limits.Set_MinimumOutputVolumeCm3(ReflectedLimits.Get_MinimumOutputVolumeCm3());
        Limits.Set_MinimumNormalExtentCm(ReflectedLimits.Get_MinimumNormalExtentCm());
        Limits.Set_PlaneToleranceCm(ReflectedLimits.Get_PlaneToleranceCm());
        Limits.Set_AbsoluteVolumeToleranceCm3(ReflectedLimits.Get_AbsoluteVolumeToleranceCm3());
        Limits.Set_RelativeVolumeTolerance(ReflectedLimits.Get_RelativeVolumeTolerance());
        Limits.Set_MaximumVertices(ReflectedLimits.Get_MaximumVertices());
        Limits.Set_MaximumTriangles(ReflectedLimits.Get_MaximumTriangles());

        auto Options = slice::FCutOptions{};
        Options.Set_Plane(Plane);
        Options.Set_Cap(Cap);
        Options.Set_Limits(Limits);
        return Options;
    }

    auto
        MakeMetrics(
            const geometry::FValidatedGeometry& InGeometry)
        -> FCk_RuntimeMesh_Metrics
    {
        const auto& Bounds = InGeometry.Get_BoundsCm();
        return FCk_RuntimeMesh_Metrics{
            InGeometry.Get_VolumeCm3(),
            InGeometry.Get_CentroidCm(),
            Bounds.Min,
            Bounds.Max,
            InGeometry.Get_Mesh().TriangleCount()};
    }

    auto
        MapSliceOutcome(
            slice::ECutOutcome InOutcome)
        -> ECk_RuntimeMesh_SliceOutcome
    {
        switch (InOutcome)
        {
            case slice::ECutOutcome::Succeeded: return ECk_RuntimeMesh_SliceOutcome::Succeeded;
            case slice::ECutOutcome::NoIntersection: return ECk_RuntimeMesh_SliceOutcome::NoIntersection;
            case slice::ECutOutcome::TouchingOnly: return ECk_RuntimeMesh_SliceOutcome::TouchingOnly;
            case slice::ECutOutcome::RejectedTooSmall: return ECk_RuntimeMesh_SliceOutcome::RejectedTooSmall;
            case slice::ECutOutcome::RejectedLimit: return ECk_RuntimeMesh_SliceOutcome::RejectedLimit;
            case slice::ECutOutcome::RejectedTopology: return ECk_RuntimeMesh_SliceOutcome::RejectedTopology;
            case slice::ECutOutcome::InvalidRequest: return ECk_RuntimeMesh_SliceOutcome::InvalidRequest;
            case slice::ECutOutcome::InternalFailure: return ECk_RuntimeMesh_SliceOutcome::InternalFailure;
        }

        CK_TRIGGER_ENSURE(TEXT("Unhandled slice::ECutOutcome [{}]"), static_cast<int32>(InOutcome));
        return ECk_RuntimeMesh_SliceOutcome::InternalFailure;
    }

    auto
        IsIndependentOwner(
            const FCk_Handle_RuntimeMesh& InSource,
            const FCk_Handle& InOwner)
        -> bool
    {
        if (ck::Is_NOT_Valid(InOwner))
        { return false; }

        const auto IsSameRegistry = InSource.Get_RegistryView().Get_RegistryHandle()
            == InOwner.Get_RegistryView().Get_RegistryHandle();

        if (NOT IsSameRegistry
            || InOwner == InSource
            || UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InOwner, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
        { return false; }

        const auto SourceInOwnerChain = UCk_Utils_EntityLifetime_UE::Get_EntityInOwnershipChain_If(InOwner,
        [&](const FCk_Handle& InAncestor)
        {
            return InAncestor == InSource;
        });

        return ck::Is_NOT_Valid(SourceInOwnerChain);
    }
}

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(UCk_Utils_RuntimeMesh_UE, FCk_Handle_RuntimeMesh, ck::FFragment_RuntimeMesh);

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_RuntimeMesh_UE::
    Add(
        FCk_Handle& InHandle,
        const FCk_RuntimeMesh_Spec& InSpec)
    -> FCk_Handle_RuntimeMesh
{
    const auto IsTargetValid = ck::IsValid(InHandle);
    CK_ENSURE_IF_NOT(IsTargetValid, TEXT("RuntimeMesh Add rejected invalid target [{}]"), InHandle)
    { return {}; }

    // World teardown is a contract violation for Add, not a deferred outcome: a mesh composed now would start an
    // asset load into a world that is going away.
    const auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
    const auto WorldEntity = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(InHandle);
    const auto IsWorldClosing = WorldEntity.Has<ck::FFragment_RuntimeMesh_Queue>()
        && WorldEntity.Get<ck::FFragment_RuntimeMesh_Queue>().Get_Closing();
    const auto CanCompose = ck::IsValid(World)
        && NOT World->IsBeingCleanedUp()
        && NOT IsWorldClosing
        && UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InHandle)
        && NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InHandle, ECk_EntityLifetime_DestructionPhase::BeginDestroy)
        && NOT Has(InHandle);
    CK_ENSURE_IF_NOT(CanCompose,
        TEXT("RuntimeMesh Add rejected target [{}]: its world is tearing down, it is being destroyed, or it already has a RuntimeMesh"),
        InHandle)
    { return {}; }

    const auto SourcePath = InSpec.Get_SourceMesh().ToSoftObjectPath();
    const auto IsSpecValid = InSpec.Get_IsValid();
    CK_ENSURE_IF_NOT(IsSpecValid,
        TEXT("RuntimeMesh Add rejected the Spec for [{}]: source [{}] is unset or the import limits are out of range"),
        InHandle, SourcePath.ToString())
    { return {}; }

    InHandle.Add<ck::FFragment_RuntimeMesh>();
    InHandle.Add<ck::FFragment_RuntimeMesh_PendingImport>(SourcePath, ck::runtimemesh::MakeImportOptions(InSpec.Get_Import()));

    return CastChecked(InHandle);
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_RuntimeMesh_UE::
    Get_SetupState(
        const FCk_Handle_RuntimeMesh& InMesh)
    -> ECk_RuntimeMesh_SetupState
{
    const auto IsMeshValid = ck::IsValid(InMesh);
    CK_ENSURE_IF_NOT(IsMeshValid, TEXT("Invalid RuntimeMesh handle [{}]"), InMesh)
    { return ECk_RuntimeMesh_SetupState::Failed; }

    return InMesh.Get<ck::FFragment_RuntimeMesh>().Get_SetupState();
}

auto
    UCk_Utils_RuntimeMesh_UE::
    Get_SetupFailure(
        const FCk_Handle_RuntimeMesh& InMesh)
    -> ECk_RuntimeMesh_SetupFailure
{
    const auto IsMeshValid = ck::IsValid(InMesh);
    CK_ENSURE_IF_NOT(IsMeshValid, TEXT("Invalid RuntimeMesh handle [{}]"), InMesh)
    { return ECk_RuntimeMesh_SetupFailure::InvalidSpec; }

    return InMesh.Get<ck::FFragment_RuntimeMesh>().Get_SetupFailure();
}

auto
    UCk_Utils_RuntimeMesh_UE::
    Get_Metrics(
        const FCk_Handle_RuntimeMesh& InMesh)
    -> FCk_RuntimeMesh_Metrics
{
    const auto IsMeshValid = ck::IsValid(InMesh);
    CK_ENSURE_IF_NOT(IsMeshValid, TEXT("Invalid RuntimeMesh handle [{}]"), InMesh)
    { return {}; }

    const auto& Mesh = InMesh.Get<ck::FFragment_RuntimeMesh>();
    const auto IsReady = Mesh.Get_SetupState() == ECk_RuntimeMesh_SetupState::Ready && Mesh.Get_Geometry().IsValid();
    CK_ENSURE_IF_NOT(IsReady, TEXT("RuntimeMesh [{}] has no metrics in setup state [{}]"), InMesh, Mesh.Get_SetupState())
    { return {}; }

    return ck::runtimemesh::MakeMetrics(*Mesh.Get_Geometry());
}

auto
    UCk_Utils_RuntimeMesh_UE::
    Copy_LocalVerticesCm(
        const FCk_Handle_RuntimeMesh& InMesh)
    -> TArray<FVector>
{
    const auto IsMeshValid = ck::IsValid(InMesh);
    CK_ENSURE_IF_NOT(IsMeshValid, TEXT("Invalid RuntimeMesh handle [{}]"), InMesh)
    { return {}; }

    const auto& Mesh = InMesh.Get<ck::FFragment_RuntimeMesh>();
    const auto IsReady = Mesh.Get_SetupState() == ECk_RuntimeMesh_SetupState::Ready && Mesh.Get_Geometry().IsValid();
    CK_ENSURE_IF_NOT(IsReady, TEXT("RuntimeMesh [{}] has no vertices in setup state [{}]"), InMesh, Mesh.Get_SetupState())
    { return {}; }

    const auto& Geometry = Mesh.Get_Geometry()->Get_Mesh();

    auto Vertices = TArray<FVector>{};
    Vertices.Reserve(Geometry.VertexCount());
    for (const auto VertexID : Geometry.VertexIndicesItr())
    { Vertices.Add(Geometry.GetVertex(VertexID)); }

    return Vertices;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_RuntimeMesh_UE::
    Request_Slice(
        FCk_Handle_RuntimeMesh& InSource,
        const FCk_Request_RuntimeMesh_Slice& InRequest,
        const FCk_Delegate_RuntimeMesh_OnSliceResolved& InReceiver,
        const FCk_Delegate_Request_OnCompleted& InDelegate)
    -> FCk_Handle_RuntimeMesh
{
    // Copies, not references: a rejection runs caller code synchronously, and the by-ref source or a delegate
    // living on the caller's object must not change underneath the rejection that reports it.
    const auto Source = InSource;
    const auto OperationID = InRequest.Get_OperationID();
    const auto Receiver = InReceiver;
    const auto Delegate = InDelegate;

    const auto Reject = [&](ECk_RuntimeMesh_SliceOutcome InReason) -> FCk_Handle_RuntimeMesh
    {
        Receiver.ExecuteIfBound(FCk_RuntimeMesh_SliceResult{OperationID, InReason, {}, {}, {}, {}, 0, 0});
        Delegate.ExecuteIfBound(Source, ECk_Request_OperationResult::Failed_NotEnqueued);
        return Source;
    };

    const auto IsSourceValid = ck::IsValid(Source);
    CK_ENSURE_IF_NOT(IsSourceValid, TEXT("RuntimeMesh Slice rejected invalid source [{}]"), Source)
    { return Reject(ECk_RuntimeMesh_SliceOutcome::InvalidRequest); }

    // A source already being destroyed is a domain outcome rather than an ensure: cancellation callbacks fired
    // during teardown may legitimately retry against it.
    if (UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(Source, ECk_EntityLifetime_DestructionPhase::BeginDestroy))
    { return Reject(ECk_RuntimeMesh_SliceOutcome::InvalidRequest); }

    if (Source.Get<ck::FFragment_RuntimeMesh>().Get_SetupState() != ECk_RuntimeMesh_SetupState::Ready)
    { return Reject(ECk_RuntimeMesh_SliceOutcome::NotReady); }

    const auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(Source);
    const auto& ResultOwner = InRequest.Get_ResultOwner();
    const auto IsRequestValid = ck::IsValid(World)
        && NOT World->IsBeingCleanedUp()
        && Receiver.IsBound()
        && InRequest.Get_IsValid()
        && ck::runtimemesh::IsIndependentOwner(Source, ResultOwner)
        && UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(ResultOwner);
    CK_ENSURE_IF_NOT(IsRequestValid,
        TEXT("RuntimeMesh Slice rejected invalid request/owner/receiver for source [{}]: operation [{}], result owner [{}], receiver bound [{}]"),
        Source, OperationID.ToString(), ResultOwner, Receiver.IsBound())
    { return Reject(ECk_RuntimeMesh_SliceOutcome::InvalidRequest); }

    auto WorldEntity = UCk_Utils_EntityLifetime_UE::Get_TransientEntity(Source);
    auto& Queue = WorldEntity.AddOrGet<ck::FFragment_RuntimeMesh_Queue>();

    if (Queue._Closing)
    { return Reject(ECk_RuntimeMesh_SliceOutcome::InvalidRequest); }

    if (Queue._InFlight.Num() >= ck::runtimemesh::QueueCapacity)
    { return Reject(ECk_RuntimeMesh_SliceOutcome::RejectedLimit); }

    const auto Key = ck::FRuntimeMesh_OperationKey{Source, OperationID};
    const auto IsOperationUnique = NOT Queue._InFlight.Contains(Key);
    CK_ENSURE_IF_NOT(IsOperationUnique,
        TEXT("RuntimeMesh Slice rejected: operation ID already in flight [{}] on source [{}]"),
        OperationID.ToString(), Source)
    { return Reject(ECk_RuntimeMesh_SliceOutcome::InvalidRequest); }

    auto Request = InRequest;

    if (Delegate.IsBound())
    { Request.Set_CompletionDelegate(Delegate); }

    Queue._InFlight.Add(Key);
    Queue._Pending.Add(ck::FRuntimeMesh_QueuedSlice{Source, WorldEntity, MoveTemp(Request), Receiver});

    return Source;
}

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_RuntimeMesh_UE::
    Adopt_Validated(
        const FCk_Handle& InResultOwner,
        ck::runtimemesh::geometry::FValidatedGeometryPtr InGeometry)
    -> FCk_Handle_RuntimeMesh
{
    const auto CanAdopt = InGeometry.IsValid()
        && UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InResultOwner)
        && NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InResultOwner, ECk_EntityLifetime_DestructionPhase::BeginDestroy);
    CK_ENSURE_IF_NOT(CanAdopt,
        TEXT("RuntimeMesh could not adopt validated slice geometry under result owner [{}] after the owner was checked"),
        InResultOwner)
    { return {}; }

    auto Created = UCk_Utils_EntityLifetime_UE::Request_CreateEntity(InResultOwner,
    [&](FCk_Handle& InCreated)
    {
        InCreated.Add<ck::FFragment_RuntimeMesh>(
            ECk_RuntimeMesh_SetupState::Ready, ECk_RuntimeMesh_SetupFailure::None, MoveTemp(InGeometry));
    });

    const auto IsCreated = ck::IsValid(Created);
    CK_ENSURE_IF_NOT(IsCreated, TEXT("RuntimeMesh could not create a result entity under owner [{}]"), InResultOwner)
    { return {}; }

    return CastChecked(Created);
}

// --------------------------------------------------------------------------------------------------------------------
