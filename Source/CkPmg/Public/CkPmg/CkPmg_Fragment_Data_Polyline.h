#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkEcs/Handle/CkHandle.h"
#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkEcs/Request/CkRequest_Data.h"

#include "CkResourceLoader/CkResourceLoader_Fragment_Data.h"

#include <Materials/MaterialInterface.h>

#include "CkPmg_Fragment_Data_Polyline.generated.h"

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType, meta=(HasNativeMake, HasNativeBreak))
struct CKPMG_API FCk_Handle_Pmg_Polyline : public FCk_Handle_TypeSafe { GENERATED_BODY() CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_Pmg_Polyline); };
CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_Pmg_Polyline);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_Pmg_Polyline_Profile : uint8
{
    // A round tube of _Sides faces around the path
    Tube,
    // A flat strip twice _Radius wide, lying across the path's side vector (level when the path runs level) and
    // facing up when the path runs level
    Ribbon
};
CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Pmg_Polyline_Profile);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPMG_API FCk_Pmg_Polyline_Spec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Pmg_Polyline_Spec);

private:
    // Soft by design: a hard ref force-loads with the owning package and roots nothing anyway (GC
    // never walks the EnTT registry). The batch on the polyline fragment roots the material.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UMaterialInterface> _Material;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_Pmg_Polyline_Profile _Profile = ECk_Pmg_Polyline_Profile::Tube;

    // Tube radius, or a ribbon's half width, in cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = "0.01"))
    float _Radius = 1.0f;

    // Faces around the tube; a ribbon ignores it
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = "3", ClampMax = "16"))
    int32 _Sides = 6;

    // The most points a SetPoints may carry. The mesh's topology is built once for this many and never rebuilt.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = "2", ClampMax = "512"))
    int32 _MaxPoints = 64;

    // Length in cm of an arrowhead ending the path at its last point: a cone on a tube, a triangle on a ribbon.
    // 0 draws no head.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
    float _ArrowLength = 0.0f;

    // The arrowhead's radius (a ribbon's half width) at its base, in cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = "0.0"))
    float _ArrowRadius = 0.0f;

public:
    CK_PROPERTY(_Material);
    CK_PROPERTY(_Profile);
    CK_PROPERTY(_Radius);
    CK_PROPERTY(_Sides);
    CK_PROPERTY(_MaxPoints);
    CK_PROPERTY(_ArrowLength);
    CK_PROPERTY(_ArrowRadius);
};

// --------------------------------------------------------------------------------------------------------------------

// Replaces the drawn path. Fewer than two points, or a path of no length, draws nothing.
USTRUCT(BlueprintType)
struct CKPMG_API FCk_Request_Pmg_Polyline_SetPoints : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_Pmg_Polyline_SetPoints);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Pmg_Polyline_SetPoints);

private:
    // World space, in cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TArray<FVector> _WorldPoints;

public:
    CK_PROPERTY_GET(_WorldPoints);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_Pmg_Polyline_SetPoints, _WorldPoints);
};

// --------------------------------------------------------------------------------------------------------------------

// Re-tubes the last drawn points at the new radius.
USTRUCT(BlueprintType)
struct CKPMG_API FCk_Request_Pmg_Polyline_SetRadius : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_Pmg_Polyline_SetRadius);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Pmg_Polyline_SetRadius);

private:
    // Tube radius in cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = "0.01"))
    float _Radius = 1.0f;

public:
    CK_PROPERTY_GET(_Radius);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_Pmg_Polyline_SetRadius, _Radius);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKPMG_API FCk_Request_Pmg_Polyline_SetMaterial : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_Pmg_Polyline_SetMaterial);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_Pmg_Polyline_SetMaterial);

private:
    // Soft by design (see FCk_Pmg_Polyline_Spec::_Material).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UMaterialInterface> _Material;

    // Not reflected: kicked by the Utils enqueue boundary; a request built raw in BP/AS carries no
    // batch and the handler resolves the soft ref resident-or-null instead.
    FCk_ResourceLoader_RootedAssetBatch _PreloadBatch;

public:
    CK_PROPERTY_GET(_Material);
    // Hand-written (not CK_PROPERTY): the batch type is deliberately not AS-registered, and the
    // macro's AngelScript accessor registration would fail loudly at every editor boot.
    auto Get_PreloadBatch() const -> const FCk_ResourceLoader_RootedAssetBatch& { return _PreloadBatch; }
    auto Set_PreloadBatch(const FCk_ResourceLoader_RootedAssetBatch& InValue) -> ThisType& { _PreloadBatch = InValue; return *this; }

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_Pmg_Polyline_SetMaterial, _Material);
};

// --------------------------------------------------------------------------------------------------------------------
