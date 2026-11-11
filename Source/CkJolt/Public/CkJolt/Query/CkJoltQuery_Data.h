#pragma once

#include "CkCore/Macros/CkMacros.h"
#include "CkCore/Time/CkTime.h"

#include "CkEcs/Handle/CkHandle.h"

#include "CkJolt/CollisionLayers/CkJoltCollisionLayer_Data.h"

#include "CkJoltQuery_Data.generated.h"

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_Jolt_ShapeType : uint8
{
    Box,
    Sphere,
    Capsule,
    Cylinder
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Jolt_ShapeType);

// --------------------------------------------------------------------------------------------------------------------

/// CkJolt-owned shape description (deliberately NOT CkShapes fragments — CkJolt sits below the
/// CkShapes feature). Box uses _HalfExtents; Sphere uses _Radius; Capsule/Cylinder use _Radius +
/// _HalfHeight (capsule half-height = cylinder section only, matching UE's FKSphylElem).
USTRUCT(BlueprintType)
struct CKJOLT_API FCk_Jolt_ShapeDimensions
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Jolt_ShapeDimensions);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_Jolt_ShapeType _ShapeType = ECk_Jolt_ShapeType::Box;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true,
              EditCondition = "_ShapeType == ECk_Jolt_ShapeType::Box"))
    FVector _HalfExtents = FVector{50.0};

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = 0.1,
              EditCondition = "_ShapeType != ECk_Jolt_ShapeType::Box"))
    float _Radius = 50.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = 0.1,
              EditCondition = "_ShapeType == ECk_Jolt_ShapeType::Capsule || _ShapeType == ECk_Jolt_ShapeType::Cylinder"))
    float _HalfHeight = 50.0f;

public:
    CK_PROPERTY(_ShapeType);
    CK_PROPERTY(_HalfExtents);
    CK_PROPERTY(_Radius);
    CK_PROPERTY(_HalfHeight);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Jolt_ShapeDimensions, _ShapeType);
};

// --------------------------------------------------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ECk_Jolt_QueryBodySelection : uint8
{
    Any,
    StaticNonSensor
};

CK_DEFINE_CUSTOM_FORMATTER_ENUM(ECk_Jolt_QueryBodySelection);

// --------------------------------------------------------------------------------------------------------------------

/// UE-channel-equivalent query filtering: a body is considered when its authored response to
/// _Channel is at least _MinResponse (Block = trace semantics, Overlap = overlap semantics).
USTRUCT(BlueprintType)
struct CKJOLT_API FCk_Jolt_QueryFilter
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Jolt_QueryFilter);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TEnumAsByte<ECollisionChannel> _Channel = ECC_Visibility;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_Jolt_PairInteraction _MinResponse = ECk_Jolt_PairInteraction::Block;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    ECk_Jolt_QueryBodySelection _BodySelection = ECk_Jolt_QueryBodySelection::Any;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _CollideWithBackFacesConvex = false;

public:
    CK_PROPERTY(_Channel);
    CK_PROPERTY(_MinResponse);
    CK_PROPERTY(_BodySelection);
    CK_PROPERTY(_CollideWithBackFacesConvex);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKJOLT_API FCk_Jolt_HitResult
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Jolt_HitResult);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    bool _HasHit = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FVector _Position = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FVector _Normal = FVector::UpVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    float _Fraction = 0.0f;

    // Valid when the hit body belongs to a live entity — probes, dynamic JoltBodies, AND baked-static source
    // actors (each baked body carries its source actor's attribution entity id as Jolt user-data).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Handle _Entity;

public:
    CK_PROPERTY(_HasHit);
    CK_PROPERTY(_Position);
    CK_PROPERTY(_Normal);
    CK_PROPERTY(_Fraction);
    CK_PROPERTY(_Entity);
};

// --------------------------------------------------------------------------------------------------------------------

/// How a dynamic body flies between contacts: the three body settings Jolt's integrator reads each step.
USTRUCT(BlueprintType)
struct CKJOLT_API FCk_Jolt_BallisticFlight
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Jolt_BallisticFlight);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = 0.0))
    float _LinearDamping = 0.05f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    float _GravityFactor = 1.0f;

    // cm/s
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = 0.0))
    float _MaxLinearVelocity = 50000.0f;

public:
    CK_PROPERTY(_LinearDamping);
    CK_PROPERTY(_GravityFactor);
    CK_PROPERTY(_MaxLinearVelocity);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKJOLT_API FCk_Jolt_BallisticCast_Params
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Jolt_BallisticCast_Params);

private:
    // World location of the swept shape's centre at launch, in cm
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FVector _StartLocation = FVector::ZeroVector;

    // cm/s
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FVector _StartVelocity = FVector::ZeroVector;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Jolt_BallisticFlight _Flight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Jolt_ShapeDimensions _Shape = FCk_Jolt_ShapeDimensions{ECk_Jolt_ShapeType::Sphere};

    // The orientation the swept shape keeps for the whole flight
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FRotator _ShapeRotation = FRotator::ZeroRotator;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Jolt_QueryFilter _Filter;

    // The flight stops here when nothing was hit
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Time _MaxDuration = FCk_Time{3.0f};

    // Fixed simulation steps per path segment: one path point and one shape sweep per segment
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true, ClampMin = 1))
    int32 _StepsPerSegment = 2;

    // Bodies of these entities never stop the flight (the launcher, what it carries)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TArray<FCk_Handle> _IgnoredEntities;

public:
    CK_PROPERTY(_StartLocation);
    CK_PROPERTY(_StartVelocity);
    CK_PROPERTY(_Flight);
    CK_PROPERTY(_Shape);
    CK_PROPERTY(_ShapeRotation);
    CK_PROPERTY(_Filter);
    CK_PROPERTY(_MaxDuration);
    CK_PROPERTY(_StepsPerSegment);
    CK_PROPERTY(_IgnoredEntities);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Jolt_BallisticCast_Params, _StartLocation, _StartVelocity);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKJOLT_API FCk_Jolt_BallisticCast_Point
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Jolt_BallisticCast_Point);

private:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FVector _Location = FVector::ZeroVector;

    // cm/s
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FVector _Velocity = FVector::ZeroVector;

    // Since launch
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Time _Time;

public:
    CK_PROPERTY(_Location);
    CK_PROPERTY(_Velocity);
    CK_PROPERTY(_Time);

public:
    CK_DEFINE_CONSTRUCTORS(FCk_Jolt_BallisticCast_Point, _Location, _Velocity, _Time);
};

// --------------------------------------------------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct CKJOLT_API FCk_Jolt_BallisticCast_Result
{
    GENERATED_BODY()

public:
    CK_GENERATED_BODY(FCk_Jolt_BallisticCast_Result);

private:
    // The shape centre's path. [0] is the start; the last point is the centre at the first hit, or where the
    // flight was at _MaxDuration. Empty when the query could not run (no Jolt world, rejected params).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    TArray<FCk_Jolt_BallisticCast_Point> _Path;

    // _HasHit is false when the flight ran to _MaxDuration. _Position is the contact point on the hit body,
    // _Fraction is along the segment that hit.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (AllowPrivateAccess = true))
    FCk_Jolt_HitResult _Hit;

public:
    CK_PROPERTY(_Path);
    CK_PROPERTY(_Hit);
};

// --------------------------------------------------------------------------------------------------------------------
