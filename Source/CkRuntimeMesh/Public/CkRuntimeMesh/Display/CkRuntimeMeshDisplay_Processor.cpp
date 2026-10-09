#include "CkRuntimeMesh/Display/CkRuntimeMeshDisplay_Processor.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Object/CkObject_Utils.h"
#include "CkCore/ObjectPooling/CkObjectPooling_Params.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"
#include "CkEcs/Scheduler/CkProcessorRegistration.h"
#include "CkEcs/Subsystem/CkEcsWorld_Subsystem.h"
#include "CkResourceLoader/CkResourceLoader_Utils.h"

#include "Components/DynamicMeshComponent.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "DynamicMesh/MeshTangents.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "VectorUtil.h"

// --------------------------------------------------------------------------------------------------------------------

CK_REGISTER_PROCESSOR(ck::FProcessor_RuntimeMeshDisplay_Setup);
CK_REGISTER_PROCESSOR(ck::FProcessor_RuntimeMeshDisplay_UpdateTransform);
CK_REGISTER_PROCESSOR(ck::FProcessor_RuntimeMeshDisplay_EndPlay);

// --------------------------------------------------------------------------------------------------------------------

namespace ck::runtimemesh::display
{
    auto
        PrepareTangents(
            UE::Geometry::FDynamicMesh3& InDisplayCopy)
        -> bool
    {
        auto* Attributes = InDisplayCopy.Attributes();
        if (ck::Is_NOT_Valid(Attributes, ck::IsValid_Policy_NullptrOnly{}))
        { return false; }

        auto* Normals = Attributes->PrimaryNormals();
        if (ck::Is_NOT_Valid(Normals, ck::IsValid_Policy_NullptrOnly{}))
        { return false; }

        auto Tangents = UE::Geometry::FMeshTangentsf{&InDisplayCopy};
        if (Attributes->NumUVLayers() > 0)
        { Tangents.ComputeTriVertexTangents(Normals, Attributes->PrimaryUV(), UE::Geometry::FComputeTangentsOptions{}); }
        else
        {
            constexpr auto ClearToZero = true;
            Tangents.InitializeTriVertexTangents(ClearToZero);
        }

        for (const auto TriangleID : InDisplayCopy.TriangleIndicesItr())
        {
            const auto NormalIDs = Normals->GetTriangle(TriangleID);
            for (auto Corner = 0; Corner < 3; ++Corner)
            {
                const auto Normal = FVector3f{FVector3d{Normals->GetElement(NormalIDs[Corner])}.GetSafeNormal()};
                auto Tangent = FVector3f{};
                auto Bitangent = FVector3f{};
                Tangents.GetPerTriangleTangent(TriangleID, Corner, Tangent, Bitangent);

                const auto TangentUnit = FVector3f{FVector3d{Tangent}.GetSafeNormal()};
                const auto BitangentUnit = FVector3f{FVector3d{Bitangent}.GetSafeNormal()};
                const auto IsUsableUvFrame = NOT Tangent.ContainsNaN() && NOT Bitangent.ContainsNaN() &&
                    TangentUnit.SizeSquared() > 0.99f && BitangentUnit.SizeSquared() > 0.99f &&
                    FMath::Abs(FVector3f::DotProduct(TangentUnit, Normal)) < 0.001f &&
                    FMath::Abs(FVector3f::DotProduct(BitangentUnit, Normal)) < 0.001f &&
                    FMath::Abs(FVector3f::DotProduct(FVector3f::CrossProduct(TangentUnit, BitangentUnit), Normal)) > 0.99f;

                if (IsUsableUvFrame)
                {
                    Tangent = TangentUnit;
                    Bitangent = BitangentUnit;
                }
                else
                {
                    // Missing or degenerate UVs have no texture-oriented basis; any finite frame around the
                    // normal is correct, and inventing UVs would change what the material samples.
                    UE::Geometry::VectorUtil::MakePerpVectors(Normal, Tangent, Bitangent);
                }

                Tangents.SetPerTriangleTangent(TriangleID, Corner, Tangent, Bitangent);
            }
        }

        Attributes->EnableTangents();
        return Tangents.CopyToOverlays(InDisplayCopy);
    }

