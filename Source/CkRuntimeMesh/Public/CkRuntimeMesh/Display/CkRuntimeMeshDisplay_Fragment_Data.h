#pragma once

#include "CkRuntimeMesh/CkRuntimeMesh_Fragment_Data.h"

#include "CkCore/Enums/CkEnums.h"
#include "CkGraphics/CkGraphics_Common.h"

#include "CkRuntimeMeshDisplay_Fragment_Data.generated.h"

class UMaterialInterface;

// --------------------------------------------------------------------------------------------------------------------

namespace ck::runtimemesh::display
{
    constexpr auto MaxMaterialSlots = int32{256};
}

// --------------------------------------------------------------------------------------------------------------------

// Terminal results that can only be known once setup runs. Spec errors never reach this enum:
// Add rejects them with an ensure and an invalid handle, and adds nothing.
UENUM(BlueprintType)
enum class ECk_RuntimeMeshDisplay_SetupFailure : uint8
{
    None,
    MissingTransform,
    InvalidTransform,
    MaterialLoadFailed,
    UnsupportedWorld,
    ComponentCreationFailed,
    TangentGenerationFailed,
    Cancelled
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_RuntimeMeshDisplay_SetupFailure);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKRUNTIMEMESH_API FCk_Handle_RuntimeMeshDisplay : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_RuntimeMeshDisplay);
};

CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_RuntimeMeshDisplay);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMeshDisplay_Visuals
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMeshDisplay_Visuals);

private:
    // One slot per geometry material ID; every slot must name a material.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    TArray<TSoftObjectPtr<UMaterialInterface>> _Materials;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _Visibility = ECk_EnableDisable::Enable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    ECk_EnableDisable _CastShadow = ECk_EnableDisable::Enable;

public:
    CK_PROPERTY(_Materials);
    CK_PROPERTY(_Visibility);
    CK_PROPERTY(_CastShadow);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMeshDisplay_Spec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMeshDisplay_Spec);

private:
    // Read once at Add: the display keeps its own reference to the immutable geometry, so the
    // geometry entity may be destroyed afterwards.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_Handle_RuntimeMesh _Geometry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_RuntimeMeshDisplay_Visuals _Visuals;

public:
    CK_PROPERTY(_Geometry);
    CK_PROPERTY(_Visuals);

public:
    /** The geometry handle is set and every one of the 1..MaxMaterialSlots material slots names a material. Whether the
     *  geometry is Ready in the owner's registry and covers every triangle material ID is checked by Add. */
    auto Get_IsValid() const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_CustomPrimitiveData _Data;

public:
    CK_PROPERTY_GET(_Data);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_RuntimeMeshDisplay_SetCustomPrimitiveData, _Data);

public:
    /** The value carries at least one float and every float it carries lands inside the engine's
     *  FCustomPrimitiveData::NumCustomPrimitiveDataFloats slots, starting at a non-negative index. */
    auto Get_IsValid() const -> bool;
};
