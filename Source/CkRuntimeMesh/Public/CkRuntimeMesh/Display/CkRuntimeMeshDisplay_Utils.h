#pragma once

#include "CkRuntimeMesh/Display/CkRuntimeMeshDisplay_Fragment_Data.h"

#include "CkEcsExt/CkEcsExt_Utils.h"
#include "CkEcsExt/Transform/CkTransform_Fragment_Data.h"

#include "CkRuntimeMeshDisplay_Utils.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UCLASS(NotBlueprintable, Meta = (ScriptMixin = "FCk_Handle_RuntimeMeshDisplay"))
class CKRUNTIMEMESH_API UCk_Utils_RuntimeMeshDisplay_UE : public UCk_Utils_Ecs_Base_UE
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(UCk_Utils_RuntimeMeshDisplay_UE);
    CK_DEFINE_CPP_CASTCHECKED_TYPESAFE(FCk_Handle_RuntimeMeshDisplay);

public:
    friend class UCk_Utils_Ecs_Base_UE;

public:
    /**
     * Adds a display of a Ready geometry to InHandle, which must own a Transform. The display copies the
     * geometry reference at Add, so the geometry entity may be destroyed afterwards. Setup is deferred:
     * it resolves the materials and then reports Ready or a terminal Failed state (Get_SetupState).
     * An invalid owner, geometry or material slot list is rejected with an ensure and an invalid handle,
     * and nothing is added.
     */
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|RuntimeMeshDisplay",
              DisplayName = "[Ck][RuntimeMeshDisplay] Add")
    static FCk_Handle_RuntimeMeshDisplay
    Add(
        UPARAM(ref) FCk_Handle_Transform& InHandle,
        const FCk_RuntimeMeshDisplay_Spec& InSpec);

    static auto
    Has(
        const FCk_Handle& InHandle) -> bool;

private:
    UFUNCTION(BlueprintCallable,
              Category = "Ck|Utils|RuntimeMeshDisplay",
              DisplayName = "[Ck][RuntimeMeshDisplay] DoCast",
              meta = (ExpandEnumAsExecs = "OutResult"))
    static FCk_Handle_RuntimeMeshDisplay
    DoCast(
        UPARAM(ref) FCk_Handle& InHandle,
        ECk_SucceededFailed& OutResult);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMeshDisplay",
              DisplayName = "[Ck][RuntimeMeshDisplay] DoCastChecked",
              meta = (CompactNodeTitle = "<AsRuntimeMeshDisplay>", BlueprintAutocast))
    static FCk_Handle_RuntimeMeshDisplay
    DoCastChecked(
        FCk_Handle InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMeshDisplay",
              DisplayName = "[Ck] Get Invalid RuntimeMeshDisplay Handle",
              meta = (CompactNodeTitle = "INVALID_RuntimeMeshDisplayHandle", Keywords = "make"))
    static FCk_Handle_RuntimeMeshDisplay
    Get_InvalidHandle() { return {}; }

public:
    // Readable while the owner is being destroyed, so a Cancelled setup stays observable.
    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMeshDisplay",
              DisplayName = "[Ck][RuntimeMeshDisplay] Get Setup State")
    static ECk_RuntimeMesh_SetupState
    Get_SetupState(
        const FCk_Handle_RuntimeMeshDisplay& InHandle);

    UFUNCTION(BlueprintPure,
              Category = "Ck|Utils|RuntimeMeshDisplay",
              DisplayName = "[Ck][RuntimeMeshDisplay] Get Setup Failure")
    static ECk_RuntimeMeshDisplay_SetupFailure
    Get_SetupFailure(
        const FCk_Handle_RuntimeMeshDisplay& InHandle);
};
