#pragma once

#include "CkCore/Format/CkFormat.h"
#include "CkCore/Macros/CkMacros.h"
#include "CkEcs/Handle/CkHandle_TypeSafe.h"
#include "CkEcs/Request/CkRequest_Data.h"

#include <CoreMinimal.h>
#include <Engine/StaticMesh.h>

#include "CkRuntimeMesh_Fragment_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_RuntimeMesh_SetupState : uint8
{
    Pending,
    Ready,
    Failed
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_RuntimeMesh_SetupState);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_RuntimeMesh_SetupFailure : uint8
{
    None,
    InvalidSpec,
    InvalidExecutionContext,
    LoadFailed,
    MissingRenderData,
    InvalidLOD,
    CpuDataUnavailable,
    LimitExceeded,
    InvalidIndexOrSection,
    InvalidMaterial,
    InvalidAttribute,
    AmbiguousSeam,
    InvalidTopology,
    SelfIntersection,
    NonPositiveVolume,
    ConversionFailed
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_RuntimeMesh_SetupFailure);

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_RuntimeMesh_SliceOutcome : uint8
{
    Succeeded,
    NoIntersection,
    TouchingOnly,
    RejectedTooSmall,
    RejectedLimit,
    RejectedTopology,
    NotReady,
    InvalidRequest,
    FailedCancelled,
    InternalFailure
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_RuntimeMesh_SliceOutcome);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType, meta = (HasNativeMake, HasNativeBreak))
struct CKRUNTIMEMESH_API FCk_Handle_RuntimeMesh : public FCk_Handle_TypeSafe
{
    GENERATED_BODY()
    CK_GENERATED_BODY_HANDLE_TYPESAFE(FCk_Handle_RuntimeMesh);
};

CK_DEFINE_CUSTOM_ISVALID_AND_FORMATTER_HANDLE_TYPESAFE(FCk_Handle_RuntimeMesh);

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMesh_ImportLimits
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMesh_ImportLimits);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _LODIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _SeamToleranceCm = 0.0001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _IntersectionToleranceCm = 0.0000001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _MinimumTriangleAreaCm2 = 0.00000001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _MinimumVolumeCm3 = 0.000001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _MaximumVertices = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _MaximumTriangles = 4096;

public:
    CK_PROPERTY(_LODIndex);
    CK_PROPERTY(_SeamToleranceCm);
    CK_PROPERTY(_IntersectionToleranceCm);
    CK_PROPERTY(_MinimumTriangleAreaCm2);
    CK_PROPERTY(_MinimumVolumeCm3);
    CK_PROPERTY(_MaximumVertices);
    CK_PROPERTY(_MaximumTriangles);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMesh_Spec
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMesh_Spec);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    TSoftObjectPtr<UStaticMesh> _SourceMesh;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_RuntimeMesh_ImportLimits _Import;

public:
    CK_PROPERTY(_SourceMesh);
    CK_PROPERTY(_Import);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_RuntimeMesh_Spec, _SourceMesh);

public:
    /** A source is named and every import limit is inside the module's finite ceilings. */
    auto Get_IsValid() const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMesh_PlaneLocal
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMesh_PlaneLocal);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FVector _PositionCm = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FVector _Normal = FVector::UpVector;

    // Fixes the cap UV orientation; orthogonalized against the normal once at execution.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FVector _Tangent = FVector::ForwardVector;

public:
    CK_PROPERTY(_PositionCm);
    CK_PROPERTY(_Normal);
    CK_PROPERTY(_Tangent);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMesh_Cap
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMesh_Cap);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _MaterialID = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FLinearColor _Color = FLinearColor::White;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _CmPerUVUnit = 10.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FVector2D _UVOffset = FVector2D::ZeroVector;

public:
    CK_PROPERTY(_MaterialID);
    CK_PROPERTY(_Color);
    CK_PROPERTY(_CmPerUVUnit);
    CK_PROPERTY(_UVOffset);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMesh_CutLimits
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMesh_CutLimits);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _MinimumOutputVolumeCm3 = 0.001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _MinimumNormalExtentCm = 0.001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _PlaneToleranceCm = 0.00001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _AbsoluteVolumeToleranceCm3 = 0.01;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    double _RelativeVolumeTolerance = 0.0001;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _MaximumVertices = 2048;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    int32 _MaximumTriangles = 4096;

