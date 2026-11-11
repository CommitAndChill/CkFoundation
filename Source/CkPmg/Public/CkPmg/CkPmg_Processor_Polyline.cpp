#include "CkPmg_Processor_Polyline.h"

#include "CkCore/Object/CkObject_Utils.h"
#include "CkCore/Diagnostics/CkDiagnosticVisibility.h"

#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "CkPmg/CkPmg_Log.h"
#include "CkPmg_Polyline_Geometry.h"

#include <MaterialDomain.h>
#include <Materials/Material.h>
#include <ProceduralMeshComponent.h>

#include "CkEcs/Request/CkRequest_Completion.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_Pmg_Polyline_Setup);
CK_REGISTER_PROCESSOR(ck::FProcessor_Pmg_Polyline_HandleRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_Pmg_Polyline_CancelPendingRequests);
CK_REGISTER_PROCESSOR(ck::FProcessor_Pmg_Polyline_EndPlay);

// --------------------------------------------------------------------------------------------------------------------

namespace ck_pmg_processor_polyline
{
    constexpr auto MeshSectionIndex = 0;
    constexpr auto CreateCollision = false;

    // Vertex colour R carries the normalised arc length; an sRGB conversion would bend that ramp.
    constexpr auto SRGBConversion = false;

    auto
        DoApplyMaterial(
            UProceduralMeshComponent* InMeshComponent,
            UMaterialInterface* InMaterial)
        -> void
    {
        if (ck::IsValid(InMaterial))
        {
            InMeshComponent->SetMaterial(MeshSectionIndex, InMaterial);
            return;
        }

        if (auto* DefaultMaterial = UMaterial::GetDefaultMaterial(MD_Surface);
            ck::IsValid(DefaultMaterial))
        { InMeshComponent->SetMaterial(MeshSectionIndex, DefaultMaterial); }
    }

