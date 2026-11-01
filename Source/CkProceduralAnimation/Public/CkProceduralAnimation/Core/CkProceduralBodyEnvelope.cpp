#include "CkProceduralAnimation/Core/CkProceduralBodyEnvelope.h"

#include "CkJolt/Query/CkJoltQuery_Utils.h"
#include "CkJolt/Subsystem/CkJolt_Subsystem.h"

#include <Engine/World.h>
#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSettings.h>

// --------------------------------------------------------------------------------------------------------------------

namespace ck_procedural_body_envelope
{
    auto
        Get_IsFloatVector(
            const FVector& InVector)
        -> bool
    {
        const auto MaxFloat = static_cast<double>(TNumericLimits<float>::Max());
        return NOT InVector.ContainsNaN() && FMath::Abs(InVector.X) <= MaxFloat
            && FMath::Abs(InVector.Y) <= MaxFloat && FMath::Abs(InVector.Z) <= MaxFloat;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        Get_IsRigid(
            const FTransform& InTransform)
        -> bool
    {
        return NOT InTransform.ContainsNaN() && Get_IsFloatVector(InTransform.GetLocation())
            && InTransform.GetRotation().IsNormalized()
            && InTransform.GetScale3D().Equals(FVector::OneVector, 1.0e-4);
    }

    // ----------------------------------------------------------------------------------------------------------------

    struct FEnclosure
    {
        FCk_Jolt_ShapeDimensions _Shape;
        FRotator _Rotation;
    };

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_Enclosure(
            const FVector& InBoxExtents,
            const FQuat& InStart,
            const FQuat& InEnd)
        -> TOptional<FEnclosure>
    {
        const auto Angle = InStart.AngularDistance(InEnd);
        // A corner moves at most D = 2*r*sin(angle/4) from the midpoint orientation, perpendicular to the rotation axis.
        // Its displacement projected onto midpoint box axis i is therefore bounded by D*sqrt(1-kMid[i]^2).
        // UE quaternion A*B applies B then A: End*Start.Inverse() is the world-space relative rotation, whose axis is
        // unrotated by the midpoint to obtain kMid. In particular, pure yaw needs no vertical rotation inflation.
        const auto RotationInflation = 2.0 * InBoxExtents.Size() * FMath::Sin(Angle * 0.25);
        const auto Midpoint = FQuat::Slerp(InStart, InEnd, 0.5).GetNormalized();
        auto AxisInflation = FVector{RotationInflation};
        const auto ShortestEnd = InEnd.GetShortestArcWith(InStart);
        const auto Relative = (ShortestEnd * InStart.Inverse()).GetNormalized();
        const auto RelativeAxis = FVector{Relative.X, Relative.Y, Relative.Z};
        const auto AxisSizeSquared = RelativeAxis.SizeSquared();
        constexpr auto MinimumAxisSizeSquared = 1.0e-12;
        constexpr auto MinimumAngle = 1.0e-6;
        // A tiny or uncertain axis retains the previous scalar bound; a fallback axis must never remove padding.
        if (Angle > MinimumAngle && NOT RelativeAxis.ContainsNaN() && FMath::IsFinite(AxisSizeSquared)
            && AxisSizeSquared > MinimumAxisSizeSquared)
        {
            const auto MidAxis = Midpoint.UnrotateVector(RelativeAxis / FMath::Sqrt(AxisSizeSquared));
            if (NOT MidAxis.ContainsNaN() && MidAxis.IsNormalized())
            {
                AxisInflation = RotationInflation * FVector{
                    FMath::Sqrt(FMath::Max(0.0, 1.0 - MidAxis.X * MidAxis.X)),
                    FMath::Sqrt(FMath::Max(0.0, 1.0 - MidAxis.Y * MidAxis.Y)),
                    FMath::Sqrt(FMath::Max(0.0, 1.0 - MidAxis.Z * MidAxis.Z))};
            }
        }
        const auto SweptExtents = InBoxExtents + AxisInflation;
        if (NOT ck_procedural_body_envelope::Get_IsFloatVector(InBoxExtents)
            || NOT ck_procedural_body_envelope::Get_IsFloatVector(SweptExtents)
            || NOT FMath::IsFinite(RotationInflation)
            || static_cast<float>(SweptExtents.GetMin()) <= JPH::cDefaultConvexRadius)
        { return {}; }
        const auto Rotation = Midpoint.Rotator();
        return FEnclosure{FCk_Jolt_ShapeDimensions{ECk_Jolt_ShapeType::Box}.Set_HalfExtents(SweptExtents), Rotation};
    }
}

// --------------------------------------------------------------------------------------------------------------------

namespace ck
{
    auto
        Get_IsProceduralBodyEnvelopePathClear(
            const UObject* InWorldContextObject,
            const FCk_SurfaceMotion_AdaptiveHeight& InSettings,
            const FCk_Jolt_QueryFilter& InFilter,
            const FTransform& InStart,
            const FTransform& InEnd,
            int32& InOutQueries,
            int32 InQueryLimit,
            TOptional<FCk_Jolt_HitResult>* OutSweepHit,
            TOptional<FProceduralBodyEnvelopeOverlapObservation>* OutOverlap,
            EProceduralBodyEnvelopeEndpointValidation InEndpointValidation)
        -> bool
    {
        if (OutSweepHit != nullptr)
        { OutSweepHit->Reset(); }
        if (OutOverlap != nullptr)
        { OutOverlap->Reset(); }
        const auto& HalfExtents = InSettings.Get_BodyHalfExtents();
        const auto Margin = InSettings.Get_CollisionMargin();
        // Reject scalar overflow before vector addition, length, inflation or native float conversion.
        const auto Padding = static_cast<double>(Margin) + static_cast<double>(JPH::cDefaultConvexRadius);
        const auto MaxFloat = static_cast<double>(TNumericLimits<float>::Max());
        const auto PaddedX = HalfExtents.X + Padding;
        const auto PaddedY = HalfExtents.Y + Padding;
        const auto PaddedZ = HalfExtents.Z + Padding;
        if (NOT FMath::IsFinite(PaddedX) || NOT FMath::IsFinite(PaddedY) || NOT FMath::IsFinite(PaddedZ)
            || PaddedX > MaxFloat || PaddedY > MaxFloat || PaddedZ > MaxFloat
            || HalfExtents.ContainsNaN() || HalfExtents.X <= 0.0 || HalfExtents.Y <= 0.0 || HalfExtents.Z <= 0.0
            || NOT FMath::IsFinite(Margin) || Margin <= 0.0f
            || NOT ck_procedural_body_envelope::Get_IsRigid(InStart)
            || NOT ck_procedural_body_envelope::Get_IsRigid(InEnd)
            || NOT ck_procedural_body_envelope::Get_IsFloatVector(InEnd.GetLocation() - InStart.GetLocation())
            || (InFilter.Get_BodySelection() != ECk_Jolt_QueryBodySelection::Any
                && InFilter.Get_BodySelection() != ECk_Jolt_QueryBodySelection::StaticNonSensor)
            || InFilter.Get_Channel() >= ECC_MAX
            || InFilter.Get_MinResponse() > ECk_Jolt_PairInteraction::Block
            || (InEndpointValidation != EProceduralBodyEnvelopeEndpointValidation::ConservativeEnclosure
                && InEndpointValidation != EProceduralBodyEnvelopeEndpointValidation::ExactStationaryEndpoint)
            || InOutQueries < 0 || InQueryLimit < 0 || InOutQueries >= InQueryLimit)
        { return false; }

        if (ck::Is_NOT_Valid(InWorldContextObject))
        { return false; }
        auto* World = InWorldContextObject->GetWorld();
        if (ck::Is_NOT_Valid(World))
        { return false; }
        auto* Jolt = World->GetSubsystem<UCk_Jolt_Subsystem>();
        if (ck::Is_NOT_Valid(Jolt))
        { return false; }
        const auto Physics = Jolt->Get_PhysicsSystem().Pin();
        if (NOT Physics.IsValid() || Jolt->Get_IsClosing())
        { return false; }

        // CkJolt's vector conversion preserves UE centimetre values. Its default convex radius is therefore 0.05 cm
        // here. Padding each axis by that radius makes the rounded Jolt box enclose the authored sharp box plus margin.
        const auto BoxExtents = HalfExtents + FVector{Margin + JPH::cDefaultConvexRadius};
        const auto Enclosure = ck_procedural_body_envelope::TryGet_Enclosure(
            BoxExtents, InStart.GetRotation(), InEnd.GetRotation());
        if (NOT Enclosure.IsSet())
        { return false; }
        const auto& Shape = Enclosure->_Shape;
        const auto& Rotation = Enclosure->_Rotation;

        if (InOutQueries >= InQueryLimit)
        { return false; }
        ++InOutQueries;
        auto StartOverlaps = UCk_Utils_JoltQuery_UE::Get_Overlap(World, InStart.GetLocation(), Rotation, Shape, InFilter);
        if (NOT StartOverlaps.IsEmpty())
        {
            if (OutOverlap != nullptr)
            { *OutOverlap = FProceduralBodyEnvelopeOverlapObservation{
                EProceduralBodyEnvelopeOverlapKind::StartEnclosureOverlap, MoveTemp(StartOverlaps)}; }
            return false;
        }

        if (InOutQueries >= InQueryLimit)
        { return false; }
        ++InOutQueries;
        const auto Sweep = UCk_Utils_JoltQuery_UE::Get_ShapeCast(World, InStart.GetLocation(), InEnd.GetLocation(), Rotation,
            Shape, InFilter);
        if (Sweep.Get_HasHit())
        {
            if (OutSweepHit != nullptr)
            { *OutSweepHit = Sweep; }
            return false;
        }

        if (InOutQueries >= InQueryLimit)
        { return false; }
        ++InOutQueries;
        auto EndOverlaps = UCk_Utils_JoltQuery_UE::Get_Overlap(World, InEnd.GetLocation(), Rotation, Shape, InFilter);
        if (NOT EndOverlaps.IsEmpty())
        {
            if (OutOverlap != nullptr)
            { *OutOverlap = FProceduralBodyEnvelopeOverlapObservation{
                EProceduralBodyEnvelopeOverlapKind::EndEnclosureOverlap, MoveTemp(EndOverlaps)}; }
            return false;
        }
        if (InEndpointValidation == EProceduralBodyEnvelopeEndpointValidation::ExactStationaryEndpoint)
        {
            if (InOutQueries >= InQueryLimit)
            { return false; }
            const auto Endpoint = ck_procedural_body_envelope::TryGet_Enclosure(
                BoxExtents, InEnd.GetRotation(), InEnd.GetRotation());
            if (NOT Endpoint.IsSet())
            { return false; }
            ++InOutQueries;
            auto EndpointOverlaps = UCk_Utils_JoltQuery_UE::Get_Overlap(World, InEnd.GetLocation(),
                Endpoint->_Rotation, Endpoint->_Shape, InFilter);
            if (NOT EndpointOverlaps.IsEmpty())
            {
                if (OutOverlap != nullptr)
                { *OutOverlap = FProceduralBodyEnvelopeOverlapObservation{
                    EProceduralBodyEnvelopeOverlapKind::ExactEndpointOverlap, MoveTemp(EndpointOverlaps)}; }
                return false;
            }
        }
        return true;
    }
}

// --------------------------------------------------------------------------------------------------------------------