public:
    CK_PROPERTY(_MinimumOutputVolumeCm3);
    CK_PROPERTY(_MinimumNormalExtentCm);
    CK_PROPERTY(_PlaneToleranceCm);
    CK_PROPERTY(_AbsoluteVolumeToleranceCm3);
    CK_PROPERTY(_RelativeVolumeTolerance);
    CK_PROPERTY(_MaximumVertices);
    CK_PROPERTY(_MaximumTriangles);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_Request_RuntimeMesh_Slice : public FCk_Request_Base
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Request_RuntimeMesh_Slice);
    CK_REQUEST_DEFINE_DEBUG_NAME(FCk_Request_RuntimeMesh_Slice);

private:
    // Caller-chosen correlation ID; must be unique among the source's in-flight slices.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FGuid _OperationID;

    // Owns both result entities, so results outlive the source. Must not be the source or its descendant.
    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_Handle _ResultOwner;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_RuntimeMesh_PlaneLocal _Plane;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_RuntimeMesh_Cap _Cap;

    UPROPERTY(EditAnywhere, BlueprintReadWrite,
              meta = (AllowPrivateAccess = true))
    FCk_RuntimeMesh_CutLimits _Limits;

public:
    CK_PROPERTY(_OperationID);
    CK_PROPERTY(_ResultOwner);
    CK_PROPERTY(_Plane);
    CK_PROPERTY(_Cap);
    CK_PROPERTY(_Limits);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Request_RuntimeMesh_Slice, _OperationID, _ResultOwner, _Plane);

public:
    /** The operation ID is set and the plane, cap and limits are finite and inside the module's ceilings. The result
     *  owner's lifetime and registry are checked by Request_Slice, which has the source to compare against. */
    auto Get_IsValid() const -> bool;
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMesh_Metrics
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMesh_Metrics);

private:
    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    double _VolumeCm3 = 0.0;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FVector _CentroidCm = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FVector _BoundsMinCm = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FVector _BoundsMaxCm = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _TriangleCount = 0;

public:
    CK_PROPERTY_GET(_VolumeCm3);
    CK_PROPERTY_GET(_CentroidCm);
    CK_PROPERTY_GET(_BoundsMinCm);
    CK_PROPERTY_GET(_BoundsMaxCm);
    CK_PROPERTY_GET(_TriangleCount);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_RuntimeMesh_Metrics,
        _VolumeCm3, _CentroidCm, _BoundsMinCm, _BoundsMaxCm, _TriangleCount);
};

// --------------------------------------------------------------------------------------------------------------------

/** Terminal payload of one slice. Positive/negative follow the request plane's normal; both handles are valid
 *  only when the outcome is Succeeded, and the consumer owns their lifetime from then on. */
USTRUCT(BlueprintType)
struct CKRUNTIMEMESH_API FCk_RuntimeMesh_SliceResult
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_RuntimeMesh_SliceResult);

private:
    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FGuid _OperationID;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    ECk_RuntimeMesh_SliceOutcome _Outcome = ECk_RuntimeMesh_SliceOutcome::InvalidRequest;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_Handle_RuntimeMesh _Positive;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_Handle_RuntimeMesh _Negative;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_RuntimeMesh_Metrics _PositiveMetrics;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    FCk_RuntimeMesh_Metrics _NegativeMetrics;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _PositiveCapTriangles = 0;

    UPROPERTY(BlueprintReadOnly,
              meta = (AllowPrivateAccess = true))
    int32 _NegativeCapTriangles = 0;

public:
    CK_PROPERTY_GET(_OperationID);
    CK_PROPERTY_GET(_Outcome);
    CK_PROPERTY_GET(_Positive);
    CK_PROPERTY_GET(_Negative);
    CK_PROPERTY_GET(_PositiveMetrics);
    CK_PROPERTY_GET(_NegativeMetrics);
    CK_PROPERTY_GET(_PositiveCapTriangles);
    CK_PROPERTY_GET(_NegativeCapTriangles);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_RuntimeMesh_SliceResult,
        _OperationID, _Outcome, _Positive, _Negative, _PositiveMetrics,
        _NegativeMetrics, _PositiveCapTriangles, _NegativeCapTriangles);
};

// --------------------------------------------------------------------------------------------------------------------

DECLARE_DYNAMIC_DELEGATE_OneParam(
    FCk_Delegate_RuntimeMesh_OnSliceResolved,
    FCk_RuntimeMesh_SliceResult, InResult);

// --------------------------------------------------------------------------------------------------------------------