    // Rewrites the fixed-topology section in place; the polyline never calls CreateMeshSection after Setup.
    auto
        DoUpdateTube(
            const FCk_Handle_Pmg_Polyline& InHandle,
            const ck::FFragment_Pmg_Polyline_Params& InParams,
            UProceduralMeshComponent* InMeshComponent,
            TConstArrayView<FVector> InWorldPoints,
            float InRadius)
        -> ck::pmg::polyline::FTubeBuffers
    {
        auto Buffers = ck::pmg::polyline::Build_Tube(InWorldPoints, InRadius, InParams.Get_Sides(), InParams.Get_MaxPoints());

        if (InWorldPoints.Num() > 0)
        {
            InMeshComponent->SetWorldTransform(FTransform{FQuat::Identity, InWorldPoints[0], FVector::OneVector});
        }

        InMeshComponent->UpdateMeshSection_LinearColor(
            MeshSectionIndex,
            Buffers._Positions,
            Buffers._Normals,
            Buffers._UV0,
            Buffers._Colors,
            Buffers._Tangents,
            SRGBConversion);

        const auto IsDrawn = Buffers._NumDrawnPoints >= 2;
        InMeshComponent->SetMeshSectionVisible(MeshSectionIndex, IsDrawn);

        ck::pmg::VeryVerbose(TEXT("Pmg Polyline [{}] drew [{}] points over [{}] cm"),
            InHandle, Buffers._NumDrawnPoints, Buffers._Length);

        return Buffers;
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_Pmg_Polyline_Setup::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Pmg_Polyline_Params& InParams,
            FFragment_Pmg_Polyline& InPolyline)
            -> void
    {
        // Returning before the NeedsSetup removal below is what re-arms this Setup next tick; the
        // pending tag is not the dirty marker, so marking it cannot re-pump the frame.
        if (InPolyline._MaterialPreloadBatch.Get_IsRequested() &&
            NOT InPolyline._MaterialPreloadBatch.Get_IsReady())
        {
            InHandle.AddOrGet<FTag_Pmg_Polyline_PendingAssetLoad>();
            return;
        }

        InHandle.Try_Remove<FTag_Pmg_Polyline_PendingAssetLoad>();

        InHandle.Remove<MarkedDirtyBy>();

        ck::pmg::Verbose(TEXT("Setting up Pmg Polyline [{}]"), InHandle);

        const auto World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
        CK_ENSURE_IF_NOT(ck::IsValid(World),
            TEXT("Pmg Polyline [{}] could not get valid World"), InHandle)
        { return; }

        auto MeshComponent = UCk_Utils_Object_UE::Request_CreateNewObject<UProceduralMeshComponent>(
            World, UProceduralMeshComponent::StaticClass(), nullptr,
            FCk_ObjectPooling_PoolParams{}.Set_RecyclePolicy(ECk_ObjectPooling_RecyclePolicy::DestroyOnRelease), nullptr);

        CK_ENSURE_IF_NOT(ck::IsValid(MeshComponent),
            TEXT("Pmg Polyline [{}] failed to create ProceduralMeshComponent"), InHandle)
        { return; }

        MeshComponent->SetWorldLocation(FVector::ZeroVector);
        MeshComponent->RegisterComponentWithWorld(World);

        if (MeshComponent->HasBegunPlay() == false)
        {
            MeshComponent->BeginPlay();
        }

        MeshComponent->SetCastShadow(false);
        MeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        const auto& MaterialPreloadBatch = InPolyline._MaterialPreloadBatch;
        const auto MaterialPreloadFailed = MaterialPreloadBatch.Get_IsRequested() && MaterialPreloadBatch.Get_HasFailed();

        // Batch-first so the applied material is the one the batch roots; the resident-or-null
        // fallback covers params built raw with an already-loaded material.
        auto ResolvedMaterial = MaterialPreloadBatch.Get_IsRequested()
            ? ::Cast<UMaterialInterface>(MaterialPreloadBatch.Get_ResolvedObject(InParams.Get_Material().ToSoftObjectPath()))
            : InParams.Get_Material().Get();

        CK_ENSURE_IF_NOT(NOT MaterialPreloadFailed,
            TEXT("Pmg Polyline [{}]: preload of Material [{}] failed — falling back to the default material."),
            InHandle, InParams.Get_Material().ToSoftObjectPath().ToString())
        { ResolvedMaterial = nullptr; }

        InPolyline._Material = ResolvedMaterial;

        // The one CreateMeshSection of the shape's life: MaxPoints rings, every vertex at the origin, so
        // every triangle is degenerate until the first SetPoints rewrites the vertex buffer.
        const auto EmptyTube = pmg::polyline::Build_Tube({}, InPolyline._Radius, InParams.Get_Sides(), InParams.Get_MaxPoints());
        MeshComponent->CreateMeshSection_LinearColor(
            ck_pmg_processor_polyline::MeshSectionIndex,
            EmptyTube._Positions,
            pmg::polyline::Build_Triangles(InParams.Get_MaxPoints(), InParams.Get_Sides()),
            EmptyTube._Normals,
            EmptyTube._UV0,
            EmptyTube._Colors,
            EmptyTube._Tangents,
            ck_pmg_processor_polyline::CreateCollision,
            ck_pmg_processor_polyline::SRGBConversion);

        MeshComponent->SetMeshSectionVisible(ck_pmg_processor_polyline::MeshSectionIndex, false);

        ck_pmg_processor_polyline::DoApplyMaterial(MeshComponent, InPolyline._Material);

        const auto ShouldBeVisible = NOT ck::diagnostic_visibility::Is_HiddenForStreamerMode();
        MeshComponent->SetVisibility(ShouldBeVisible, true);
        MeshComponent->SetHiddenInGame(NOT ShouldBeVisible);

        MeshComponent->UpdateBounds();
        MeshComponent->MarkRenderStateDirty();

        InPolyline._MeshComponent = MeshComponent;

        ck::pmg::Verbose(TEXT("Pmg Polyline [{}] setup complete"), InHandle);
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_Pmg_Polyline_HandleRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Pmg_Polyline_Params& InParams,
            FFragment_Pmg_Polyline& InPolyline,
            FFragment_Pmg_Polyline_Requests& InRequests)
            -> void
    {
        const auto MaterialRequests = InRequests._SetMaterialRequests;
        const auto RadiusRequests = InRequests._SetRadiusRequests;
        const auto PointsRequests = InRequests._SetPointsRequests;

        InRequests._SetMaterialRequests.Reset();
        InRequests._SetRadiusRequests.Reset();
        InRequests._SetPointsRequests.Reset();

        auto* MeshComponent = InPolyline._MeshComponent.Get();

        const auto HasMeshComponent = ck::IsValid(MeshComponent);
        CK_ENSURE_IF_NOT(HasMeshComponent,
            TEXT("Pmg Polyline [{}] has no mesh component after Setup; its requests are dropped"), InHandle)
        {
            ck::request::FireCancelledForPending(InHandle, MaterialRequests);
            ck::request::FireCancelledForPending(InHandle, RadiusRequests);
            ck::request::FireCancelledForPending(InHandle, PointsRequests);
            InHandle.Remove<MarkedDirtyBy>();
            return;
        }

        // Materials apply in order. One still loading holds itself and every later material back to the
        // next tick (radius and points are not held: they must draw this frame).
        for (auto Index = 0; Index < MaterialRequests.Num(); ++Index)
        {
            const auto& Request = MaterialRequests[Index];
            const auto& PreloadBatch = Request.Get_PreloadBatch();

            if (PreloadBatch.Get_IsRequested() && NOT PreloadBatch.Get_IsReady())
            {
                InRequests._SetMaterialRequests.Insert(MaterialRequests.GetData() + Index, MaterialRequests.Num() - Index, 0);
                InHandle.AddOrGet<FTag_Pmg_Polyline_PendingAssetLoad>();
                break;
            }

            auto Result = ECk_Request_OperationResult::Failed;
            const auto Guard = ck::MakeCompletionGuard(Request, InHandle, Result);

            const auto& SoftMaterial = Request.Get_Material();
            const auto PreloadFailed = PreloadBatch.Get_IsRequested() && PreloadBatch.Get_HasFailed();

            // Batch-first so the applied material is the one the batch roots; the
            // resident-or-null fallback covers requests built raw in BP/AS.
            auto ResolvedMaterial = PreloadBatch.Get_IsRequested()
                ? ::Cast<UMaterialInterface>(PreloadBatch.Get_ResolvedObject(SoftMaterial.ToSoftObjectPath()))
                : SoftMaterial.Get();

            CK_ENSURE_IF_NOT(NOT PreloadFailed,
                TEXT("Pmg Polyline [{}]: preload of Material [{}] failed — the material update is skipped."),
                InHandle, SoftMaterial.ToSoftObjectPath().ToString())
            { continue; }

            if (ck::Is_NOT_Valid(ResolvedMaterial))
            { continue; }

            InPolyline._Material = ResolvedMaterial;
            InPolyline._MaterialPreloadBatch = PreloadBatch;
            MeshComponent->SetMaterial(ck_pmg_processor_polyline::MeshSectionIndex, ResolvedMaterial);

            Result = ECk_Request_OperationResult::Succeeded;
        }

        if (InRequests._SetMaterialRequests.IsEmpty())
        { InHandle.Try_Remove<FTag_Pmg_Polyline_PendingAssetLoad>(); }

        auto NeedsRebuild = false;

        for (const auto& Request : RadiusRequests)
        {
            auto Result = ECk_Request_OperationResult::Failed;
            const auto Guard = ck::MakeCompletionGuard(Request, InHandle, Result);

            InPolyline._Radius = Request.Get_Radius();
            NeedsRebuild = true;

            Result = ECk_Request_OperationResult::Succeeded;
        }

        // Only the last SetPoints of a drain is drawn; the earlier ones are superseded within the same frame.
        for (const auto& Request : PointsRequests)
        {
            auto Result = ECk_Request_OperationResult::Failed;
            const auto Guard = ck::MakeCompletionGuard(Request, InHandle, Result);

            Result = ECk_Request_OperationResult::Succeeded;
        }

        if (NOT PointsRequests.IsEmpty())
        {
            InPolyline._WorldPoints = PointsRequests.Last().Get_WorldPoints();
            NeedsRebuild = true;
        }

        if (NeedsRebuild)
        {
            const auto Buffers = ck_pmg_processor_polyline::DoUpdateTube(
                InHandle, InParams, MeshComponent, InPolyline._WorldPoints, InPolyline._Radius);

            InPolyline._NumDrawnPoints = Buffers._NumDrawnPoints;
            InPolyline._DrawnLength = Buffers._Length;
        }

        if (InRequests._SetMaterialRequests.IsEmpty() &&
            InRequests._SetRadiusRequests.IsEmpty() &&
            InRequests._SetPointsRequests.IsEmpty())
        {
            InHandle.Remove<MarkedDirtyBy>();
        }
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_Pmg_Polyline_CancelPendingRequests::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_Pmg_Polyline_Requests& InRequests)
        -> void
    {
        ck::request::FireCancelledForPending(InHandle, InRequests.Get_SetMaterialRequests());
        ck::request::FireCancelledForPending(InHandle, InRequests.Get_SetRadiusRequests());
        ck::request::FireCancelledForPending(InHandle, InRequests.Get_SetPointsRequests());
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_Pmg_Polyline_EndPlay::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_Pmg_Polyline& InPolyline)
            -> void
    {
        ck::pmg::Verbose(TEXT("Tearing down Pmg Polyline [{}]"), InHandle);

        auto MeshComponent = InPolyline._MeshComponent.Get();
        if (ck::IsValid(MeshComponent))
        {
            // unpin before DestroyComponent (destroy garbage-marks the object, failing release's validity check)
            UCk_Utils_Object_UE::TryReleaseToPool(MeshComponent);
            MeshComponent->DestroyComponent();
        }

        InPolyline._MeshComponent.Reset();
    }
}

// --------------------------------------------------------------------------------------------------------------------