    auto
        ReleaseWorld(
            UWorld* InWorld)
        -> void
    {
        if (ck::Is_NOT_Valid(InWorld, ck::IsValid_Policy_NullptrOnly{}))
        { return; }

        // A world type without an ECS world (editor preview, inactive) has no displays to release.
        auto Transient = UCk_Utils_EcsWorld_Subsystem_UE::TryGet_TransientEntityForWorld(*InWorld);
        if (ck::Is_NOT_Valid(Transient, ck::IsValid_Policy_IncludePendingKill{}))
        { return; }

        auto Displays = TArray<FCk_Entity>{};
        Transient.View<FFragment_RuntimeMeshDisplay>().ForEach(
        [&](FCk_Entity InEntity, FFragment_RuntimeMeshDisplay&)
        {
            Displays.Add(InEntity);
        });

        for (const auto Entity : Displays)
        {
            auto Handle = ck::MakeHandle(Entity, Transient);
            auto& Display = Handle.Get<FFragment_RuntimeMeshDisplay, ck::IsValid_Policy_IncludePendingKill>();
            FProcessor_RuntimeMeshDisplay_EndPlay::Release(Handle, Display);
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        FProcessor_RuntimeMeshDisplay_Setup::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMeshDisplay& InDisplay,
            FFragment_RuntimeMeshDisplay_Setup& InSetup)
        -> void
    {
        const auto Fail = [&](ECk_RuntimeMeshDisplay_SetupFailure InFailure)
        {
            InDisplay._SetupState = ECk_RuntimeMesh_SetupState::Failed;
            InDisplay._SetupFailure = InFailure;
            InHandle.Remove<MarkedDirtyBy>();
            InHandle.Remove<FFragment_RuntimeMeshDisplay_Setup>();
        };

        if (NOT InSetup._Materials.Get_IsRequested())
        {
            const auto MaterialPaths = ck::algo::Transform<TArray<FSoftObjectPath>>(InSetup._Visuals.Get_Materials(),
            [](const TSoftObjectPtr<UMaterialInterface>& InMaterial)
            {
                return InMaterial.ToSoftObjectPath();
            });

            InSetup._Materials = UCk_Utils_ResourceLoader_UE::RequestLoad_RootedBatch(
                FName{TEXT("RuntimeMeshDisplay.Setup")}, MaterialPaths);
        }

        const auto MaterialsLoaded = NOT InSetup._Materials.Get_HasFailed();
        CK_ENSURE_IF_NOT(MaterialsLoaded, TEXT("RuntimeMeshDisplay [{}] failed to load its materials"), InHandle)
        {
            Fail(ECk_RuntimeMeshDisplay_SetupFailure::MaterialLoadFailed);
            return;
        }

        if (NOT InSetup._Materials.Get_IsReady())
        { return; }

        auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InHandle);
        if (ck::Is_NOT_Valid(World) || World->IsBeingCleanedUp())
        {
            Fail(ECk_RuntimeMeshDisplay_SetupFailure::UnsupportedWorld);
            return;
        }

        if (NOT InHandle.Has<FFragment_Transform>())
        {
            Fail(ECk_RuntimeMeshDisplay_SetupFailure::MissingTransform);
            return;
        }

        const auto Transform = InHandle.Get<FFragment_Transform>().Get_Transform();
        if (NOT Transform.IsValid())
        {
            Fail(ECk_RuntimeMeshDisplay_SetupFailure::InvalidTransform);
            return;
        }

        auto ResolvedMaterials = TArray<UMaterialInterface*>{};
        for (const auto& Material : InSetup._Visuals.Get_Materials())
        {
            auto* Resolved = ::Cast<UMaterialInterface>(InSetup._Materials.Get_ResolvedObject(Material.ToSoftObjectPath()));
            const auto IsResolved = ck::IsValid(Resolved);
            CK_ENSURE_IF_NOT(IsResolved, TEXT("RuntimeMeshDisplay [{}] could not resolve material [{}]"),
                InHandle, Material.ToSoftObjectPath().ToString())
            {
                Fail(ECk_RuntimeMeshDisplay_SetupFailure::MaterialLoadFailed);
                return;
            }

            ResolvedMaterials.Add(Resolved);
        }

        auto MeshCopy = InSetup._Geometry->Get_Mesh();
        const auto TangentsReady = runtimemesh::display::PrepareTangents(MeshCopy);
        CK_ENSURE_IF_NOT(TangentsReady, TEXT("RuntimeMeshDisplay [{}] could not build tangent overlays"), InHandle)
        {
            Fail(ECk_RuntimeMeshDisplay_SetupFailure::TangentGenerationFailed);
            return;
        }

        const auto PoolParams = FCk_ObjectPooling_PoolParams{}
            .Set_RecyclePolicy(ECk_ObjectPooling_RecyclePolicy::DestroyOnRelease);

        auto* Component = UCk_Utils_Object_UE::Request_CreateNewObject<UDynamicMeshComponent>(
            World, UDynamicMeshComponent::StaticClass(), nullptr, PoolParams, nullptr);

        const auto ComponentCreated = ck::IsValid(Component);
        CK_ENSURE_IF_NOT(ComponentCreated, TEXT("RuntimeMeshDisplay [{}] failed to create its DynamicMeshComponent"), InHandle)
        {
            Fail(ECk_RuntimeMeshDisplay_SetupFailure::ComponentCreationFailed);
            return;
        }

        constexpr auto DeferCollisionUpdates = true;
        constexpr auto EnableComplexAsSimple = false;
        constexpr auto ImmediateUpdate = false;
        constexpr auto GenerateOverlapEvents = false;
        constexpr auto SimulatePhysics = false;
        constexpr auto AffectsNavigation = false;
        constexpr auto IsEditable = false;

        // A display is a rendering adapter: collision and simulation belong to CkJolt, never to Chaos.
        Component->SetDeferredCollisionUpdatesEnabled(DeferCollisionUpdates, ImmediateUpdate);
        Component->SetComplexAsSimpleCollisionEnabled(EnableComplexAsSimple, ImmediateUpdate);
        Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Component->SetGenerateOverlapEvents(GenerateOverlapEvents);
        Component->SetSimulatePhysics(SimulatePhysics);
        Component->SetCanEverAffectNavigation(AffectsNavigation);
        Component->SetMobility(EComponentMobility::Movable);
        Component->SetTangentsType(EDynamicMeshComponentTangentsMode::ExternallyProvided);
        Component->SetMesh(MoveTemp(MeshCopy));
        Component->SetIsEditable(IsEditable);

        for (auto Index = 0; Index < ResolvedMaterials.Num(); ++Index)
        { Component->SetMaterial(Index, ResolvedMaterials[Index]); }

        const auto IsVisible = InSetup._Visuals.Get_Visibility() == ECk_EnableDisable::Enable;
        Component->SetVisibility(IsVisible);
        Component->SetHiddenInGame(NOT IsVisible);
        Component->SetCastShadow(InSetup._Visuals.Get_CastShadow() == ECk_EnableDisable::Enable);
        Component->SetWorldTransform(Transform);
        Component->RegisterComponentWithWorld(World);

        const auto IsRegistered = Component->IsRegistered();
        CK_ENSURE_IF_NOT(IsRegistered, TEXT("RuntimeMeshDisplay [{}] could not register its component with World [{}]"),
            InHandle, World->GetName())
        {
            UCk_Utils_Object_UE::TryReleaseToPool(Component);
            Component->DestroyComponent();
            Fail(ECk_RuntimeMeshDisplay_SetupFailure::ComponentCreationFailed);
            return;
        }

        if (World->HasBegunPlay() && NOT Component->HasBegunPlay())
        { Component->BeginPlay(); }

        InDisplay._Component = Component;
        InDisplay._SetupState = ECk_RuntimeMesh_SetupState::Ready;
        InHandle.Remove<MarkedDirtyBy>();
        InHandle.Remove<FFragment_RuntimeMeshDisplay_Setup>();
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_RuntimeMeshDisplay_UpdateTransform::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            const FFragment_RuntimeMeshDisplay& InDisplay,
            const FFragment_Transform& InTransform)
        -> void
    {
        if (InDisplay.Get_SetupState() != ECk_RuntimeMesh_SetupState::Ready)
        { return; }

        const auto& Transform = InTransform.Get_Transform();
        const auto IsTransformValid = Transform.IsValid();
        CK_ENSURE_IF_NOT(IsTransformValid, TEXT("RuntimeMeshDisplay [{}] received an invalid Transform"), InHandle)
        { return; }

        auto* Component = InDisplay.Get_Component().Get();
        const auto HasComponent = ck::IsValid(Component);
        CK_ENSURE_IF_NOT(HasComponent, TEXT("Ready RuntimeMeshDisplay [{}] no longer has its component"), InHandle)
        { return; }

        Component->SetWorldTransform(Transform);
    }

