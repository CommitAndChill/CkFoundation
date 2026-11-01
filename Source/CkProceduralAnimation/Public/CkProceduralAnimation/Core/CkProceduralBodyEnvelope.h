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
                EProceduralBodyEnvelopeEndpointValidation::ConservativeEnclosure)
        -> bool;
}

// --------------------------------------------------------------------------------------------------------------------
