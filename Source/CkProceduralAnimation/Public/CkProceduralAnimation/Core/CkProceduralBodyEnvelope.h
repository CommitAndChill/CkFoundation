#pragma once

#include "CkProceduralAnimation/SurfaceMotion/CkSurfaceMotion_Fragment_Data.h"

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    enum class EProceduralBodyEnvelopeEndpointValidation : uint8
    {
        ConservativeEnclosure,
        ExactStationaryEndpoint
    };

    enum class EProceduralBodyEnvelopeOverlapKind : uint8
    {
        StartEnclosureOverlap,
        EndEnclosureOverlap,
        ExactEndpointOverlap
    };

    // Existing native contacts from a rejected enclosure, not support or sweep authority.
    struct CKPROCEDURALANIMATION_API FProceduralBodyEnvelopeOverlapObservation
    {
        CK_GENERATED_BODY(FProceduralBodyEnvelopeOverlapObservation);

    private:
        EProceduralBodyEnvelopeOverlapKind _Kind = EProceduralBodyEnvelopeOverlapKind::StartEnclosureOverlap;
        TArray<FCk_Jolt_HitResult> _Hits;

    public:
        CK_PROPERTY_GET(_Kind);
        CK_PROPERTY_GET(_Hits);

    public:
        CK_DEFINE_CONSTRUCTORS(FProceduralBodyEnvelopeOverlapObservation, _Kind, _Hits);
    };

    // A copy of the enclosure geometry actually selected by a path proof, not collision or admission authority.
    // Numeric construction supports pure validation; production clipping must use the same-call output below.
    struct CKPROCEDURALANIMATION_API FProceduralBodyEnvelopeEnclosureObservation
    {
        CK_GENERATED_BODY(FProceduralBodyEnvelopeEnclosureObservation);

    private:
        bool _HasObservation = false;
        FTransform _Start = FTransform::Identity;
        FTransform _End = FTransform::Identity;
        FVector _HalfExtents = FVector::ZeroVector;
        FRotator _Rotation = FRotator::ZeroRotator;
        FVector _NativeHalfExtents = FVector::ZeroVector;
        FQuat _NativeRotation = FQuat::Identity;
        double _WorldUpRadius = 0.0;

    public:
        CK_PROPERTY_GET(_HasObservation);
        CK_PROPERTY_GET(_Start);
        CK_PROPERTY_GET(_End);
        CK_PROPERTY_GET(_HalfExtents);
        CK_PROPERTY_GET(_Rotation);
        CK_PROPERTY_GET(_NativeHalfExtents);
        CK_PROPERTY_GET(_NativeRotation);
        CK_PROPERTY_GET(_WorldUpRadius);

        FProceduralBodyEnvelopeEnclosureObservation() = default;

        FProceduralBodyEnvelopeEnclosureObservation(
            const FTransform& InStart,
            const FTransform& InEnd,
            const FVector& InHalfExtents,
            const FRotator& InRotation);
    };

    // Constructs one represented world-Up body placement only. The caller must separately validate fresh hit
    // attribution to its world and all native/retained-contact/envelope proofs before publishing this proposal.
    CKPROCEDURALANIMATION_API auto
        TryGet_ProceduralBodyPlaneClip(
            const FProceduralBodyEnvelopeEnclosureObservation& InEnclosure,
            const FTransform& InCurrent,
            const FTransform& InRejectedPartial,
            const FCk_Jolt_HitResult& InHit,
            const FVector& InUp)
        -> TOptional<FTransform>;

    // Proves the rigid box clear for the complete shortest-arc rotation and linear translation path. Rotation is
    // conservatively enclosed by a fixed midpoint-oriented box; touching or overlapping geometry rejects the path.
    // Every native overlap or sweep consumes the shared query budget before it runs. Invalid input or exhausted budget
    // rejects without treating an unavailable physics world as empty geometry. ExactStationaryEndpoint adds a fourth
    // overlap using the endpoint's stationary enclosure to catch native numerical disagreement with the swept
    // enclosure. The default retains the three-query conservative enclosure path.
    CKPROCEDURALANIMATION_API auto
        Get_IsProceduralBodyEnvelopePathClear(
            const UObject* InWorldContextObject,
            const FCk_SurfaceMotion_AdaptiveHeight& InSettings,
            const FCk_Jolt_QueryFilter& InFilter,
            const FTransform& InStart,
            const FTransform& InEnd,
            int32& InOutQueries,
            int32 InQueryLimit,
            TOptional<FCk_Jolt_HitResult>* OutSweepHit = nullptr,
            TOptional<FProceduralBodyEnvelopeOverlapObservation>* OutOverlap = nullptr,
            EProceduralBodyEnvelopeEndpointValidation InEndpointValidation =
                EProceduralBodyEnvelopeEndpointValidation::ConservativeEnclosure,
            FProceduralBodyEnvelopeEnclosureObservation* OutEnclosure = nullptr)
        -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
