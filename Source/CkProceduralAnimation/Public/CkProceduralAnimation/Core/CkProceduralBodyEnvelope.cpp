#include "CkProceduralAnimation/Core/CkProceduralBodyEnvelope.h"

#include "CkJolt/Query/CkJoltQuery_Utils.h"
#include "CkJolt/CkJolt_Utils.h"
#include "CkJolt/Subsystem/CkJolt_Subsystem.h"

#include <Engine/World.h>
#include <limits>
#include <Jolt/Math/Mat44.h>
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
            EProceduralBodyEnvelopeEndpointValidation InEndpointValidation,
            FProceduralBodyEnvelopeEnclosureObservation* OutEnclosure)
        -> bool
    {
        if (OutEnclosure != nullptr)
        { *OutEnclosure = FProceduralBodyEnvelopeEnclosureObservation{}; }
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
        if (OutEnclosure != nullptr)
        { *OutEnclosure = FProceduralBodyEnvelopeEnclosureObservation{InStart, InEnd, Shape.Get_HalfExtents(), Rotation}; }

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

namespace ck
{
    FProceduralBodyEnvelopeEnclosureObservation::
        FProceduralBodyEnvelopeEnclosureObservation(
            const FTransform& InStart,
            const FTransform& InEnd,
            const FVector& InHalfExtents,
            const FRotator& InRotation)
    {
        if (NOT ck_procedural_body_envelope::Get_IsRigid(InStart)
            || NOT ck_procedural_body_envelope::Get_IsRigid(InEnd)
            || NOT ck_procedural_body_envelope::Get_IsFloatVector(InHalfExtents)
            || InHalfExtents.GetMin() <= 0.0 || InRotation.ContainsNaN()
            || FMath::Abs(InRotation.Pitch) > 360.0 || FMath::Abs(InRotation.Yaw) > 360.0
            || FMath::Abs(InRotation.Roll) > 360.0)
        { return; }
        // Match CreateShape_FromDimensions and the query transform's native float conversions exactly.
        const auto NativeExtents = ck::jolt::Conv(InHalfExtents);
        if (NativeExtents.GetX() <= JPH::cDefaultConvexRadius
            || NativeExtents.GetY() <= JPH::cDefaultConvexRadius
            || NativeExtents.GetZ() <= JPH::cDefaultConvexRadius)
        { return; }
        const auto NativeQuaternion = ck::jolt::Conv(InRotation.Quaternion());
        if (NOT NativeQuaternion.IsNormalized())
        { return; }
        const auto NativeMatrix = JPH::Mat44::sRotation(NativeQuaternion);
        const auto Radius = FMath::Abs(static_cast<double>(NativeMatrix.GetColumn3(0).GetZ())) * NativeExtents.GetX()
            + FMath::Abs(static_cast<double>(NativeMatrix.GetColumn3(1).GetZ())) * NativeExtents.GetY()
            + FMath::Abs(static_cast<double>(NativeMatrix.GetColumn3(2).GetZ())) * NativeExtents.GetZ();
        // A rounded box is inside its sharp half-extents box. The coefficients above are the actual native float
        // matrix, not the original double quaternion. A float corner transform uses at most three products and two
        // additions: gamma(5), with unit roundoff epsilon/2. This denominator also bounds the two double additions
        // above. It bounds arithmetic error only: no native shape, padding or query is changed.
        const auto RoundoffBound = Radius / (1.0 - 8.0 * std::numeric_limits<float>::epsilon());
        if (NOT FMath::IsFinite(RoundoffBound) || RoundoffBound <= 0.0
            || RoundoffBound > static_cast<double>(TNumericLimits<float>::Max()))
        { return; }
        _Start = InStart;
        _End = InEnd;
        _HalfExtents = InHalfExtents;
        _Rotation = InRotation;
        _NativeHalfExtents = ck::jolt::Conv(NativeExtents);
        _NativeRotation = ck::jolt::Conv(NativeQuaternion);
        _WorldUpRadius = RoundoffBound;
        _HasObservation = true;
    }

    // ----------------------------------------------------------------------------------------------------------------

    auto
        TryGet_ProceduralBodyPlaneClip(
            const FProceduralBodyEnvelopeEnclosureObservation& InEnclosure,
            const FTransform& InCurrent,
            const FTransform& InRejectedPartial,
            const FCk_Jolt_HitResult& InHit,
            const FVector& InUp)
        -> TOptional<FTransform>
    {
        if (NOT InEnclosure.Get_HasObservation() || InUp != FVector::UpVector
            || NOT ck_procedural_body_envelope::Get_IsRigid(InCurrent)
            || NOT ck_procedural_body_envelope::Get_IsRigid(InRejectedPartial)
            || NOT InEnclosure.Get_Start().Equals(InCurrent, 0.0)
            || NOT InEnclosure.Get_End().Equals(InRejectedPartial, 0.0)
            || InCurrent.GetRotation() != InRejectedPartial.GetRotation()
            || InCurrent.GetScale3D() != InRejectedPartial.GetScale3D()
            || InCurrent.GetLocation().X != InRejectedPartial.GetLocation().X
            || InCurrent.GetLocation().Y != InRejectedPartial.GetLocation().Y
            || InRejectedPartial.GetLocation().Z >= InCurrent.GetLocation().Z
            || NOT InHit.Get_HasHit() || NOT FMath::IsFinite(InHit.Get_Fraction())
            || InHit.Get_Fraction() <= 0.0f || InHit.Get_Fraction() >= 1.0f
            || NOT ck_procedural_body_envelope::Get_IsFloatVector(InHit.Get_Position())
            || InHit.Get_Normal() != FVector::UpVector)
        { return {}; }
        const auto Radius = InEnclosure.Get_WorldUpRadius();
        const auto PlaneAndRadius = InHit.Get_Position().Z + Radius;
        constexpr auto HeightGoalSeparation = 0.1;
        const auto UnrepresentedHeight = PlaneAndRadius + HeightGoalSeparation;
        if (NOT FMath::IsFinite(Radius) || Radius <= 0.0 || NOT FMath::IsFinite(PlaneAndRadius)
            || NOT FMath::IsFinite(UnrepresentedHeight)
            || FMath::Abs(UnrepresentedHeight) > static_cast<double>(TNumericLimits<float>::Max()))
        { return {}; }
        const auto Height = static_cast<double>(static_cast<float>(UnrepresentedHeight));
        if (NOT FMath::IsFinite(Height) || Height <= PlaneAndRadius
            || Height <= InRejectedPartial.GetLocation().Z || Height >= InCurrent.GetLocation().Z)
        { return {}; }
        auto Candidate = InCurrent;
        Candidate.SetLocation(FVector{InCurrent.GetLocation().X, InCurrent.GetLocation().Y, Height});
        return Candidate;
    }
}