    // --------------------------------------------------------------------------------------------------------------------

    auto
        FProcessor_RuntimeMeshDisplay_EndPlay::
        ForEachEntity(
            TimeType InDeltaT,
            HandleType InHandle,
            FFragment_RuntimeMeshDisplay& InDisplay)
        -> void
    {
        Release(InHandle, InDisplay);
    }

    auto
        FProcessor_RuntimeMeshDisplay_EndPlay::
        Release(
            FCk_Handle InHandle,
            FFragment_RuntimeMeshDisplay& InDisplay)
        -> void
    {
        if (InDisplay._SetupState == ECk_RuntimeMesh_SetupState::Pending)
        {
            InDisplay._SetupState = ECk_RuntimeMesh_SetupState::Failed;
            InDisplay._SetupFailure = ECk_RuntimeMeshDisplay_SetupFailure::Cancelled;
        }

        InHandle.Try_Remove<FFragment_RuntimeMeshDisplay_Setup, ck::IsValid_Policy_IncludePendingKill>();
        InHandle.Try_Remove<FTag_RuntimeMeshDisplay_NeedsSetup, ck::IsValid_Policy_IncludePendingKill>();

        auto* Component = InDisplay._Component.Get();
        InDisplay._Component.Reset();

        // Unpin before destroying: destroy garbage-marks the component, which fails the release's validity check.
        if (ck::IsValid(Component))
        {
            UCk_Utils_Object_UE::TryReleaseToPool(Component);
            Component->DestroyComponent();
        }
    }
}

// --------------------------------------------------------------------------------------------------------------------
