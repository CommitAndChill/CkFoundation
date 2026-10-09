#include "CkRuntimeMesh/Display/CkRuntimeMeshDisplay_Utils.h"

#include "CkRuntimeMesh/CkRuntimeMesh_Fragment.h"
#include "CkRuntimeMesh/CkRuntimeMesh_Utils.h"
#include "CkRuntimeMesh/Display/CkRuntimeMeshDisplay_Fragment.h"

#include "CkCore/Algorithms/CkAlgorithms.h"
#include "CkCore/Ensure/CkEnsure.h"
#include "CkCore/Format/CkFormat.h"
#include "CkCore/Validation/CkIsValid.h"
#include "CkEcs/EntityLifetime/CkEntityLifetime_Utils.h"

#include "DynamicMesh/DynamicMeshAttributeSet.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck_runtime_mesh_display_utils
{
    // Empty when a valid Spec is admissible on this owner. Every check reads only what the checks before it proved
    // readable; the owner's Transform is guaranteed by its handle type.
    auto
        DoGet_AddRejection(
            const FCk_Handle_Transform& InOwner,
            const FCk_RuntimeMeshDisplay_Spec& InSpec)
        -> FString
    {
        const auto IsLiveOwner = ck::IsValid(InOwner) &&
            UCk_Utils_EntityLifetime_UE::Get_CanCreateEntity(InOwner) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(InOwner, ECk_EntityLifetime_DestructionPhase::BeginDestroy);
        if (NOT IsLiveOwner)
        { return TEXT("the owner is not a live composition owner"); }

        const auto* World = UCk_Utils_EntityLifetime_UE::Get_WorldForEntity(InOwner);
        if (ck::Is_NOT_Valid(World) || World->IsBeingCleanedUp())
        { return TEXT("the owner's world is missing or being cleaned up"); }

        if (UCk_Utils_RuntimeMeshDisplay_UE::Has(InOwner))
        { return TEXT("the owner already has a RuntimeMeshDisplay"); }

        const auto& Geometry = InSpec.Get_Geometry();
        const auto IsLiveGeometry = ck::IsValid(Geometry) &&
            NOT UCk_Utils_EntityLifetime_UE::Get_IsPendingDestroy(Geometry, ECk_EntityLifetime_DestructionPhase::BeginDestroy) &&
            Geometry.Get_RegistryView().Get_RegistryHandle() == InOwner.Get_RegistryView().Get_RegistryHandle();
        if (NOT IsLiveGeometry)
        { return TEXT("the geometry is not a live RuntimeMesh in the owner's registry"); }

        const auto& GeometryFragment = Geometry.Get<ck::FFragment_RuntimeMesh>();
        if (GeometryFragment.Get_SetupState() != ECk_RuntimeMesh_SetupState::Ready ||
            ck::Is_NOT_Valid(GeometryFragment.Get_Geometry()))
        { return TEXT("the geometry is not Ready"); }

        const auto& Materials = InSpec.Get_Visuals().Get_Materials();
        const auto& Mesh = GeometryFragment.Get_Geometry()->Get_Mesh();
        const auto* MaterialIDs = Mesh.HasAttributes() ? Mesh.Attributes()->GetMaterialID() : nullptr;
        if (ck::Is_NOT_Valid(MaterialIDs, ck::IsValid_Policy_NullptrOnly{}))
        { return TEXT("the geometry carries no material IDs"); }

        for (const auto TriangleID : Mesh.TriangleIndicesItr())
        {
            const auto MaterialID = MaterialIDs->GetValue(TriangleID);
            if (NOT Materials.IsValidIndex(MaterialID))
            {
                return ck::Format_UE(TEXT("triangle {} uses material ID {} but only {} slots are given"),
                    TriangleID, MaterialID, Materials.Num());
            }
        }

        return {};
    }
}

// --------------------------------------------------------------------------------------------------------------------

CK_DEFINE_HAS_CAST_CONV_HANDLE_TYPESAFE(
    UCk_Utils_RuntimeMeshDisplay_UE, FCk_Handle_RuntimeMeshDisplay, ck::FFragment_RuntimeMeshDisplay);

// --------------------------------------------------------------------------------------------------------------------

auto
    UCk_Utils_RuntimeMeshDisplay_UE::
    Add(
        FCk_Handle_Transform& InHandle,
        const FCk_RuntimeMeshDisplay_Spec& InSpec)
    -> FCk_Handle_RuntimeMeshDisplay
{
    const auto IsSpecValid = InSpec.Get_IsValid();
    CK_ENSURE_IF_NOT(IsSpecValid,
        TEXT("RuntimeMeshDisplay Add rejected the Spec for owner [{}]: geometry unset or material slots outside 1..{} / empty"),
        InHandle, ck::runtimemesh::display::MaxMaterialSlots)
    { return {}; }

    const auto Rejection = ck_runtime_mesh_display_utils::DoGet_AddRejection(InHandle, InSpec);
    const auto IsAdmissible = Rejection.IsEmpty();
    CK_ENSURE_IF_NOT(IsAdmissible, TEXT("RuntimeMeshDisplay Add rejected for owner [{}]: {}"), InHandle, Rejection)
    { return {}; }

    const auto& Geometry = InSpec.Get_Geometry().Get<ck::FFragment_RuntimeMesh>().Get_Geometry();
    InHandle.Add<ck::FFragment_RuntimeMeshDisplay>();
    InHandle.Add<ck::FFragment_RuntimeMeshDisplay_Setup>(Geometry, InSpec.Get_Visuals());
    InHandle.Add<ck::FTag_RuntimeMeshDisplay_NeedsSetup>();

    return CastChecked(InHandle);
}

auto
    UCk_Utils_RuntimeMeshDisplay_UE::
    Get_SetupState(
        const FCk_Handle_RuntimeMeshDisplay& InHandle)
    -> ECk_RuntimeMesh_SetupState
{
    const auto IsHandleValid = ck::IsValid(InHandle);
    CK_ENSURE_IF_NOT(IsHandleValid, TEXT("Get_SetupState received an invalid RuntimeMeshDisplay handle [{}]"), InHandle)
    { return ECk_RuntimeMesh_SetupState::Failed; }

    return InHandle.Get<ck::FFragment_RuntimeMeshDisplay>().Get_SetupState();
}

auto
    UCk_Utils_RuntimeMeshDisplay_UE::
    Get_SetupFailure(
        const FCk_Handle_RuntimeMeshDisplay& InHandle)
    -> ECk_RuntimeMeshDisplay_SetupFailure
{
    const auto IsHandleValid = ck::IsValid(InHandle);
    CK_ENSURE_IF_NOT(IsHandleValid, TEXT("Get_SetupFailure received an invalid RuntimeMeshDisplay handle [{}]"), InHandle)
    { return ECk_RuntimeMeshDisplay_SetupFailure::None; }

    return InHandle.Get<ck::FFragment_RuntimeMeshDisplay>().Get_SetupFailure();
}

// --------------------------------------------------------------------------------------------------------------------
